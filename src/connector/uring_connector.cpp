/**
 * @file uring_connector.cpp
 * @brief io_uring connector implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uring_connector.hpp"
#include "uring_ring.hpp"
#include "libe3/logger.hpp"

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>

namespace libe3 {

using namespace socket_link;

namespace {

constexpr const char* LOG_TAG = "UringConn";
constexpr int RECEIVE_TIMEOUT_MS = 500;       // As long as the POSIX link's receive waits
constexpr int SETUP_PUMP_MS = 100;
constexpr int64_t SETUP_PUMP_AFTER_NS = 1'000'000'000;   // The setup thread reads if receive() has been idle this long
constexpr unsigned SLOT_COUNT = 256;          // Connections beyond it use plain descriptors
constexpr size_t STREAM_CAPACITY = 2 * (MAX_PDU + TCP_PREFIX_BYTES);  // TCP: room for two whole frames
constexpr size_t MESSAGE_CAPACITY = MAX_PDU + 1;                     // One byte more than a PDU may have

// user_data: what the operation is in the top byte, the connection or send below it.
enum Op : uint64_t { OP_ACCEPT = 1, OP_RECV = 2, OP_WAKE = 3, OP_SEND = 4, OP_CANCEL = 5 };
constexpr uint64_t ID_MASK = (uint64_t{1} << 56) - 1;

inline uint64_t data_for(Op op, uint64_t id) { return (static_cast<uint64_t>(op) << 56) | (id & ID_MASK); }
inline Op op_of(uint64_t data) { return static_cast<Op>(data >> 56); }
inline uint64_t id_of(uint64_t data) { return data & ID_MASK; }

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool read_number(const char* name, long low, long high, long& out) {
    const char* text = std::getenv(name);
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < low || value > high) {
        E3_LOG_WARN(LOG_TAG) << "Ignoring " << name << "=" << text << ": expected a number from "
                             << low << " to " << high;
        return false;
    }
    out = value;
    return true;
}

/// What a completed receive gave.
enum class Got { MORE, CLOSED, FAILED };

}  // namespace

// ===========================================================================
// Settings
// ===========================================================================

UringE3Connector::Settings UringE3Connector::Settings::from_environment() {
    Settings s;
    long value = 0;
    if (read_number("LIBE3_URING_DEPTH", 4, 4096, value)) s.depth = static_cast<unsigned>(value);
    if (read_number("LIBE3_URING_FIXED_FILES", 0, 1, value)) s.fixed_files = value != 0;
    if (read_number("LIBE3_URING_FIXED_BUFFERS", 0, 1, value)) s.fixed_buffers = value != 0;
    if (read_number("LIBE3_URING_SQPOLL", 0, 1, value)) s.sqpoll = value != 0;
    if (read_number("LIBE3_URING_SQPOLL_CPU", -1, 4095, value)) s.sqpoll_cpu = static_cast<int>(value);
    if (read_number("LIBE3_URING_BUSY_POLL_US", 0, 10'000'000, value)) s.busy_poll_us = static_cast<unsigned>(value);
    return s;
}

std::string UringE3Connector::Settings::describe() const {
    std::ostringstream out;
    out << "depth " << depth << ", registered files " << (fixed_files ? "on" : "off")
        << ", registered buffers " << (fixed_buffers ? "on" : "off")
        << ", kernel submission thread " << (sqpoll ? "on" : "off");
    if (sqpoll && sqpoll_cpu >= 0) out << " (CPU " << sqpoll_cpu << ")";
    out << ", busy poll " << busy_poll_us << " us";
    return out.str();
}

// ===========================================================================
// Connections, slots and queued PDUs
// ===========================================================================

/** @brief Registered-file and buffer slots, shared with the connections that hold them. */
struct UringE3Connector::Slots {
    std::mutex mu;                 ///< Guards everything below
    UringRing* rx{nullptr};        ///< The receive ring; null once the rings are gone
    UringRing* tx{nullptr};        ///< The send ring; null once the rings are gone
    bool files{false};             ///< Connections take a registered-file slot
    bool buffers{false};           ///< Connections register their receive buffer
    std::vector<int> free;         ///< Slots no connection holds

    /**
     * @brief Take a free slot.
     * @return The slot, or -1 when all are taken
     */
    int take() {
        std::lock_guard<std::mutex> lock(mu);
        if (free.empty()) return -1;
        const int slot = free.back();
        free.pop_back();
        return slot;
    }
};

/** @brief One connection. The reader alone touches the receive state. Closes its socket when the last reference goes. */
struct UringE3Connector::Conn {
    int fd{-1};                    ///< The socket
    uint64_t token{0};             ///< Never reused by this connector
    E3TransportLayer transport{E3TransportLayer::TCP};   ///< The connector's transport
    std::shared_ptr<Slots> slots;  ///< Where its slot goes back to
    int slot{-1};                  ///< Taken from slots, or -1
    int file_slot{-1};             ///< Registered in both rings, or -1
    int buffer_slot{-1};           ///< The receive buffer is registered there, or -1
    std::unique_ptr<uint8_t[]> rx; ///< The receive buffer
    size_t capacity{0};            ///< Its size
    size_t held{0};                ///< TCP: stream bytes kept; SCTP: bytes of the message joined so far
    bool recv_in_flight{false};    ///< A receive is armed on the ring
    bool seen_first{false};        ///< The first PDU, the setup request, was read
    bool closed{false};            ///< Dropped, or the peer closed
    struct iovec iov{};            ///< SCTP: the receive's buffer
    struct msghdr msg{};           ///< SCTP: the receive's header; the kernel writes flags and control back
    alignas(struct cmsghdr) char control[SCTP_RECEIVE_CONTROL_BYTES]{};   ///< SCTP: the receive info

    /**
     * @brief The socket as the rings name it.
     * @return Its descriptor and registered-file slot
     */
    RingFile file() const { return RingFile{fd, file_slot}; }

