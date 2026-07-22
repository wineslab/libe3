/**
 * @file posix_connector.cpp
 * @brief POSIX socket connector implementation
 *
 * Ported from the original C implementation's posix_* functions in e3_connector.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "posix_connector.hpp"
#include "libe3/logger.hpp"
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/time.h>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <poll.h>
#include <thread>
#include <chrono>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "PosixConn";
constexpr const char* IPC_BASE_DIR = "/tmp/dapps";
constexpr int POLL_TIMEOUT_MS = 500;   // Timeout for graceful shutdown
constexpr int SEND_TIMEOUT_MS = 500;   // Per-peer broadcast send timeout (drop a stalled peer)
constexpr int SETUP_RECV_TIMEOUT_MS = 2000;  // Give up on a silent setup peer (bounded, not forever)

/**
 * @brief Wait for socket to become ready with timeout
 * @param sockfd Socket file descriptor
 * @param timeout_ms Timeout in milliseconds
 * @return 1 if ready, 0 if timeout, -1 on error
 */
int wait_for_socket(int sockfd, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    return poll(&pfd, 1, timeout_ms);
}

/**
 * @brief Disable Nagle-style segment bundling on a connected data socket.
 *
 * Without this every framed message (small length prefix + payload) stalls
 * roughly one RTT waiting for the previous segment's SACK/ACK: measured as a
 * hard ~214 msg/s ceiling on SCTP regardless of offered rate. Real-time
 * control-loop traffic must be flushed per message, so NODELAY is set on
 * every accepted and connected TCP/SCTP data socket (Linux does not reliably
 * inherit it from the listener). No-op for UNIX-domain (IPC) sockets.
 */
void set_nodelay(int sockfd, E3TransportLayer transport) {
    int one = 1;
    int ret = 0;
    if (transport == E3TransportLayer::TCP) {
        ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    } else if (transport == E3TransportLayer::SCTP) {
        ret = setsockopt(sockfd, IPPROTO_SCTP, SCTP_NODELAY, &one, sizeof(one));
    }
    if (ret != 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set NODELAY: " << strerror(errno);
    }
}

/**
 * @brief Bound how long a broadcast send() blocks on a single peer socket.
 *
 * A dApp that connects and then stops reading fills its socket buffer.
 * Without a send timeout the RAN's outbound I/O thread would block forever in
 * send_in_chunks, stalling delivery to every other peer and hanging stop().
 * With SO_SNDTIMEO a stalled send fails with EAGAIN/EWOULDBLOCK, the peer is
 * dropped, and the rest keep receiving, mirroring ZMQ PUB's "drop the slow
 * subscriber" behavior.
 */
void set_send_timeout(int sockfd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set SO_SNDTIMEO: " << strerror(errno);
    }
}

/**
 * @brief Bound how long a blocking recv() waits on a socket.
 *
 * Used on the accepted setup connection: without it a peer that connects to
 * the setup port and never sends would wedge the setup thread in recv()
 * forever, blocking every later handshake and hanging stop() on the join
 * (shutdown() only unblocks the listeners). On timeout recv() returns
 * EAGAIN/EWOULDBLOCK and the caller drops the stale connection.
 */
void set_recv_timeout(int sockfd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set SO_RCVTIMEO: " << strerror(errno);
    }
}

/**
 * @brief Put a listening socket in non-blocking mode.
 *
 * If a client resets the connection between the poll() that reported the
 * listener readable and the following accept(), a blocking listener makes
 * accept() wait for the next connection and stalls the whole I/O thread. A
 * non-blocking listener returns EAGAIN instead, which the accept sites treat
 * as "nothing to accept right now".
 */
void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set O_NONBLOCK: " << strerror(errno);
    }
}

/**
 * @brief Accept every connection currently pending on a listener.
 *
 * Non-blocking (0-timeout poll before each accept, and the listener itself is
 * O_NONBLOCK). Each accepted peer socket gets NODELAY and a bounded send
 * timeout, then is appended to @p peers. Returns the number of peers accepted
 * in this call.
 */
int drain_accept(int listener, E3TransportLayer transport,
                 std::vector<int>& peers, const char* label) {
    int accepted = 0;
    while (listener >= 0 && wait_for_socket(listener, 0) == 1) {
        int fd = accept(listener, nullptr, nullptr);
        if (fd < 0) {
            break;  // EAGAIN/EINTR/shutdown: nothing more to accept now
        }
        set_nodelay(fd, transport);
        set_send_timeout(fd, SEND_TIMEOUT_MS);
        peers.push_back(fd);
        ++accepted;
        E3_LOG_INFO(LOG_TAG) << label << " peer connected ("
                             << peers.size() << " total)";
    }
    return accepted;
}
}

