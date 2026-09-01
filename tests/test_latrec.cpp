/**
 * @file test_latrec.cpp
 * @brief Tests for the latrec stamp recorder.
 *
 * Covers the ring format, output-directory placement, wrap accounting, the
 * per-thread rings and their lifetime, and the accuracy of what is recorded:
 * a known delay injected between two stamps must read back as that delay.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/latrec.h"
#include "latrec_ring_reader.hpp"
#include "test_framework.hpp"

#include <sys/wait.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using latrec_test::RingFile;
using latrec_test::seq_of;
using latrec_test::stage_of;

/* A ring directory that exists for one test and is removed after it. Publishes
 * itself as the process-wide output directory, so the per-thread rings this
 * test opens land here and not in the compiled-in default. */
class TraceDir {
public:
    TraceDir() {
        char tmpl[] = "/tmp/latrec_test_XXXXXX";
        path_ = mkdtemp(tmpl) ? tmpl : "";
        latrec_set_output_dir(path_.empty() ? nullptr : path_.c_str());
    }
    ~TraceDir() {
        latrec_set_output_dir(nullptr);
        if (!path_.empty()) {
            std::string cmd = "rm -rf '" + path_ + "'";
            if (system(cmd.c_str()) != 0) { /* best effort */ }
        }
    }
    const std::string& path() const { return path_; }
    const char* c_path() const { return path_.c_str(); }
    std::string file(const std::string& name) const {
        return path_ + "/" + name + ".latrec";
    }
    size_t count_files() const {
        std::string cmd = "ls -1 '" + path_ + "'/*.latrec 2>/dev/null | wc -l";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) return 0;
        char buf[32] = {0};
        if (!fgets(buf, sizeof(buf), p)) buf[0] = '0';
        pclose(p);
        return static_cast<size_t>(atoi(buf));
    }
private:
    std::string path_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------

TEST(header_and_record_layout) {
    ASSERT_EQ(sizeof(latrec_rec), 32u);
    ASSERT_EQ(sizeof(latrec_hdr), static_cast<size_t>(LATREC_HDR_LEN));

    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "fmt", 12), 1);
    latrec_stamp(&r, 7, LATREC_ENCODE_E3AP_DONE, 11, 13);
    latrec_close(&r);

    RingFile f(dir.file("fmt"));
    ASSERT_TRUE(f.ok);
    ASSERT_EQ(f.magic, static_cast<uint32_t>(LATREC_MAGIC));
    ASSERT_EQ(f.version, LATREC_VERSION);
    ASSERT_EQ(f.rec_size, 32u);
    ASSERT_EQ(f.entries, 1ull << 12);
    ASSERT_STREQ(f.name.c_str(), "fmt");
    ASSERT_GT(f.clock_ns, 0u);                  // the open-time clock self-test ran
    ASSERT_GT(f.t0_real, 0u);
    ASSERT_GE(f.t1_mono, f.t0_mono);            // close() refreshed the pair
    ASSERT_EQ(f.rec_count, 1u);
    ASSERT_EQ(f.valid.size(), 1u);
    ASSERT_EQ(seq_of(f.valid[0]), 7u);
    ASSERT_EQ(stage_of(f.valid[0]), LATREC_ENCODE_E3AP_DONE);
    ASSERT_EQ(f.valid[0].aux, 11u);
    ASSERT_EQ(f.valid[0].aux2, 13u);
}

TEST(seq_is_48_bits_and_does_not_corrupt_stage_or_cpu) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "wide", 12), 1);
    const uint64_t big = 0x0000FFFFFFFFFFFFull;         // max 48-bit seq
    latrec_stamp(&r, big, LATREC_DROP, 0, 0);
    latrec_stamp(&r, big + 1, LATREC_DROP, 0, 0);    // must wrap to 0
    latrec_close(&r);

    RingFile f(dir.file("wide"));
    ASSERT_EQ(f.valid.size(), 2u);
    ASSERT_EQ(seq_of(f.valid[0]), big);
    ASSERT_EQ(stage_of(f.valid[0]), LATREC_DROP);
    ASSERT_EQ(seq_of(f.valid[1]), 0u);
    ASSERT_EQ(stage_of(f.valid[1]), LATREC_DROP);
}