    /** @brief Empty its slots in the rings that are still there, and close the socket. */
    ~Conn() {
        if (slot >= 0 && slots) {
            std::lock_guard<std::mutex> lock(slots->mu);
            if (slots->rx != nullptr) {
                if (file_slot >= 0) slots->rx->set_file(static_cast<unsigned>(file_slot), -1);
                if (buffer_slot >= 0) slots->rx->set_buffer(static_cast<unsigned>(buffer_slot), nullptr, 0);
            }
            if (slots->tx != nullptr && file_slot >= 0) {
                slots->tx->set_file(static_cast<unsigned>(file_slot), -1);
            }
            slots->free.push_back(slot);
        }
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

/** @brief A PDU read and not yet taken, or the close of a connection. */
struct UringE3Connector::Ready {
    std::shared_ptr<Conn> conn;    ///< Setup queue: who to answer
    uint64_t token{0};             ///< Who sent it
    std::vector<uint8_t> data;     ///< The PDU
    bool closed{false};            ///< The connection closed after the PDUs before this one
};

/** @brief One write in flight, kept in place until the kernel completes it. */
struct UringE3Connector::SendOp {
    std::shared_ptr<Conn> conn;    ///< Where it goes
    size_t done{0};                ///< Bytes the kernel took so far, prefix included
    int state{0};                  ///< 0 in flight, 1 sent, 2 failed
    int error{0};                  ///< Why it failed
    struct iovec iov[2]{};         ///< What is left to write: the prefix, then the PDU
    struct msghdr msg{};           ///< The sendmsg header
    alignas(struct cmsghdr) char control[SCTP_SEND_CONTROL_BYTES]{};   ///< SCTP: the send info
};

// ===========================================================================
// Creation and teardown
// ===========================================================================

UringE3Connector::UringE3Connector(E3TransportLayer transport_layer, const std::string& endpoint, uint16_t port,
                                   const PosixOptions& options, const Settings& settings)
    : transport_layer_(transport_layer)
    , endpoint_uri_(endpoint)
    , port_(port)
    , options_(options)
    , settings_(settings)
{
    setup_endpoint_ = link_endpoint(transport_layer, endpoint);
}

std::unique_ptr<UringE3Connector> UringE3Connector::create(
    E3TransportLayer transport_layer, const std::string& endpoint, uint16_t port, const PosixOptions& options) {
    std::unique_ptr<UringE3Connector> conn(
        new UringE3Connector(transport_layer, endpoint, port, options, Settings::from_environment()));
    std::string error;
    int code = 0;
    if (!conn->open(&error, &code)) {
        std::string hint;
        if (code == EPERM || code == EACCES) {
            hint = "io_uring is not permitted for this process: a seccomp profile refuses it (Docker's default "
                   "does: run with --security-opt seccomp=unconfined, or a profile that allows io_uring_setup, "
                   "io_uring_enter and io_uring_register), or the sysctl kernel.io_uring_disabled is 1 or 2";
        } else if (code == ENOSYS) {
            hint = "this kernel has no io_uring, or one older than Linux 5.11";
        } else if (code == ENOMEM) {
            hint = "the rings could not be locked in memory (RLIMIT_MEMLOCK)";
        }
        E3_LOG_ERROR(LOG_TAG) << "Cannot use the io_uring link: " << error << (hint.empty() ? "" : ". ") << hint
                              << ". The POSIX link speaks the same wire and needs none of this.";
        return nullptr;
    }
    E3_LOG_INFO(LOG_TAG) << "Creating io_uring connector, one endpoint: " << conn->setup_endpoint_ << " port "
                         << port << "; rings: " << conn->settings_.describe();
    return conn;
}

bool UringE3Connector::open(std::string* error, int* error_code) {
    RingOptions options;
    options.depth = settings_.depth;
    options.sqpoll = settings_.sqpoll;
    options.sqpoll_cpu = settings_.sqpoll_cpu;
    options.file_slots = settings_.fixed_files ? SLOT_COUNT : 0;
    options.buffer_slots = settings_.fixed_buffers ? SLOT_COUNT : 0;
    rx_ = UringRing::create(options, nullptr, error, error_code);
    if (!rx_ && options.sqpoll) {
        E3_LOG_WARN(LOG_TAG) << "The kernel refused a submission thread (" << *error << "); going on without one";
        options.sqpoll = false;
        settings_.sqpoll = false;
        rx_ = UringRing::create(options, nullptr, error, error_code);
    }
    if (!rx_) {
        return false;
    }
    RingOptions send_options = options;
    send_options.buffer_slots = 0;
    tx_ = UringRing::create(send_options, rx_.get(), error, error_code);
    if (!tx_ && send_options.sqpoll) {
        tx_ = UringRing::create(send_options, nullptr, error, error_code);  // its own submission thread
    }
    if (!tx_) {
        rx_.reset();
        return false;
    }
    settings_.depth = rx_->sq_entries();
    if (settings_.fixed_files && (rx_->file_slots() == 0 || tx_->file_slots() == 0)) {
        E3_LOG_WARN(LOG_TAG) << "The kernel refused registered files; using plain descriptors";
        settings_.fixed_files = false;
    }
    if (settings_.fixed_buffers && rx_->buffer_slots() == 0) {
        E3_LOG_WARN(LOG_TAG) << "The kernel refused registered buffers; receiving into plain buffers";
        settings_.fixed_buffers = false;
    }
    slots_ = std::make_shared<Slots>();
    slots_->rx = rx_.get();
    slots_->tx = tx_.get();
    slots_->files = settings_.fixed_files;
    slots_->buffers = settings_.fixed_buffers;
    if (slots_->files || slots_->buffers) {
        for (int slot = static_cast<int>(SLOT_COUNT) - 1; slot >= 0; --slot) slots_->free.push_back(slot);
    }
    wake_fd_ = eventfd(0, EFD_CLOEXEC);
    if (wake_fd_ < 0) {
        *error_code = errno;
        *error = std::string("eventfd: ") + strerror(errno);
        return false;
    }
    completions_.reserve(rx_->cq_entries());
    send_completions_.reserve(tx_->cq_entries());
    return true;
}

UringE3Connector::~UringE3Connector() {
    dispose();
}

SocketSpec UringE3Connector::spec() const {
    return SocketSpec{transport_layer_, setup_endpoint_, parse_target(endpoint_uri_, port_), options_, LOG_TAG};
}

std::shared_ptr<UringE3Connector::Conn> UringE3Connector::make_conn(int fd) {
    auto conn = std::make_shared<Conn>();
    conn->fd = fd;
    conn->token = next_token_.fetch_add(1);
    conn->transport = transport_layer_;
    conn->slots = slots_;
    conn->capacity = transport_layer_ == E3TransportLayer::TCP ? STREAM_CAPACITY : MESSAGE_CAPACITY;
    conn->rx.reset(new uint8_t[conn->capacity]);
    if (slots_->files || slots_->buffers) {
        conn->slot = slots_->take();
    }
    if (conn->slot >= 0) {
        const auto slot = static_cast<unsigned>(conn->slot);
        if (slots_->files) {
            if (rx_->set_file(slot, fd) == 0 && tx_->set_file(slot, fd) == 0) {
                conn->file_slot = conn->slot;
            } else {
                rx_->set_file(slot, -1);
                tx_->set_file(slot, -1);
            }
        }
        // SCTP needs recvmsg for the receive info, which takes no registered buffer.
        if (slots_->buffers && transport_layer_ != E3TransportLayer::SCTP) {
            const int rc = rx_->set_buffer(slot, conn->rx.get(), conn->capacity);
            if (rc == 0) {
                conn->buffer_slot = conn->slot;
            } else {
                E3_LOG_WARN(LOG_TAG) << "Could not register a receive buffer (" << strerror(-rc)
                                     << "); this connection receives into a plain one";
            }
        }
    }
    return conn;
}

// ===========================================================================
// Reading: whoever holds reader_mutex_ drives the receive ring
// ===========================================================================

template <typename Prepare>
bool UringE3Connector::prepare(UringRing& ring, Prepare&& how) {
    // A full submission queue empties when it is handed over; with a kernel thread, when it has caught up.
    for (int attempt = 0; attempt < 10000; ++attempt) {
        if (how()) {
            return true;
        }
        if (ring.submit() < 0 || attempt > 100) {
            std::this_thread::yield();
        }
    }
    E3_LOG_ERROR(LOG_TAG) << "The submission queue stayed full";
    return false;
}

bool UringE3Connector::arm_recv(Conn& conn) {
    const uint64_t data = data_for(OP_RECV, conn.token);
    bool armed;
    if (transport_layer_ == E3TransportLayer::SCTP) {
        conn.iov.iov_base = conn.rx.get() + conn.held;
        conn.iov.iov_len = conn.capacity - conn.held;
        conn.msg = {};
        conn.msg.msg_iov = &conn.iov;
        conn.msg.msg_iovlen = 1;
        conn.msg.msg_control = conn.control;
        conn.msg.msg_controllen = sizeof(conn.control);
        armed = prepare(*rx_, [&] { return rx_->recvmsg(conn.file(), &conn.msg, 0, data); });
    } else {
        uint8_t* at = conn.rx.get() + conn.held;
        const size_t room = conn.capacity - conn.held;
        if (conn.buffer_slot >= 0) {
            armed = prepare(*rx_, [&] {
                return rx_->read_fixed(conn.file(), at, room, static_cast<unsigned>(conn.buffer_slot), data);
            });
        } else {
            // IPC: MSG_TRUNC makes the result the message's own length, so one too long is seen as such.
            const int flags = transport_layer_ == E3TransportLayer::IPC ? MSG_TRUNC : 0;
            armed = prepare(*rx_, [&] { return rx_->recv(conn.file(), at, room, flags, data); });
        }
    }
    conn.recv_in_flight = armed;
    return armed;
}

void UringE3Connector::arm() {
    if (shutdown_requested_.load()) {
        return;
    }
    if (!wake_armed_ && wake_fd_ >= 0) {
        wake_armed_ = prepare(*rx_, [&] { return rx_->read_eventfd(wake_fd_, &wake_value_, data_for(OP_WAKE, 0)); });
    }
    if (role_ == E3Role::DAPP) {
        auto conn = client();
        if (conn && !conn->recv_in_flight && !conn->closed) {
            arm_recv(*conn);
        }
        return;
    }
    if (listener_ >= 0 && !accept_armed_ && now_ns() >= accept_paused_until_ns_) {
        accept_length_ = sizeof(accept_address_);
        accept_armed_ = prepare(*rx_, [&] {
            return rx_->accept(RingFile{listener_, -1}, reinterpret_cast<sockaddr*>(&accept_address_),
                               &accept_length_, data_for(OP_ACCEPT, 0));
        });
    }
    for (auto& entry : conns_) {
        Conn& conn = *entry.second;
        if (!conn.recv_in_flight && !conn.closed) {
            arm_recv(conn);
        }
    }
}

/**
 * @brief Turn what a completed receive brought into whole PDUs.
 *
 * TCP: stream bytes, cut by the length prefix. IPC: one receive is one PDU, and one longer than
 * the limit is an error rather than a silent cut. SCTP: one recvmsg is one fragment of a message,
 * joined up to MSG_EOR; notifications are skipped, and a payload protocol id other than 0 and
 * libe3's is an error.
 * @param transport The connector's transport
 * @param rx The connection's receive buffer
 * @param held Bytes kept in it from earlier receives; updated
 * @param msg SCTP: the receive's header, with the flags and control data the kernel wrote
 * @param first Nothing has been read from this connection yet
 * @param result The completion's result: bytes, 0 for a close, or -errno
 * @param pdus Whole PDUs are appended here
 * @return MORE to go on receiving, CLOSED or FAILED to drop the connection
 */
static Got consume_into(E3TransportLayer transport, uint8_t* rx, size_t& held, const struct msghdr& msg,
                        bool first, int result, std::vector<std::vector<uint8_t>>& pdus) {
    if (result == 0) {
        return Got::CLOSED;
    }
    if (result < 0) {
        if (result == -EAGAIN || result == -EINTR || result == -ECANCELED || result == -ENOBUFS) {
            return Got::MORE;
        }
        E3_LOG_DEBUG(LOG_TAG) << "Receive failed: " << strerror(-result);
        return Got::FAILED;
    }
    const auto n = static_cast<size_t>(result);
    if (transport == E3TransportLayer::TCP) {
        held += n;
        size_t offset = 0;
        for (;;) {
            size_t size = 0;
            const StreamFrame frame = parse_stream_frame(rx + offset, held - offset, size);
            if (frame == StreamFrame::NEED_MORE) {
                break;
            }
            if (frame == StreamFrame::INVALID) {
                E3_LOG_ERROR(LOG_TAG) << "Invalid PDU length " << size;
                if (first && pdus.empty()) {
                    E3_LOG_INFO(LOG_TAG) << "The first message is not a length-prefixed PDU. A dApp built before "
                                            "libe3 0.6 sends its setup request without a prefix.";
                }
                return Got::FAILED;
            }
            const uint8_t* start = rx + offset + TCP_PREFIX_BYTES;
            pdus.emplace_back(start, start + size);
            offset += TCP_PREFIX_BYTES + size;
        }
        if (offset > 0) {
            std::memmove(rx, rx + offset, held - offset);
            held -= offset;
        }
        return Got::MORE;
    }
    if (transport == E3TransportLayer::IPC) {
        if (n > MAX_PDU) {
            E3_LOG_ERROR(LOG_TAG) << "PDU longer than " << MAX_PDU << " bytes";
            return Got::FAILED;
        }
        pdus.emplace_back(rx, rx + n);
        return Got::MORE;
    }
    if (sctp_is_notification(msg)) {
        return Got::MORE;
    }
    if (held == 0 && !sctp_ppid_acceptable(msg, LOG_TAG)) {
        return Got::FAILED;
    }
    held += n;
    if (held > MAX_PDU) {
        E3_LOG_ERROR(LOG_TAG) << "PDU longer than " << MAX_PDU << " bytes";
        return Got::FAILED;
    }
    if (msg.msg_flags & MSG_EOR) {
        pdus.emplace_back(rx, rx + held);
        held = 0;
    }
    return Got::MORE;
}

void UringE3Connector::handle(const RingCompletion& completion) {
    const uint64_t id = id_of(completion.user_data);
    switch (op_of(completion.user_data)) {
        case OP_WAKE:
            wake_armed_ = false;
            break;
        case OP_ACCEPT:
            accept_armed_ = false;
            on_accept(completion.res);
            break;
        case OP_RECV: {
            if (role_ == E3Role::DAPP) {
                auto conn = client();
                if (!conn || conn->token != id) {
                    break;  // a connection that was replaced
                }
                conn->recv_in_flight = false;
                std::vector<std::vector<uint8_t>> pdus;
                const Got got = consume_into(transport_layer_, conn->rx.get(), conn->held, conn->msg,
                                             !conn->seen_first, completion.res, pdus);
                for (auto& pdu : pdus) {
                    conn->seen_first = true;
                    client_ready_.push_back(std::move(pdu));
                }
                if (got != Got::MORE) {
                    conn->closed = true;
                    (got == Got::CLOSED ? client_closed_ : client_failed_) = true;
                }
                break;
            }
            auto found = conns_.find(id);
            if (found == conns_.end()) {
                break;
            }
            std::shared_ptr<Conn> conn = found->second;
            conn->recv_in_flight = false;
            on_recv(conn, completion.res);
            break;
        }
        default:
            break;
    }
}

void UringE3Connector::on_accept(int result) {
    if (result < 0) {
        if (result != -EINTR && result != -EAGAIN && result != -ECANCELED && result != -ECONNABORTED) {
            E3_LOG_WARN(LOG_TAG) << "Accept failed: " << strerror(-result);
            accept_paused_until_ns_ = now_ns() + 100'000'000;  // a full descriptor table must not spin the reader
        }
        return;
    }
    const int fd = result;
    size_t connected;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        connected = peers_.size();
    }
    const SocketSpec where = spec();
    if (!admit(where, accept_address_, connected)) {
        ::close(fd);
        return;
    }
    tune_socket(where, fd, SocketKind::ACCEPTED);
    auto conn = make_conn(fd);
    conns_[conn->token] = conn;
    size_t count;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.push_back(conn);
        count = peers_.size();
    }
    E3_LOG_INFO(LOG_TAG) << "Peer connected (" << count << " total)";
}

