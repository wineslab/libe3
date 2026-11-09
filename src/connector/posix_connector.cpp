/**
 * @file posix_connector.cpp
 * @brief POSIX socket connector implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "posix_connector.hpp"
#include "libe3/logger.hpp"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef LIBE3_ENABLE_SCTP
#include <netinet/sctp.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "PosixConn";
constexpr const char* IPC_BASE_DIR = "/tmp/dapps";
// What E3Config holds for setup_endpoint unless the program sets it. The ZeroMQ link
// shares the string, so the POSIX link maps it to its own default instead of changing it.
constexpr const char* SHARED_DEFAULT_ENDPOINT = "ipc:///tmp/dapps/setup";
constexpr const char* DEFAULT_IPC_PATH = "/tmp/dapps/e3";
constexpr int POLL_TIMEOUT_MS = 500;          // How long one read waits, so shutdown is noticed
constexpr int SEND_TIMEOUT_MS = 500;          // Per-peer send timeout (drop a stalled peer)
constexpr int CONNECT_ATTEMPTS = 10;          // The dApp waits for a RAN that is not up yet
constexpr int CONNECT_PAUSE_MS = 200;
constexpr int LISTEN_BACKLOG = 64;
constexpr size_t MAX_PDU = MAX_PROTOCOL_DATA_SIZE * 2;   // One limit for every transport and direction
constexpr uint32_t SCTP_PPID = 0x00E3A001;    // Project-private; no payload protocol id is registered for E3AP
constexpr int SCTP_PROCEDURE_STREAM = 0;
constexpr int SCTP_INDICATION_STREAM = 1;
constexpr int64_t SETUP_PUMP_AFTER_NS = 1'000'000'000;   // The setup thread reads if receive() has been idle this long

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/** @brief Wait for a socket to become readable: 1 if ready, 0 on timeout, -1 on error. */
int wait_readable(int fd, int timeout_ms) {
    struct pollfd pfd{fd, POLLIN, 0};
    return poll(&pfd, 1, timeout_ms);
}

/**
 * @brief Strip a URI scheme ("ipc://", "tcp://") from an endpoint, leaving a path.
 */
std::string strip_scheme(const std::string& endpoint) {
    auto pos = endpoint.find("://");
    return pos == std::string::npos ? endpoint : endpoint.substr(pos + 3);
}

/**
 * @brief Create a socket of the SCTP protocol.
 *
 * SCTP is optional at build time (LIBE3_ENABLE_SCTP, off by default). The enumerator
 * stays in the ABI either way, so asking for SCTP against a build without it has to
 * fail loudly here rather than fall through to TCP, which would look as if it worked
 * while putting traffic on the wrong protocol. Returns -1 with errno set, like socket().
 */
int sctp_socket(int family) {
#ifdef LIBE3_ENABLE_SCTP
    return socket(family, SOCK_STREAM, IPPROTO_SCTP);
#else
    (void)family;
    E3_LOG_ERROR(LOG_TAG) << "SCTP transport requested but libe3 was built without it. "
                             "Rebuild with -DLIBE3_ENABLE_SCTP=ON (needs libsctp-dev), "
                             "or use the TCP or IPC transport.";
    errno = EPROTONOSUPPORT;
    return -1;
#endif
}

void set_timeout(int fd, int option, int timeout_ms, const char* name) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv)) != 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set " << name << ": " << strerror(errno);
    }
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set O_NONBLOCK: " << strerror(errno);
    }
}

// ---------------------------------------------------------------------------
// One PDU per message
// ---------------------------------------------------------------------------

/** @brief The outcome of looking for the next PDU on a connection. */
enum class Read { PDU, NONE, CLOSED, ERROR };

/**
 * @brief Take a length-prefixed PDU off the front of buffered stream bytes (TCP).
 *
 * The prefix is four bytes in network order; zero and more than MAX_PDU are refused.
 */
Read pop_stream_frame(std::vector<uint8_t>& rx, std::vector<uint8_t>& out) {
    if (rx.size() < sizeof(uint32_t)) {
        return Read::NONE;
    }
    uint32_t size = 0;
    std::memcpy(&size, rx.data(), sizeof(size));
    size = ntohl(size);
    if (size == 0 || size > MAX_PDU) {
        E3_LOG_ERROR(LOG_TAG) << "Invalid PDU length " << size;
        return Read::ERROR;
    }
    if (rx.size() < sizeof(uint32_t) + size) {
        return Read::NONE;
    }
    out.assign(rx.begin() + sizeof(uint32_t), rx.begin() + sizeof(uint32_t) + size);
    rx.erase(rx.begin(), rx.begin() + sizeof(uint32_t) + size);
    return Read::PDU;
}

bool would_block(int error) {
    return error == EINTR || error == EAGAIN || error == EWOULDBLOCK;
}

