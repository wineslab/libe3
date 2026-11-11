/**
 * @file uring_connector.hpp
 * @brief The io_uring E3 connector: the POSIX link's wire, with the waiting and the sends done through io_uring
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_URING_CONNECTOR_HPP
#define LIBE3_URING_CONNECTOR_HPP

#include "libe3/e3_connector.hpp"
#include "socket_link.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace libe3 {

class UringRing;
struct RingCompletion;

/**
 * @brief io_uring E3 connector: same endpoint, sockets and bytes as PosixE3Connector
 *
 * A peer cannot tell this link from the POSIX one: one connection per dApp on one endpoint,
 * SCTP, TCP or IPC (a sequenced-packet socket), one PDU per message, TCP length-prefixed both
 * ways, SCTP payload protocol id 0x00E3A001 with indications on stream 1. A RAN of one link
 * serves a dApp of the other. What differs is how the bytes move:
 *
 * - Two rings. The receive ring holds a receive armed on every connection (and the accept,
 *   on a RAN); whichever thread reads waits on it, so a received PDU costs one io_uring_enter
 *   (re-arm and wait together) where POSIX needs poll() and recv(). The send ring takes the
 *   writes; a RAN that sends to N dApps submits the N writes at once and waits for them
 *   together, so a peer that stopped reading delays no other peer.
 * - No thread of its own. The threads that call receive() and send() drive the rings, so
 *   send() returns when the kernel has the bytes, as with POSIX, and SEND_DONE means the same
 *   on both links.
 * - Ring options, read once from the environment when the connector is made, none on by
 *   default except registered files: LIBE3_URING_DEPTH, LIBE3_URING_FIXED_FILES,
 *   LIBE3_URING_FIXED_BUFFERS, LIBE3_URING_SQPOLL, LIBE3_URING_SQPOLL_CPU and
 *   LIBE3_URING_BUSY_POLL_US (docs/connectors/uring.md). Each falls back to the plain way
 *   when the kernel refuses it; settings() says what is in effect.
 *
 * Linux 5.11 or later with io_uring allowed for the process (Docker's default seccomp
 * profile and kernel.io_uring_disabled refuse it): create() returns null and logs why otherwise.
 */
class UringE3Connector : public E3Connector {
public:
    /** @brief The ring options, as asked for or, from settings(), as in effect. */
    struct Settings {
        unsigned depth{64};           ///< LIBE3_URING_DEPTH: submission queue entries of each ring
        bool fixed_files{true};       ///< LIBE3_URING_FIXED_FILES: sockets in the rings' registered-file tables
        bool fixed_buffers{false};    ///< LIBE3_URING_FIXED_BUFFERS: receive into registered buffers (TCP, IPC)
        bool sqpoll{false};           ///< LIBE3_URING_SQPOLL: a kernel thread takes the submissions
        int sqpoll_cpu{-1};           ///< LIBE3_URING_SQPOLL_CPU: the CPU of that thread
        unsigned busy_poll_us{0};     ///< LIBE3_URING_BUSY_POLL_US: poll for a completion this long before sleeping

        /**
         * @brief The options set in the environment, the defaults for the others.
         * @return The settings asked for
         */
        static Settings from_environment();
        /**
         * @brief One line for a log.
         * @return The settings in words
         */
        std::string describe() const;
    };

    /**
     * @brief Make a connector, or null when io_uring cannot be used here (the reason is logged)
     * @param transport_layer SCTP, TCP or IPC
     * @param endpoint As for PosixE3Connector
     * @param port As for PosixE3Connector
     * @param options Socket and admission options, as for PosixE3Connector
     * @return The connector, or null
     */
    static std::unique_ptr<UringE3Connector> create(
        E3TransportLayer transport_layer,
        const std::string& endpoint,
        uint16_t port,
        const PosixOptions& options
    );

    /** @brief Close every socket and both rings. */
    ~UringE3Connector() override;

    ErrorCode setup_initial_connection() override;
    int recv_setup_request(std::vector<uint8_t>& buffer) override;
    ErrorCode send_response(const std::vector<uint8_t>& data) override;
    ErrorCode setup_inbound_connection() override;
    int receive(std::vector<uint8_t>& buffer) override;
    ErrorCode setup_outbound_connection() override;
    ErrorCode send(const std::vector<uint8_t>& data) override;
    ErrorCode send_to(uint64_t peer_token, const std::vector<uint8_t>& data, PduClass kind) override;

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
        return E3LinkLayer::URING;
    }

    E3TransportLayer transport_layer() const noexcept override {
        return transport_layer_;
    }

    bool is_connected() const noexcept override {
        return connected_;
    }

    /**
     * @brief The ring options in effect: those asked for, less what the kernel refused.
     * @return The settings in effect
     */
    const Settings& settings() const noexcept {
        return settings_;
    }

