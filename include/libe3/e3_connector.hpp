/**
 * @file e3_connector.hpp
 * @brief Abstract E3 Connector interface for transport layer
 *
 * Defines the abstract interface that all transport connectors must implement.
 * Ported from the original C implementation's E3Connector structure.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
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
    /** @brief Move constructor. */
    E3Connector(E3Connector&&) = default;
    /**
     * @brief Move assignment.
     * @return This connector
     */
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
     * @return Number of bytes received; 0 on timeout (nothing yet);
     *         ErrorCode::NOT_CONNECTED once the peer has closed (dApp role);
     *         any other negative ErrorCode on a transport error
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

    // =========================================================================
    // Client-side (dApp role) operations
    //
    // Mirrors of the server-side methods above but with bind/listen replaced
    // by connect. Default implementations return INVALID_PARAM so connectors
    // that have not been taught the dApp role surface a clear error instead
    // of silently misbehaving. ZmqE3Connector and PosixE3Connector override
    // these.
    // =========================================================================

    /**
     * @brief Connect the setup channel (dApp role: REQ connect).
     * @return ErrorCode::SUCCESS on success, ErrorCode::INVALID_PARAM if the connector has no dApp role
     */
    virtual ErrorCode setup_initial_connection_client() {
        return ErrorCode::INVALID_PARAM;
    }

    /**
     * @brief Send a SetupRequest on the setup channel (dApp role).
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    virtual ErrorCode send_setup_request_client(const std::vector<uint8_t>& /*data*/) {
        return ErrorCode::INVALID_PARAM;
    }

    /**
     * @brief Receive a SetupResponse on the setup channel (dApp role).
     * @return Number of bytes received, 0 on timeout, a negative ErrorCode on error
     */
    virtual int recv_setup_response_client(std::vector<uint8_t>& /*buffer*/) {
        return static_cast<int>(ErrorCode::INVALID_PARAM);
    }

    /**
     * @brief Connect the inbound channel (dApp role: SUB connect to the
     * RAN's publisher endpoint).
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    virtual ErrorCode setup_inbound_connection_client() {
        return ErrorCode::INVALID_PARAM;
    }

    /**
     * @brief Connect the outbound channel (dApp role: PUB connect to the
     * RAN's subscriber endpoint).
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    virtual ErrorCode setup_outbound_connection_client() {
        return ErrorCode::INVALID_PARAM;
    }

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
     * @brief Which peer supplied the bytes the last receive() returned (RAN role)
     *
     * Call it on the thread that calls receive(), right after a receive() that
     * returned bytes. A token names one accepted connection and is never reused
     * by the same connector, so a closed peer's token cannot name a later one.
     *
     * @return The peer's token, or 0 for a connector that cannot tell its
     *         peers apart (the default, and ZeroMQ)
     */
    virtual uint64_t last_peer_token() const { return 0; }

    /**
     * @brief Peers whose connection has closed since the last call (RAN role)
     *
     * Call it on the thread that calls receive(). Each token is returned once,
     * and only after the bytes that peer sent before closing were delivered.
     * The default returns nothing: a connector that cannot tell its peers apart
     * cannot tell that one left.
     *
     * @return The tokens of the peers that closed, oldest first
     */
    virtual std::vector<uint64_t> take_closed_peers() { return {}; }

    /**
     * @brief Get the link layer type
     * @return The link layer this connector runs on
     */
    virtual E3LinkLayer link_layer() const noexcept = 0;

    /**
     * @brief Get the transport layer type
     * @return The transport layer this connector runs on
     */
    virtual E3TransportLayer transport_layer() const noexcept = 0;

    /**
     * @brief Check if connector is connected
     * @return true once the connector has set up its connections
     */
    virtual bool is_connected() const noexcept = 0;

    /**
     * @brief Set role (RAN or DAPP). Affects bind vs connect direction and
     * cleanup behaviour. Must be set before any setup_* call. Default RAN.
     * @param role The role of this peer
     */
    void set_role(E3Role role) noexcept { role_ = role; }

    /**
     * @brief Get current role.
     * @return The role of this peer
     */
    E3Role role() const noexcept { return role_; }

    /**
     * @brief Get setup endpoint
     * @return The endpoint of the setup channel
     */
    const std::string& setup_endpoint() const noexcept { return setup_endpoint_; }

    /**
     * @brief Get inbound endpoint
     * @return The endpoint of the inbound channel
     */
    const std::string& inbound_endpoint() const noexcept { return inbound_endpoint_; }

    /**
     * @brief Get outbound endpoint
     * @return The endpoint of the outbound channel
     */
    const std::string& outbound_endpoint() const noexcept { return outbound_endpoint_; }

protected:
    E3Connector() = default;

    std::string setup_endpoint_;     ///< Endpoint of the setup channel
    std::string inbound_endpoint_;   ///< Endpoint of the inbound channel
    std::string outbound_endpoint_;  ///< Endpoint of the outbound channel
    E3Role role_{E3Role::RAN};       ///< Role of this peer
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
 * @param setup_port Port of the setup channel (SCTP and TCP; IPC uses the endpoint path)
 * @param inbound_port Port of the inbound channel (SCTP and TCP)
 * @param outbound_port Port of the outbound channel (SCTP and TCP)
 * @param io_threads Number of I/O threads (for ZMQ)
 * @param role Role of the peer (RAN or dApp)
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
    size_t io_threads = 2,
    E3Role role = E3Role::RAN
);

} // namespace libe3

#endif // LIBE3_E3_CONNECTOR_HPP