/**
 * @brief Read from @p fd once and say whether a whole PDU is now available in @p out.
 *
 * @p rx holds what an earlier call read and did not yet make a PDU. TCP: stream bytes,
 * parsed by the length prefix. IPC: one recv is one PDU, and a PDU longer than the
 * buffer is an error rather than a silent cut. SCTP: one recvmsg is one fragment of a
 * message, joined up to MSG_EOR; notifications are skipped, and a payload protocol id
 * other than 0 and the one libe3 stamps is an error.
 */
Read read_one(int fd, E3TransportLayer transport, std::vector<uint8_t>& rx, std::vector<uint8_t>& out) {
    if (transport == E3TransportLayer::TCP) {
        if (Read buffered = pop_stream_frame(rx, out); buffered != Read::NONE) {
            return buffered;
        }
        std::array<uint8_t, 16384> chunk;
        ssize_t n = ::recv(fd, chunk.data(), chunk.size(), 0);
        if (n > 0) {
            rx.insert(rx.end(), chunk.begin(), chunk.begin() + n);
            return pop_stream_frame(rx, out);
        }
        if (n == 0) {
            return Read::CLOSED;
        }
        return would_block(errno) ? Read::NONE : Read::ERROR;
    }

    if (transport == E3TransportLayer::IPC) {
        out.resize(MAX_PDU);
        struct iovec iov{out.data(), out.size()};
        struct msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        ssize_t n = ::recvmsg(fd, &msg, 0);
        if (n < 0) {
            return would_block(errno) ? Read::NONE : Read::ERROR;
        }
        if (n == 0) {
            return Read::CLOSED;
        }
        if (msg.msg_flags & MSG_TRUNC) {
            E3_LOG_ERROR(LOG_TAG) << "PDU longer than " << MAX_PDU << " bytes";
            return Read::ERROR;
        }
        out.resize(static_cast<size_t>(n));
        return Read::PDU;
    }

#ifdef LIBE3_ENABLE_SCTP
    static thread_local std::array<uint8_t, MAX_PDU> chunk;
    alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(struct sctp_rcvinfo))];
    struct iovec iov{chunk.data(), chunk.size()};
    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    ssize_t n = ::recvmsg(fd, &msg, 0);
    if (n < 0) {
        return would_block(errno) ? Read::NONE : Read::ERROR;
    }
    if (n == 0) {
        return Read::CLOSED;
    }
    if (msg.msg_flags & MSG_NOTIFICATION) {
        return Read::NONE;
    }
    if (rx.empty()) {
        for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c != nullptr; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level == IPPROTO_SCTP && c->cmsg_type == SCTP_RCVINFO) {
                struct sctp_rcvinfo info;
                std::memcpy(&info, CMSG_DATA(c), sizeof(info));
                const uint32_t ppid = ntohl(info.rcv_ppid);
                if (ppid != 0 && ppid != SCTP_PPID) {
                    E3_LOG_ERROR(LOG_TAG) << "Message with payload protocol id 0x" << std::hex << ppid;
                    return Read::ERROR;
                }
            }
        }
    }
    rx.insert(rx.end(), chunk.begin(), chunk.begin() + n);
    if (rx.size() > MAX_PDU) {
        E3_LOG_ERROR(LOG_TAG) << "PDU longer than " << MAX_PDU << " bytes";
        return Read::ERROR;
    }
    if (msg.msg_flags & MSG_EOR) {
        out = std::move(rx);
        rx.clear();
        return Read::PDU;
    }
    return Read::NONE;
#else
    (void)rx;
    (void)out;
    return Read::ERROR;
#endif
}

/**
 * @brief Send one PDU as one message.
 *
 * TCP writes the length prefix and the payload together. IPC and SCTP send the payload
 * alone: a sequenced-packet message and an SCTP message each keep their boundary. SCTP
 * stamps the payload protocol id and picks the stream from @p kind.
 */
ErrorCode write_pdu(int fd, E3TransportLayer transport, const std::vector<uint8_t>& data, PduClass kind) {
    if (data.empty() || data.size() > MAX_PDU) {
        E3_LOG_ERROR(LOG_TAG) << "PDU of " << data.size() << " bytes cannot be sent";
        return ErrorCode::INVALID_PARAM;
    }

    if (transport == E3TransportLayer::TCP) {
        uint32_t prefix = htonl(static_cast<uint32_t>(data.size()));
        struct iovec iov[2];
        iov[0].iov_base = &prefix;
        iov[0].iov_len = sizeof(prefix);
        iov[1].iov_base = const_cast<uint8_t*>(data.data());
        iov[1].iov_len = data.size();
        size_t first = 0;
        const size_t total = sizeof(prefix) + data.size();
        size_t sent = 0;
        while (sent < total) {
            struct iovec cur[2] = {iov[0], iov[1]};
            size_t skip = sent;
            first = 0;
            while (first < 2 && skip >= cur[first].iov_len) {
                skip -= cur[first].iov_len;
                ++first;
            }
            cur[first].iov_base = static_cast<uint8_t*>(cur[first].iov_base) + skip;
            cur[first].iov_len -= skip;
            struct msghdr msg{};
            msg.msg_iov = cur + first;
            msg.msg_iovlen = 2 - first;
            ssize_t n = ::sendmsg(fd, &msg, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                E3_LOG_ERROR(LOG_TAG) << "Failed to send: " << strerror(errno);
                return ErrorCode::TRANSPORT_ERROR;
            }
            sent += static_cast<size_t>(n);
        }
        return ErrorCode::SUCCESS;
    }

    if (transport == E3TransportLayer::IPC) {
        ssize_t n;
        do {
            n = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        } while (n < 0 && errno == EINTR);
        if (n < 0 || static_cast<size_t>(n) != data.size()) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to send: " << strerror(errno);
            return ErrorCode::TRANSPORT_ERROR;
        }
        return ErrorCode::SUCCESS;
    }

