/**
 * @file mpmc_queue.hpp
 * @brief Lock-free bounded Multi-Producer Multi-Consumer (MPMC) ring buffer
 *
 * Implements Dmitry Vyukov's MPMC bounded queue algorithm.  Each slot carries
 * an atomic sequence counter that acts as a generation tag, eliminating the
 * need for CAS on the data itself and making the queue safe for any number of
 * concurrent producers and consumers with very low overhead.
 *
 * Design highlights:
 *  - Zero locks / zero condition variables in the fast path
 *  - head_ and tail_ are on separate cache lines to eliminate false sharing
 *  - Capacity is rounded up to the nearest power of two for cheap masking
 *  - Provides try_push / try_pop (non-blocking) used by ResponseQueue
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_MPMC_QUEUE_HPP
#define LIBE3_MPMC_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <new>

namespace libe3 {

/** @brief Assumed cache-line size (bytes) used for alignment padding. */
static constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Lock-free bounded MPMC ring buffer.
 *
 * @tparam T  Element type.  Must be default-constructible and movable.
 *
 * Usage:
 * @code
 *   MpmcQueue<MyMsg> q(256);
 *   // producer:
 *   if (!q.try_push(std::move(msg))) { // queue full }
 *   // consumer:
 *   MyMsg out;
 *   if (q.try_pop(out)) { // use out }
 * @endcode
 */
template<typename T>
class MpmcQueue {
public:
    /**
     * @brief Construct a queue with at least @p min_capacity slots.
     *
     * The actual capacity is rounded up to the next power of two (minimum 2).
     * @param min_capacity Minimum number of slots (0 is treated as 1).
     */
    explicit MpmcQueue(size_t min_capacity) {
        // Round up to next power of two (minimum 2 to make the mask sensible)
        capacity_ = 2;
        while (capacity_ < min_capacity) capacity_ <<= 1;
        mask_ = capacity_ - 1;

        // Allocate cache-line aligned buffer to prevent false sharing between
        // adjacent slots' sequence counters on small-element queues.
        buffer_ = static_cast<Slot*>(
            ::operator new(sizeof(Slot) * capacity_,
                           std::align_val_t{CACHE_LINE_SIZE})
        );

        for (size_t i = 0; i < capacity_; ++i) {
            new (&buffer_[i]) Slot();
            // Sequence == index means "slot is free for the next producer
            // whose enqueue position is i".
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~MpmcQueue() {
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].~Slot();
        }
        ::operator delete(buffer_, std::align_val_t{CACHE_LINE_SIZE});
    }

    // Non-copyable, non-movable
    MpmcQueue(const MpmcQueue&) = delete;
    MpmcQueue& operator=(const MpmcQueue&) = delete;
    MpmcQueue(MpmcQueue&&) = delete;
    MpmcQueue& operator=(MpmcQueue&&) = delete;

    /**
     * @brief Try to enqueue an item (non-blocking, move semantics).
     * @return true on success; false if the queue is full.
     */
    bool try_push(T&& data) {
        size_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = buffer_[pos & mask_];
            size_t seq  = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq)
                          - static_cast<intptr_t>(pos);

            if (diff == 0) {
                // Slot is ready for this producer; claim it.
                if (head_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    slot.data = std::move(data);
                    // Mark slot as containing valid data for consumer at pos+1.
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // Another producer claimed this slot; reload and retry.
            } else if (diff < 0) {
                return false; // Queue is full
            } else {
                // Another producer advanced head past us; reload.
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Try to enqueue an item (copy overload).
     */
    bool try_push(const T& data) {
        T copy = data;
        return try_push(std::move(copy));
    }

    /**
     * @brief Try to dequeue an item (non-blocking).
     * @param[out] data Receives the dequeued item on success.
     * @return true on success; false if the queue is empty.
     */
    bool try_pop(T& data) {
        size_t pos = tail_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = buffer_[pos & mask_];
            size_t seq  = slot.sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq)
                          - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                // Slot holds data for this consumer; claim it.
                if (tail_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed)) {
                    data = std::move(slot.data);
                    // Recycle slot for the producer that wraps around.
                    slot.sequence.store(pos + capacity_,
                                        std::memory_order_release);
                    return true;
                }
                // Another consumer claimed this slot; reload and retry.
            } else if (diff < 0) {
                return false; // Queue is empty
            } else {
                // Another consumer advanced tail past us; reload.
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Approximate number of items currently in the queue.
     *
     * Due to concurrent modifications this value may be slightly stale,
     * but is always in [0, capacity()].
     */
    size_t size_approx() const noexcept {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);
        return h >= t ? h - t : 0;
    }

    /** @brief Return true if the queue appears empty (approximate). */
    bool empty_approx() const noexcept { return size_approx() == 0; }

    /** @brief Actual slot capacity (always a power of two). */
    size_t capacity() const noexcept { return capacity_; }

private:
    struct Slot {
        std::atomic<size_t> sequence{0};
        T data;
    };

    Slot*  buffer_{nullptr};
    size_t capacity_{0};
    size_t mask_{0};

    // Separate cache lines for head and tail to prevent producer/consumer
    // false sharing.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
};

/**
 * @brief Emit a CPU-level pause/yield hint inside spin-wait loops.
 *
 * Reduces power consumption and inter-core bus traffic while spinning.
 * On architectures without a dedicated pause instruction this is a no-op.
 */
inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("yield" ::: "memory");
#endif
}

} // namespace libe3

#endif // LIBE3_MPMC_QUEUE_HPP