void UringE3Connector::on_recv(const std::shared_ptr<Conn>& conn, int result) {
    std::vector<std::vector<uint8_t>> pdus;
    const Got got = consume_into(transport_layer_, conn->rx.get(), conn->held, conn->msg, !conn->seen_first,
                                 result, pdus);
    for (auto& pdu : pdus) {
        deliver(conn, std::move(pdu));
    }
    if (got != Got::MORE) {
        drop(conn);
    }
}

void UringE3Connector::deliver(const std::shared_ptr<Conn>& conn, std::vector<uint8_t>&& pdu) {
    ++produced_;
    if (!conn->seen_first) {
        conn->seen_first = true;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            setup_queue_.push_back(Ready{conn, conn->token, std::move(pdu), false});
        }
        setup_ready_.notify_one();
        return;
    }
    std::lock_guard<std::mutex> lock(queue_mutex_);
    ready_.push_back(Ready{nullptr, conn->token, std::move(pdu), false});
}

void UringE3Connector::drop(const std::shared_ptr<Conn>& conn) {
    if (conn->closed) {
        return;
    }
    conn->closed = true;
    ++produced_;
    ::shutdown(conn->fd, SHUT_RDWR);
    conns_.erase(conn->token);
    size_t remaining;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.erase(std::remove(peers_.begin(), peers_.end(), conn), peers_.end());
        remaining = peers_.size();
    }
    {
        // After the PDUs it sent, so the caller hears of the close only once they were taken.
        std::lock_guard<std::mutex> lock(queue_mutex_);
        ready_.push_back(Ready{nullptr, conn->token, {}, true});
    }
    if (!conn->seen_first) {
        E3_LOG_INFO(LOG_TAG) << "A connection closed before it sent a complete setup request. A dApp built "
                                "before libe3 0.6 uses three connections and unframed messages, which "
                                "this endpoint does not serve.";
    }
    E3_LOG_INFO(LOG_TAG) << "Peer disconnected (" << remaining << " remain)";
}