#ifdef LIBE3_ENABLE_SCTP
    alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(struct sctp_sndinfo))]{};
    struct iovec iov{const_cast<uint8_t*>(data.data()), data.size()};
    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = IPPROTO_SCTP;
    c->cmsg_type = SCTP_SNDINFO;
    c->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));
    struct sctp_sndinfo info{};
    info.snd_sid = kind == PduClass::INDICATION ? SCTP_INDICATION_STREAM : SCTP_PROCEDURE_STREAM;
    info.snd_ppid = htonl(SCTP_PPID);
    std::memcpy(CMSG_DATA(c), &info, sizeof(info));
    ssize_t n;
    do {
        n = ::sendmsg(fd, &msg, MSG_NOSIGNAL);
    } while (n < 0 && errno == EINTR);
    if (n < 0 || static_cast<size_t>(n) != data.size()) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send: " << strerror(errno);
        return ErrorCode::TRANSPORT_ERROR;
    }
    return ErrorCode::SUCCESS;
#else
    (void)kind;
    return ErrorCode::TRANSPORT_ERROR;
#endif
}

/** @brief The text of a peer's address, with an IPv4-mapped IPv6 address shown as IPv4. */
std::string address_text(const sockaddr_storage& ss) {
    char text[INET6_ADDRSTRLEN] = {};
    if (ss.ss_family == AF_INET) {
        const auto* in = reinterpret_cast<const sockaddr_in*>(&ss);
        inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text));
    } else if (ss.ss_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(&ss);
        if (IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr)) {
            inet_ntop(AF_INET, &in6->sin6_addr.s6_addr[12], text, sizeof(text));
        } else {
            inet_ntop(AF_INET6, &in6->sin6_addr, text, sizeof(text));
        }
    }
    return text;
}
} // anonymous namespace

/** @brief One accepted connection of a RAN. Closes its socket when the last reference goes. */
struct PosixE3Connector::Peer {
    int fd;                            ///< The connection
    uint64_t token;                    ///< Never reused by this connector
    std::vector<uint8_t> rx;           ///< Bytes read and not yet a whole PDU; the reader's alone
    bool seen_first{false};            ///< The first PDU, the setup request, was read; the reader's alone
    std::mutex write_mutex;            ///< One writer at a time

    /**
     * @brief Take over an accepted connection.
     * @param f The connected socket, which this object closes
     * @param t The token that names the connection
     */
    Peer(int f, uint64_t t) : fd(f), token(t) {}