TEST(unwritten_slots_stay_invalid) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "sparse", 12), 1);
    for (int i = 0; i < 10; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_RECV, 0, 0);
    latrec_close(&r);
    RingFile f(dir.file("sparse"));
    ASSERT_EQ(f.valid.size(), 10u);      // 4086 untouched slots have t_ns == 0
    ASSERT_EQ(f.entries, 4096u);
}

TEST(timestamps_are_monotonic_within_a_ring) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "mono", 14), 1);
    for (int i = 0; i < 5000; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_DEQUEUE, 0, 0);
    latrec_close(&r);
    RingFile f(dir.file("mono"));
    ASSERT_EQ(f.valid.size(), 5000u);
    for (size_t i = 1; i < f.valid.size(); i++) {
        ASSERT_GE(f.valid[i].t_ns, f.valid[i - 1].t_ns);
    }
}

// ---------------------------------------------------------------------------
// Accuracy: a known delay must read back as that delay.
// ---------------------------------------------------------------------------

TEST(measures_an_injected_delay) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "truth", 12), 1);
    const int64_t injected_us = 5000;                    // 5 ms
    latrec_stamp(&r, 1, LATREC_ENQUEUE, 0, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(injected_us));
    latrec_stamp(&r, 1, LATREC_SEND_DONE, 0, 0);
    latrec_close(&r);

    RingFile f(dir.file("truth"));
    ASSERT_EQ(f.valid.size(), 2u);
    const int64_t measured_us =
        static_cast<int64_t>(f.valid[1].t_ns - f.valid[0].t_ns) / 1000;
    // sleep_for guarantees a lower bound only, and a loaded host adds more,
    // so the bounds are wide. They check the magnitude and the unit.
    ASSERT_GE(measured_us, injected_us - 500);
    ASSERT_LT(measured_us, injected_us * 4);
}

TEST(stamp_ordering_matches_call_ordering) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "order", 12), 1);
    const uint8_t chain[] = {LATREC_ENQUEUE, LATREC_DEQUEUE,
                             LATREC_ENCODE_E3AP_DONE, LATREC_SEND_DONE};
    for (uint8_t s : chain) {
        latrec_stamp(&r, 99, s, 0, 0);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    latrec_close(&r);
    RingFile f(dir.file("order"));
    ASSERT_EQ(f.valid.size(), 4u);
    for (size_t i = 0; i < 4; i++) {
        ASSERT_EQ(stage_of(f.valid[i]), chain[i]);
        ASSERT_EQ(seq_of(f.valid[i]), 99u);
        if (i) ASSERT_GT(f.valid[i].t_ns, f.valid[i - 1].t_ns);
    }
}

// ---------------------------------------------------------------------------
// Wrap
// ---------------------------------------------------------------------------

TEST(wrap_overwrites_oldest_and_is_detectable) {
    TraceDir dir;
    latrec_t r;
    const unsigned log2 = 12;
    const uint64_t cap = 1ull << log2;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "wrap", log2), 1);
    const uint64_t written = cap + cap / 2;              // 1.5 laps
    for (uint64_t i = 0; i < written; i++) {
        latrec_stamp(&r, i + 1, LATREC_RECV, 0, 0);
    }
    latrec_close(&r);

    RingFile f(dir.file("wrap"));
    ASSERT_EQ(f.valid.size(), cap);                      // full, not more
    ASSERT_EQ(f.rec_count, written);                     // total ever written
    ASSERT_GT(f.rec_count, f.entries);                   // lets a reader compute loss
    // Exactly one descent in t_ns marks the wrap point for the offline reader.
    size_t descents = 0;
    for (size_t i = 1; i < f.valid.size(); i++) {
        if (f.valid[i].t_ns < f.valid[i - 1].t_ns) descents++;
    }
    ASSERT_EQ(descents, 1u);
    // The oldest surviving seq is the one written cap records ago.
    uint64_t min_seq = UINT64_MAX;
    for (const auto& rec : f.valid) min_seq = std::min(min_seq, seq_of(rec));
    ASSERT_EQ(min_seq, written - cap + 1);
}

// ---------------------------------------------------------------------------
// Heartbeat and close
// ---------------------------------------------------------------------------