void UringE3Connector::pump(int timeout_ms) {
    produced_ = 0;
    const int64_t deadline = now_ns() + static_cast<int64_t>(timeout_ms) * 1'000'000;
    for (;;) {
        arm();
        const int64_t left_us = (deadline - now_ns()) / 1000;
        if (left_us <= 0) {
            return;
        }
        completions_.clear();
        const int got = rx_->wait(completions_, 1, left_us, settings_.busy_poll_us);
        if (got < 0) {
            E3_LOG_ERROR(LOG_TAG) << "Waiting on the receive ring failed: " << strerror(-got);
            return;
        }
        for (const auto& completion : completions_) {
            handle(completion);
        }
        if (produced_ > 0 || shutdown_requested_.load()) {
            return;
        }
    }
}

bool UringE3Connector::take_ready(std::vector<uint8_t>& buffer, bool& closed_noted) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!ready_.empty() && ready_.front().closed) {
        {
            std::lock_guard<std::mutex> closed(closed_mutex_);
            closed_peers_.push_back(ready_.front().token);
        }
        ready_.pop_front();
        closed_noted = true;
    }
    if (ready_.empty()) {
        return false;
    }
    Ready& next = ready_.front();
    buffer = std::move(next.data);
    last_peer_token_ = next.token;
    ready_.pop_front();
    return true;
}