    /** @brief Close the socket. */
    ~Peer() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

/** @brief A PDU read from a peer and waiting for the thread that takes it. */
struct PosixE3Connector::Queued {
    std::shared_ptr<Peer> peer;        ///< Who sent it
    uint64_t token{0};                 ///< The sender's token
    std::vector<uint8_t> data;         ///< The PDU
};

PosixE3Connector::PosixE3Connector(
    E3TransportLayer transport_layer,
    const std::string& endpoint,
    uint16_t port,
    const PosixOptions& options
)
    : transport_layer_(transport_layer)
    , endpoint_uri_(endpoint)
    , port_(port)
    , options_(options)
{
    if (transport_layer == E3TransportLayer::IPC) {
        setup_endpoint_ = endpoint == SHARED_DEFAULT_ENDPOINT ? DEFAULT_IPC_PATH : strip_scheme(endpoint);
    } else {
        setup_endpoint_ = endpoint;
    }
    E3_LOG_INFO(LOG_TAG) << "Creating POSIX connector, one endpoint: " << setup_endpoint_
                         << " port " << port_;
}

PosixE3Connector::~PosixE3Connector() {
    dispose();
}

PosixE3Connector::Target PosixE3Connector::target() const {
    Target where{"", std::to_string(port_)};
    std::string rest;
    for (const char* scheme : {"tcp://", "sctp://"}) {
        if (endpoint_uri_.rfind(scheme, 0) == 0) {
            rest = endpoint_uri_.substr(std::strlen(scheme));
            break;
        }
    }
    if (rest.empty()) {
        return where;
    }
    if (rest[0] == '[') {
        const auto close = rest.find(']');
        if (close == std::string::npos) {
            return where;
        }
        where.host = rest.substr(1, close - 1);
        if (close + 1 < rest.size() && rest[close + 1] == ':') {
            where.port = rest.substr(close + 2);
        }
    } else {
        const auto colon = rest.rfind(':');
        if (colon != std::string::npos && rest.find(':') == colon) {
            where.host = rest.substr(0, colon);
            where.port = rest.substr(colon + 1);
        } else {
            where.host = rest;
        }
    }
    if (where.port.empty()) {
        where.port = std::to_string(port_);
    }
    return where;
}

int PosixE3Connector::make_socket(int family) const {
    switch (transport_layer_) {
        case E3TransportLayer::IPC:
            return socket(AF_UNIX, SOCK_SEQPACKET, 0);
        case E3TransportLayer::SCTP:
            return sctp_socket(family);
        default:
            return socket(family, SOCK_STREAM, 0);
    }
}

void PosixE3Connector::tune_socket(int fd, SocketKind kind) const {
    if (transport_layer_ == E3TransportLayer::IPC) {
        if (kind == SocketKind::ACCEPTED) {
            set_timeout(fd, SO_SNDTIMEO, SEND_TIMEOUT_MS, "SO_SNDTIMEO");
        }
        return;
    }
    const int one = 1;
    if (kind == SocketKind::LISTENER) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
    if (kind != SocketKind::LISTENER) {
        set_timeout(fd, SO_SNDTIMEO, SEND_TIMEOUT_MS, "SO_SNDTIMEO");
    }
    if (transport_layer_ == E3TransportLayer::TCP) {
        if (kind != SocketKind::LISTENER && setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(LOG_TAG) << "Failed to set TCP_NODELAY: " << strerror(errno);
        }
        return;
    }
#ifdef LIBE3_ENABLE_SCTP
    if (kind != SocketKind::ACCEPTED) {
        // Before the association exists, on both peers: two streams, and buffers and
        // delayed SACK that do not stall a message larger than the Linux defaults allow.
        struct sctp_initmsg init{};
        init.sinit_num_ostreams = 2;
        init.sinit_max_instreams = 2;
        if (setsockopt(fd, IPPROTO_SCTP, SCTP_INITMSG, &init, sizeof(init)) != 0) {
            E3_LOG_WARN(LOG_TAG) << "Failed to request two SCTP streams: " << strerror(errno);
        }
        if (options_.socket_buffer_bytes > 0) {
            const int size = static_cast<int>(options_.socket_buffer_bytes);
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        }
        if (!options_.sctp_delayed_sack) {
            struct sctp_sack_info sack{};
            sack.sack_assoc_id = 0;
            sack.sack_delay = 0;
            sack.sack_freq = 1;
            if (setsockopt(fd, IPPROTO_SCTP, SCTP_DELAYED_SACK, &sack, sizeof(sack)) != 0) {
                E3_LOG_WARN(LOG_TAG) << "Failed to turn delayed SACK off: " << strerror(errno);
            }
        }
    }
    if (kind != SocketKind::LISTENER) {
        if (setsockopt(fd, IPPROTO_SCTP, SCTP_NODELAY, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(LOG_TAG) << "Failed to set SCTP_NODELAY: " << strerror(errno);
        }
        if (setsockopt(fd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(LOG_TAG) << "Failed to ask for the SCTP receive info: " << strerror(errno);
        }
    }
#endif
}

int PosixE3Connector::listen_on(const Target& where) {
    int fd = -1;
    int result = -1;
    if (transport_layer_ == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1 && mkdir(IPC_BASE_DIR, 0777) == -1 && errno != EEXIST) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to create IPC directory: " << strerror(errno);
            return -1;
        }
        chmod(IPC_BASE_DIR, 0777);
        if (setup_endpoint_.size() >= sizeof(sockaddr_un::sun_path)) {
            E3_LOG_ERROR(LOG_TAG) << "IPC path is too long: " << setup_endpoint_;
            return -1;
        }
        fd = make_socket(AF_UNIX);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, setup_endpoint_.c_str(), setup_endpoint_.size());
        unlink(setup_endpoint_.c_str());
        result = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    } else if (where.host.empty()) {
        // Any address, both families: a dual-stack socket, or IPv4 alone where there is no IPv6.
        fd = make_socket(AF_INET6);
        if (fd >= 0) {
            const int off = 0;
            setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
            tune_socket(fd, SocketKind::LISTENER);
            struct sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_addr = in6addr_any;
            addr.sin6_port = htons(static_cast<uint16_t>(std::stoi(where.port)));
            result = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
            if (result < 0) {
                close(fd);
                fd = -1;
            }
        }
        if (fd < 0) {
            fd = make_socket(AF_INET);
            if (fd < 0) {
                return -1;
            }
            tune_socket(fd, SocketKind::LISTENER);
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(static_cast<uint16_t>(std::stoi(where.port)));
            result = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        }
    } else {
        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        struct addrinfo* found = nullptr;
        if (getaddrinfo(where.host.c_str(), where.port.c_str(), &hints, &found) != 0 || found == nullptr) {
            E3_LOG_ERROR(LOG_TAG) << "Cannot resolve " << where.host;
            return -1;
        }
        fd = make_socket(found->ai_family);
        if (fd >= 0) {
            tune_socket(fd, SocketKind::LISTENER);
            result = bind(fd, found->ai_addr, found->ai_addrlen);
        }
        freeaddrinfo(found);
        if (fd < 0) {
            return -1;
        }
    }
    if (result < 0 || listen(fd, LISTEN_BACKLOG) < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to listen on " << setup_endpoint_ << ": " << strerror(errno);
        close(fd);
        return -1;
    }
    set_nonblocking(fd);
    return fd;
}

