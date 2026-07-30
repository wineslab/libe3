/**
 * @file test_latrec.cpp
 * @brief Tests for the latrec stamp recorder.
 *
 * Covers the ring format, the LATREC_DIR gate, wrap accounting, the
 * per-thread rings and their lifetime, and the accuracy of what is recorded:
 * a known delay injected between two stamps must read back as that delay.
 *
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

/* A LATREC_DIR that exists for one test and is removed after it. */
class TraceDir {
public:
    explicit TraceDir(bool enable = true) {
        char tmpl[] = "/tmp/latrec_test_XXXXXX";
        path_ = mkdtemp(tmpl) ? tmpl : "";
        if (enable && !path_.empty()) setenv("LATREC_DIR", path_.c_str(), 1);
        else unsetenv("LATREC_DIR");
    }
    ~TraceDir() {
        unsetenv("LATREC_DIR");
        if (!path_.empty()) {
            std::string cmd = "rm -rf '" + path_ + "'";
            if (system(cmd.c_str()) != 0) { /* best effort */ }
        }
    }
    const std::string& path() const { return path_; }
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
// The LATREC_DIR gate: instrumented builds must be inert in production.
// ---------------------------------------------------------------------------

TEST(disabled_without_latrec_dir) {
    TraceDir dir(/*enable=*/false);
    latrec_t r;
    ASSERT_EQ(latrec_open(&r, "off", 12), 0);   // 0 == tracing disabled
    ASSERT_EQ(r.enabled, 0);
    // Each entry point is called against a disabled ring.
    latrec_stamp(&r, 1, LATREC_L0_ENQUEUE, 2, 3);
    latrec_heartbeat(&r);
    latrec_refresh_cpu(&r);
    latrec_close(&r);
    ASSERT_EQ(dir.count_files(), 0u);
}

TEST(disabled_tls_ring_never_creates_a_file) {
    TraceDir dir(/*enable=*/false);
    std::thread t([] {
        latrec_tls_open_as("should.not.exist");
        for (int i = 0; i < 100; i++) latrec_tstamp(1, LATREC_L4_RECV, 0, 0);
        latrec_ctx_set(42);
        ASSERT_EQ(latrec_ctx(), 0u);            // guarded: disabled ring is shared
    });
    t.join();
    ASSERT_EQ(dir.count_files(), 0u);
}

// ---------------------------------------------------------------------------
// Format
// ---------------------------------------------------------------------------

TEST(header_and_record_layout) {
    ASSERT_EQ(sizeof(latrec_rec), 32u);
    ASSERT_EQ(sizeof(latrec_hdr), static_cast<size_t>(LATREC_HDR_LEN));

    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open(&r, "fmt", 12), 1);
    latrec_stamp(&r, 7, LATREC_L2_ENCODE_DONE, 11, 13);
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
    ASSERT_EQ(stage_of(f.valid[0]), LATREC_L2_ENCODE_DONE);
    ASSERT_EQ(f.valid[0].aux, 11u);
    ASSERT_EQ(f.valid[0].aux2, 13u);
}

TEST(seq_is_48_bits_and_does_not_corrupt_stage_or_cpu) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open(&r, "wide", 12), 1);
    const uint64_t big = 0x0000FFFFFFFFFFFFull;         // max 48-bit seq
    latrec_stamp(&r, big, LATREC_L9_DROP, 0, 0);
    latrec_stamp(&r, big + 1, LATREC_L9_DROP, 0, 0);    // must wrap to 0
    latrec_close(&r);

    RingFile f(dir.file("wide"));
    ASSERT_EQ(f.valid.size(), 2u);
    ASSERT_EQ(seq_of(f.valid[0]), big);
    ASSERT_EQ(stage_of(f.valid[0]), LATREC_L9_DROP);
    ASSERT_EQ(seq_of(f.valid[1]), 0u);
    ASSERT_EQ(stage_of(f.valid[1]), LATREC_L9_DROP);
}

TEST(unwritten_slots_stay_invalid) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open(&r, "sparse", 12), 1);
    for (int i = 0; i < 10; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_L4_RECV, 0, 0);
    latrec_close(&r);
    RingFile f(dir.file("sparse"));
    ASSERT_EQ(f.valid.size(), 10u);      // 4086 untouched slots have t_ns == 0
    ASSERT_EQ(f.entries, 4096u);
}