bool UringE3Connector::receiver_idle() const {
    return now_ns() - last_receive_ns_.load() > SETUP_PUMP_AFTER_NS;
}

/**
 * Take back every operation armed on the receive ring, and handle what completes meanwhile as
 * usual (data that arrived is kept). The kernel runs a completion's work on the thread that armed
 * the operation, and the signal it uses to make that thread run it cuts short that thread's other
 * blocking calls (a connect() with a send timeout returns EINTR). So a thread that reads only in
 * passing (the setup thread standing in for an idle receive()) leaves nothing armed behind it.
 */
void UringE3Connector::drain_rx_locked(int64_t timeout_us) {
    if (!rx_) {
        return;
    }
    auto armed = [&] {
        std::vector<uint64_t> found;
        if (accept_armed_) found.push_back(data_for(OP_ACCEPT, 0));
        if (wake_armed_) found.push_back(data_for(OP_WAKE, 0));
        for (auto& entry : conns_) {
            if (entry.second->recv_in_flight) found.push_back(data_for(OP_RECV, entry.first));
        }
        if (auto conn = client(); conn && conn->recv_in_flight) {
            found.push_back(data_for(OP_RECV, conn->token));
        }
        return found;
    };
    std::vector<uint64_t> in_flight = armed();
    if (in_flight.empty()) {
        return;
    }
    for (uint64_t data : in_flight) {
        prepare(*rx_, [&] { return rx_->cancel(data, data_for(OP_CANCEL, 0)); });
    }
    const int64_t deadline = now_ns() + timeout_us * 1000;
    while (!armed().empty() && now_ns() < deadline) {
        completions_.clear();
        if (rx_->wait(completions_, 1, std::max<int64_t>(1, (deadline - now_ns()) / 1000), 0) < 0) {
            break;
        }
        for (const auto& completion : completions_) {
            handle(completion);
        }
    }
}

// ===========================================================================
// RAN role
// ===========================================================================

ErrorCode UringE3Connector::setup_initial_connection() {
    // A blocking listener: io_uring gives -EAGAIN back for an O_NONBLOCK one instead of waiting.
    listener_ = listen_on(spec(), /*nonblocking=*/false);
    if (listener_ < 0) {
        return ErrorCode::CONNECTION_FAILED;
    }
    connected_ = true;
    E3_LOG_INFO(LOG_TAG) << "Listening on " << setup_endpoint_ << " port " << port_;
    return ErrorCode::SUCCESS;
}

ErrorCode UringE3Connector::setup_inbound_connection() {
    return listener_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_INITIALIZED;
}

ErrorCode UringE3Connector::setup_outbound_connection() {
    return listener_ >= 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_INITIALIZED;
}