int PosixE3Connector::connect_to(const Target& where) {
    if (transport_layer_ == E3TransportLayer::IPC) {
        if (setup_endpoint_.size() >= sizeof(sockaddr_un::sun_path)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        int fd = make_socket(AF_UNIX);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, setup_endpoint_.c_str(), setup_endpoint_.size());
        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            const int saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }
        tune_socket(fd, SocketKind::CLIENT);
        return fd;
    }
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* found = nullptr;
    const std::string host = where.host.empty() ? "127.0.0.1" : where.host;
    if (getaddrinfo(host.c_str(), where.port.c_str(), &hints, &found) != 0 || found == nullptr) {
        errno = EHOSTUNREACH;
        return -1;
    }
    int fd = -1;
    int saved = ECONNREFUSED;
    for (struct addrinfo* a = found; a != nullptr && fd < 0; a = a->ai_next) {
        int candidate = make_socket(a->ai_family);
        if (candidate < 0) {
            saved = errno;
            continue;
        }
        tune_socket(candidate, SocketKind::CLIENT);
        if (connect(candidate, a->ai_addr, a->ai_addrlen) == 0) {
            fd = candidate;
        } else {
            saved = errno;
            close(candidate);
        }
    }
    freeaddrinfo(found);
    if (fd < 0) {
        errno = saved;
    }
    return fd;
}

bool PosixE3Connector::admit(const void* address) const {
    if (transport_layer_ == E3TransportLayer::IPC) {
        return true;
    }
    const auto& ss = *static_cast<const sockaddr_storage*>(address);
    if (!options_.allowed_peers.empty()) {
        const std::string text = address_text(ss);
        if (std::find(options_.allowed_peers.begin(), options_.allowed_peers.end(), text)
                == options_.allowed_peers.end()) {
            E3_LOG_WARN(LOG_TAG) << "Refused a connection from " << text << ": not in the allowed peers";
            return false;
        }
    }
    if (options_.maximum_associations > 0) {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        if (peers_.size() >= options_.maximum_associations) {
            E3_LOG_WARN(LOG_TAG) << "Refused a connection from " << address_text(ss)
                                 << ": " << options_.maximum_associations << " peers are connected";
            return false;
        }
    }
    return true;
}

// ===========================================================================
// RAN role
// ===========================================================================

ErrorCode PosixE3Connector::setup_initial_connection() {
    listener_ = listen_on(target());
    if (listener_ < 0) {
        return ErrorCode::CONNECTION_FAILED;
    }
    connected_ = true;
    E3_LOG_INFO(LOG_TAG) << "Listening on " << setup_endpoint_ << " port " << port_;
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::setup_inbound_connection() {
    // The endpoint that setup_initial_connection() opened carries the inbound traffic too.
    return listener_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_INITIALIZED;
}

ErrorCode PosixE3Connector::setup_outbound_connection() {
    // The same connections carry the outbound traffic.
    return listener_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_INITIALIZED;
}

void PosixE3Connector::accept_pending() {
    while (listener_ >= 0 && wait_readable(listener_, 0) == 1) {
        sockaddr_storage address{};
        socklen_t length = sizeof(address);
        int fd = accept(listener_, reinterpret_cast<struct sockaddr*>(&address), &length);
        if (fd < 0) {
            break;  // Nothing more to accept now
        }
        if (!admit(&address)) {
            close(fd);
            continue;
        }
        tune_socket(fd, SocketKind::ACCEPTED);
        auto peer = std::make_shared<Peer>(fd, next_token_++);
        size_t count;
        {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            peers_.push_back(peer);
            count = peers_.size();
        }
        E3_LOG_INFO(LOG_TAG) << "Peer connected (" << count << " total)";
    }
}

std::shared_ptr<PosixE3Connector::Peer> PosixE3Connector::find_peer(uint64_t token) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& peer : peers_) {
        if (peer->token == token) {
            return peer;
        }
    }
    return nullptr;
}

