/**
 * @file posix_connector.hpp
 * @brief POSIX socket-based E3 Connector
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_POSIX_CONNECTOR_HPP
#define LIBE3_POSIX_CONNECTOR_HPP

#include "libe3/e3_connector.hpp"

namespace libe3 {

/**
 * @brief POSIX socket-based E3 connector
 *
 * Provides transport using POSIX sockets. Supports TCP, SCTP, and IPC transports.
 * Ported from the original C implementation's posix_* functions.
 */
class PosixE3Connector : public E3Connector {
public:
    PosixE3Connector(
        E3TransportLayer transport_layer,
        const std::string& setup_endpoint,
        const std::string& inbound_endpoint,
        const std::string& outbound_endpoint
    );
    
    ~PosixE3Connector() override;

    [[nodiscard]] ErrorCode setup_initial_connection() override;
    [[nodiscard]] int recv_setup_request(std::vector<uint8_t>& buffer) override;
    [[nodiscard]] ErrorCode send_response(const std::vector<uint8_t>& data) override;
    [[nodiscard]] ErrorCode setup_inbound_connection() override;
    [[nodiscard]] int receive(std::vector<uint8_t>& buffer) override;
    [[nodiscard]] ErrorCode setup_outbound_connection() override;
    [[nodiscard]] ErrorCode send(const std::vector<uint8_t>& data) override;
    void dispose() override;
    
    [[nodiscard]] E3LinkLayer link_layer() const noexcept override {
        return E3LinkLayer::POSIX;
    }
    
    [[nodiscard]] E3TransportLayer transport_layer() const noexcept override {
        return transport_layer_;
    }
    
    [[nodiscard]] bool is_connected() const noexcept override {
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
    
    bool connected_{false};
    
    static constexpr int CHUNK_SIZE = 8192;
    static constexpr int UNUSED_SOCKET = -2;
    
    int send_in_chunks(int sockfd, const uint8_t* buffer, size_t buffer_size);
    int recv_with_size(int sockfd, std::vector<uint8_t>& buffer);
};

} // namespace libe3

#endif // LIBE3_POSIX_CONNECTOR_HPP