int UringE3Connector::receive(std::vector<uint8_t>& buffer) {
    if (role_ == E3Role::DAPP) {
        auto conn = client();
        if (!conn) {
            return static_cast<int>(ErrorCode::NOT_CONNECTED);
        }
        if (awaiting_setup_response_.load()) {
            std::unique_lock<std::mutex> lock(await_mutex_);
            await_cv_.wait_for(lock, std::chrono::milliseconds(RECEIVE_TIMEOUT_MS), [this] {
                return !awaiting_setup_response_.load() || shutdown_requested_.load();
            });
            if (awaiting_setup_response_.load()) {
                return 0;
            }
        }
        std::lock_guard<std::mutex> reader(reader_mutex_);
        return read_client(buffer, RECEIVE_TIMEOUT_MS);
    }
    if (listener_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    last_receive_ns_.store(now_ns());
    bool closed_noted = false;
    if (take_ready(buffer, closed_noted)) {
        return static_cast<int>(buffer.size());
    }
    if (closed_noted) {
        return 0;  // so the caller learns of the closed peers now rather than after the next wait
    }
    {
        std::lock_guard<std::mutex> reader(reader_mutex_);
        if (!shutdown_requested_.load()) {
            pump(RECEIVE_TIMEOUT_MS);
        }
    }
    if (take_ready(buffer, closed_noted)) {
        return static_cast<int>(buffer.size());
    }
    return 0;
}

int UringE3Connector::recv_setup_request(std::vector<uint8_t>& buffer) {
    if (listener_ < 0) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(RECEIVE_TIMEOUT_MS);
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!setup_queue_.empty()) {
                Ready item = std::move(setup_queue_.front());
                setup_queue_.pop_front();
                buffer = std::move(item.data);
                setup_conn_ = item.conn;
                last_setup_token_ = item.token;
                return static_cast<int>(buffer.size());
            }
        }
        if (shutdown_requested_.load() || std::chrono::steady_clock::now() >= deadline) {
            return 0;
        }
        // The inbound thread is the reader. When nothing calls receive() (a caller that only
        // serves setup), this thread reads instead.
        if (receiver_idle() && reader_mutex_.try_lock()) {
            pump(SETUP_PUMP_MS);
            drain_rx_locked(100'000);
            reader_mutex_.unlock();
        } else {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            setup_ready_.wait_for(lock, std::chrono::milliseconds(50),
                                  [this] { return !setup_queue_.empty() || shutdown_requested_.load(); });
        }
    }
}

ErrorCode UringE3Connector::send_response(const std::vector<uint8_t>& data) {
    std::shared_ptr<Conn> conn = setup_conn_;
    if (!conn) {
        return ErrorCode::NOT_CONNECTED;
    }
    if (shutdown_requested_.load()) {
        return ErrorCode::CANCELLED;
    }
    std::lock_guard<std::mutex> lock(tx_mutex_);
    targets_.assign(1, conn);
    return transmit(data, PduClass::PROCEDURE);
}

ErrorCode UringE3Connector::send(const std::vector<uint8_t>& data) {
    return send_to(0, data, PduClass::PROCEDURE);
}

ErrorCode UringE3Connector::send_to(uint64_t peer_token, const std::vector<uint8_t>& data, PduClass kind) {
    // Checked first: shutdown() leaves the sockets open until dispose(), so a send racing it
    // would otherwise be a real EPIPE, indistinguishable from a transport failure.
    if (shutdown_requested_.load()) {
        return ErrorCode::CANCELLED;
    }
    if (data.empty() || data.size() > MAX_PDU) {
        E3_LOG_ERROR(LOG_TAG) << "PDU of " << data.size() << " bytes cannot be sent";
        return ErrorCode::INVALID_PARAM;
    }
    if (role_ == E3Role::DAPP) {
        auto conn = client();
        if (!conn) {
            return ErrorCode::NOT_CONNECTED;
        }
        std::lock_guard<std::mutex> lock(tx_mutex_);
        targets_.assign(1, conn);
        return transmit(data, PduClass::PROCEDURE);
    }
    if (listener_ < 0) {
        return ErrorCode::NOT_CONNECTED;
    }
    std::lock_guard<std::mutex> lock(tx_mutex_);
    targets_.clear();
    {
        std::lock_guard<std::mutex> peers(peers_mutex_);
        if (peer_token == 0) {
            targets_.assign(peers_.begin(), peers_.end());
        } else {
            for (const auto& conn : peers_) {
                if (conn->token == peer_token) {
                    targets_.push_back(conn);
                    break;
                }
            }
        }
    }
    if (targets_.empty()) {
        return peer_token == 0 ? ErrorCode::SUCCESS : ErrorCode::NOT_CONNECTED;
    }
    return transmit(data, kind);
}

// ===========================================================================
// Sending: whoever holds tx_mutex_ drives the send ring
// ===========================================================================

bool UringE3Connector::prepare_send(size_t index, const std::vector<uint8_t>& data, PduClass kind) {
    SendOp& op = send_ops_[index];
    const RingFile file = op.conn->file();
    const uint64_t user_data = data_for(OP_SEND, index);
    op.msg = {};
    if (transport_layer_ == E3TransportLayer::TCP) {
        // The prefix and the PDU in one write; after a short write, what is left of them.
        size_t count = 0;
        if (op.done < TCP_PREFIX_BYTES) {
            op.iov[count++] = iovec{prefix_ + op.done, TCP_PREFIX_BYTES - op.done};
            op.iov[count++] = iovec{const_cast<uint8_t*>(data.data()), data.size()};
        } else {
            const size_t skip = op.done - TCP_PREFIX_BYTES;
            op.iov[count++] = iovec{const_cast<uint8_t*>(data.data()) + skip, data.size() - skip};
        }
        op.msg.msg_iov = op.iov;
        op.msg.msg_iovlen = count;
        return prepare(*tx_, [&] { return tx_->sendmsg(file, &op.msg, MSG_NOSIGNAL, user_data); });
    }
    if (transport_layer_ == E3TransportLayer::IPC) {
        return prepare(*tx_, [&] { return tx_->send(file, data.data(), data.size(), MSG_NOSIGNAL, user_data); });
    }
    op.iov[0] = iovec{const_cast<uint8_t*>(data.data()), data.size()};
    op.msg.msg_iov = op.iov;
    op.msg.msg_iovlen = 1;
    set_sctp_send_info(op.msg, op.control, kind);
    return prepare(*tx_, [&] { return tx_->sendmsg(file, &op.msg, MSG_NOSIGNAL, user_data); });
}

/**
 * Send @p data to every connection in targets_, all writes submitted together, and wait until
 * each is done or SEND_TIMEOUT_MS has passed. A write still waiting then is cancelled and its
 * connection shut, as the POSIX link drops a peer whose send timed out; the reader then reports
 * it closed. Returns only when the kernel holds no reference to the data any more.
 */