void PosixE3Connector::drop_peer(const std::shared_ptr<Peer>& peer) {
    ::shutdown(peer->fd, SHUT_RDWR);
    size_t remaining;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.erase(std::remove(peers_.begin(), peers_.end(), peer), peers_.end());
        remaining = peers_.size();
    }
    {
        std::lock_guard<std::mutex> lock(closed_mutex_);
        closed_peers_.push_back(peer->token);
    }
    if (!peer->seen_first) {
        E3_LOG_INFO(LOG_TAG) << "A connection closed before it sent a complete setup request. A dApp built "
                                "before libe3 0.6 uses three connections and unframed messages, which "
                                "this endpoint does not serve.";
    }
    E3_LOG_INFO(LOG_TAG) << "Peer disconnected (" << remaining << " remain)";
}

void PosixE3Connector::push_setup(const std::shared_ptr<Peer>& peer, std::vector<uint8_t>&& pdu) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        setup_queue_.push_back(Queued{peer, peer->token, std::move(pdu)});
    }
    setup_ready_.notify_one();
}

bool PosixE3Connector::receiver_idle() const {
    return now_ns() - last_receive_ns_.load() > SETUP_PUMP_AFTER_NS;
}

/**
 * Look for one PDU for the inbound path, for at most @p timeout_ms. Whoever calls this
 * holds reader_mutex_. The first PDU of a connection is the setup request and is queued
 * for recv_setup_request() instead. A connection that closed is dropped only after every
 * PDU it had sent was handed on.
 */
bool PosixE3Connector::pump(std::vector<uint8_t>& out, uint64_t& token, int timeout_ms) {
    std::vector<std::shared_ptr<Peer>> snapshot;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        snapshot = peers_;
    }

    // Stream bytes read earlier may already hold a whole PDU.
    for (const auto& peer : snapshot) {
        if (transport_layer_ != E3TransportLayer::TCP) {
            break;
        }
        while (!peer->rx.empty()) {
            Read r = pop_stream_frame(peer->rx, out);
            if (r == Read::ERROR) {
                drop_peer(peer);
                break;
            }
            if (r != Read::PDU) {
                break;
            }
            if (!peer->seen_first) {
                peer->seen_first = true;
                push_setup(peer, std::move(out));
                continue;
            }
            token = peer->token;
            return true;
        }
    }

    std::vector<struct pollfd> pfds;
    pfds.reserve(1 + snapshot.size());
    pfds.push_back({listener_, POLLIN, 0});
    for (const auto& peer : snapshot) {
        pfds.push_back({peer->fd, POLLIN, 0});
    }
    int ready = poll(pfds.data(), pfds.size(), timeout_ms);
    if (ready <= 0) {
        return false;
    }
    if (pfds[0].revents & POLLIN) {
        accept_pending();
    }

    const size_t count = snapshot.size();
    for (size_t k = 0; k < count; ++k) {
        const size_t index = (round_robin_ + k) % count;
        const auto& peer = snapshot[index];
        if (!(pfds[index + 1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
            continue;
        }
        Read r = read_one(peer->fd, transport_layer_, peer->rx, out);
        if (r == Read::PDU) {
            if (!peer->seen_first) {
                peer->seen_first = true;
                push_setup(peer, std::move(out));
                continue;
            }
            round_robin_ = index + 1;
            token = peer->token;
            return true;
        }
        if (r == Read::ERROR && !peer->seen_first && transport_layer_ == E3TransportLayer::TCP) {
            E3_LOG_INFO(LOG_TAG) << "The first message is not a length-prefixed PDU. A dApp built before "
                                    "libe3 0.6 sends its setup request without a prefix.";
        }
        if (r == Read::CLOSED || r == Read::ERROR
            || (r == Read::NONE && (pfds[index + 1].revents & (POLLERR | POLLNVAL)))) {
            drop_peer(peer);
        }
    }
    if (count > 0) {
        round_robin_ = (round_robin_ + 1) % count;
    }
    return false;
}

int PosixE3Connector::receive(std::vector<uint8_t>& buffer) {
    if (role_ == E3Role::DAPP) {
        return receive_client(buffer);
    }
    if (listener_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    last_receive_ns_.store(now_ns());
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!inbound_queue_.empty()) {
            Queued item = std::move(inbound_queue_.front());
            inbound_queue_.pop_front();
            buffer = std::move(item.data);
            last_peer_token_ = item.token;
            return static_cast<int>(buffer.size());
        }
    }
    std::lock_guard<std::mutex> reader(reader_mutex_);
    uint64_t token = 0;
    if (pump(buffer, token, POLL_TIMEOUT_MS)) {
        last_peer_token_ = token;
        return static_cast<int>(buffer.size());
    }
    return 0;
}

