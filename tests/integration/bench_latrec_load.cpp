/**
 * @file bench_latrec_load.cpp
 * @brief libe3 under mixed bidirectional load, across a sweep of offered rates.
 *
 * Drives a colocated RAN + dApp pair (ZMQ IPC, one process) with all three
 * traffic types running at once -- indications carrying IQ downstream, xApp
 * controls relayed downstream, dApp reports upstream -- and reports, per rate
 * tier, achieved throughput two ways: independent atomic counters incremented
 * in the receiving handlers (always on, latrec-free) and, in a build with the
 * recorder compiled in, what latrec recorded for each flow and each stage.
 * This is the end-to-end ablation vehicle: build it twice, once with
 * -DLIBE3_ENABLE_LATREC=OFF and once ON, and compare the independent counters
 * between the two runs. The latrec-derived tables only exist in the ON build,
 * by construction -- there is no third "compiled in but idle" configuration,
 * since an enabled build always records.
 *
 * Run together, the flows contend for the inbound loop, the report worker,
 * the outbound queue and the single process-wide sequence counter. Unlike
 * bench_full_loop_latency, which paces one round trip at a time and keeps its
 * own timestamps, the latrec-derived figures here are read back from the
 * shipped instrumentation.
 *
 * Flows are told apart by the PduType that DEQUEUE already records in aux2, so
 * one merged read of the rings still yields per-flow numbers. All tiers share
 * one ring directory and are separated by the wall-clock window each ran in,
 * because a thread's ring is opened once and cannot follow a change of
 * directory.
 *
 * Usage: bench_latrec_load [ring-directory]. Without an argument an ON build
 * creates a temporary directory and removes it at exit.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/e3_agent.hpp>
#include <libe3/latrec.h>

#include "latrec_ring_reader.hpp"
#include "sm_simple/simple_service_model.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace libe3;
using namespace latrec_test;

namespace {

constexpr int kSecondsPerTier = 4;

/* Offered load per tier. ind_hz == 0 emits indications as fast as the SM can;
 * the control and report rates stay finite so the upstream direction keeps
 * running while the downstream one saturates. */
struct Tier { const char* name; uint64_t ind_hz, ctrl_hz, rep_hz; };
const Tier kTiers[] = {
    {"low",     100,   10,   10},
    {"medium",  1000,  100,  100},
    {"high",    10000, 500,  500},
    {"flood",   0,     1000, 1000},
};

struct Stats { size_t n{0}; double p50{-1}, p99{-1}, max{-1}; };

/* Sorts once and returns all three figures: as separate call arguments, the
 * order of the sort and the max read would be unspecified. */
Stats summarize(std::vector<double> v) {
    Stats s;
    s.n = v.size();
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.p50 = v[v.size() / 2];
    s.p99 = v[std::min(v.size() - 1,
                       static_cast<size_t>(static_cast<double>(v.size()) * 0.99))];
    s.max = v.back();
    return s;
}

struct Window { Tier tier; uint64_t t0, t1; };

/* Independent, latrec-free counters: incremented in the receiving handlers,
 * so they measure end-to-end delivery regardless of whether latrec is
 * compiled in, enabled, or capturing. */
struct FlowCounters {
    std::atomic<uint64_t> ind_recv{0}, ctrl_recv{0}, rep_recv{0};
};

struct TierCounts { Tier tier; uint64_t ind, ctrl, rep; };

/* One tier's records, indexed by seq then stage. */
using Table = std::map<uint64_t, std::map<uint8_t, latrec_rec>>;

Table index_window(const std::vector<latrec_rec>& recs, const Window& w) {
    Table t;
    for (const auto& r : recs) {
        if (r.t_ns >= w.t0 && r.t_ns <= w.t1) t[seq_of(r)].emplace(stage_of(r), r);
    }
    return t;
}

Stats leg(const Table& t, uint8_t a, uint8_t b) {
    std::vector<double> v;
    for (const auto& [seq, s] : t) {
        (void)seq;
        const auto ia = s.find(a), ib = s.find(b);
        if (ia == s.end() || ib == s.end()) continue;
        if (ib->second.t_ns < ia->second.t_ns) continue;
        v.push_back(static_cast<double>(ib->second.t_ns - ia->second.t_ns) / 1000.0);
    }
    return summarize(v);
}

