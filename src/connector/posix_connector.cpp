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

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "PosixConn";
constexpr const char* IPC_BASE_DIR = "/tmp/dapps";
constexpr uint16_t DEFAULT_SETUP_PORT = 9990;
constexpr uint16_t DEFAULT_INBOUND_PORT = 9999;
constexpr uint16_t DEFAULT_OUTBOUND_PORT = 9991;
}

PosixE3Connector::PosixE3Connector(
    E3TransportLayer transport_layer,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint
)
    : transport_layer_(transport_layer)
    , setup_connection_socket_(UNUSED_SOCKET)
    , inbound_connection_socket_(UNUSED_SOCKET)
    , outbound_connection_socket_(UNUSED_SOCKET)
{
    setup_endpoint_ = setup_endpoint;
    inbound_endpoint_ = inbound_endpoint;
    outbound_endpoint_ = outbound_endpoint;
    
    E3_LOG_INFO(LOG_TAG) << "Creating POSIX connector";
    E3_LOG_DEBUG(LOG_TAG) << "  Setup endpoint: " << setup_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Inbound endpoint: " << inbound_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Outbound endpoint: " << outbound_endpoint_;
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
        addr.sin_port = htons(DEFAULT_SETUP_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_SETUP_PORT);
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
    
    // Accept connection
    setup_connection_socket_ = accept(setup_socket_, nullptr, nullptr);
    if (setup_connection_socket_ < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to accept setup connection: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    ssize_t ret = recv(setup_connection_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive setup request: " << strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(ret);
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
    
    if (transport_layer_ == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_INBOUND_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_INBOUND_PORT);
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
    
    // Accept connection (blocking)
    inbound_connection_socket_ = accept(inbound_socket_, nullptr, nullptr);
    if (inbound_connection_socket_ < 0) {
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
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    ssize_t ret = recv(inbound_connection_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
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
    
    if (transport_layer_ == E3TransportLayer::SCTP) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_OUTBOUND_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    else if (transport_layer_ == E3TransportLayer::TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEFAULT_OUTBOUND_PORT);
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
    // Send the buffer size first
    uint32_t network_order_size = htonl(static_cast<uint32_t>(buffer_size));
    ssize_t sent = ::send(sockfd, &network_order_size, sizeof(network_order_size), 0);
    if (sent != sizeof(network_order_size)) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send buffer size: " << strerror(errno);
        return -1;
    }
    
    size_t total_sent = 0;
    while (total_sent < buffer_size) {
        ssize_t bytes_to_send = std::min(static_cast<size_t>(CHUNK_SIZE), buffer_size - total_sent);
        
        ssize_t chunk_sent = 0;
        while (chunk_sent < bytes_to_send) {
            ssize_t sent_chunk = ::send(sockfd, buffer + total_sent + chunk_sent, 
                                       bytes_to_send - chunk_sent, 0);
            if (sent_chunk == -1) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to send data: " << strerror(errno);
                return -1;
            }
            chunk_sent += sent_chunk;
        }
        total_sent += chunk_sent;
    }
    
    return 0;
}

} // namespace libe3