int PosixE3Connector::recv_setup_request(std::vector<uint8_t>& buffer) {
    if (listener_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(POLL_TIMEOUT_MS);
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!setup_queue_.empty()) {
                Queued item = std::move(setup_queue_.front());
                setup_queue_.pop_front();
                buffer = std::move(item.data);
                setup_peer_ = item.peer;
                last_setup_token_ = item.token;
                return static_cast<int>(buffer.size());
            }
        }
        if (shutdown_requested_.load() || std::chrono::steady_clock::now() >= deadline) {
            return 0;
        }
        // The inbound thread is the reader. When nothing calls receive() (a caller that only
        // serves setup), this thread reads instead.
        if (receiver_idle() && reader_mutex_.try_lock()) {
            std::vector<uint8_t> pdu;
            uint64_t token = 0;
            if (pump(pdu, token, 100)) {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                inbound_queue_.push_back(Queued{nullptr, token, std::move(pdu)});
            }
            reader_mutex_.unlock();
        } else {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            setup_ready_.wait_for(lock, std::chrono::milliseconds(50),
                                  [this] { return !setup_queue_.empty() || shutdown_requested_.load(); });
        }
    }
}

ErrorCode PosixE3Connector::send_response(const std::vector<uint8_t>& data) {
    std::shared_ptr<Peer> peer = setup_peer_;
    if (!peer) {
        return ErrorCode::NOT_CONNECTED;
    }
    std::lock_guard<std::mutex> lock(peer->write_mutex);
    return write_pdu(peer->fd, transport_layer_, data, PduClass::PROCEDURE);
}

ErrorCode PosixE3Connector::send(const std::vector<uint8_t>& data) {
    return send_to(0, data, PduClass::PROCEDURE);
}

ErrorCode PosixE3Connector::send_to(uint64_t peer_token, const std::vector<uint8_t>& data, PduClass kind) {
    // Checked first: shutdown() leaves the fd valid until dispose(), so a send racing it
    // would otherwise be a real EPIPE, indistinguishable from a transport failure.
    if (shutdown_requested_.load()) {
        return ErrorCode::CANCELLED;
    }
    if (role_ == E3Role::DAPP) {
        if (client_fd_ < 0) {
            return ErrorCode::NOT_CONNECTED;
        }
        std::lock_guard<std::mutex> lock(client_write_mutex_);
        return write_pdu(client_fd_, transport_layer_, data, PduClass::PROCEDURE);
    }
    if (listener_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }

    std::vector<std::shared_ptr<Peer>> targets;
    if (peer_token == 0) {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        targets = peers_;
    } else if (auto peer = find_peer(peer_token)) {
        targets.push_back(peer);
    } else {
        return ErrorCode::NOT_CONNECTED;
    }

    bool failed = false;
    size_t sent = 0;
    for (const auto& peer : targets) {
        std::lock_guard<std::mutex> lock(peer->write_mutex);
        if (write_pdu(peer->fd, transport_layer_, data, kind) != ErrorCode::SUCCESS) {
            // Dead or stalled: closing it makes the reader drop it and report it closed.
            ::shutdown(peer->fd, SHUT_RDWR);
            failed = true;
        } else {
            ++sent;
        }
    }
    if (failed && sent == 0) {
        return ErrorCode::TRANSPORT_ERROR;
    }
    return ErrorCode::SUCCESS;
}

uint64_t PosixE3Connector::last_peer_token() const {
    return last_peer_token_;
}

uint64_t PosixE3Connector::last_setup_peer_token() const {
    return last_setup_token_;
}

std::vector<uint64_t> PosixE3Connector::take_closed_peers() {
    std::lock_guard<std::mutex> lock(closed_mutex_);
    std::vector<uint64_t> closed;
    closed.swap(closed_peers_);
    return closed;
}

// ===========================================================================
// dApp role: one connection
// ===========================================================================