TEST(heartbeat_publishes_progress_without_closing) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "beat", 12), 1);
    for (int i = 0; i < 25; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_RECV, 0, 0);
    latrec_heartbeat(&r);

    RingFile mid(dir.file("beat"));                      // read while still open
    ASSERT_EQ(mid.rec_count, 25u);
    ASSERT_EQ(mid.valid.size(), 25u);
    ASSERT_GE(mid.t1_mono, mid.t0_mono);

    for (int i = 0; i < 5; i++) latrec_stamp(&r, 100, LATREC_RECV, 0, 0);
    latrec_close(&r);
    RingFile end(dir.file("beat"));
    ASSERT_EQ(end.rec_count, 30u);
}

// ---------------------------------------------------------------------------
// Per-thread rings
// ---------------------------------------------------------------------------

TEST(each_thread_gets_its_own_named_ring) {
    TraceDir dir;
    std::thread a([] { latrec_tls_open_as("role.a"); latrec_tstamp(1, LATREC_RECV, 0, 0); });
    std::thread b([] { latrec_tls_open_as("role.b"); latrec_tstamp(2, LATREC_RECV, 0, 0); });
    a.join();
    b.join();
    ASSERT_EQ(dir.count_files(), 2u);
}

TEST(same_role_on_two_threads_does_not_collide) {
    TraceDir dir;
    std::thread a([] { latrec_tls_open_as("same"); latrec_tstamp(1, LATREC_RECV, 0, 0); });
    std::thread b([] { latrec_tls_open_as("same"); latrec_tstamp(2, LATREC_RECV, 0, 0); });
    a.join();
    b.join();
    ASSERT_EQ(dir.count_files(), 2u);      // disambiguated by tid, so nothing is truncated
}

TEST(tls_open_is_idempotent) {
    TraceDir dir;
    std::thread t([] {
        latrec_t* first = latrec_tls_open_as("once");
        latrec_t* again = latrec_tls_open_as("ignored-second-name");
        ASSERT_EQ(first, again);
        latrec_tstamp(1, LATREC_RECV, 0, 0);
    });
    t.join();
    ASSERT_EQ(dir.count_files(), 1u);
}

TEST(seq_counter_is_exact_under_contention) {
    constexpr int kThreads = 8, kPer = 20000;
    std::vector<std::thread> ts;
    std::atomic<uint64_t> xr{0};
    std::vector<std::vector<uint64_t>> got(kThreads);
    for (int i = 0; i < kThreads; i++) {
        ts.emplace_back([&, i] {
            got[static_cast<size_t>(i)].reserve(kPer);
            for (int k = 0; k < kPer; k++) got[static_cast<size_t>(i)].push_back(latrec_seq_next());
        });
    }
    for (auto& t : ts) t.join();
    std::set<uint64_t> all;
    for (auto& v : got) for (uint64_t s : v) all.insert(s);
    ASSERT_EQ(all.size(), static_cast<size_t>(kThreads) * kPer);   // no duplicates
    (void)xr;
}

// ---------------------------------------------------------------------------
// Lifetime: a ring must outlive the thread that opened it.
//
// A ring held in thread-local storage is released when its thread exits, while
// the exit flush still holds a pointer to it. glibc caches exited threads'
// stacks, so the freed mapping stays readable until churn evicts it; the fault
// therefore needs many threads and a real process exit, hence the fork.
// ---------------------------------------------------------------------------

TEST(exit_flush_survives_threads_that_already_exited) {
    TraceDir dir;
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        for (int i = 0; i < 48; i++) {
            std::thread t([i] {
                latrec_tls_open_as(("churn" + std::to_string(i)).c_str());
                latrec_tstamp(static_cast<uint64_t>(i), LATREC_RECV, 0, 0);
            });
            t.join();                       // thread exits; its ring must not
        }                                   // be freed with its TLS block
        exit(0);                            // runs the atexit flush
    }
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status));         // not WIFSIGNALED: no SIGSEGV
    ASSERT_EQ(WEXITSTATUS(status), 0);
}