ErrorCode UringE3Connector::transmit(const std::vector<uint8_t>& data, PduClass kind) {
    const size_t count = targets_.size();
    if (send_ops_.size() < count) {
        send_ops_.resize(count);
    }
    const size_t total = (transport_layer_ == E3TransportLayer::TCP ? TCP_PREFIX_BYTES : 0) + data.size();
    if (transport_layer_ == E3TransportLayer::TCP) {
        write_stream_prefix(prefix_, data.size());
    }
    size_t outstanding = 0;
    for (size_t i = 0; i < count; ++i) {
        SendOp& op = send_ops_[i];
        op.conn = targets_[i];
        op.done = 0;
        op.error = 0;
        op.state = 0;
        if (prepare_send(i, data, kind)) {
            ++outstanding;
        } else {
            op.state = 2;
            op.error = EBUSY;
        }
    }

    auto complete = [&](const RingCompletion& c, bool after_deadline) {
        if (op_of(c.user_data) != OP_SEND) {
            return;
        }
        SendOp& op = send_ops_[id_of(c.user_data)];
        if (op.state != 0) {
            return;
        }
        if (c.res >= 0) {
            op.done += static_cast<size_t>(c.res);
            if (op.done >= total) {
                op.state = 1;
            } else if (transport_layer_ == E3TransportLayer::TCP && !after_deadline) {
                if (prepare_send(id_of(c.user_data), data, kind)) return;  // the rest of a short write
                op.state = 2;
                op.error = EBUSY;
            } else {
                op.state = 2;
                op.error = after_deadline ? ETIMEDOUT : EMSGSIZE;
            }
        } else if ((c.res == -EINTR || c.res == -EAGAIN) && !after_deadline) {
            if (prepare_send(id_of(c.user_data), data, kind)) return;
            op.state = 2;
            op.error = EBUSY;
        } else {
            op.state = 2;
            op.error = c.res == -ECANCELED ? ETIMEDOUT : -c.res;
        }
        --outstanding;
    };

    const int64_t deadline = now_ns() + static_cast<int64_t>(SEND_TIMEOUT_MS) * 1'000'000;
    while (outstanding > 0) {
        const int64_t left_us = (deadline - now_ns()) / 1000;
        if (left_us <= 0) {
            break;
        }
        send_completions_.clear();
        const int got = tx_->wait(send_completions_, 1, left_us, settings_.busy_poll_us);
        if (got < 0) {
            E3_LOG_ERROR(LOG_TAG) << "Waiting on the send ring failed: " << strerror(-got);
            break;
        }
        for (const auto& c : send_completions_) {
            complete(c, false);
        }
    }
    // Writes still waiting: cancel them, and wait until the kernel lets go of the buffers.
    while (outstanding > 0) {
        for (size_t i = 0; i < count; ++i) {
            if (send_ops_[i].state == 0) {
                const uint64_t target = data_for(OP_SEND, i);
                prepare(*tx_, [&] { return tx_->cancel(target, data_for(OP_CANCEL, i)); });
            }
        }
        send_completions_.clear();
        tx_->wait(send_completions_, 1, 100'000, 0);
        for (const auto& c : send_completions_) {
            complete(c, true);
        }
    }

    size_t sent = 0;
    size_t failed = 0;
    for (size_t i = 0; i < count; ++i) {
        SendOp& op = send_ops_[i];
        if (op.state == 1) {
            ++sent;
        } else {
            ++failed;
            E3_LOG_ERROR(LOG_TAG) << "Failed to send: " << strerror(op.error);
            // Dead or stalled: shutting it makes the reader drop it and report it closed.
            ::shutdown(op.conn->fd, SHUT_RDWR);
        }
        op.conn.reset();
    }
    targets_.clear();
    if (failed > 0 && sent == 0) {
        return ErrorCode::TRANSPORT_ERROR;
    }
    return ErrorCode::SUCCESS;
}

uint64_t UringE3Connector::last_peer_token() const {
    return last_peer_token_;
}

uint64_t UringE3Connector::last_setup_peer_token() const {
    return last_setup_token_;
}

std::vector<uint64_t> UringE3Connector::take_closed_peers() {
    std::lock_guard<std::mutex> lock(closed_mutex_);
    std::vector<uint64_t> closed;
    closed.swap(closed_peers_);
    return closed;
}

// ===========================================================================
// dApp role: one connection
// ===========================================================================

std::shared_ptr<UringE3Connector::Conn> UringE3Connector::client() const {
    std::lock_guard<std::mutex> lock(client_mutex_);
    return client_;
}

ErrorCode UringE3Connector::connect_client() {
    const SocketSpec where = spec();
    int fd = -1;
    for (int attempt = 0; attempt < CONNECT_ATTEMPTS && fd < 0; ++attempt) {
        fd = connect_to(where);
        if (fd < 0) {
            if (shutdown_requested_.load()) {
                return ErrorCode::CANCELLED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_PAUSE_MS));
        }
    }
    if (fd < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect to " << setup_endpoint_ << " port " << where.target.port
                              << ": " << strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    auto conn = make_conn(fd);
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_ = conn;
    }
    client_ready_.clear();
    client_closed_ = false;
    client_failed_ = false;
    // Until the setup response is read, the setup thread is the only reader: the inbound
    // thread must not take it for an indication.
    awaiting_setup_response_.store(true);
    connected_ = true;
    E3_LOG_INFO(LOG_TAG) << "Connected to " << setup_endpoint_ << " port " << where.target.port;
    return ErrorCode::SUCCESS;
}