TEST(timestamps_are_monotonic_within_a_ring) {
    TraceDir dir;
    latrec_t r;
    ASSERT_EQ(latrec_open(&r, "mono", 14), 1);
    for (int i = 0; i < 5000; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_L1_DEQUEUE, 0, 0);
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
    ASSERT_EQ(latrec_open(&r, "truth", 12), 1);
    const int64_t injected_us = 5000;                    // 5 ms
    latrec_stamp(&r, 1, LATREC_L0_ENQUEUE, 0, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(injected_us));
    latrec_stamp(&r, 1, LATREC_L3_SEND_DONE, 0, 0);
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
    ASSERT_EQ(latrec_open(&r, "order", 12), 1);
    const uint8_t chain[] = {LATREC_L0_ENQUEUE, LATREC_L1_DEQUEUE,
                             LATREC_L2_ENCODE_DONE, LATREC_L3_SEND_DONE};
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
    ASSERT_EQ(latrec_open(&r, "wrap", log2), 1);
    const uint64_t written = cap + cap / 2;              // 1.5 laps
    for (uint64_t i = 0; i < written; i++) {
        latrec_stamp(&r, i + 1, LATREC_L4_RECV, 0, 0);
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
    ASSERT_EQ(latrec_open(&r, "beat", 12), 1);
    for (int i = 0; i < 25; i++) latrec_stamp(&r, static_cast<uint64_t>(i), LATREC_L4_RECV, 0, 0);
    latrec_heartbeat(&r);

    RingFile mid(dir.file("beat"));                      // read while still open
    ASSERT_EQ(mid.rec_count, 25u);
    ASSERT_EQ(mid.valid.size(), 25u);
    ASSERT_GE(mid.t1_mono, mid.t0_mono);

    for (int i = 0; i < 5; i++) latrec_stamp(&r, 100, LATREC_L4_RECV, 0, 0);
    latrec_close(&r);
    RingFile end(dir.file("beat"));
    ASSERT_EQ(end.rec_count, 30u);
}

// ---------------------------------------------------------------------------
// Per-thread rings
// ---------------------------------------------------------------------------

TEST(each_thread_gets_its_own_named_ring) {
    TraceDir dir;
    std::thread a([] { latrec_tls_open_as("role.a"); latrec_tstamp(1, LATREC_L4_RECV, 0, 0); });
    std::thread b([] { latrec_tls_open_as("role.b"); latrec_tstamp(2, LATREC_L4_RECV, 0, 0); });
    a.join();
    b.join();
    ASSERT_EQ(dir.count_files(), 2u);
}

TEST(same_role_on_two_threads_does_not_collide) {
    TraceDir dir;
    std::thread a([] { latrec_tls_open_as("same"); latrec_tstamp(1, LATREC_L4_RECV, 0, 0); });
    std::thread b([] { latrec_tls_open_as("same"); latrec_tstamp(2, LATREC_L4_RECV, 0, 0); });
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
        latrec_tstamp(1, LATREC_L4_RECV, 0, 0);
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
                latrec_tstamp(static_cast<uint64_t>(i), LATREC_L4_RECV, 0, 0);
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
            for (int i = 0; i < 17; i++) latrec_tstamp(1, LATREC_L4_RECV, 0, 0);
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
        latrec_tstamp(1, LATREC_L4_RECV, 0, 0);
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
        latrec_tstamp(latrec_ctx(), LATREC_LC0_SEND_ENTER, 64, 0);
    });
    t.join();
    ASSERT_EQ(seen, 0xABCDEFu);
    // The ring is named with the thread's tid, so scan the directory rather
    // than guessing the filename, and confirm the stamp carried the context.
    const auto recs = latrec_test::read_ring_dir(dir.path());
    size_t with_ctx = 0;
    for (const auto& r : recs) {
        if (stage_of(r) == LATREC_LC0_SEND_ENTER && seq_of(r) == 0xABCDEFu) with_ctx++;
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
    ASSERT_EQ(latrec_open(&r, "concurrent", kLog2), 1);

    std::atomic<bool> writing{true};
    std::atomic<uint64_t> torn{0}, seen{0};

    std::thread reader([&] {
        // Read the live mapping the way the out-of-process drainer will.
        const latrec_rec* recs =
            reinterpret_cast<const latrec_rec*>(static_cast<uint8_t*>(r.map) + LATREC_HDR_LEN);
        const uint64_t entries = 1ull << kLog2;
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

    for (uint64_t i = 1; i <= kWrites; i++) latrec_stamp(&r, i, LATREC_L4_RECV, i * 2, i * 3);
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
    // different measurements.
    const uint8_t ids[] = {
        LATREC_P0_PUSH_ENTRY, LATREC_P1_COPY_DONE, LATREC_P2_MASK_DONE,
        LATREC_P3_INFO_BUILT, LATREC_P4_CHAN_LOCKED, LATREC_P5_PUBLISHED,
        LATREC_P6_RX_ENTRY, LATREC_P7_UESPEC_DONE, LATREC_P8_UL_IND_DONE,
        LATREC_P9_RING_READY,
        LATREC_W0_WAKE, LATREC_W1_SLOT_SELECT, LATREC_W2_META_BUILT,
        LATREC_W3_ENCODE_DONE, LATREC_W4_SENT_TO_E3,
        LATREC_W5_WAIT_ENTER, LATREC_W6_SKIPPED, LATREC_LE0_EMIT_ENTER,
        LATREC_L0_ENQUEUE, LATREC_L1_DEQUEUE, LATREC_L2_ENCODE_DONE,
        LATREC_L3_SEND_DONE, LATREC_L4_RECV, LATREC_L5_DECODED,
        LATREC_L6_DISPATCHED, LATREC_L7_REPORT_QUEUED, LATREC_L8_REPORT_DONE,
        LATREC_L9_DROP, LATREC_LS0_SETUP_RECV, LATREC_LS1_SETUP_SENT,
        LATREC_LQ0_QUEUED, LATREC_LQ1_POLLED,
        LATREC_LC0_SEND_ENTER, LATREC_LC1_SEND_RETURNED,
        LATREC_D0_RECV, LATREC_D1_PARSED, LATREC_D2_DISPATCHED,
        LATREC_D3_HANDLER_IN, LATREC_D4_RX_ACCOUNTED, LATREC_D5_ADMITTED,
        LATREC_D6_COMPUTED, LATREC_D7_DETECTED, LATREC_D8_SM_SENT,
        LATREC_D9_SNAPPED,
        LATREC_D10_HANDLER_OUT, LATREC_D11_L2SCAN_DONE, LATREC_D12_ENCODE_DONE,
        LATREC_D13_SENSE_IN, LATREC_D14_SENSE_OUT, LATREC_D15_CONVERT_DONE,
        LATREC_D16_DETECT_PRE, LATREC_D17_GPU_SUBMIT, LATREC_D18_GPU_RETIRED,
        LATREC_D19_VIZ_SENT,
        LATREC_V0_SNAP_TAKEN, LATREC_V1_QUANTIZED,
        LATREC_V2_PUBLISHED, LATREC_V3_WOKE,
        LATREC_E0_SUB_SENT, LATREC_E1_SUB_CONFIRMED, LATREC_E2_SETUP_SENT,
        LATREC_E3_SETUP_RESP, LATREC_E4_SETUP_READY,
        LATREC_C0_CONTEXT,
        LATREC_M0_SLOT_ENTRY, LATREC_M1_LOCK_HELD, LATREC_M2_BLOCK_APPLIED,
        LATREC_M3_UL_DONE, LATREC_M4_DL_DONE, LATREC_M5_PUCCH_DONE,
        LATREC_M6_SENSING_DONE, LATREC_M7_SLOT_EXIT,
        LATREC_T0_SM_START, LATREC_T1_SM_STOP, LATREC_T2_STATUS_IN,
        LATREC_T3_PERIOD_SET, LATREC_T4_RIC_UPDATED, LATREC_T5_STATUS_DONE,
        LATREC_S0_RECORD_IN, LATREC_S1_PUBLISHED, LATREC_S2_WORKER_WAKE,
        LATREC_S3_RANGES_READ, LATREC_S4_SHM_WRITTEN, LATREC_S5_ENCODE_DONE,
        LATREC_S6_SENT_TO_E3, LATREC_S7_WAIT_ENTER, LATREC_S8_SKIPPED,
        LATREC_B0_CTRL_RECV, LATREC_B1_DECODED, LATREC_B2_PREPARED,
        LATREC_B3_UL_INSTALLED, LATREC_B4_INSTALLED, LATREC_B5_ACKED,
        LATREC_B6_LIVE_ON_AIR,
        LATREC_G0_CTRL_RECV, LATREC_G1_CTRL_E3_SENT, LATREC_G8_REP_RECV,
        LATREC_G9_REP_TO_E2, LATREC_K0_CTRL_RECV, LATREC_K1_CTRL_APPLIED,
        LATREC_K2_CTRL_ACKED, LATREC_K3_CTRL_SENT, LATREC_K4_ACK_RECV,
        LATREC_K5_CTRL_DECODED, LATREC_K6_PUBLISHED,
        LATREC_R0_REP_BUILT, LATREC_R1_REP_SENT,
        LATREC_X0_CTRL_REQ, LATREC_X1_CTRL_SM_ENC, LATREC_X2_CTRL_E2AP_ENC,
        LATREC_X3_CTRL_SCTP_SENT, LATREC_X4_AG_E2AP_DEC, LATREC_X5_AG_CTRL_IN,
        LATREC_X6_AG_SM_DEC, LATREC_X7_AG_CTRL_OUT, LATREC_X8_AG_IND_ENC,
        LATREC_X9_AG_IND_E2AP, LATREC_XA_AG_IND_SENT, LATREC_XB_IND_RECV,
        LATREC_XC_IND_DEC, LATREC_XD_IND_DISPATCH, LATREC_XE_REP_RECV,
    };
    std::set<uint8_t> seen(std::begin(ids), std::end(ids));
    ASSERT_EQ(seen.size(), sizeof(ids) / sizeof(ids[0]));
}

int main() {
    return RUN_ALL_TESTS();
}