TEST(exit_flush_publishes_rec_count) {
    TraceDir dir;
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        std::thread t([] {
            latrec_tls_open_as("flushed");
            for (int i = 0; i < 17; i++) latrec_tstamp(1, LATREC_RECV, 0, 0);
        });
        t.join();
        exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT_EQ(WEXITSTATUS(status), 0);
    // Name carries the child's tid, so find the single ring in the directory.
    std::string cmd = "ls -1 '" + dir.path() + "'/flushed.*.latrec 2>/dev/null | head -1";
    FILE* p = popen(cmd.c_str(), "r");
    char buf[512] = {0};
    if (p) { if (!fgets(buf, sizeof(buf), p)) buf[0] = 0; pclose(p); }
    std::string path(buf);
    while (!path.empty() && (path.back() == '\n' || path.back() == ' ')) path.pop_back();
    ASSERT_FALSE(path.empty());
    RingFile f(path);
    ASSERT_TRUE(f.ok);
    ASSERT_EQ(f.rec_count, 17u);            // set by the exit flush, not by close()
    ASSERT_EQ(f.valid.size(), 17u);
}

// ---------------------------------------------------------------------------
// Capacity override
// ---------------------------------------------------------------------------

namespace {
uint64_t entries_with_env(const char* value) {
    TraceDir dir;
    if (value) setenv("LATREC_ENTRIES_LOG2", value, 1);
    else unsetenv("LATREC_ENTRIES_LOG2");
    uint64_t entries = 0;
    std::thread t([&] {
        latrec_tls_open_as("cap");
        latrec_tstamp(1, LATREC_RECV, 0, 0);
        entries = latrec_tls->mask + 1;
    });
    t.join();
    unsetenv("LATREC_ENTRIES_LOG2");
    return entries;
}
}  // namespace

TEST(entries_log2_env_is_honoured_and_validated) {
    ASSERT_EQ(entries_with_env("16"), 1ull << 16);      // in range
    ASSERT_EQ(entries_with_env("13"), 1ull << 13);
    const uint64_t fallback = entries_with_env(nullptr);
    ASSERT_GT(fallback, 0u);
    ASSERT_EQ(entries_with_env("99"), fallback);        // out of range -> default
    ASSERT_EQ(entries_with_env("0"), fallback);
    ASSERT_EQ(entries_with_env("not-a-number"), fallback);
    ASSERT_EQ(entries_with_env(""), fallback);
}

// ---------------------------------------------------------------------------
// Thread-local context (used by the connector, which is handed no seq)
// ---------------------------------------------------------------------------

TEST(ctx_round_trips_on_an_enabled_ring) {
    TraceDir dir;
    uint64_t seen = 0;
    std::thread t([&] {
        latrec_tls_open_as("ctx");
        latrec_ctx_set(0xABCDEFu);
        seen = latrec_ctx();
        latrec_tstamp(latrec_ctx(), LATREC_SEND_DONE, 64, 0);
    });
    t.join();
    ASSERT_EQ(seen, 0xABCDEFu);
    // The ring is named with the thread's tid, so scan the directory rather
    // than guessing the filename, and confirm the stamp carried the context.
    const auto recs = latrec_test::read_ring_dir(dir.path());
    size_t with_ctx = 0;
    for (const auto& r : recs) {
        if (stage_of(r) == LATREC_SEND_DONE && seq_of(r) == 0xABCDEFu) with_ctx++;
    }
    ASSERT_EQ(with_ctx, 1u);
}

// ---------------------------------------------------------------------------
// Concurrent reading: the reason t_ns is released last.
// ---------------------------------------------------------------------------