void UringE3Connector::close_client_locked() {
    auto conn = client();
    if (!conn) {
        return;
    }
    ::shutdown(conn->fd, SHUT_RDWR);
    // Its receive comes back with the shutdown; until then the kernel may still write into its buffer.
    const int64_t deadline = now_ns() + 1'000'000'000;
    bool cancelled = false;
    while (conn->recv_in_flight && now_ns() < deadline) {
        if (!cancelled && now_ns() > deadline - 900'000'000) {
            const uint64_t target = data_for(OP_RECV, conn->token);
            prepare(*rx_, [&] { return rx_->cancel(target, data_for(OP_CANCEL, 0)); });
            cancelled = true;
        }
        completions_.clear();
        rx_->wait(completions_, 1, 100'000, 0);
        for (const auto& c : completions_) {
            if (op_of(c.user_data) == OP_RECV && id_of(c.user_data) == conn->token) {
                conn->recv_in_flight = false;
            } else if (op_of(c.user_data) == OP_WAKE) {
                wake_armed_ = false;
            }
        }
    }
    if (conn->recv_in_flight) {
        retired_.push_back(conn);  // kept until the rings are gone
    }
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_.reset();
    }
    client_ready_.clear();
}

ErrorCode UringE3Connector::setup_initial_connection_client() {
    return connect_client();
}

ErrorCode UringE3Connector::send_setup_request_client(const std::vector<uint8_t>& data) {
    if (!client()) {
        return ErrorCode::NOT_CONNECTED;
    }
    // The RAN takes the first PDU of a connection for the setup request, so a second
    // attempt needs a connection of its own, as a ZeroMQ request socket is reopened.
    if (setup_request_sent_) {
        {
            std::lock_guard<std::mutex> reader(reader_mutex_);
            close_client_locked();
        }
        ErrorCode rc = connect_client();
        if (rc != ErrorCode::SUCCESS) {
            return rc;
        }
    }
    setup_request_sent_ = true;
    return send_to(0, data, PduClass::PROCEDURE);
}

int UringE3Connector::recv_setup_response_client(std::vector<uint8_t>& buffer) {
    if (!client()) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    if (!awaiting_setup_response_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 0;
    }
    std::lock_guard<std::mutex> reader(reader_mutex_);
    const int n = read_client(buffer, RECEIVE_TIMEOUT_MS);
    if (n > 0) {
        awaiting_setup_response_.store(false);
        await_cv_.notify_all();
    }
    return n;
}

int UringE3Connector::read_client(std::vector<uint8_t>& buffer, int timeout_ms) {
    if (!client() || !rx_) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    const int64_t deadline = now_ns() + static_cast<int64_t>(timeout_ms) * 1'000'000;
    for (;;) {
        if (!client_ready_.empty()) {
            buffer = std::move(client_ready_.front());
            client_ready_.pop_front();
            return static_cast<int>(buffer.size());
        }
        if (client_failed_) {
            return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        if (client_closed_) {
            return static_cast<int>(ErrorCode::NOT_CONNECTED);
        }
        if (shutdown_requested_.load()) {
            return 0;
        }
        arm();
        const int64_t left_us = (deadline - now_ns()) / 1000;
        if (left_us <= 0) {
            return 0;
        }
        completions_.clear();
        const int got = rx_->wait(completions_, 1, left_us, settings_.busy_poll_us);
        if (got < 0) {
            E3_LOG_ERROR(LOG_TAG) << "Waiting on the receive ring failed: " << strerror(-got);
            return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
        }
        for (const auto& completion : completions_) {
            handle(completion);
        }
    }
}

ErrorCode UringE3Connector::setup_inbound_connection_client() {
    return client() ? ErrorCode::SUCCESS : ErrorCode::NOT_CONNECTED;
}

ErrorCode UringE3Connector::setup_outbound_connection_client() {
    return client() ? ErrorCode::SUCCESS : ErrorCode::NOT_CONNECTED;
}

// ===========================================================================
// Shutdown
// ===========================================================================

void UringE3Connector::shutdown() {
    E3_LOG_DEBUG(LOG_TAG) << "Shutting down io_uring connector";
    shutdown_requested_.store(true);
    setup_ready_.notify_all();
    await_cv_.notify_all();
    if (listener_ >= 0) {
        ::shutdown(listener_, SHUT_RDWR);
    }
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& conn : peers_) {
            ::shutdown(conn->fd, SHUT_RDWR);
        }
    }
    if (auto conn = client()) {
        ::shutdown(conn->fd, SHUT_RDWR);
    }
    if (wake_fd_ >= 0) {
        const uint64_t one = 1;
        ssize_t written = ::write(wake_fd_, &one, sizeof(one));
        (void)written;
    }
}

void UringE3Connector::dispose() {
    if (disposed_) {
        return;
    }
    disposed_ = true;
    E3_LOG_DEBUG(LOG_TAG) << "Disposing io_uring connector";
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (const auto& conn : peers_) {
            ::shutdown(conn->fd, SHUT_RDWR);
        }
    }
    if (auto conn = client()) {
        ::shutdown(conn->fd, SHUT_RDWR);
    }
    if (wake_fd_ >= 0) {
        const uint64_t one = 1;
        ssize_t written = ::write(wake_fd_, &one, sizeof(one));
        (void)written;
    }
    {
        std::lock_guard<std::mutex> reader(reader_mutex_);
        drain_rx_locked(200'000);
        conns_.clear();
        client_ready_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        setup_queue_.clear();
        ready_.clear();
    }
    setup_conn_.reset();
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(closed_mutex_);
        closed_peers_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(tx_mutex_);
        targets_.clear();
        send_ops_.clear();
    }
    if (slots_) {
        std::lock_guard<std::mutex> lock(slots_->mu);
        slots_->rx = nullptr;
        slots_->tx = nullptr;
    }
    tx_.reset();
    rx_.reset();
    retired_.clear();
    if (listener_ >= 0) {
        ::close(listener_);
        listener_ = -1;
        // Only the RAN, which binds, has a socket file to remove. The directory is shared
        // between agents and tests and stays, so that removing it cannot race a bind in it.
        if (transport_layer_ == E3TransportLayer::IPC) {
            unlink(setup_endpoint_.c_str());
        }
    }
    if (wake_fd_ >= 0) {
        ::close(wake_fd_);
        wake_fd_ = -1;
    }
    connected_ = false;
    E3_LOG_INFO(LOG_TAG) << "io_uring connector disposed";
}

}  // namespace libe3
