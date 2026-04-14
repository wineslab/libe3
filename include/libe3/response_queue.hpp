/**
 * @file response_queue.hpp
 * @brief Lock-free bounded queue for E3AP outbound PDUs
 *
 * Replaces the original mutex + condition-variable implementation with a
 * lock-free MPMC ring buffer (see mpmc_queue.hpp) to eliminate mutex
 * contention on the hot publish path and to reduce latency jitter in
 * sub-millisecond control loops.
 *
 * Producer/consumer pattern in libe3:
 *  - Producers: subscriber_loop thread, sm_data_handler_loop thread, and
 *               any caller of E3Interface::queue_outbound() (MPSC / MPMC).
 *  - Consumer:  publisher_loop thread (single consumer).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_RESPONSE_QUEUE_HPP
#define LIBE3_RESPONSE_QUEUE_HPP

#include "types.hpp"
#include "mpmc_queue.hpp"
#include <atomic>
#include <optional>
#include <chrono>

namespace libe3 {

/**
 * @brief Lock-free bounded queue for E3AP PDUs.
 *
 * Provides the same API as the previous mutex-based implementation but uses
 * an MPMC lock-free ring buffer internally.  Blocking pop variants use an
 * adaptive spin-wait strategy:
 *  1. Spin with CPU pause hints  (lowest latency, nanoseconds)
 *  2. Thread yield               (cooperative, microseconds)
 *  3. Short sleep (50 µs)        (idle wait, avoids busy-loop when quiet)
 */
class ResponseQueue {
public:
    /**
     * @brief Construct a ResponseQueue.
     * @param capacity Minimum ring buffer capacity (rounded up to next power
     *                 of two, default 128).
     */
    explicit ResponseQueue(size_t capacity = 128);

    /**
     * @brief Destructor – signals shutdown so any blocked pop() returns.
     */
    ~ResponseQueue();

    // Non-copyable, non-movable
    ResponseQueue(const ResponseQueue&) = delete;
    ResponseQueue& operator=(const ResponseQueue&) = delete;
    ResponseQueue(ResponseQueue&&) = delete;
    ResponseQueue& operator=(ResponseQueue&&) = delete;

    /**
     * @brief Enqueue a PDU (non-blocking, lock-free).
     * @return ErrorCode::SUCCESS on success.
     * @return ErrorCode::BUFFER_TOO_SMALL if the ring buffer is full.
     * @return ErrorCode::NOT_INITIALIZED if shutdown() has been called.
     */
    ErrorCode push(Pdu pdu);

    /**
     * @brief Dequeue a PDU, blocking indefinitely until one is available.
     *
     * Returns an empty Pdu{} if shutdown() is called while waiting.
     */
    Pdu pop();

    /**
     * @brief Dequeue a PDU with a maximum wait duration.
     * @return The PDU on success; std::nullopt on timeout or shutdown.
     */
    std::optional<Pdu> pop(std::chrono::milliseconds timeout);

    /**
     * @brief Try to dequeue without blocking.
     * @return The PDU if one was available; std::nullopt otherwise.
     */
    std::optional<Pdu> try_pop();

    /** @brief Return true if the queue appears empty (approximate). */
    bool empty() const;

    /** @brief Return the approximate number of items in the queue. */
    size_t size() const;

    /** @brief Return the ring buffer capacity (always a power of two). */
    size_t capacity() const noexcept { return ring_.capacity(); }

    /** @brief Discard all items currently in the queue. */
    void clear();

    /**
     * @brief Signal shutdown so that all blocked pop() calls return promptly.
     */
    void shutdown();

    /** @brief Return true if shutdown() has been called. */
    bool is_shutdown() const;

private:
    MpmcQueue<Pdu> ring_;
    std::atomic<bool> shutdown_{false};

    // Adaptive spin-wait tuning constants
    static constexpr size_t SPIN_COUNT  = 40;   ///< CPU-pause iterations
    static constexpr size_t YIELD_COUNT = 100;  ///< thread-yield iterations
    /// Sleep duration between attempts in the slow path (µs)
    static constexpr auto SLEEP_DURATION = std::chrono::microseconds(50);
};

} // namespace libe3

#endif // LIBE3_RESPONSE_QUEUE_HPP