namespace {
/**
 * @brief Strip ZMQ-style URI prefix (e.g. "ipc://", "tcp://") from endpoint strings
 *
 * POSIX sockets need plain filesystem paths or addresses, not ZMQ URIs.
 * For IPC: "ipc:///tmp/dapps/setup" -> "/tmp/dapps/setup"
 * For TCP: "tcp://127.0.0.1:9990" -> "127.0.0.1:9990"
 */
std::string strip_uri_prefix(const std::string& endpoint) {
    auto pos = endpoint.find("://");
    if (pos != std::string::npos) {
        return endpoint.substr(pos + 3);
    }
    return endpoint;
}
} // anonymous namespace

PosixE3Connector::PosixE3Connector(
    E3TransportLayer transport_layer,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint,
    uint16_t setup_port,
    uint16_t inbound_port,
    uint16_t outbound_port
)
    : transport_layer_(transport_layer)
    , setup_connection_socket_(UNUSED_SOCKET)
    , inbound_connection_socket_(UNUSED_SOCKET)
    , outbound_connection_socket_(UNUSED_SOCKET)
    , setup_port_(setup_port)
    , inbound_port_(inbound_port)
    , outbound_port_(outbound_port)
{
    setup_endpoint_ = strip_uri_prefix(setup_endpoint);
    inbound_endpoint_ = strip_uri_prefix(inbound_endpoint);
    outbound_endpoint_ = strip_uri_prefix(outbound_endpoint);

    E3_LOG_INFO(LOG_TAG) << "Creating POSIX connector";
    E3_LOG_DEBUG(LOG_TAG) << "  Setup endpoint: " << setup_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Inbound endpoint: " << inbound_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Outbound endpoint: " << outbound_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Setup port: " << setup_port_;
    E3_LOG_DEBUG(LOG_TAG) << "  Inbound port: " << inbound_port_;
    E3_LOG_DEBUG(LOG_TAG) << "  Outbound port: " << outbound_port_;
}

PosixE3Connector::~PosixE3Connector() {
    dispose();
}

