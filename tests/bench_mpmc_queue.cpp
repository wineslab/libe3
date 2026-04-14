/**
 * @file bench_mpmc_queue.cpp
 * @brief Stress test and latency/throughput benchmark for MpmcQueue and ResponseQueue
 *
 * Measures end-to-end push-to-pop latency at different queue capacities,
 * throughput under SPSC/MPSC/MPMC producer-consumer configurations, and
 * validates no-loss/no-duplicate guarantees under heavy concurrent load.
 *
 * Output is markdown-formatted for easy embedding in CI PR comments.
 *
 * Return value: 0 if all correctness checks pass, 1 otherwise.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/mpmc_queue.hpp"
#include "libe3/response_queue.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace libe3;
using namespace std::chrono;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline uint64_t now_ns() {
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(
            steady_clock::now().time_since_epoch()
        ).count()
    );
}

/** Compute the @p p-th percentile (0–100) from a sorted sample vector. */
static int64_t percentile(const std::vector<int64_t>& sorted, double p) {
    if (sorted.empty()) return 0;
    size_t idx = static_cast<size_t>(p / 100.0 * static_cast<double>(sorted.size()));
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

// ---------------------------------------------------------------------------
// Section 1 – MpmcQueue end-to-end latency (SPSC)
// ---------------------------------------------------------------------------

struct MpmcLatResult {
    size_t  capacity;
    int64_t p50_ns;
    int64_t p95_ns;
    int64_t p99_ns;
    int64_t p999_ns;
};

/**
 * Measure push-to-pop latency for a single-producer single-consumer scenario.
 * The timestamp of a successful push is embedded as the item value (nanoseconds
 * from steady_clock epoch); the consumer computes elapsed time on pop.
 */
static MpmcLatResult mpmc_spsc_latency(size_t queue_cap, int n_items) {
    MpmcQueue<uint64_t> q(queue_cap);
    std::vector<int64_t> latencies(static_cast<size_t>(n_items));
    std::atomic<int> consumed{0};

    // Warm-up: prime the ring buffer and CPU caches
    for (int w = 0; w < 200; ++w) {
        uint64_t ts = now_ns();
        while (!q.try_push(ts)) std::this_thread::yield();
        uint64_t val;
        while (!q.try_pop(val)) std::this_thread::yield();
    }

    std::thread producer([&]() {
        for (int i = 0; i < n_items; ++i) {
            uint64_t ts;
            // Record timestamp as close to the successful push as possible
            do { ts = now_ns(); } while (!q.try_push(ts));
        }
    });

    std::thread consumer([&]() {
        int idx = 0;
        while (idx < n_items) {
            uint64_t val;
            if (q.try_pop(val)) {
                int64_t lat = static_cast<int64_t>(now_ns() - val);
                if (lat < 0) lat = 0;
                latencies[static_cast<size_t>(idx)] = lat;
                ++idx;
            } else {
                std::this_thread::yield();
            }
        }
        consumed.store(idx, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    std::sort(latencies.begin(), latencies.end());
    return {
        q.capacity(),
        percentile(latencies, 50.0),
        percentile(latencies, 95.0),
        percentile(latencies, 99.0),
        percentile(latencies, 99.9),
    };
}

// ---------------------------------------------------------------------------
// Section 2 – ResponseQueue end-to-end latency (SPSC)
// ---------------------------------------------------------------------------

struct RQLatResult {
    size_t  actual_capacity;
    int64_t p50_ns;
    int64_t p95_ns;
    int64_t p99_ns;
};

/**
 * Like the MpmcQueue latency test but uses the full ResponseQueue API.
 * The push timestamp (nanoseconds) is stored in Pdu::timestamp, which the
 * Pdu constructor sets to milliseconds; we intentionally override it with
 * nanoseconds for this measurement.
 */
static RQLatResult rq_spsc_latency(size_t queue_cap, int n_items) {
    ResponseQueue rq(queue_cap);
    std::vector<int64_t> latencies(static_cast<size_t>(n_items));

    // Warm-up
    for (int w = 0; w < 200; ++w) {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        pdu.timestamp = now_ns();
        while (rq.push(pdu) != ErrorCode::SUCCESS) std::this_thread::yield();
        (void)rq.pop();
    }

    std::thread producer([&]() {
        for (int i = 0; i < n_items; ++i) {
            Pdu pdu(PduType::INDICATION_MESSAGE);
            ErrorCode rc;
            do {
                pdu.timestamp = now_ns();
                rc = rq.push(pdu);
                if (rc != ErrorCode::SUCCESS) std::this_thread::yield();
            } while (rc != ErrorCode::SUCCESS);
        }
    });

    std::thread consumer([&]() {
        for (int idx = 0; idx < n_items; ++idx) {
            Pdu pdu = rq.pop();
            int64_t lat = static_cast<int64_t>(now_ns() - pdu.timestamp);
            if (lat < 0) lat = 0;
            latencies[static_cast<size_t>(idx)] = lat;
        }
    });

    producer.join();
    consumer.join();

    std::sort(latencies.begin(), latencies.end());
    return {
        rq.capacity(),
        percentile(latencies, 50.0),
        percentile(latencies, 95.0),
        percentile(latencies, 99.0),
    };
}

// ---------------------------------------------------------------------------
// Section 3 – Throughput
// ---------------------------------------------------------------------------

struct TputResult {
    std::string config;
    size_t      capacity;
    double      mops_per_sec;
};

static TputResult measure_throughput(std::string config_name,
                                     int n_producers, int n_consumers,
                                     size_t queue_cap, int n_total) {
    MpmcQueue<int> q(queue_cap);
    std::atomic<bool> all_pushed{false};
    std::atomic<int>  produced{0};
    std::atomic<int>  consumed{0};
    int per_producer = n_total / n_producers;

    auto t_start = steady_clock::now();

    std::vector<std::thread> producers;
    producers.reserve(static_cast<size_t>(n_producers));
    for (int p = 0; p < n_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < per_producer; ++i) {
                while (!q.try_push(p * per_producer + i))
                    std::this_thread::yield();
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(static_cast<size_t>(n_consumers));
    for (int c = 0; c < n_consumers; ++c) {
        consumers.emplace_back([&]() {
            int val;
            while (!all_pushed.load(std::memory_order_acquire)
                   || !q.empty_approx()) {
                if (q.try_pop(val))
                    consumed.fetch_add(1, std::memory_order_relaxed);
                else
                    std::this_thread::yield();
            }
            while (q.try_pop(val))
                consumed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    for (auto& t : producers) t.join();
    all_pushed.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    auto t_end = steady_clock::now();
    double sec = duration<double>(t_end - t_start).count();
    double mops = static_cast<double>(consumed.load()) / 1e6 / sec;
    return {std::move(config_name), q.capacity(), mops};
}

// ---------------------------------------------------------------------------
// Section 4 – Stress / correctness under heavy concurrent load
// ---------------------------------------------------------------------------

struct StressResult {
    std::string name;
    int         total_items;
    bool        passed;
    std::string error_detail;
};

static StressResult stress_test(std::string name, int n_producers, int n_consumers,
                                size_t queue_cap, int n_total) {
    MpmcQueue<int> q(queue_cap);
    std::atomic<bool> all_pushed{false};
    std::atomic<int>  produced{0};
    std::atomic<int>  consumed{0};
    int per_producer = n_total / n_producers;
    int actual_total = per_producer * n_producers;

    std::vector<std::thread> producers;
    producers.reserve(static_cast<size_t>(n_producers));
    for (int p = 0; p < n_producers; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < per_producer; ++i) {
                while (!q.try_push(1)) std::this_thread::yield();
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(static_cast<size_t>(n_consumers));
    for (int c = 0; c < n_consumers; ++c) {
        consumers.emplace_back([&]() {
            int val;
            while (!all_pushed.load(std::memory_order_acquire)
                   || !q.empty_approx()) {
                if (q.try_pop(val))
                    consumed.fetch_add(1, std::memory_order_relaxed);
                else
                    std::this_thread::yield();
            }
            while (q.try_pop(val))
                consumed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    for (auto& t : producers) t.join();
    all_pushed.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    bool ok = (produced.load() == actual_total)
           && (consumed.load() == actual_total);
    std::string err;
    if (!ok) {
        std::ostringstream oss;
        oss << "produced=" << produced.load()
            << " consumed=" << consumed.load()
            << " expected=" << actual_total;
        err = oss.str();
    }
    return {std::move(name), actual_total, ok, std::move(err)};
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    bool all_ok = true;

    // ---- 1. MpmcQueue SPSC latency ----------------------------------------
    constexpr int LAT_ITEMS = 30000;
    const std::vector<size_t> caps = {16, 64, 256, 1024, 4096};

    std::vector<MpmcLatResult> mlat;
    mlat.reserve(caps.size());
    for (size_t c : caps)
        mlat.push_back(mpmc_spsc_latency(c, LAT_ITEMS));

    std::cout << "## MpmcQueue End-to-End Latency (SPSC, "
              << LAT_ITEMS << " items per queue size)\n\n";
    std::cout << "| Queue Capacity | P50 (ns) | P95 (ns) | P99 (ns) | P99.9 (ns) |\n";
    std::cout << "|---------------:|---------:|---------:|---------:|-----------:|\n";
    for (auto& r : mlat) {
        std::cout
            << "| " << std::setw(14) << r.capacity
            << " | " << std::setw(8)  << r.p50_ns
            << " | " << std::setw(8)  << r.p95_ns
            << " | " << std::setw(8)  << r.p99_ns
            << " | " << std::setw(10) << r.p999_ns
            << " |\n";
    }
    std::cout << "\n";

    // ---- 2. ResponseQueue SPSC latency -------------------------------------
    std::vector<RQLatResult> rlat;
    rlat.reserve(caps.size());
    for (size_t c : caps)
        rlat.push_back(rq_spsc_latency(c, LAT_ITEMS));

    std::cout << "## ResponseQueue End-to-End Latency (SPSC, "
              << LAT_ITEMS << " items per queue size)\n\n";
    std::cout << "| Queue Capacity | P50 (ns) | P95 (ns) | P99 (ns) |\n";
    std::cout << "|---------------:|---------:|---------:|---------:|\n";
    for (auto& r : rlat) {
        std::cout
            << "| " << std::setw(14) << r.actual_capacity
            << " | " << std::setw(8)  << r.p50_ns
            << " | " << std::setw(8)  << r.p95_ns
            << " | " << std::setw(8)  << r.p99_ns
            << " |\n";
    }
    std::cout << "\n";

    // ---- 3. Throughput -------------------------------------------------------
    constexpr int TPUT_ITEMS = 400000;
    std::vector<TputResult> tput = {
        measure_throughput("SPSC (1P×1C)",  1, 1, 256,  TPUT_ITEMS),
        measure_throughput("MPSC (4P×1C)",  4, 1, 1024, TPUT_ITEMS),
        measure_throughput("MPMC (4P×4C)",  4, 4, 4096, TPUT_ITEMS),
        measure_throughput("MPMC (8P×4C)",  8, 4, 4096, TPUT_ITEMS),
    };

    std::cout << "## Throughput (" << TPUT_ITEMS << " items per configuration)\n\n";
    std::cout << "| Configuration | Queue Cap | Throughput (Mops/s) |\n";
    std::cout << "|:--------------|----------:|--------------------:|\n";
    for (auto& r : tput) {
        std::cout
            << "| " << std::left  << std::setw(13) << r.config
            << " | " << std::right << std::setw(9) << r.capacity
            << " | " << std::fixed << std::setprecision(2)
                     << std::setw(19) << r.mops_per_sec
            << " |\n";
    }
    std::cout << "\n";

    // ---- 4. Stress / correctness ---------------------------------------------
    constexpr int STRESS_ITEMS = 100000;
    std::vector<StressResult> stress = {
        stress_test("SPSC (1P×1C)",  1, 1, 512,  STRESS_ITEMS),
        stress_test("MPSC (4P×1C)",  4, 1, 512,  STRESS_ITEMS),
        stress_test("MPMC (4P×4C)",  4, 4, 1024, STRESS_ITEMS),
        stress_test("MPMC (8P×8C)",  8, 8, 2048, STRESS_ITEMS),
    };

    std::cout << "## Stress / Correctness (" << STRESS_ITEMS << " items each)\n\n";
    std::cout << "| Test          | Items   | Result |\n";
    std::cout << "|:--------------|--------:|:------:|\n";
    for (auto& r : stress) {
        std::cout
            << "| " << std::left  << std::setw(13) << r.name
            << " | " << std::right << std::setw(7) << r.total_items
            << " | " << (r.passed ? "✅ PASS" : "❌ FAIL") << " |";
        if (!r.passed) {
            std::cout << " <!-- " << r.error_detail << " -->";
            all_ok = false;
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    if (!all_ok) {
        std::cerr << "ERROR: one or more stress tests FAILED\n";
    }
    return all_ok ? 0 : 1;
}
