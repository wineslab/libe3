/**
 * @file socket_link.cpp
 * @brief Endpoint, sockets and framing shared by the POSIX and the io_uring link
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "socket_link.hpp"
#include "libe3/logger.hpp"

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
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace libe3 {
namespace socket_link {

namespace {

/**
 * @brief Create a socket of the SCTP protocol.
 *
 * SCTP is optional at build time (LIBE3_ENABLE_SCTP, off by default). The enumerator
 * stays in the ABI either way, so asking for SCTP against a build without it has to
 * fail loudly here rather than fall through to TCP, which would look as if it worked
 * while putting traffic on the wrong protocol. Returns -1 with errno set, like socket().
 */
int sctp_socket(int family, const char* log_tag) {
#ifdef LIBE3_ENABLE_SCTP
    (void)log_tag;
    return socket(family, SOCK_STREAM, IPPROTO_SCTP);
#else
    (void)family;
    E3_LOG_ERROR(log_tag) << "SCTP transport requested but libe3 was built without it. "
                             "Rebuild with -DLIBE3_ENABLE_SCTP=ON (needs libsctp-dev), "
                             "or use the TCP or IPC transport.";
    errno = EPROTONOSUPPORT;
    return -1;
#endif
}

void set_timeout(int fd, int option, int timeout_ms, const char* name, const char* log_tag) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv)) != 0) {
        E3_LOG_WARN(log_tag) << "Failed to set " << name << ": " << strerror(errno);
    }
}

void set_nonblocking(int fd, const char* log_tag) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        E3_LOG_WARN(log_tag) << "Failed to set O_NONBLOCK: " << strerror(errno);
    }
}

}  // namespace

std::string strip_scheme(const std::string& endpoint) {
    auto pos = endpoint.find("://");
    return pos == std::string::npos ? endpoint : endpoint.substr(pos + 3);
}

std::string link_endpoint(E3TransportLayer transport, const std::string& endpoint) {
    if (transport == E3TransportLayer::IPC) {
        return endpoint == SHARED_DEFAULT_ENDPOINT ? DEFAULT_IPC_PATH : strip_scheme(endpoint);
    }
    return endpoint;
}