/* Messages of one PduType, identified by the type recorded in aux2 at `stage`. */
size_t count_type(const Table& t, uint8_t stage, PduType type) {
    size_t n = 0;
    for (const auto& [seq, s] : t) {
        (void)seq;
        const auto it = s.find(stage);
        if (it != s.end() && it->second.aux2 == static_cast<uint64_t>(type)) n++;
    }
    return n;
}

size_t count_stage(const Table& t, uint8_t stage) {
    size_t n = 0;
    for (const auto& [seq, s] : t) { (void)seq; if (s.count(stage)) n++; }
    return n;
}

/* Drops split by the reason recorded in aux2. */
std::string drop_breakdown(const Table& t) {
    std::map<uint64_t, size_t> by_reason;
    for (const auto& [seq, s] : t) {
        (void)seq;
        const auto it = s.find(LATREC_DROP);
        if (it != s.end()) by_reason[it->second.aux2]++;
    }
    if (by_reason.empty()) return "-";
    const char* names[] = {"?", "queue-full", "encode", "send",
                           "decode", "report-queue", "no-handler", "session-queue",
                           "filtered", "shutdown"};
    std::string out;
    for (const auto& [reason, n] : by_reason) {
        if (!out.empty()) out += " ";
        out += (reason < sizeof(names) / sizeof(names[0]) ? names[reason] : "?");
        out += "=" + std::to_string(n);
    }
    return out;
}

std::string mkdir_tmp(const char* prefix) {
    std::string tmpl = std::string("/tmp/") + prefix + "_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const char* d = mkdtemp(buf.data());
    return d ? d : "/tmp";
}

/* The example SM prints one line per indication below 1 kHz. stdout is
 * redirected for the traffic phases and restored before the report. */
class MuteStdout {
public:
    MuteStdout() : saved_(dup(STDOUT_FILENO)) {
        const int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { fflush(stdout); dup2(devnull, STDOUT_FILENO); close(devnull); }
    }
    ~MuteStdout() {
        if (saved_ >= 0) { fflush(stdout); dup2(saved_, STDOUT_FILENO); close(saved_); }
    }
    MuteStdout(const MuteStdout&) = delete;
    MuteStdout& operator=(const MuteStdout&) = delete;
private:
    int saved_;
};

/* Paced driver: emits at rate_hz until stopped, on its own latrec ring, since
 * queue_outbound stamps L0 on the caller's thread. */
template <typename Fn>
std::thread spawn_driver(const char* role, uint64_t rate_hz,
                         std::atomic<bool>& stop, Fn fn) {
    return std::thread([role, rate_hz, &stop, fn] {
        latrec_tls_open_as(role);
        const auto period = std::chrono::microseconds(1'000'000ull / rate_hz);
        auto next = std::chrono::steady_clock::now();
        while (!stop.load(std::memory_order_relaxed)) {
            fn();
            next += period;
            std::this_thread::sleep_until(next);
        }
    });
}

}  // namespace

