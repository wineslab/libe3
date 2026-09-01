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
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
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
#include <mutex>
#include <condition_variable>

namespace libe3 {

/**
 * @brief Lock-free bounded blocking queue.
 *
 * Built on an MPMC lock-free ring buffer.  Blocking pop variants use an
 * adaptive wait strategy:
 *  1. Spin with CPU pause hints  (lowest latency, nanoseconds)
 *  2. Thread yield               (cooperative, microseconds)
 *  3. Block on a condition variable, woken by push()/shutdown() (idle wait,
 *     avoids busy-loop when quiet; wake latency instead of a poll period)
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

        // Wake a consumer parked in the Phase 3 condition-variable wait (see
        // pop()). Taking wait_mutex_ here (even with an empty critical
        // section) is required, not just style: it is what prevents the
        // classic missed-wakeup race against a consumer that is between its
        // try_pop() check and its cv.wait() call. See the class-level
        // comment on pop() for the full argument.
        {
            std::lock_guard<std::mutex> lock(wait_mutex_);
        }
        wait_cv_.notify_one();

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

        // Phase 3: Block on the condition variable until data arrives or
        // shutdown is signalled. wait_mutex_ is held across the check and
        // the wait() call (not just during the wait itself) so that a
        // concurrent push()'s notify (which also takes wait_mutex_, see
        // push()) can never land in the gap between "we checked and it was
        // empty" and "we're registered as a waiter" -- that gap is exactly
        // where a naive condvar/futex notify-on-push design loses wakeups.
        std::unique_lock<std::mutex> lock(wait_mutex_);
        while (true) {
            if (ring_.try_pop(item)) return item;
            if (shutdown_.load(std::memory_order_relaxed)) return T{};
            wait_cv_.wait(lock);
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

        // Phase 3: Block on the condition variable until data arrives,
        // shutdown is signalled, or the deadline passes. See the infinite
        // pop() overload above for why wait_mutex_ must be held across the
        // check-then-wait sequence.
        {
            std::unique_lock<std::mutex> lock(wait_mutex_);
            while (std::chrono::steady_clock::now() < deadline) {
                if (ring_.try_pop(item)) return item;
                if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
                wait_cv_.wait_until(lock, deadline);
            }
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
        // Wake any consumer blocked in pop()'s Phase 3 wait immediately,
        // rather than letting it wait out its (possibly infinite) wait.
        {
            std::lock_guard<std::mutex> lock(wait_mutex_);
        }
        wait_cv_.notify_all();
        E3_LOG_DEBUG(LOG_TAG) << "Queue shutdown signalled";
    }

    /** @brief Return true if shutdown() has been called. */
    bool is_shutdown() const {
        return shutdown_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clear shutdown and discard any residual items so the queue is
     * reusable after a shutdown(). The queue object itself is not moved, so any
     * reference/pointer held by a consumer stays valid across a re-arm.
     */
    void rearm() {
        clear();
        shutdown_.store(false, std::memory_order_relaxed);
        E3_LOG_DEBUG(LOG_TAG) << "Queue re-armed";
    }

private:
    static constexpr const char* LOG_TAG = "Queue";

    MpmcQueue<T> ring_;
    std::atomic<bool> shutdown_{false};

    // Phase 3 wait/notify primitives (see pop() and push()).
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;

    // Adaptive spin-wait tuning constants
    static constexpr size_t SPIN_COUNT  = 40;   ///< CPU-pause iterations
    static constexpr size_t YIELD_COUNT = 100;  ///< thread-yield iterations
};

} // namespace libe3

#endif // LIBE3_LOCKFREE_QUEUE_HPP