Target parse_target(const std::string& endpoint_uri, uint16_t port) {
    Target where{"", std::to_string(port)};
    std::string rest;
    for (const char* scheme : {"tcp://", "sctp://"}) {
        if (endpoint_uri.rfind(scheme, 0) == 0) {
            rest = endpoint_uri.substr(std::strlen(scheme));
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
        where.port = std::to_string(port);
    }
    return where;
}

int make_socket(const SocketSpec& spec, int family) {
    switch (spec.transport) {
        case E3TransportLayer::IPC:
            return socket(AF_UNIX, SOCK_SEQPACKET, 0);
        case E3TransportLayer::SCTP:
            return sctp_socket(family, spec.log_tag);
        default:
            return socket(family, SOCK_STREAM, 0);
    }
}

void tune_socket(const SocketSpec& spec, int fd, SocketKind kind) {
    const char* tag = spec.log_tag;
    if (spec.transport == E3TransportLayer::IPC) {
        if (kind == SocketKind::ACCEPTED) {
            set_timeout(fd, SO_SNDTIMEO, SEND_TIMEOUT_MS, "SO_SNDTIMEO", tag);
        }
        return;
    }
    const int one = 1;
    if (kind == SocketKind::LISTENER) {
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
    if (kind != SocketKind::LISTENER) {
        set_timeout(fd, SO_SNDTIMEO, SEND_TIMEOUT_MS, "SO_SNDTIMEO", tag);
    }
    if (spec.transport == E3TransportLayer::TCP) {
        if (kind != SocketKind::LISTENER && setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(tag) << "Failed to set TCP_NODELAY: " << strerror(errno);
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
            E3_LOG_WARN(tag) << "Failed to request two SCTP streams: " << strerror(errno);
        }
        if (spec.options.socket_buffer_bytes > 0) {
            const int size = static_cast<int>(spec.options.socket_buffer_bytes);
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
        }
        if (!spec.options.sctp_delayed_sack) {
            struct sctp_sack_info sack{};
            sack.sack_assoc_id = 0;
            sack.sack_delay = 0;
            sack.sack_freq = 1;
            if (setsockopt(fd, IPPROTO_SCTP, SCTP_DELAYED_SACK, &sack, sizeof(sack)) != 0) {
                E3_LOG_WARN(tag) << "Failed to turn delayed SACK off: " << strerror(errno);
            }
        }
    }
    struct sctp_rtoinfo rto{};
    rto.srto_initial = SCTP_RTO_INITIAL_MS;
    rto.srto_min = SCTP_RTO_MIN_MS;
    rto.srto_max = SCTP_RTO_MAX_MS;
    if (setsockopt(fd, IPPROTO_SCTP, SCTP_RTOINFO, &rto, sizeof(rto)) != 0) {
        E3_LOG_WARN(tag) << "Failed to set the SCTP retransmission timeout: " << strerror(errno);
    }
    if (kind != SocketKind::LISTENER) {
        if (setsockopt(fd, IPPROTO_SCTP, SCTP_NODELAY, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(tag) << "Failed to set SCTP_NODELAY: " << strerror(errno);
        }
        if (setsockopt(fd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &one, sizeof(one)) != 0) {
            E3_LOG_WARN(tag) << "Failed to ask for the SCTP receive info: " << strerror(errno);
        }
    }
#endif
}

int listen_on(const SocketSpec& spec, bool nonblocking) {
    const char* tag = spec.log_tag;
    const Target& where = spec.target;
    int fd = -1;
    int result = -1;
    if (spec.transport == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1 && mkdir(IPC_BASE_DIR, 0777) == -1 && errno != EEXIST) {
            E3_LOG_ERROR(tag) << "Failed to create IPC directory: " << strerror(errno);
            return -1;
        }
        chmod(IPC_BASE_DIR, 0777);
        if (spec.path.size() >= sizeof(sockaddr_un::sun_path)) {
            E3_LOG_ERROR(tag) << "IPC path is too long: " << spec.path;
            return -1;
        }
        fd = make_socket(spec, AF_UNIX);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, spec.path.c_str(), spec.path.size());
        unlink(spec.path.c_str());
        result = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    } else if (where.host.empty()) {
        // Any address, both families: a dual-stack socket, or IPv4 alone where there is no IPv6.
        fd = make_socket(spec, AF_INET6);
        if (fd >= 0) {
            const int off = 0;
            setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
            tune_socket(spec, fd, SocketKind::LISTENER);
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
            fd = make_socket(spec, AF_INET);
            if (fd < 0) {
                return -1;
            }
            tune_socket(spec, fd, SocketKind::LISTENER);
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
            E3_LOG_ERROR(tag) << "Cannot resolve " << where.host;
            return -1;
        }
        fd = make_socket(spec, found->ai_family);
        if (fd >= 0) {
            tune_socket(spec, fd, SocketKind::LISTENER);
            result = bind(fd, found->ai_addr, found->ai_addrlen);
        }
        freeaddrinfo(found);
        if (fd < 0) {
            return -1;
        }
    }
    if (result < 0 || listen(fd, LISTEN_BACKLOG) < 0) {
        E3_LOG_ERROR(tag) << "Failed to listen on "
                          << (spec.transport == E3TransportLayer::IPC ? spec.path : where.host + ":" + where.port)
                          << ": " << strerror(errno);
        close(fd);
        return -1;
    }
    if (nonblocking) {
        set_nonblocking(fd, tag);
    }
    return fd;
}

int connect_to(const SocketSpec& spec) {
    if (spec.transport == E3TransportLayer::IPC) {
        if (spec.path.size() >= sizeof(sockaddr_un::sun_path)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        int fd = make_socket(spec, AF_UNIX);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, spec.path.c_str(), spec.path.size());
        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            const int saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }
        tune_socket(spec, fd, SocketKind::CLIENT);
        return fd;
    }
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* found = nullptr;
    const std::string host = spec.target.host.empty() ? "127.0.0.1" : spec.target.host;
    if (getaddrinfo(host.c_str(), spec.target.port.c_str(), &hints, &found) != 0 || found == nullptr) {
        errno = EHOSTUNREACH;
        return -1;
    }
    int fd = -1;
    int saved = ECONNREFUSED;
    for (struct addrinfo* a = found; a != nullptr && fd < 0; a = a->ai_next) {
        int candidate = make_socket(spec, a->ai_family);
        if (candidate < 0) {
            saved = errno;
            continue;
        }
        tune_socket(spec, candidate, SocketKind::CLIENT);
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

bool admit(const SocketSpec& spec, const sockaddr_storage& address, size_t connected) {
    if (spec.transport == E3TransportLayer::IPC) {
        return true;
    }
    if (!spec.options.allowed_peers.empty()) {
        const std::string text = address_text(address);
        if (std::find(spec.options.allowed_peers.begin(), spec.options.allowed_peers.end(), text)
                == spec.options.allowed_peers.end()) {
            E3_LOG_WARN(spec.log_tag) << "Refused a connection from " << text << ": not in the allowed peers";
            return false;
        }
    }
    if (spec.options.maximum_associations > 0 && connected >= spec.options.maximum_associations) {
        E3_LOG_WARN(spec.log_tag) << "Refused a connection from " << address_text(address)
                                  << ": " << spec.options.maximum_associations << " peers are connected";
        return false;
    }
    return true;
}

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

StreamFrame parse_stream_frame(const uint8_t* data, size_t size, size_t& pdu_size) {
    if (size < TCP_PREFIX_BYTES) {
        return StreamFrame::NEED_MORE;
    }
    uint32_t length = 0;
    std::memcpy(&length, data, sizeof(length));
    length = ntohl(length);
    if (length == 0 || length > MAX_PDU) {
        pdu_size = length;
        return StreamFrame::INVALID;
    }
    if (size < TCP_PREFIX_BYTES + length) {
        return StreamFrame::NEED_MORE;
    }
    pdu_size = length;
    return StreamFrame::PDU;
}

void write_stream_prefix(uint8_t (&prefix)[TCP_PREFIX_BYTES], size_t size) {
    const uint32_t length = htonl(static_cast<uint32_t>(size));
    std::memcpy(prefix, &length, sizeof(length));
}

void set_sctp_send_info(struct msghdr& msg, void* control, PduClass kind) {
#ifdef LIBE3_ENABLE_SCTP
    static_assert(CMSG_SPACE(sizeof(struct sctp_sndinfo)) <= SCTP_SEND_CONTROL_BYTES, "control room");
    std::memset(control, 0, CMSG_SPACE(sizeof(struct sctp_sndinfo)));
    msg.msg_control = control;
    msg.msg_controllen = CMSG_SPACE(sizeof(struct sctp_sndinfo));
    struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = IPPROTO_SCTP;
    c->cmsg_type = SCTP_SNDINFO;
    c->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));
    struct sctp_sndinfo info{};
    info.snd_sid = kind == PduClass::INDICATION ? SCTP_INDICATION_STREAM : SCTP_PROCEDURE_STREAM;
    info.snd_ppid = htonl(SCTP_PPID);
    std::memcpy(CMSG_DATA(c), &info, sizeof(info));
#else
    (void)msg;
    (void)control;
    (void)kind;
#endif
}

