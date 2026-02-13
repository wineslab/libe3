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
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <grp.h>
#include <cstring>
#include <cerrno>
#include <poll.h>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "PosixConn";
constexpr const char* IPC_BASE_DIR = "/tmp/dapps";
constexpr int POLL_TIMEOUT_MS = 500;  // Timeout for graceful shutdown

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
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(setup_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
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
    
    // Accept connection
    setup_connection_socket_ = accept(setup_socket_, nullptr, nullptr);
    if (setup_connection_socket_ < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK || shutdown_requested_.load()) {
            return 0;  // Allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to accept setup connection: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    ssize_t ret = recv(setup_connection_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
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
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(inbound_port_);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
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
    
    inbound_socket_ = sock;
    
    // Wait for incoming connection with timeout loop
    // This loop allows periodic checks during shutdown
    while (!shutdown_requested_.load()) {
        int poll_ret = wait_for_socket(inbound_socket_, POLL_TIMEOUT_MS);
        if (poll_ret < 0) {
            if (errno == EINTR || shutdown_requested_.load()) {
                break;  // Interrupted or shutdown requested
            }
            E3_LOG_ERROR(LOG_TAG) << "Poll failed on inbound socket: " << strerror(errno);
            return ErrorCode::CONNECTION_FAILED;
        }
        if (poll_ret == 0) {
            // Timeout - check shutdown flag and continue waiting
            continue;
        }
        break;  // Socket is ready
    }
    
    if (shutdown_requested_.load()) {
        E3_LOG_INFO(LOG_TAG) << "Inbound connection setup cancelled by shutdown";
        return ErrorCode::CANCELLED;
    }
    
    // Accept connection
    inbound_connection_socket_ = accept(inbound_socket_, nullptr, nullptr);
    if (inbound_connection_socket_ < 0) {
        if (shutdown_requested_.load()) {
            return ErrorCode::CANCELLED;
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to accept inbound connection: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Inbound connection established";
    return ErrorCode::SUCCESS;
}

int PosixE3Connector::receive(std::vector<uint8_t>& buffer) {
    if (inbound_connection_socket_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    
    // Wait for data with timeout
    int poll_ret = wait_for_socket(inbound_connection_socket_, POLL_TIMEOUT_MS);
    if (poll_ret == 0) {
        // Timeout - return 0 to allow shutdown check
        return 0;
    }
    if (poll_ret < 0) {
        if (errno == EINTR) {
            return 0;  // Interrupted, allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Poll failed on receive: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    ssize_t ret = recv(inbound_connection_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // Allow shutdown check
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(static_cast<size_t>(ret));
    E3_LOG_TRACE(LOG_TAG) << "Received: " << ret << " bytes";
    return static_cast<int>(ret);
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
    
    outbound_socket_ = sock;
    
    // Accept connection (blocking)
    outbound_connection_socket_ = accept(outbound_socket_, nullptr, nullptr);
    if (outbound_connection_socket_ < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to accept outbound connection: " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Outbound connection established";
    return ErrorCode::SUCCESS;
}

ErrorCode PosixE3Connector::send(const std::vector<uint8_t>& data) {
    if (outbound_connection_socket_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }
    
    int ret = send_in_chunks(outbound_connection_socket_, data.data(), data.size());
    if (ret < 0) {
        return ErrorCode::TRANSPORT_ERROR;
    }
    
    E3_LOG_TRACE(LOG_TAG) << "Sent: " << data.size() << " bytes";
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
    
    if (inbound_socket_ >= 0) {
        close(inbound_socket_);
    }
    
    if (outbound_socket_ >= 0) {
        close(outbound_socket_);
    }
    
    if (setup_socket_ >= 0) {
        close(setup_socket_);
    }
    
    // Clean up IPC files
    if (transport_layer_ == E3TransportLayer::IPC) {
        unlink(setup_endpoint_.c_str());
        unlink(inbound_endpoint_.c_str());
        unlink(outbound_endpoint_.c_str());
        rmdir(IPC_BASE_DIR);
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
    // Send the buffer size first (4 bytes, network byte order)
    uint32_t network_order_size = htonl(static_cast<uint32_t>(buffer_size));
    ssize_t sent = ::send(sockfd, &network_order_size, sizeof(network_order_size), 0);
    if (sent != sizeof(network_order_size)) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send buffer size: " << strerror(errno);
        return -1;
    }
    
    size_t total_sent = 0;
    while (total_sent < buffer_size) {
        ssize_t bytes_to_send = static_cast<ssize_t>(std::min(static_cast<size_t>(CHUNK_SIZE), buffer_size - total_sent));
        
        ssize_t chunk_sent = 0;
        while (chunk_sent < bytes_to_send) {
            ssize_t sent_chunk = ::send(sockfd, buffer + total_sent + chunk_sent, 
                                       static_cast<size_t>(bytes_to_send - chunk_sent), 0);
            if (sent_chunk == -1) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to send data: " << strerror(errno);
                return -1;
            }
            chunk_sent += sent_chunk;
        }
        total_sent += static_cast<size_t>(chunk_sent);
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
