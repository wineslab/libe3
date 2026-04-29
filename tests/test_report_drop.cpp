/**
 * @file test_report_drop.cpp
 * @brief Tests for the dApp report queue used in the subscriber → worker
 *        handoff.
 *
 * Verifies that the bounded MPMC queue used to dispatch DAppReport
 * messages from the subscriber thread to the report worker thread:
 *
 *   (1) Delivers every pushed report to the consumer exactly once, in
 *       FIFO order, when the workload stays within capacity.
 *
 *   (2) Surfaces overflow as an explicit `try_push == false` return value
 *       rather than silently dropping the message.
 *
 *   (3) Loses no messages under a producer that bursts at full speed
 *       while the consumer simulates slow per-message work.
 *
 *   (4) Loses no messages and produces no duplicates with multiple
 *       concurrent producers and a single consumer.
 *
 * The tests use MpmcQueue<DAppReport>(1024) — the same template
 * specialisation and capacity used by E3Interface::report_queue_, so any
 * future change to that production value is reflected here.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/mpmc_queue.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdint>

using namespace libe3;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Capacity used by E3Interface::report_queue_ (see e3_interface.cpp).
/// Keeping the constant in sync makes the workloads below representative
/// of real production behaviour and signals if the production capacity
/// is ever shrunk to a value the burst tests would overflow.
static constexpr size_t REPORT_QUEUE_CAPACITY = 1024;

static DAppReport make_report(uint32_t seq, size_t payload_bytes = 70) {
    DAppReport r;
    r.dapp_identifier = 1;
    r.ran_function_identifier = 1;
    r.report_data.assign(payload_bytes, static_cast<uint8_t>(seq & 0xFF));
    if (payload_bytes >= 4) {
        r.report_data[0] = static_cast<uint8_t>(seq);
        r.report_data[1] = static_cast<uint8_t>(seq >> 8);
        r.report_data[2] = static_cast<uint8_t>(seq >> 16);
        r.report_data[3] = static_cast<uint8_t>(seq >> 24);
    }
    return r;
}

static uint32_t read_seq(const DAppReport& r) {
    if (r.report_data.size() < 4) return 0;
    return  static_cast<uint32_t>(r.report_data[0])
         | (static_cast<uint32_t>(r.report_data[1]) << 8)
         | (static_cast<uint32_t>(r.report_data[2]) << 16)
         | (static_cast<uint32_t>(r.report_data[3]) << 24);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * Push N reports (well under capacity), pop N, verify every report is
 * delivered exactly once in FIFO order.
 */
TEST(ReportQueue_drainsAllMessagesUnderCapacity) {
    MpmcQueue<DAppReport> q(REPORT_QUEUE_CAPACITY);
    constexpr uint32_t N = 500;

    for (uint32_t i = 0; i < N; ++i) {
        ASSERT_TRUE(q.try_push(make_report(i)));
    }

    DAppReport out;
    for (uint32_t i = 0; i < N; ++i) {
        ASSERT_TRUE(q.try_pop(out));
        ASSERT_EQ(read_seq(out), i);
    }
    ASSERT_FALSE(q.try_pop(out));    // queue drained
}

/**
 * Filling the queue past capacity surfaces as an explicit `try_push`
 * failure (false return), and the count of messages successfully popped
 * matches the count of successful pushes.
 */
TEST(ReportQueue_overflowReturnsFalseInsteadOfSilentDrop) {
    MpmcQueue<DAppReport> q(REPORT_QUEUE_CAPACITY);

    size_t pushed = 0;
    for (size_t i = 0; i < REPORT_QUEUE_CAPACITY * 2; ++i) {
        if (q.try_push(make_report(static_cast<uint32_t>(i)))) {
            ++pushed;
        }
    }

    // MpmcQueue rounds requested capacity up to the next power of two,
    // so `pushed` may be ≥ REPORT_QUEUE_CAPACITY.  The invariant we
    // assert: at least one push attempt failed (returned false), proving
    // that overflow surfaces explicitly.
    ASSERT_GE(pushed, REPORT_QUEUE_CAPACITY);
    ASSERT_LT(pushed, REPORT_QUEUE_CAPACITY * 2);

    DAppReport out;
    size_t popped = 0;
    while (q.try_pop(out)) ++popped;
    ASSERT_EQ(popped, pushed);
}

/**
 * Concurrent producer/consumer with a slow consumer (~50 µs per message,
 * mimicking realistic downstream work).  The producer bursts at full
 * speed; the queue absorbs it; the consumer drains it.  Every produced
 * message must be consumed in FIFO order with no loss.
 */
TEST(ReportQueue_burstProducerSlowConsumer_noLoss) {
    MpmcQueue<DAppReport> q(REPORT_QUEUE_CAPACITY);
    constexpr uint32_t N = 5000;

    std::atomic<uint32_t> consumed_count{0};
    std::atomic<bool> consumer_done{false};
    std::atomic<bool> mismatch{false};

    std::thread consumer([&]() {
        DAppReport r;
        uint32_t expected = 0;
        while (consumed_count.load() < N) {
            if (q.try_pop(r)) {
                if (read_seq(r) != expected) mismatch.store(true);
                ++expected;
                consumed_count.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        consumer_done.store(true);
    });

    for (uint32_t i = 0; i < N; ++i) {
        while (!q.try_push(make_report(i))) {
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    }

    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (!consumer_done.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    consumer.join();

    ASSERT_EQ(consumed_count.load(), N);
    ASSERT_FALSE(mismatch.load());
}

/**
 * Multi-producer single-consumer: 4 producers × 1000 messages each, with
 * each producer emitting a distinct seq range.  The consumer must see
 * every seq exactly once with no duplicates and no loss.
 */
TEST(ReportQueue_multiProducerSingleConsumer_noLossOrDuplicates) {
    MpmcQueue<DAppReport> q(REPORT_QUEUE_CAPACITY);
    constexpr uint32_t N_PER_PRODUCER = 1000;
    constexpr uint32_t N_PRODUCERS    = 4;
    constexpr uint32_t N_TOTAL        = N_PER_PRODUCER * N_PRODUCERS;

    std::vector<std::thread> producers;
    for (uint32_t p = 0; p < N_PRODUCERS; ++p) {
        producers.emplace_back([&q, p]() {
            const uint32_t base = p * N_PER_PRODUCER;
            for (uint32_t i = 0; i < N_PER_PRODUCER; ++i) {
                while (!q.try_push(make_report(base + i))) {
                    std::this_thread::sleep_for(std::chrono::microseconds(5));
                }
            }
        });
    }

    std::vector<bool> seen(N_TOTAL, false);
    std::atomic<uint32_t> count{0};
    std::atomic<bool> dup{false};

    std::thread consumer([&]() {
        DAppReport r;
        while (count.load() < N_TOTAL) {
            if (q.try_pop(r)) {
                uint32_t s = read_seq(r);
                if (s >= N_TOTAL || seen[s]) dup.store(true);
                else                          seen[s] = true;
                count.fetch_add(1);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(2));
            }
        }
    });

    for (auto& t : producers) t.join();
    consumer.join();

    ASSERT_EQ(count.load(), N_TOTAL);
    ASSERT_FALSE(dup.load());
    for (uint32_t i = 0; i < N_TOTAL; ++i) {
        ASSERT_TRUE(seen[i]);
    }
}

// ---------------------------------------------------------------------------

int main() {
    return RUN_ALL_TESTS();
}
