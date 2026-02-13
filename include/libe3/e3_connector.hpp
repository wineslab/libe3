/**
 * @file e3_connector.hpp
 * @brief Abstract E3 Connector interface for transport layer
 *
 * Defines the abstract interface that all transport connectors must implement.
 * Ported from the original C implementation's E3Connector structure.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_CONNECTOR_HPP
#define LIBE3_E3_CONNECTOR_HPP

#include "types.hpp"
#include <memory>
#include <string>
#include <functional>

namespace libe3 {

/**
 * @brief Abstract base class for E3AP transport connectors
 *
 * This class defines the transport-agnostic interface that all connectors
 * must implement. The design mirrors the original C implementation's
 * function pointer approach, but uses virtual methods for type safety.
 *
 * Derived classes:
 * - PosixE3Connector: POSIX socket-based transport (TCP, SCTP, IPC)
 * - ZmqE3Connector: ZeroMQ-based transport
 */
class E3Connector {
public:
    virtual ~E3Connector() = default;

    // Non-copyable, movable
    E3Connector(const E3Connector&) = delete;
    E3Connector& operator=(const E3Connector&) = delete;
    E3Connector(E3Connector&&) = default;
    E3Connector& operator=(E3Connector&&) = default;

    /**
     * @brief Set up the initial connection (setup channel)
     *
     * This establishes the REQ/REP channel used for E3 Setup procedures.
     * @return ErrorCode::SUCCESS on success, error code otherwise
     */
    virtual ErrorCode setup_initial_connection() = 0;

    /**
     * @brief Receive a setup request
     *
     * Blocks until a setup request is received on the setup channel.
     * @param buffer Buffer to store received data
     * @return Number of bytes received, or negative error code
     */
    virtual int recv_setup_request(std::vector<uint8_t>& buffer) = 0;

    /**
     * @brief Send a setup response
     * @param data Response data to send
     * @return ErrorCode::SUCCESS on success, error code otherwise
     */
    virtual ErrorCode send_response(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Set up the inbound connection (SUB channel)
     *
     * This establishes the channel for receiving control actions from dApps.
     * @return ErrorCode::SUCCESS on success, error code otherwise
     */
    virtual ErrorCode setup_inbound_connection() = 0;

    /**
     * @brief Receive data from the inbound channel
     * @param buffer Buffer to store received data
     * @return Number of bytes received, or negative error code
     */
    virtual int receive(std::vector<uint8_t>& buffer) = 0;

    /**
     * @brief Set up the outbound connection (PUB channel)
     *
     * This establishes the channel for sending indications to dApps.
     * @return ErrorCode::SUCCESS on success, error code otherwise
     */
    virtual ErrorCode setup_outbound_connection() = 0;

    /**
     * @brief Send data on the outbound channel
     * @param data Data to send
     * @return ErrorCode::SUCCESS on success, error code otherwise
     */
    virtual ErrorCode send(const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Clean up and release all resources
     */
    virtual void dispose() = 0;
    
    /**
     * @brief Interrupt blocking operations for shutdown
     *
     * Called before joining threads to unblock any blocking socket operations.
     * Default implementation does nothing (for connectors that use timeouts).
     */
    virtual void shutdown() {}

    /**
     * @brief Get the link layer type
     */
    virtual E3LinkLayer link_layer() const noexcept = 0;

    /**
     * @brief Get the transport layer type
     */
    virtual E3TransportLayer transport_layer() const noexcept = 0;

    /**
     * @brief Check if connector is connected
     */
    virtual bool is_connected() const noexcept = 0;

    /**
     * @brief Get setup endpoint
     */
    const std::string& setup_endpoint() const noexcept { return setup_endpoint_; }

    /**
     * @brief Get inbound endpoint
     */
    const std::string& inbound_endpoint() const noexcept { return inbound_endpoint_; }

    /**
     * @brief Get outbound endpoint
     */
    const std::string& outbound_endpoint() const noexcept { return outbound_endpoint_; }

protected:
    E3Connector() = default;
    
    std::string setup_endpoint_;
    std::string inbound_endpoint_;
    std::string outbound_endpoint_;
};

/**
 * @brief Factory function to create appropriate connector
 *
 * Creates a connector instance based on the link and transport layers specified.
 *
 * @param link_layer Link layer to use (ZMQ or POSIX)
 * @param transport_layer Transport layer to use (SCTP, TCP, or IPC)
 * @param setup_endpoint Endpoint for setup channel
 * @param inbound_endpoint Endpoint for inbound channel
 * @param outbound_endpoint Endpoint for outbound channel
 * @param io_threads Number of I/O threads (for ZMQ)
 * @return Unique pointer to created connector, nullptr on failure
 */
std::unique_ptr<E3Connector> create_connector(
    E3LinkLayer link_layer,
    E3TransportLayer transport_layer,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint,
    uint16_t setup_port,
    uint16_t inbound_port,
    uint16_t outbound_port,
    size_t io_threads = 2
);

} // namespace libe3

#endif // LIBE3_E3_CONNECTOR_HPP