ErrorCode PosixE3Connector::setup_initial_connection_client() {
    const Target where = target();
    int fd = -1;
    for (int attempt = 0; attempt < CONNECT_ATTEMPTS && fd < 0; ++attempt) {
        fd = connect_to(where);
        if (fd < 0) {
            if (shutdown_requested_.load()) {
                return ErrorCode::CANCELLED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_PAUSE_MS));
        }
    }
    if (fd < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect to " << setup_endpoint_ << " port " << where.port
                              << ": " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    client_fd_ = fd;
    client_rx_.clear();
    // Until the setup response is read, the setup thread is the only reader: the inbound
    // thread must not take it for an indication.
    awaiting_setup_response_.store(true);
    connected_ = true;
    E3_LOG_INFO(LOG_TAG) << "Connected to " << setup_endpoint_ << " port " << where.port;
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::reconnect_client() {
    if (client_fd_ >= 0) {
        close(client_fd_);
        client_fd_ = -1;
    }
    return setup_initial_connection_client();
}

ErrorCode PosixE3Connector::send_setup_request_client(const std::vector<uint8_t>& data) {
    if (client_fd_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }
    // The RAN takes the first PDU of a connection for the setup request, so a second
    // attempt needs a connection of its own, as a ZeroMQ request socket is reopened.
    if (setup_request_sent_) {
        ErrorCode rc = reconnect_client();
        if (rc != ErrorCode::SUCCESS) {
            return rc;
        }
    }
    setup_request_sent_ = true;
    std::lock_guard<std::mutex> lock(client_write_mutex_);
    return write_pdu(client_fd_, transport_layer_, data, PduClass::PROCEDURE);
}

int PosixE3Connector::recv_setup_response_client(std::vector<uint8_t>& buffer) {
    if (client_fd_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    if (!awaiting_setup_response_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    std::lock_guard<std::mutex> reader(reader_mutex_);
    Read r = Read::NONE;
    if (transport_layer_ == E3TransportLayer::TCP) {
        r = pop_stream_frame(client_rx_, buffer);
    }
    if (r == Read::NONE) {
        const int ready = wait_readable(client_fd_, POLL_TIMEOUT_MS);
        if (ready == 0) {
            return 0;
        }
        if (ready < 0) {
            return errno == EINTR ? 0 : static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        r = read_one(client_fd_, transport_layer_, client_rx_, buffer);
    }
    if (r == Read::PDU) {
        awaiting_setup_response_.store(false);
        await_cv_.notify_all();
        return static_cast<int>(buffer.size());
    }
    if (r == Read::CLOSED) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    return r == Read::ERROR ? static_cast<int>(ErrorCode::TRANSPORT_ERROR) : 0;
}

ErrorCode PosixE3Connector::setup_inbound_connection_client() {
    return client_fd_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_CONNECTED;
}

ErrorCode PosixE3Connector::setup_outbound_connection_client() {
    return client_fd_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_CONNECTED;
}

int PosixE3Connector::receive_client(std::vector<uint8_t>& buffer) {
    if (client_fd_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    if (awaiting_setup_response_.load()) {
        std::unique_lock<std::mutex> lock(await_mutex_);
        await_cv_.wait_for(lock, std::chrono::milliseconds(POLL_TIMEOUT_MS), [this] {
            return !awaiting_setup_response_.load() || shutdown_requested_.load();
        });
        if (awaiting_setup_response_.load()) {
            return 0;
        }
    }
    std::lock_guard<std::mutex> reader(reader_mutex_);
    Read r = Read::NONE;
    if (transport_layer_ == E3TransportLayer::TCP) {
        r = pop_stream_frame(client_rx_, buffer);
    }
    if (r == Read::NONE) {
        const int ready = wait_readable(client_fd_, POLL_TIMEOUT_MS);
        if (ready == 0) {
            return 0;
        }
        if (ready < 0) {
            return errno == EINTR ? 0 : static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        r = read_one(client_fd_, transport_layer_, client_rx_, buffer);
    }
    switch (r) {
        case Read::PDU:
            return static_cast<int>(buffer.size());
        case Read::CLOSED:
            return static_cast<int>(ErrorCode::NOT_CONNECTED);
        case Read::ERROR:
            return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        default:
            return 0;
    }
}

// ===========================================================================
// Shutdown
// ===========================================================================

void PosixE3Connector::shutdown() {
    E3_LOG_DEBUG(LOG_TAG) << "Shutting down POSIX connector";
    shutdown_requested_.store(true);
    setup_ready_.notify_all();
    await_cv_.notify_all();
    if (listener_ >= 0) {
        ::shutdown(listener_, SHUT_RDWR);
    }
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& peer : peers_) {
            ::shutdown(peer->fd, SHUT_RDWR);
        }
    }
    if (client_fd_ >= 0) {
        ::shutdown(client_fd_, SHUT_RDWR);
    }
}

void PosixE3Connector::dispose() {
    E3_LOG_DEBUG(LOG_TAG) << "Disposing POSIX connector";
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        setup_queue_.clear();
        inbound_queue_.clear();
    }
    setup_peer_.reset();
    {
        std::lock_guard<std::mutex> lock(closed_mutex_);
        closed_peers_.clear();
    }
    if (client_fd_ >= 0) {
        close(client_fd_);
        client_fd_ = -1;
    }
    if (listener_ >= 0) {
        close(listener_);
        listener_ = -1;
        // Only the RAN, which binds, has a socket file to remove. The directory is shared
        // between agents and tests and stays, so that removing it cannot race a bind in it.
        if (transport_layer_ == E3TransportLayer::IPC) {
            unlink(setup_endpoint_.c_str());
        }
    }
    connected_ = false;
    E3_LOG_INFO(LOG_TAG) << "POSIX connector disposed";
}

} // namespace libe3