ErrorCode PosixE3Connector::setup_initial_connection() {
    int sock;
    int ret;
    
    // Create IPC directory if needed
    if (transport_layer_ == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1) {
            if (mkdir(IPC_BASE_DIR, 0777) == -1 && errno != EEXIST) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to create IPC directory: " << strerror(errno);
                return ErrorCode::CONNECTION_FAILED;
            }
        }
        chmod(IPC_BASE_DIR, 0777);
    }
    
    if (transport_layer_ == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(setup_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(setup_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else { // IPC
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, setup_endpoint_.c_str(), sizeof(addr.sun_path) - 1);
        unlink(setup_endpoint_.c_str()); // Remove existing socket file
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create setup socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind setup socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    ret = listen(sock, 5);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to listen on setup socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    set_nonblocking(sock);  // accept() must not block the setup thread on a client RST

    setup_socket_ = sock;
    connected_ = true;
    
    E3_LOG_INFO(LOG_TAG) << "Setup socket listening on " << setup_endpoint_;
    return ErrorCode::SUCCESS;
}

int PosixE3Connector::recv_setup_request(std::vector<uint8_t>& buffer) {
    if (setup_socket_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    
    // Wait for incoming connection with timeout
    int poll_ret = wait_for_socket(setup_socket_, POLL_TIMEOUT_MS);
    if (poll_ret == 0) {
        // Timeout - return 0 to allow shutdown check
        return 0;
    }
    if (poll_ret < 0) {
        if (errno == EINTR) {
            return 0;  // Interrupted, allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Poll failed on setup socket: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    // Close the previous accepted setup connection before accepting a new one;
    // otherwise every dApp after the first leaks one fd. The prior setup
    // exchange's send_response has already completed by the time the setup
    // loop calls recv_setup_request again.
    if (setup_connection_socket_ >= 0) {
        close(setup_connection_socket_);
        setup_connection_socket_ = -1;
    }

    // Accept connection
    setup_connection_socket_ = accept(setup_socket_, nullptr, nullptr);
    if (setup_connection_socket_ < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK || shutdown_requested_.load()) {
            return 0;  // Allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to accept setup connection: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    set_nodelay(setup_connection_socket_, transport_layer_);
    // Bound the blocking recv below so a peer that connects and never sends
    // cannot wedge the setup thread (and hang stop()) or block later handshakes.
    set_recv_timeout(setup_connection_socket_, SETUP_RECV_TIMEOUT_MS);

    buffer.resize(DEFAULT_BUFFER_SIZE);
    ssize_t ret = recv(setup_connection_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // Silent (or interrupted) peer: drop this connection and let the
            // setup loop re-poll the listener / observe shutdown, rather than
            // blocking here and starving other dApps' handshakes.
            close(setup_connection_socket_);
            setup_connection_socket_ = -1;
            return 0;
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive setup request: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(static_cast<size_t>(ret));
    E3_LOG_DEBUG(LOG_TAG) << "Received setup request: " << ret << " bytes";
    return static_cast<int>(ret);
}

ErrorCode PosixE3Connector::send_response(const std::vector<uint8_t>& data) {
    if (setup_connection_socket_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }
    
    int ret = send_in_chunks(setup_connection_socket_, data.data(), data.size());
    if (ret < 0) {
        return ErrorCode::TRANSPORT_ERROR;
    }
    
    E3_LOG_DEBUG(LOG_TAG) << "Sent response: " << data.size() << " bytes";
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::setup_inbound_connection() {
    int sock;
    int ret;
    
    // Ensure IPC directory exists (defensive — setup_initial_connection normally creates it)
    if (transport_layer_ == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1) {
            if (mkdir(IPC_BASE_DIR, 0777) == -1 && errno != EEXIST) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to create IPC directory: " << strerror(errno);
                return ErrorCode::CONNECTION_FAILED;
            }
        }
        chmod(IPC_BASE_DIR, 0777);
    }
    
    if (transport_layer_ == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(inbound_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(inbound_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else { // IPC
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, inbound_endpoint_.c_str(), sizeof(addr.sun_path) - 1);
        unlink(inbound_endpoint_.c_str());
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create inbound socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind inbound socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    ret = listen(sock, 5);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to listen on inbound socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    set_nonblocking(sock);  // drain_accept() must not block the inbound thread on a client RST

    inbound_socket_ = sock;

    // Multi-peer: do not block waiting for the first dApp here. receive()
    // polls the listener alongside every accepted peer socket, so peers can
    // attach (and re-attach) at any point in the connector's lifetime.
    E3_LOG_INFO(LOG_TAG) << "Inbound listener ready (multi-peer)";
    return ErrorCode::SUCCESS;
}

int PosixE3Connector::receive(std::vector<uint8_t>& buffer) {
    // On the dApp role our peer (RAN) uses send() = send_in_chunks() which
    // emits a 4-byte length-prefix frame, so we decode it accordingly. The
    // dApp keeps its original single-socket path.
    if (role_ == E3Role::DAPP) {
        if (inbound_connection_socket_ < 0) {
            return static_cast<int>(ErrorCode::NOT_CONNECTED);
        }
        int poll_ret = wait_for_socket(inbound_connection_socket_, POLL_TIMEOUT_MS);
        if (poll_ret == 0) {
            return 0;  // Timeout - allow shutdown check
        }
        if (poll_ret < 0) {
            if (errno == EINTR) {
                return 0;  // Interrupted, allow shutdown check
            }
            E3_LOG_ERROR(LOG_TAG) << "Poll failed on receive: " << strerror(errno);
            return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        return recv_with_size(inbound_connection_socket_, buffer);
    }

    // RAN role, multi-peer: poll the listener (new peers) plus every
    // accepted subscriber socket. Raw (unframed) recv per peer is preserved
    // for compatibility with the Python dApp framework's framing convention.
    if (inbound_socket_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }

    std::vector<struct pollfd> pfds;
    pfds.reserve(1 + inbound_peer_sockets_.size());
    pfds.push_back({inbound_socket_, POLLIN, 0});
    for (int fd : inbound_peer_sockets_) {
        pfds.push_back({fd, POLLIN, 0});
    }

    int poll_ret = poll(pfds.data(), pfds.size(), POLL_TIMEOUT_MS);
    if (poll_ret == 0) {
        return 0;  // Timeout - allow shutdown check
    }
    if (poll_ret < 0) {
        if (errno == EINTR) {
            return 0;  // Interrupted, allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Poll failed on receive: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }

    if (pfds[0].revents & POLLIN) {
        drain_accept(inbound_socket_, transport_layer_, inbound_peer_sockets_,
                     "Inbound (subscriber)");
    }

    // Read from the next ready peer, returning its data (remaining ready peers
    // stay readable and are picked up on the next call; the inbound loop calls
    // receive() continuously). The scan starts from a rotating cursor so a peer
    // with continuously-queued data cannot starve the others.
    const size_t n_peers = pfds.size() - 1;  // pfds[0] is the listener
    for (size_t k = 0; k < n_peers; ++k) {
        const size_t idx = (inbound_rr_ + k) % n_peers;  // 0-based peer index
        const size_t i = idx + 1;                        // pfds index (0 == listener)
        const short re = pfds[i].revents;
        if (!(re & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
            continue;
        }
        const int fd = pfds[i].fd;
        bool broken = (re & (POLLHUP | POLLERR | POLLNVAL)) != 0;
        ssize_t ret = -1;
        if (re & POLLIN) {
            buffer.resize(DEFAULT_BUFFER_SIZE);
            ret = recv(fd, buffer.data(), buffer.size(), 0);
            if (ret == 0) {
                broken = true;
            } else if (ret < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                broken = true;
            }
        }
        // Deliver any bytes we actually read before acting on a broken flag: on
        // AF_UNIX/IPC a clean disconnect after a final message (control, report,
        // release) arrives as POLLIN|POLLHUP on the same poll, so dropping first
        // would silently lose that last message. A broken peer with nothing left
        // to read is dropped on the next call, where recv() returns 0/EAGAIN and
        // ret > 0 is false (this also keeps the 100% CPU busy-loop guarded: a
        // half-broken socket with no data drops immediately below).
        if (ret > 0) {
            buffer.resize(static_cast<size_t>(ret));
            E3_LOG_TRACE(LOG_TAG) << "Received: " << ret << " bytes";
            inbound_rr_ = idx + 1;  // next call starts after the peer we just served
            return static_cast<int>(ret);
        }
        if (broken) {
            // Peer closed or hard error and nothing left to deliver: drop it and
            // keep serving the rest. pfds is a local copy, so continuing to scan
            // its other fds is safe.
            close(fd);
            inbound_peer_sockets_.erase(
                std::remove(inbound_peer_sockets_.begin(),
                            inbound_peer_sockets_.end(), fd),
                inbound_peer_sockets_.end());
            E3_LOG_INFO(LOG_TAG) << "Inbound (subscriber) peer disconnected ("
                                 << inbound_peer_sockets_.size() << " remain)";
            continue;
        }
    }
    // Nothing read this round: advance the cursor so the start point keeps rotating.
    if (n_peers > 0) {
        inbound_rr_ = (inbound_rr_ + 1) % n_peers;
    }
    return 0;
}

ErrorCode PosixE3Connector::setup_outbound_connection() {
    int sock;
    int ret;
    
    // Ensure IPC directory exists (defensive — setup_initial_connection normally creates it)
    if (transport_layer_ == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1) {
            if (mkdir(IPC_BASE_DIR, 0777) == -1 && errno != EEXIST) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to create IPC directory: " << strerror(errno);
                return ErrorCode::CONNECTION_FAILED;
            }
        }
        chmod(IPC_BASE_DIR, 0777);
    }
    
    if (transport_layer_ == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(outbound_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(outbound_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else { // IPC
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, outbound_endpoint_.c_str(), sizeof(addr.sun_path) - 1);
        unlink(outbound_endpoint_.c_str());
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create outbound socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind outbound socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    ret = listen(sock, 5);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to listen on outbound socket: " << strerror(errno);
        close(sock);
        return ErrorCode::CONNECTION_FAILED;
    }
    set_nonblocking(sock);  // drain_accept() must not block the outbound thread on a client RST

    outbound_socket_ = sock;

    // Multi-peer: no blocking accept here. send() drains the listener before
    // each broadcast, so peers can attach at any time; with no peer attached
    // a send is dropped, mirroring ZMQ PUB semantics with no subscriber.
    E3_LOG_INFO(LOG_TAG) << "Outbound listener ready (multi-peer)";
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::send(const std::vector<uint8_t>& data) {
    // dApp role sends to RAN's receive() which uses raw recv. Use a raw
    // send to match. The dApp keeps its original single-socket path.
    if (role_ == E3Role::DAPP) {
        if (outbound_connection_socket_ < 0) {
            return ErrorCode::NOT_CONNECTED;
        }
        ssize_t sent = ::send(outbound_connection_socket_, data.data(), data.size(), 0);
        if (sent < 0 || static_cast<size_t>(sent) != data.size()) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to send dApp outbound: " << strerror(errno);
            return ErrorCode::TRANSPORT_ERROR;
        }
        E3_LOG_TRACE(LOG_TAG) << "Sent (dApp raw): " << data.size() << " bytes";
        return ErrorCode::SUCCESS;
    }

    // RAN role, multi-peer: accept any newly connected peers, then broadcast
    // the framed message to all of them. Delivery mirrors ZMQ PUB/SUB: every
    // peer gets every message and each dApp filters by its own identifier;
    // with no peer attached the message is dropped (PUB with no subscriber).
    if (outbound_socket_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }
    drain_accept(outbound_socket_, transport_layer_, outbound_peer_sockets_,
                 "Outbound (indication)");

    bool any_failed = false;
    for (auto it = outbound_peer_sockets_.begin();
         it != outbound_peer_sockets_.end();) {
        if (send_in_chunks(*it, data.data(), data.size()) < 0) {
            // Dead peer: drop it and keep serving the rest.
            close(*it);
            it = outbound_peer_sockets_.erase(it);
            any_failed = true;
            E3_LOG_INFO(LOG_TAG) << "Outbound (indication) peer disconnected ("
                                 << outbound_peer_sockets_.size() << " remain)";
        } else {
            ++it;
        }
    }

    if (any_failed && outbound_peer_sockets_.empty()) {
        return ErrorCode::TRANSPORT_ERROR;
    }
    E3_LOG_TRACE(LOG_TAG) << "Sent: " << data.size() << " bytes to "
                          << outbound_peer_sockets_.size() << " peer(s)";
    return ErrorCode::SUCCESS;
}

// ===========================================================================
// Client-side (dApp role) implementations
//
// Mirrors of the server-side methods but with connect() in place of
// bind()/listen()/accept(). For IPC the dApp connects to setup_endpoint_
// (same as RAN binds). For TCP the dApp connects to 127.0.0.1:port. For
// SCTP the same TCP-style address is used; ubuntu-latest runners lack
// the sctp kernel module so SCTP client creation may fail at socket()
// time — callers should treat that as "transport not supported".
// ===========================================================================

namespace {
// Connect a fresh socket to (addr, port_or_path) for the given transport.
// Returns the fd on success or -1 on error.
int posix_connect_for(E3TransportLayer transport,
                      const std::string& endpoint,
                      uint16_t port) {
    if (transport == E3TransportLayer::IPC) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return -1;
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(sock);
            return -1;
        }
        return sock;
    }
    int sock;
    if (transport == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    } else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (sock < 0) return -1;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    set_nodelay(sock, transport);
    return sock;
}
}  // anonymous namespace

ErrorCode PosixE3Connector::setup_initial_connection_client() {
    // Retry with a short backoff so we don't race the RAN's bind on startup.
    int sock = -1;
    for (int retry = 0; retry < 10 && sock < 0; ++retry) {
        sock = posix_connect_for(transport_layer_, setup_endpoint_, setup_port_);
        if (sock < 0) {
            if (shutdown_requested_.load()) return ErrorCode::CANCELLED;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect setup socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    setup_connection_socket_ = sock;
    connected_ = true;
    E3_LOG_INFO(LOG_TAG) << "Setup socket connected (client) to "
                         << setup_endpoint_ << " port=" << setup_port_;
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::send_setup_request_client(const std::vector<uint8_t>& data) {
    if (setup_connection_socket_ < 0) return ErrorCode::NOT_CONNECTED;
    // The RAN's recv_setup_request reads via raw recv() (no length prefix),
    // so the dApp must send the SetupRequest the same way. The setup
    // channel doesn't carry chunked messages so a single send() is fine.
    ssize_t sent = ::send(setup_connection_socket_, data.data(), data.size(), 0);
    if (sent < 0 || static_cast<size_t>(sent) != data.size()) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send setup request: " << strerror(errno);
        return ErrorCode::TRANSPORT_ERROR;
    }
    E3_LOG_DEBUG(LOG_TAG) << "Sent SetupRequest: " << data.size() << " bytes";
    return ErrorCode::SUCCESS;
}

int PosixE3Connector::recv_setup_response_client(std::vector<uint8_t>& buffer) {
    if (setup_connection_socket_ < 0) return static_cast<int>(ErrorCode::NOT_CONNECTED);

    // Wait for data with timeout to allow cooperative shutdown.
    int poll_ret = wait_for_socket(setup_connection_socket_, POLL_TIMEOUT_MS);
    if (poll_ret == 0) return 0;
    if (poll_ret < 0) return static_cast<int>(ErrorCode::TRANSPORT_ERROR);

    return recv_with_size(setup_connection_socket_, buffer);
}

ErrorCode PosixE3Connector::setup_inbound_connection_client() {
    // dApp inbound = RAN's outbound endpoint (publisher).
    int sock = -1;
    for (int retry = 0; retry < 10 && sock < 0; ++retry) {
        sock = posix_connect_for(transport_layer_, outbound_endpoint_, outbound_port_);
        if (sock < 0) {
            if (shutdown_requested_.load()) return ErrorCode::CANCELLED;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect inbound socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    inbound_connection_socket_ = sock;
    E3_LOG_INFO(LOG_TAG) << "Inbound socket connected (client) to "
                         << outbound_endpoint_ << " port=" << outbound_port_;
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::setup_outbound_connection_client() {
    // dApp outbound = RAN's inbound endpoint (subscriber).
    int sock = -1;
    for (int retry = 0; retry < 10 && sock < 0; ++retry) {
        sock = posix_connect_for(transport_layer_, inbound_endpoint_, inbound_port_);
        if (sock < 0) {
            if (shutdown_requested_.load()) return ErrorCode::CANCELLED;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (sock < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect outbound socket: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    outbound_connection_socket_ = sock;
    E3_LOG_INFO(LOG_TAG) << "Outbound socket connected (client) to "
                         << inbound_endpoint_ << " port=" << inbound_port_;
    return ErrorCode::SUCCESS;
}

void PosixE3Connector::shutdown() {
    E3_LOG_DEBUG(LOG_TAG) << "Shutting down POSIX connector";
    shutdown_requested_.store(true);
    
    // Close listening sockets to interrupt blocking accept() calls
    if (setup_socket_ >= 0) {
        ::shutdown(setup_socket_, SHUT_RDWR);
    }
    if (inbound_socket_ >= 0) {
        ::shutdown(inbound_socket_, SHUT_RDWR);
    }
    // The outbound listener was missing here, so a RAN whose outbound
    // accept() was still waiting for a dApp could never be joined by stop().
    if (outbound_socket_ >= 0) {
        ::shutdown(outbound_socket_, SHUT_RDWR);
    }
}

void PosixE3Connector::dispose() {
    E3_LOG_DEBUG(LOG_TAG) << "Disposing POSIX connector";
    
    if (inbound_connection_socket_ != UNUSED_SOCKET && inbound_connection_socket_ >= 0) {
        close(inbound_connection_socket_);
    }
    
    if (setup_connection_socket_ != UNUSED_SOCKET && setup_connection_socket_ >= 0) {
        close(setup_connection_socket_);
    }
    
    if (outbound_connection_socket_ != UNUSED_SOCKET && outbound_connection_socket_ >= 0) {
        close(outbound_connection_socket_);
    }

    for (int fd : inbound_peer_sockets_) {
        close(fd);
    }
    inbound_peer_sockets_.clear();
    for (int fd : outbound_peer_sockets_) {
        close(fd);
    }
    outbound_peer_sockets_.clear();

    if (inbound_socket_ >= 0) {
        close(inbound_socket_);
    }
    
    if (outbound_socket_ >= 0) {
        close(outbound_socket_);
    }
    
    if (setup_socket_ >= 0) {
        close(setup_socket_);
    }
    
    // Clean up IPC socket files only if we created them (RAN side binds).
    // The dApp side connects, so it has nothing to unlink.
    if (transport_layer_ == E3TransportLayer::IPC && role_ == E3Role::RAN) {
        unlink(setup_endpoint_.c_str());
        unlink(inbound_endpoint_.c_str());
        unlink(outbound_endpoint_.c_str());
        // Deliberately do NOT rmdir(IPC_BASE_DIR): it is shared across agents
        // and tests. Removing it races with another agent binding a socket
        // inside it (ENOENT). Leaving the empty directory behind is harmless.
    }
    
    setup_socket_ = -1;
    setup_connection_socket_ = UNUSED_SOCKET;
    inbound_socket_ = -1;
    inbound_connection_socket_ = UNUSED_SOCKET;
    outbound_socket_ = -1;
    outbound_connection_socket_ = UNUSED_SOCKET;
    connected_ = false;
    
    E3_LOG_INFO(LOG_TAG) << "POSIX connector disposed";
}

int PosixE3Connector::send_in_chunks(int sockfd, const uint8_t* buffer, size_t buffer_size) {
    // Coalesce the 4-byte length prefix (network byte order) and the payload
    // into a single writev() so each message costs one syscall and one wire
    // segment instead of two. The old separate send() for the prefix produced
    // a tiny segment per message, which interacted badly with Nagle-style
    // bundling (see set_nodelay) and doubled the syscall cost of the data path.
    uint32_t network_order_size = htonl(static_cast<uint32_t>(buffer_size));
    struct iovec iov[2];
    iov[0].iov_base = &network_order_size;
    iov[0].iov_len = sizeof(network_order_size);
    iov[1].iov_base = const_cast<uint8_t*>(buffer);
    iov[1].iov_len = buffer_size;

    size_t total_len = sizeof(network_order_size) + buffer_size;
    size_t total_sent = 0;
    while (total_sent < total_len) {
        ssize_t sent;
        if (total_sent < sizeof(network_order_size)) {
            // First pass (or short write inside the prefix): send prefix +
            // payload together, skipping whatever already went out.
            struct iovec cur[2];
            cur[0].iov_base = reinterpret_cast<uint8_t*>(&network_order_size) + total_sent;
            cur[0].iov_len = sizeof(network_order_size) - total_sent;
            cur[1] = iov[1];
            sent = ::writev(sockfd, cur, 2);
        } else {
            size_t payload_sent = total_sent - sizeof(network_order_size);
            size_t to_send = std::min(static_cast<size_t>(CHUNK_SIZE),
                                      buffer_size - payload_sent);
            sent = ::send(sockfd, buffer + payload_sent, to_send, 0);
        }
        if (sent < 0) {
            if (errno == EINTR) continue;
            E3_LOG_ERROR(LOG_TAG) << "Failed to send data: " << strerror(errno);
            return -1;
        }
        total_sent += static_cast<size_t>(sent);
    }

    return 0;
}

int PosixE3Connector::recv_with_size(int sockfd, std::vector<uint8_t>& buffer) {
    // Read the 4-byte size prefix (network byte order) — mirrors send_in_chunks
    uint32_t network_order_size = 0;
    size_t header_received = 0;
    while (header_received < sizeof(network_order_size)) {
        ssize_t ret = ::recv(sockfd,
                             reinterpret_cast<uint8_t*>(&network_order_size) + header_received,
                             sizeof(network_order_size) - header_received, 0);
        if (ret <= 0) {
            if (ret == 0) {
                E3_LOG_DEBUG(LOG_TAG) << "Connection closed while reading size header";
            } else {
                E3_LOG_ERROR(LOG_TAG) << "Failed to receive size header: " << strerror(errno);
            }
            return ret == 0 ? 0 : static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        header_received += static_cast<size_t>(ret);
    }

    uint32_t payload_size = ntohl(network_order_size);
    if (payload_size == 0 || payload_size > MAX_PROTOCOL_DATA_SIZE * 2) {
        E3_LOG_ERROR(LOG_TAG) << "Invalid payload size: " << payload_size;
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }

    buffer.resize(payload_size);
    size_t total_received = 0;
    while (total_received < payload_size) {
        size_t to_read = std::min(static_cast<size_t>(CHUNK_SIZE), payload_size - total_received);
        ssize_t ret = ::recv(sockfd, buffer.data() + total_received, to_read, 0);
        if (ret <= 0) {
            if (ret == 0) {
                E3_LOG_ERROR(LOG_TAG) << "Connection closed during chunked receive";
            } else {
                E3_LOG_ERROR(LOG_TAG) << "Failed to receive chunk: " << strerror(errno);
            }
            return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        total_received += static_cast<size_t>(ret);
    }

    E3_LOG_TRACE(LOG_TAG) << "recv_with_size: " << payload_size << " bytes";
    return static_cast<int>(payload_size);
}

} // namespace libe3
