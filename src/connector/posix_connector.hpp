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
#include "socket_link.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace libe3 {

/**
 * @brief POSIX socket-based E3 connector: one endpoint per dApp
 *
 * Supports TCP, SCTP and IPC. A dApp opens one connection to the RAN's single
 * endpoint and the setup, inbound and outbound traffic all use it; the three
 * channels of E3Connector are logical. The first PDU on a connection is the
 * dApp's setup request and goes to recv_setup_request(); every later PDU goes
 * to receive(). The inbound thread is the only reader of the sockets, so the
 * receive path costs the same as with one channel per socket.
 *
 * Each PDU is one message. SCTP and IPC (a sequenced-packet socket) keep message
 * boundaries and carry no prefix. TCP is a byte stream and prefixes each PDU
 * with its length as four bytes in network order, in both directions. SCTP
 * stamps payload protocol id 0x00E3A001 and sends indications on stream 1,
 * everything else on stream 0.
 *
 * The RAN names each accepted connection with a token that is never reused
 * (see E3Connector::last_peer_token) and can send to one peer or to all.
 */
class PosixE3Connector : public E3Connector {
public:
    /**
     * @brief Create a POSIX connector
     *
     * @param transport_layer SCTP, TCP or IPC
     * @param endpoint Endpoint of the link. IPC: a path, "ipc://" prefix optional,
     *        and the library's shared default is replaced by /tmp/dapps/e3.
     *        TCP and SCTP: "tcp://host:port" or "sctp://host:port" (a host in
     *        brackets for IPv6); anything else means the default host and @p port
     * @param port Port of the link (SCTP and TCP) when the endpoint names none
     * @param options Socket and admission options
     */
    PosixE3Connector(
        E3TransportLayer transport_layer,
        const std::string& endpoint,
        uint16_t port,
        const PosixOptions& options
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
    ErrorCode send_to(uint64_t peer_token, const std::vector<uint8_t>& data, PduClass kind) override;

    // Client-side (dApp role) operations.
    ErrorCode setup_initial_connection_client() override;
    ErrorCode send_setup_request_client(const std::vector<uint8_t>& data) override;
    int recv_setup_response_client(std::vector<uint8_t>& buffer) override;
    ErrorCode setup_inbound_connection_client() override;
    ErrorCode setup_outbound_connection_client() override;

    void dispose() override;
    void shutdown() override;
    uint64_t last_peer_token() const override;
    uint64_t last_setup_peer_token() const override;
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
    struct Peer;
    struct Queued;

    socket_link::SocketSpec spec() const;
    void accept_pending();
    std::shared_ptr<Peer> find_peer(uint64_t token);
    bool pump(std::vector<uint8_t>& out, uint64_t& token, int timeout_ms);
    void drop_peer(const std::shared_ptr<Peer>& peer);
    void push_setup(const std::shared_ptr<Peer>& peer, std::vector<uint8_t>&& pdu);
    bool receiver_idle() const;
    int receive_client(std::vector<uint8_t>& buffer);
    ErrorCode reconnect_client();

    E3TransportLayer transport_layer_;
    std::string endpoint_uri_;   ///< The endpoint as given, to read a host and port from
    uint16_t port_;
    PosixOptions options_;

    int listener_{-1};
    mutable std::mutex peers_mutex_;               ///< Guards peers_
    std::vector<std::shared_ptr<Peer>> peers_;     ///< RAN role: accepted connections
    uint64_t next_token_{1};                       ///< Only the reader takes tokens
    size_t round_robin_{0};                        ///< Reader's rotating start, so no peer starves the rest

    std::mutex reader_mutex_;                      ///< Held by whoever reads the sockets
    std::atomic<int64_t> last_receive_ns_{0};      ///< When receive() was last called

    std::mutex queue_mutex_;                       ///< Guards setup_queue_ and inbound_queue_
    std::condition_variable setup_ready_;          ///< Signals setup_queue_ and shutdown
    std::deque<Queued> setup_queue_;               ///< First PDUs of connections
    std::deque<Queued> inbound_queue_;             ///< PDUs read while the setup thread was the reader
    std::shared_ptr<Peer> setup_peer_;             ///< Sender of the last setup request, for send_response
    uint64_t last_setup_token_{0};
    uint64_t last_peer_token_{0};
    std::mutex closed_mutex_;
    std::vector<uint64_t> closed_peers_;

    int client_fd_{-1};                            ///< dApp role: the one connection
    std::vector<uint8_t> client_rx_;               ///< Stream bytes or message fragments not yet a PDU
    std::mutex client_write_mutex_;
    bool setup_request_sent_{false};
    std::atomic<bool> awaiting_setup_response_{false};
    std::mutex await_mutex_;
    std::condition_variable await_cv_;

    bool connected_{false};
    std::atomic<bool> shutdown_requested_{false};
};

} // namespace libe3

#endif // LIBE3_POSIX_CONNECTOR_HPP