private:
    struct Conn;
    struct Slots;
    struct Ready;
    struct SendOp;

    UringE3Connector(E3TransportLayer transport_layer, const std::string& endpoint, uint16_t port,
                     const PosixOptions& options, const Settings& settings);
    bool open(std::string* error, int* error_code);
    socket_link::SocketSpec spec() const;

    std::shared_ptr<Conn> make_conn(int fd);
    void arm();
    bool arm_recv(Conn& conn);
    template <typename Prepare> bool prepare(UringRing& ring, Prepare&& how);
    void pump(int timeout_ms);
    void handle(const RingCompletion& completion);
    void on_accept(int result);
    void on_recv(const std::shared_ptr<Conn>& conn, int result);
    void deliver(const std::shared_ptr<Conn>& conn, std::vector<uint8_t>&& pdu);
    void drop(const std::shared_ptr<Conn>& conn);
    bool take_ready(std::vector<uint8_t>& buffer, bool& closed_noted);
    bool receiver_idle() const;
    ErrorCode transmit(const std::vector<uint8_t>& data, PduClass kind);
    bool prepare_send(size_t index, const std::vector<uint8_t>& data, PduClass kind);
    int read_client(std::vector<uint8_t>& buffer, int timeout_ms);
    void close_client_locked();
    ErrorCode connect_client();
    std::shared_ptr<Conn> client() const;
    void drain_rx_locked(int64_t timeout_us);

    E3TransportLayer transport_layer_;
    std::string endpoint_uri_;
    uint16_t port_;
    PosixOptions options_;
    Settings settings_;

    std::unique_ptr<UringRing> rx_;                 ///< Accepts and receives; driven by whoever holds reader_mutex_
    std::unique_ptr<UringRing> tx_;                 ///< Sends; driven by whoever holds tx_mutex_
    std::shared_ptr<Slots> slots_;                  ///< Registered-file and buffer slots, shared with the connections
    int wake_fd_{-1};                               ///< An eventfd that shutdown() writes to wake the reader

    // Reader state: only the holder of reader_mutex_ touches it.
    std::mutex reader_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<Conn>> conns_;   ///< RAN role: by token
    std::vector<RingCompletion> completions_;
    sockaddr_storage accept_address_{};
    socklen_t accept_length_{0};
    bool accept_armed_{false};
    int64_t accept_paused_until_ns_{0};
    uint64_t wake_value_{0};
    bool wake_armed_{false};
    size_t produced_{0};                            ///< PDUs and closes the current pump handed on
    std::deque<std::vector<uint8_t>> client_ready_; ///< dApp role: PDUs read and not yet returned
    bool client_closed_{false};
    bool client_failed_{false};

    int listener_{-1};
    std::atomic<uint64_t> next_token_{1};           ///< Never reused by this connector
    mutable std::mutex peers_mutex_;                ///< Guards peers_
    std::vector<std::shared_ptr<Conn>> peers_;      ///< RAN role: what senders may send to
    std::atomic<int64_t> last_receive_ns_{0};

    std::mutex queue_mutex_;                        ///< Guards setup_queue_ and ready_
    std::condition_variable setup_ready_;
    std::deque<Ready> setup_queue_;                 ///< First PDUs of connections
    std::deque<Ready> ready_;                       ///< Later PDUs, and closes, in the order they came
    std::shared_ptr<Conn> setup_conn_;              ///< Sender of the last setup request, for send_response
    uint64_t last_setup_token_{0};
    uint64_t last_peer_token_{0};
    std::mutex closed_mutex_;
    std::vector<uint64_t> closed_peers_;

    std::mutex tx_mutex_;                           ///< Held by whoever drives the send ring
    std::vector<std::shared_ptr<Conn>> targets_;    ///< Where the current send goes
    std::vector<SendOp> send_ops_;
    std::vector<RingCompletion> send_completions_;
    uint8_t prefix_[socket_link::TCP_PREFIX_BYTES]{};

    mutable std::mutex client_mutex_;               ///< Guards client_
    std::shared_ptr<Conn> client_;                  ///< dApp role: the one connection
    std::vector<std::shared_ptr<Conn>> retired_;    ///< Replaced connections the kernel had not let go of
    bool setup_request_sent_{false};
    std::atomic<bool> awaiting_setup_response_{false};
    std::mutex await_mutex_;
    std::condition_variable await_cv_;

    bool connected_{false};
    bool disposed_{false};
    std::atomic<bool> shutdown_requested_{false};
};

}  // namespace libe3

#endif  // LIBE3_URING_CONNECTOR_HPP
