/**
 * @file posix_connector.hpp
 * @brief POSIX socket-based E3 Connector
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_POSIX_CONNECTOR_HPP
#define LIBE3_POSIX_CONNECTOR_HPP

#include "libe3/e3_connector.hpp"
#include <atomic>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace libe3 {

/**
 * @brief POSIX socket-based E3 connector
 *
 * Provides transport using POSIX sockets. Supports TCP, SCTP, and IPC transports.
 * Ported from the original C implementation's posix_* functions.
 *
 * RAN role serves multiple dApp peers: both data listeners keep accepting
 * for the connector's lifetime, receive() polls every accepted subscriber
 * socket, and send() broadcasts each framed message to every accepted
 * indication socket (mirroring the ZMQ PUB/SUB semantics, where the RAN
 * broadcasts and each dApp filters by its own dApp identifier). Peer
 * sockets are only touched by their owning I/O thread: inbound sockets by
 * the inbound loop, outbound sockets by the outbound loop.
 */
class PosixE3Connector : public E3Connector {
public:
    /**
     * @brief Create a POSIX connector
     *
     * @param transport_layer SCTP, TCP or IPC
     * @param setup_endpoint Endpoint of the setup channel
     * @param inbound_endpoint Endpoint of the inbound channel
     * @param outbound_endpoint Endpoint of the outbound channel
     * @param setup_port Port of the setup channel (SCTP and TCP)
     * @param inbound_port Port of the inbound channel (SCTP and TCP)
     * @param outbound_port Port of the outbound channel (SCTP and TCP)
     */
    PosixE3Connector(
        E3TransportLayer transport_layer,
        const std::string& setup_endpoint,
        const std::string& inbound_endpoint,
        const std::string& outbound_endpoint,
        uint16_t setup_port,
        uint16_t inbound_port,
        uint16_t outbound_port
    );
    
    /** @brief Close every socket. */
    ~PosixE3Connector() override;

    ErrorCode setup_initial_connection() override;
    int recv_setup_request(std::vector<uint8_t>& buffer) override;
    ErrorCode send_response(const std::vector<uint8_t>& data) override;
    ErrorCode setup_inbound_connection() override;
    int receive(std::vector<uint8_t>& buffer) override;
    ErrorCode setup_outbound_connection() override;
    ErrorCode send(const std::vector<uint8_t>& data) override;

    // Client-side (dApp role) operations.
    ErrorCode setup_initial_connection_client() override;
    ErrorCode send_setup_request_client(const std::vector<uint8_t>& data) override;
    int recv_setup_response_client(std::vector<uint8_t>& buffer) override;
    ErrorCode setup_inbound_connection_client() override;
    ErrorCode setup_outbound_connection_client() override;

    void dispose() override;
    void shutdown() override;
    uint64_t last_peer_token() const override;
    std::vector<uint64_t> take_closed_peers() override;
    
    E3LinkLayer link_layer() const noexcept override {
        return E3LinkLayer::POSIX;
    }
    
    E3TransportLayer transport_layer() const noexcept override {
        return transport_layer_;
    }
    
    bool is_connected() const noexcept override {
        return connected_;
    }

private:
    E3TransportLayer transport_layer_;
    
    int setup_socket_{-1};
    int setup_connection_socket_{-1};
    int inbound_socket_{-1};
    int inbound_connection_socket_{-1};
    int outbound_socket_{-1};
    int outbound_connection_socket_{-1};

    // RAN-role peer sockets (multi-peer). During normal operation each vector
    // is touched only by its owning I/O thread: inbound_peer_sockets_ by the
    // inbound (receive) loop, outbound_peer_sockets_ by the outbound (send)
    // loop. dispose() and the destructor also iterate and clear both vectors
    // from the caller's thread; that is race-free ONLY because
    // E3Interface::stop() joins the inbound and outbound threads before calling
    // dispose(). A caller that disposes without joining those threads first
    // would race on these vectors.
    std::vector<int> inbound_peer_sockets_;

    // Inbound peers are named by a token that is never reused, because a file
    // descriptor is (see E3Connector::last_peer_token). Owned by the inbound
    // thread, like inbound_peer_sockets_.
    std::unordered_map<int, uint64_t> inbound_peer_tokens_;
    uint64_t next_peer_token_{1};
    uint64_t last_peer_token_{0};
    std::vector<uint64_t> closed_peers_;
    std::vector<int> outbound_peer_sockets_;

    // Rotating start index for the inbound peer scan in receive(), so no single
    // peer with continuously-queued data can starve the others. Owned by the
    // inbound thread; taken modulo the current peer count on each use.
    size_t inbound_rr_{0};

    uint16_t setup_port_;
    uint16_t inbound_port_;
    uint16_t outbound_port_;
    
    bool connected_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    static constexpr int CHUNK_SIZE = 8192;
    static constexpr int UNUSED_SOCKET = -2;
    
    int send_in_chunks(int sockfd, const uint8_t* buffer, size_t buffer_size);
    int recv_with_size(int sockfd, std::vector<uint8_t>& buffer);
};

} // namespace libe3

#endif // LIBE3_POSIX_CONNECTOR_HPP