TEST(a_reader_never_sees_a_half_written_record) {
    // The writer keeps aux == seq*2 and aux2 == seq*3, so a record whose t_ns
    // is set but whose payload belongs to a different write is detectable. The
    // release store compiles to a plain mov on x86 and to stlr on weakly
    // ordered targets, where it is what orders the payload before t_ns.
    //
    // The ring is sized so the writer cannot lap it during the test. The
    // release store makes a single record self-consistent; it does not stop a
    // slot being rewritten under a reader, so on a ring that wraps a reader
    // can pair one write's t_ns with the next write's payload.
    TraceDir dir;
    constexpr uint64_t kWrites = 400000;
    constexpr unsigned kLog2 = 20;                 // 1 Mi slots > kWrites
    latrec_t r;
    ASSERT_EQ(latrec_open_in(&r, dir.c_path(), "concurrent", kLog2), 1);

    std::atomic<bool> writing{true};
    std::atomic<bool> reader_up{false};
    std::atomic<uint64_t> torn{0}, seen{0};

    std::thread reader([&] {
        // Read the live mapping the way the out-of-process drainer will.
        const latrec_rec* recs =
            reinterpret_cast<const latrec_rec*>(static_cast<uint8_t*>(r.map) + LATREC_HDR_LEN);
        const uint64_t entries = 1ull << kLog2;
        reader_up.store(true, std::memory_order_release);
        while (writing.load(std::memory_order_relaxed)) {
            for (uint64_t i = 0; i < entries; i++) {
                const uint64_t t = LATREC_LOAD_ACQUIRE(&recs[i].t_ns);
                if (!t) continue;
                const uint64_t sc = recs[i].sc;
                const uint64_t seq = sc & 0x0000FFFFFFFFFFFFull;
                const uint64_t aux = recs[i].aux, aux2 = recs[i].aux2;
                seen.fetch_add(1, std::memory_order_relaxed);
                if (aux != seq * 2 || aux2 != seq * 3) torn.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Creating the reader does not put it on a CPU: wait until it runs.
    while (!reader_up.load(std::memory_order_acquire)) std::this_thread::yield();
    uint64_t i = 1;
    for (; i <= kWrites; i++) latrec_stamp(&r, i, LATREC_RECV, i * 2, i * 3);
    for (const uint64_t cap = 1ull << kLog2;
         i <= cap && seen.load(std::memory_order_relaxed) == 0; i++) {
        // Extend the window until the reader samples a record, within one ring
        // pass: a wrap pairs one write's t_ns with the next write's payload.
        latrec_stamp(&r, i, LATREC_RECV, i * 2, i * 3);
    }
    writing.store(false);
    reader.join();
    latrec_close(&r);

    std::printf("      reader inspected %llu records\n",
                static_cast<unsigned long long>(seen.load()));
    ASSERT_GT(seen.load(), 0u);        // the reader really did observe live data
    ASSERT_EQ(torn.load(), 0u);
}

TEST(stage_ids_are_unique) {
    // The catalog is a cross-repo contract: a duplicate id silently merges two
    // different measurements. Every identifier the catalog defines belongs
    // here, so a new one that collides fails this test rather than a run.
    const uint8_t ids[] = {
        LATREC_RECORD_BEGIN, LATREC_PROCESS_BEGIN,
        LATREC_ENCODE_E3SM_BEGIN, LATREC_ENCODE_E3SM_DONE,
        LATREC_DECODE_E3SM_BEGIN, LATREC_DECODE_E3SM_DONE,
        LATREC_EMIT_ENTER, LATREC_ENQUEUE, LATREC_DEQUEUE,
        LATREC_ENCODE_E3AP_DONE, LATREC_SEND_DONE,
        LATREC_RECV, LATREC_DECODE_E3AP_DONE,
        LATREC_DELIVER_BEGIN, LATREC_DELIVER_DONE,
        LATREC_REPORT_QUEUED, LATREC_REPORT_DONE,
        LATREC_SESSION_QUEUED, LATREC_SESSION_POLLED,
        LATREC_DROP,
        LATREC_SETUP_BEGIN, LATREC_SETUP_DONE,
        LATREC_SUB_BEGIN, LATREC_SUB_DONE,
        LATREC_SM_START, LATREC_SM_STOP,
        LATREC_SM_STATUS_BEGIN, LATREC_SM_STATUS_DONE,
        LATREC_PROCESS_DONE, LATREC_CREATE_OUTPUT,
        LATREC_APPLY_POLICY_DONE, LATREC_ADMITTED,
        LATREC_APPLY_CONTROL_DONE, LATREC_LIVE_ON_AIR,
        LATREC_ACK_SENT, LATREC_ACK_RECV,
        LATREC_BRIDGE_IN, LATREC_BRIDGE_OUT,
        LATREC_ENCODE_E2AP_BEGIN, LATREC_ENCODE_E2AP_DONE,
        LATREC_DECODE_E2AP_BEGIN, LATREC_DECODE_E2AP_DONE,
        LATREC_DECODE_E2SM_BEGIN, LATREC_DECODE_E2SM_DONE,
        LATREC_XAPP_PROCESS_DONE,
        LATREC_ENCODE_E2SM_BEGIN, LATREC_ENCODE_E2SM_DONE,
        LATREC_WAIT_ENTER, LATREC_SKIPPED, LATREC_CONTEXT,
    };
    std::set<uint8_t> seen(std::begin(ids), std::end(ids));
    ASSERT_EQ(seen.size(), sizeof(ids) / sizeof(ids[0]));
}

int main() {
    return RUN_ALL_TESTS();
}