bool sctp_is_notification(const struct msghdr& msg) {
#ifdef LIBE3_ENABLE_SCTP
    return (msg.msg_flags & MSG_NOTIFICATION) != 0;
#else
    (void)msg;
    return false;
#endif
}

bool sctp_ppid_acceptable(const struct msghdr& msg, const char* log_tag) {
#ifdef LIBE3_ENABLE_SCTP
    static_assert(CMSG_SPACE(sizeof(struct sctp_rcvinfo)) <= SCTP_RECEIVE_CONTROL_BYTES, "control room");
    if (msg.msg_controllen == 0) {
        return true;
    }
    auto& m = const_cast<struct msghdr&>(msg);
    for (struct cmsghdr* c = CMSG_FIRSTHDR(&m); c != nullptr; c = CMSG_NXTHDR(&m, c)) {
        if (c->cmsg_level == IPPROTO_SCTP && c->cmsg_type == SCTP_RCVINFO) {
            struct sctp_rcvinfo info;
            std::memcpy(&info, CMSG_DATA(c), sizeof(info));
            const uint32_t ppid = ntohl(info.rcv_ppid);
            if (ppid != 0 && ppid != SCTP_PPID) {
                E3_LOG_ERROR(log_tag) << "Message with payload protocol id 0x" << std::hex << ppid;
                return false;
            }
        }
    }
    return true;
#else
    (void)msg;
    (void)log_tag;
    return true;
#endif
}

}  // namespace socket_link
}  // namespace libe3