int main(int argc, char** argv) {
#ifdef LIBE3_ENABLE_LATREC
    constexpr bool tracing = true;
#else
    constexpr bool tracing = false;
#endif
    // A caller-named directory is kept; one we create ourselves is removed at
    // exit, so a bare run leaves nothing behind.
    std::string trace_dir;
    bool owns_dir = false;
    if (tracing) {
        if (argc > 1 && *argv[1]) {
            trace_dir = argv[1];
        } else {
            char tmpl[] = "/tmp/latrec_load_XXXXXX";
            if (const char* d = mkdtemp(tmpl)) { trace_dir = d; owns_dir = true; }
        }
        latrec_set_output_dir(trace_dir.empty() ? nullptr : trace_dir.c_str());
    }

    std::printf("libe3 under mixed bidirectional load (%d s per tier, ZMQ IPC)\n",
                kSecondsPerTier);
    std::printf("  downstream: indications (IQ) + xApp controls   "
                "upstream: dApp reports\n");
    std::printf("  latrec: %s\n\n",
                tracing ? trace_dir.c_str() : "not compiled in (LIBE3_ENABLE_LATREC=OFF)");

    std::vector<Window> windows;
    std::vector<TierCounts> tier_counts;
    {
        MuteStdout mute;
        for (const Tier& tier : kTiers) {
            const std::string ipc = mkdir_tmp("latrec_mix_ipc");
            E3Config ran_cfg;
            ran_cfg.role = E3Role::RAN;
            ran_cfg.ran_identifier = "mix-ran";
            ran_cfg.link_layer = E3LinkLayer::ZMQ;
            ran_cfg.transport_layer = E3TransportLayer::IPC;
            ran_cfg.log_level = 0;
            ran_cfg.setup_endpoint      = "ipc://" + ipc + "/setup";
            ran_cfg.subscriber_endpoint = "ipc://" + ipc + "/dapp_socket";
            ran_cfg.publisher_endpoint  = "ipc://" + ipc + "/e3_socket";
            auto dapp_cfg = ran_cfg;
            dapp_cfg.role = E3Role::DAPP;
            dapp_cfg.dapp_name = "MixDApp";

            FlowCounters counters;
            E3Agent ran(ran_cfg);
            ran.set_dapp_report_handler([&counters](const DAppReport&) {
                counters.rep_recv.fetch_add(1, std::memory_order_relaxed);
            });
            auto sm = std::make_unique<libe3_examples::SimpleServiceModel>(
                tier.ind_hz ? 1'000'000ull / tier.ind_hz : 0, EncodingFormat::ASN1,
                libe3_examples::SimpleServiceModel::PacingMode::FixedRate);
            if (ran.register_sm(std::move(sm)) != ErrorCode::SUCCESS ||
                ran.start() != ErrorCode::SUCCESS) {
                std::fprintf(stderr, "RAN start failed\n");
                return 1;
            }
            E3Agent dapp(dapp_cfg);
            dapp.set_indication_handler([&counters](const IndicationMessage&) {
                counters.ind_recv.fetch_add(1, std::memory_order_relaxed);
            });
            dapp.set_xapp_control_handler([&counters](const XAppControlAction&) {
                counters.ctrl_recv.fetch_add(1, std::memory_order_relaxed);
            });
            if (dapp.start() != ErrorCode::SUCCESS) {
                std::fprintf(stderr, "dApp start failed\n");
                return 1;
            }
            dapp.wait_for_setup(std::chrono::seconds(5));
            dapp.subscribe(/*ran_function_id=*/1, {}, {});
            std::this_thread::sleep_for(std::chrono::milliseconds(400));

            std::atomic<bool> stop{false};
            std::thread ctrl = spawn_driver("mix.ctrl", tier.ctrl_hz, stop, [&ran] {
                ran.send_xapp_control(1, 1, std::vector<uint8_t>(16, 0xC0), /*sequence_id=*/1);
            });
            std::thread rep = spawn_driver("mix.report", tier.rep_hz, stop, [&dapp] {
                dapp.send_report(1, std::vector<uint8_t>(32, 0xAB), /*sequence_id=*/1);
            });

            const uint64_t t0 = latrec_now_ns();
            std::this_thread::sleep_for(std::chrono::seconds(kSecondsPerTier));
            const uint64_t t1 = latrec_now_ns();
            windows.push_back({tier, t0, t1});

            stop.store(true);
            ctrl.join();
            rep.join();
            dapp.stop();
            ran.stop();
            // Sampled after stop() has joined every internal thread, so
            // nothing more will be processed and the counts are final.
            tier_counts.push_back({tier, counters.ind_recv.load(), counters.ctrl_recv.load(),
                                    counters.rep_recv.load()});
            const std::string rm = "rm -rf '" + ipc + "'";
            if (system(rm.c_str()) != 0) { /* best effort */ }
        }
    }

    bool ok = true;

    // Independent counters: always meaningful, whether or not latrec is
    // compiled in, enabled, or capturing -- this is the ablation's actual
    // comparison point between an untraced and a traced run.
    std::printf("Achieved throughput per flow, msg/s (independent counters; offered in brackets)\n");
    std::printf("| tier   | indications      | xApp controls   | dApp reports    |\n");
    std::printf("|--------|------------------|-----------------|-----------------|\n");
    for (const auto& tc : tier_counts) {
        const double s = static_cast<double>(kSecondsPerTier);
        char offered[24];
        if (tc.tier.ind_hz) std::snprintf(offered, sizeof(offered), "%llu",
                                          static_cast<unsigned long long>(tc.tier.ind_hz));
        else std::snprintf(offered, sizeof(offered), "max");
        std::printf("| %-6s | %7.0f [%6s] | %6.0f [%6llu] | %6.0f [%6llu] |\n",
                    tc.tier.name, static_cast<double>(tc.ind) / s, offered,
                    static_cast<double>(tc.ctrl) / s,
                    static_cast<unsigned long long>(tc.tier.ctrl_hz),
                    static_cast<double>(tc.rep) / s,
                    static_cast<unsigned long long>(tc.tier.rep_hz));
        // A flow with no records at all indicates a broken path.
        if (tc.ind == 0 || tc.ctrl == 0 || tc.rep == 0) {
            std::fprintf(stderr, "ERROR: tier '%s' lost a whole flow "
                                 "(ind=%llu ctrl=%llu rep=%llu)\n",
                         tc.tier.name, static_cast<unsigned long long>(tc.ind),
                         static_cast<unsigned long long>(tc.ctrl),
                         static_cast<unsigned long long>(tc.rep));
            ok = false;
        }
    }

    if (tracing) {
        bool wrapped = false;
        const std::vector<latrec_rec> recs = read_ring_dir(trace_dir, &wrapped);
        std::vector<Table> tables;

        std::printf("\nAchieved throughput per flow, msg/s (latrec-derived, L1_DEQUEUE)\n");
        std::printf("| tier   | indications      | xApp controls   | dApp reports    | drops |\n");
        std::printf("|--------|------------------|-----------------|-----------------|-------|\n");
        for (const auto& w : windows) {
            const Table t = index_window(recs, w);
            tables.push_back(t);
            const double s = static_cast<double>(w.t1 - w.t0) / 1e9;
            const size_t ind = count_type(t, LATREC_DEQUEUE, PduType::INDICATION_MESSAGE);
            const size_t ctl = count_type(t, LATREC_DEQUEUE, PduType::XAPP_CONTROL_ACTION);
            const size_t rep = count_type(t, LATREC_DEQUEUE, PduType::DAPP_REPORT);
            const size_t drops = count_stage(t, LATREC_DROP);
            char offered[24];
            if (w.tier.ind_hz) std::snprintf(offered, sizeof(offered), "%llu",
                                             static_cast<unsigned long long>(w.tier.ind_hz));
            else std::snprintf(offered, sizeof(offered), "max");
            std::printf("| %-6s | %7.0f [%6s] | %6.0f [%6llu] | %6.0f [%6llu] | %5zu |\n",
                        w.tier.name, static_cast<double>(ind) / s, offered,
                        static_cast<double>(ctl) / s,
                        static_cast<unsigned long long>(w.tier.ctrl_hz),
                        static_cast<double>(rep) / s,
                        static_cast<unsigned long long>(w.tier.rep_hz), drops);
        }

        std::printf("\nStage latency, us (p50 / p99 / max)\n");
        std::printf("| tier   | outbound dequeue..send | inbound recv..decoded "
                    "| report queue              |\n");
        std::printf("|--------|------------------------|-----------------------"
                    "|---------------------------|\n");
        for (size_t i = 0; i < windows.size(); i++) {
            const Stats ob = leg(tables[i], LATREC_DEQUEUE, LATREC_SEND_DONE);
            const Stats in = leg(tables[i], LATREC_RECV, LATREC_DECODE_E3AP_DONE);
            const Stats rq = leg(tables[i], LATREC_REPORT_QUEUED, LATREC_REPORT_DONE);
            std::printf("| %-6s | %7.1f %7.1f %6.1f | %6.1f %7.1f %6.1f "
                        "| %6.1f %7.1f %8.1f |\n",
                        windows[i].tier.name,
                        ob.p50, ob.p99, ob.max, in.p50, in.p99, in.max,
                        rq.p50, rq.p99, rq.max);
        }

        std::printf("\nDrops by reason\n");
        for (size_t i = 0; i < windows.size(); i++) {
            std::printf("  %-6s %s\n", windows[i].tier.name,
                        drop_breakdown(tables[i]).c_str());
        }

        std::printf("\n%s\n",
                    wrapped ? "NOTE: a ring wrapped; the busiest tier is sampled, not complete."
                            : "No ring wrapped: every stamp in every tier was captured.");
    } else {
        std::printf("\nBuilt with LIBE3_ENABLE_LATREC=OFF: no latrec-derived tables "
                    "this run (this is the ablation's untraced configuration).\n");
    }

    if (owns_dir && !trace_dir.empty()) {
        const std::string rm = "rm -rf '" + trace_dir + "'";
        if (system(rm.c_str()) != 0) { /* best effort */ }
    }

    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
