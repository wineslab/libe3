/**
 * @file test_mpmc_queue.cpp
 * @brief Unit tests for the lock-free MpmcQueue ring buffer
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/mpmc_queue.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>

using namespace libe3;

// ---------------------------------------------------------------------------
// Basic correctness
// ---------------------------------------------------------------------------

TEST(MpmcQueue_initial_state) {
    MpmcQueue<int> q(4);
    ASSERT_TRUE(q.empty_approx());
    ASSERT_EQ(q.size_approx(), 0u);
    // Capacity rounded up to power of two
    ASSERT_EQ(q.capacity(), 4u);
}

TEST(MpmcQueue_capacity_rounded_to_power_of_two) {
    MpmcQueue<int> q(5);
    ASSERT_EQ(q.capacity(), 8u); // next power of two >= 5
}

TEST(MpmcQueue_push_pop_single) {
    MpmcQueue<int> q(4);
    ASSERT_TRUE(q.try_push(42));
    ASSERT_EQ(q.size_approx(), 1u);

    int val = 0;
    ASSERT_TRUE(q.try_pop(val));
    ASSERT_EQ(val, 42);
    ASSERT_TRUE(q.empty_approx());
}

TEST(MpmcQueue_fifo_order) {
    MpmcQueue<int> q(16);
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(q.try_push(i));
    }
    for (int i = 0; i < 8; ++i) {
        int val = -1;
        ASSERT_TRUE(q.try_pop(val));
        ASSERT_EQ(val, i);
    }
    ASSERT_TRUE(q.empty_approx());
}

TEST(MpmcQueue_capacity_limit) {
    MpmcQueue<int> q(4);
    // Fill to capacity
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(q.try_push(i));
    }
    // Next push must fail
    ASSERT_FALSE(q.try_push(99));
}

TEST(MpmcQueue_try_pop_empty_returns_false) {
    MpmcQueue<int> q(4);
    int val = 0;
    ASSERT_FALSE(q.try_pop(val));
}

TEST(MpmcQueue_copy_overload) {
    MpmcQueue<int> q(4);
    const int v = 7;
    ASSERT_TRUE(q.try_push(v));
    int out = 0;
    ASSERT_TRUE(q.try_pop(out));
    ASSERT_EQ(out, 7);
}

// ---------------------------------------------------------------------------
// Concurrency – single producer, single consumer (SPSC)
// ---------------------------------------------------------------------------

TEST(MpmcQueue_spsc_throughput) {
    constexpr int N = 10000;
    MpmcQueue<int> q(512);
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield();
            }
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        int val;
        while (consumed.load(std::memory_order_relaxed) < N) {
            if (q.try_pop(val)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.load(), N);
    ASSERT_TRUE(q.empty_approx());
    (void)done; // used by consumer via release/acquire
}

// ---------------------------------------------------------------------------
// Concurrency – multiple producers, single consumer (MPSC)
// ---------------------------------------------------------------------------

TEST(MpmcQueue_mpsc_no_lost_items) {
    constexpr int PRODUCERS    = 4;
    constexpr int ITEMS_EACH   = 1000;
    constexpr int TOTAL        = PRODUCERS * ITEMS_EACH;

    MpmcQueue<int> q(512);
    std::atomic<int> consumed{0};
    std::atomic<bool> all_pushed{false};

    std::vector<std::thread> producers;
    producers.reserve(static_cast<size_t>(PRODUCERS));
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ITEMS_EACH; ++i) {
                while (!q.try_push(p * ITEMS_EACH + i)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    std::thread consumer([&]() {
        int val;
        while (!all_pushed.load(std::memory_order_acquire)
               || !q.empty_approx()) {
            if (q.try_pop(val)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        // Drain any remaining items
        while (q.try_pop(val)) {
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (auto& t : producers) t.join();
    all_pushed.store(true, std::memory_order_release);
    consumer.join();

    ASSERT_EQ(consumed.load(), TOTAL);
}

// ---------------------------------------------------------------------------
// Concurrency – multiple producers, multiple consumers (MPMC)
// ---------------------------------------------------------------------------

TEST(MpmcQueue_mpmc_no_lost_or_duplicate_items) {
    constexpr int PRODUCERS  = 4;
    constexpr int CONSUMERS  = 4;
    constexpr int ITEMS_EACH = 500;
    constexpr int TOTAL      = PRODUCERS * ITEMS_EACH;

    MpmcQueue<int> q(1024);
    std::atomic<int> pushed{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> all_pushed{false};

    std::vector<std::thread> producers;
    producers.reserve(static_cast<size_t>(PRODUCERS));
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < ITEMS_EACH; ++i) {
                while (!q.try_push(1)) { // value doesn't matter for count test
                    std::this_thread::yield();
                }
                pushed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(static_cast<size_t>(CONSUMERS));
    for (int c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            int val;
            while (!all_pushed.load(std::memory_order_acquire)
                   || !q.empty_approx()) {
                if (q.try_pop(val)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    all_pushed.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    ASSERT_EQ(pushed.load(),   TOTAL);
    ASSERT_EQ(consumed.load(), TOTAL);
}

// ---------------------------------------------------------------------------
// cpu_relax smoke test – just verify it compiles and doesn't crash
// ---------------------------------------------------------------------------

TEST(cpu_relax_callable) {
    for (int i = 0; i < 10; ++i) {
        cpu_relax();
    }
    ASSERT_TRUE(true); // reached without crash
}

int main() {
    return RUN_ALL_TESTS();
}
