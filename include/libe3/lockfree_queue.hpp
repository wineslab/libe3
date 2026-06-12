/**
 * @file lockfree_queue.hpp
 * @brief Lock-free bounded blocking queue for the E3AP hot paths
 *
 * Wraps the lock-free MPMC ring buffer (see mpmc_queue.hpp) in a small,
 * reusable object that adds blocking pop variants and shutdown semantics.
 * It eliminates mutex contention on the hot publish path and reduces latency
 * jitter in sub-millisecond control loops.
 *
 * libe3 uses two specialisations:
 *  - LockFreeQueue<Pdu>        — outbound E3AP PDUs (publisher/outbound thread
 *                                consumes; many threads produce).
 *  - LockFreeQueue<DAppReport> — inbound dApp reports handed from the RAN
 *                                inbound thread to the report worker thread.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_LOCKFREE_QUEUE_HPP
#define LIBE3_LOCKFREE_QUEUE_HPP

#include "types.hpp"
#include "mpmc_queue.hpp"
#include "logger.hpp"
#include <atomic>
#include <optional>
#include <chrono>
#include <thread>

namespace libe3 {

/**
 * @brief Lock-free bounded blocking queue.
 *
 * Built on an MPMC lock-free ring buffer.  Blocking pop variants use an
 * adaptive spin-wait strategy:
 *  1. Spin with CPU pause hints  (lowest latency, nanoseconds)
 *  2. Thread yield               (cooperative, microseconds)
 *  3. Short sleep (50 µs)        (idle wait, avoids busy-loop when quiet)
 *
 * @tparam T  Element type. Must be default-constructible and movable.
 */
template<typename T>
class LockFreeQueue {
public:
    /**
     * @brief Construct a LockFreeQueue.
     * @param capacity Minimum ring buffer capacity (rounded up to next power
     *                 of two, default 128).
     */
    explicit LockFreeQueue(size_t capacity = 128)
        : ring_(capacity)
    {
        E3_LOG_DEBUG(LOG_TAG) << "Lock-free queue created, capacity="
                              << ring_.capacity();
    }

    /**
     * @brief Destructor – signals shutdown so any blocked pop() returns.
     */
    ~LockFreeQueue() {
        shutdown();
        E3_LOG_DEBUG(LOG_TAG) << "Lock-free queue destroyed";
    }

    // Non-copyable, non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;

    /**
     * @brief Enqueue an item (non-blocking, lock-free).
     * @return ErrorCode::SUCCESS on success.
     * @return ErrorCode::BUFFER_TOO_SMALL if the ring buffer is full.
     * @return ErrorCode::NOT_INITIALIZED if shutdown() has been called.
     */
    ErrorCode push(T item) {
        if (shutdown_.load(std::memory_order_relaxed)) {
            return ErrorCode::NOT_INITIALIZED;
        }

        if (!ring_.try_push(std::move(item))) {
            E3_LOG_WARN(LOG_TAG) << "Queue full, dropping message";
            return ErrorCode::BUFFER_TOO_SMALL;
        }

        E3_LOG_TRACE(LOG_TAG) << "Pushed item";
        return ErrorCode::SUCCESS;
    }

    /**
     * @brief Dequeue an item, blocking indefinitely until one is available.
     *
     * Returns a default-constructed T{} if shutdown() is called while waiting.
     */
    T pop() {
        T item;

        // Phase 1: CPU-pause spin (nanosecond latency when producer is fast)
        for (size_t i = 0; i < SPIN_COUNT; ++i) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return T{};
            cpu_relax();
        }

        // Phase 2: Cooperative yield (microsecond range)
        for (size_t i = 0; i < YIELD_COUNT; ++i) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return T{};
            std::this_thread::yield();
        }

        // Phase 3: Short sleep until data arrives or shutdown is signalled
        while (true) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return T{};
            std::this_thread::sleep_for(SLEEP_DURATION);
        }
    }

    /**
     * @brief Dequeue an item with a maximum wait duration.
     * @return The item on success; std::nullopt on timeout or shutdown.
     */
    std::optional<T> pop(std::chrono::milliseconds timeout) {
        T item;
        auto deadline = std::chrono::steady_clock::now() + timeout;

        // Phase 1: CPU-pause spin
        for (size_t i = 0; i < SPIN_COUNT; ++i) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
            cpu_relax();
        }

        // Phase 2: Cooperative yield
        for (size_t i = 0; i < YIELD_COUNT; ++i) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
            std::this_thread::yield();
        }

        // Phase 3: Timed sleep
        while (std::chrono::steady_clock::now() < deadline) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
            std::this_thread::sleep_for(SLEEP_DURATION);
        }

        // One last attempt after deadline
        if (ring_.try_pop(item)) return item;
        return std::nullopt;
    }

    /**
     * @brief Try to dequeue without blocking.
     * @return The item if one was available; std::nullopt otherwise.
     */
    std::optional<T> try_pop() {
        T item;
        if (ring_.try_pop(item)) return item;
        return std::nullopt;
    }

    /** @brief Return true if the queue appears empty (approximate). */
    bool empty() const { return ring_.empty_approx(); }

    /** @brief Return the approximate number of items in the queue. */
    size_t size() const { return ring_.size_approx(); }

    /** @brief Return the ring buffer capacity (always a power of two). */
    size_t capacity() const noexcept { return ring_.capacity(); }

    /** @brief Discard all items currently in the queue. */
    void clear() {
        T item;
        while (ring_.try_pop(item)) {}
        E3_LOG_DEBUG(LOG_TAG) << "Queue cleared";
    }

    /**
     * @brief Signal shutdown so that all blocked pop() calls return promptly.
     */
    void shutdown() {
        shutdown_.store(true, std::memory_order_relaxed);
        E3_LOG_DEBUG(LOG_TAG) << "Queue shutdown signalled";
    }

    /** @brief Return true if shutdown() has been called. */
    bool is_shutdown() const {
        return shutdown_.load(std::memory_order_relaxed);
    }

private:
    static constexpr const char* LOG_TAG = "Queue";

    MpmcQueue<T> ring_;
    std::atomic<bool> shutdown_{false};

    // Adaptive spin-wait tuning constants
    static constexpr size_t SPIN_COUNT  = 40;   ///< CPU-pause iterations
    static constexpr size_t YIELD_COUNT = 100;  ///< thread-yield iterations
    /// Sleep duration between attempts in the slow path (µs)
    static constexpr auto SLEEP_DURATION = std::chrono::microseconds(50);
};

} // namespace libe3

#endif // LIBE3_LOCKFREE_QUEUE_HPP
