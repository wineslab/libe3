/**
 * @file zmq_connector.hpp
 * @brief ZeroMQ-based E3 Connector
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_ZMQ_CONNECTOR_HPP
#define LIBE3_ZMQ_CONNECTOR_HPP

#include "libe3/e3_connector.hpp"
#include <memory>

namespace libe3 {

/**
 * @brief ZeroMQ-based E3 connector
 *
 * Provides transport using ZeroMQ sockets. Supports TCP and IPC transports.
 * Ported from the original C implementation's zeromq_* functions.
 */
class ZmqE3Connector : public E3Connector {
public:
    ZmqE3Connector(
        E3TransportLayer transport_layer,
        const std::string& setup_endpoint,
        const std::string& inbound_endpoint,
        const std::string& outbound_endpoint,
        uint16_t setup_port,
        uint16_t inbound_port,
        uint16_t outbound_port,
        size_t io_threads
    );
    
    ~ZmqE3Connector() override;

    ErrorCode setup_initial_connection() override;
    int recv_setup_request(std::vector<uint8_t>& buffer) override;
    ErrorCode send_response(const std::vector<uint8_t>& data) override;
    ErrorCode setup_inbound_connection() override;
    int receive(std::vector<uint8_t>& buffer) override;
    ErrorCode setup_outbound_connection() override;
    ErrorCode send(const std::vector<uint8_t>& data) override;
    void dispose() override;
    void shutdown() override;
    
    E3LinkLayer link_layer() const noexcept override {
        return E3LinkLayer::ZMQ;
    }
    
    E3TransportLayer transport_layer() const noexcept override {
        return transport_layer_;
    }
    
    bool is_connected() const noexcept override {
        return connected_;
    }

private:
    E3TransportLayer transport_layer_;
    size_t io_threads_;
    uint16_t setup_port_;
    uint16_t inbound_port_;
    uint16_t outbound_port_;
    
    void* context_{nullptr};
    void* setup_socket_{nullptr};
    void* inbound_socket_{nullptr};
    void* outbound_socket_{nullptr};
    
    bool connected_{false};
    
    void setup_ipc_permissions(const std::string& path);
    bool reset_setup_socket();
};

} // namespace libe3

#endif // LIBE3_ZMQ_CONNECTOR_HPP
