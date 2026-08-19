/**
 * @file bench_full_loop_latency.cpp
 * @brief Full-loop control-loop latency benchmark for libe3.
 *
 * Drives N round-trip iterations of the Simple service-model control loop
 * through a colocated RAN + dApp E3Agent pair (over ZMQ IPC, single process)
 * and reports per-phase timings read back from latrec, so this benchmark
 * measures libE3's and the benchDApp's actual instrumented internals rather
 * than a separate, ad hoc timing mechanism. The result is a markdown table
 * suitable for posting as a PR comment, mirroring the existing MPMC queue
 * benchmark in tests/bench_mpmc_queue.cpp. Requires a LIBE3_ENABLE_LATREC
 * build (see cmake/libe3Tests.cmake, which skips this target otherwise).
 *
 * This benchmark drives the shipped Simple SM (examples/sm_simple), not a
 * stripped-down copy, so the indication carries a live wall-clock timestamp
 * rather than a zero. That makes the encoded indication a few bytes larger and
 * adds a clock read to phase 1; the numbers here are therefore not comparable
 * to earlier runs that used the old inline benchmark SM and must be treated as
 * a fresh baseline.
 *
 * Phases captured (all read from latrec's CLOCK_MONOTONIC records):
 *   1. Collect indication data   — LATREC_EX0_COLLECT_BEGIN -> EX1_ENCODE_BEGIN
 *      (includes the SM's live-timestamp clock read; see note above)
 *   2. Create & encode indication — EX1_ENCODE_BEGIN -> EX2_SEND_INDICATION
 *   3. Deliver indication         — EX2_SEND_INDICATION -> LF0_FILTER_PASSED
 *   4. Decode indication          — LF0_FILTER_PASSED -> BD1_DECODED
 *   5. Process data               — BD1_DECODED -> BD2_CTRL_ENCODE_BEGIN
 *   6. Create & encode control    — BD2_CTRL_ENCODE_BEGIN -> BD3_CTRL_ENCODE_DONE
 *   7. Deliver control            — BD3_CTRL_ENCODE_DONE -> EX3_CTRL_RECV
 *   8. Decode & return control    — EX3_CTRL_RECV -> EX4_CTRL_DONE
 *
 * EX0..EX4 are the shipped reference SM's own stamps (examples/sm_simple);
 * BD1..BD3 are this benchmark's own dApp-side handler stamps, standing in for
 * a real dApp's decode/process/encode-control work. Both round-trip legs
 * cross libE3's own instrumented outbound/inbound chain (LE0, L0..L6) in
 * between EX2/LF0 and BD3/EX3, which this benchmark does not need to inspect
 * directly to reconstruct the 8 phases: PingPong pacing keeps exactly one
 * round trip in flight at a time, so EX0..EX4/BD1..BD3 (keyed on the SM's own
 * business seq) and LF0 (keyed on libE3's own inbound seq, with no shared key
 * across the wire) can be paired up by position -- the i-th LF0 chronologically
 * belongs to the i-th round trip -- rather than by bridging through libE3's
 * message_id, which wraps at 1000 well before this benchmark's iteration count.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/latrec.h>
#include "sm_simple/e3sm_simple_wrapper.hpp"
#include "sm_simple/simple_service_model.hpp"
#include "latrec_ring_reader.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace libe3;
using namespace std::chrono;
using clk = steady_clock;

namespace {

constexpr int kIterations = 1000;
constexpr int kWarmupIterations = 50;
constexpr uint32_t kRanFunctionId = 1;
constexpr uint32_t kControlId = 1;

// The 9 timestamps of one round trip, resolved from latrec records.
struct RoundTrip {
    uint64_t seq = 0;
    uint64_t t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0, t8 = 0, t9 = 0;
};

std::string make_ipc_dir() {
    char tmpl[] = "/tmp/libe3_bench_full_loop_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

std::string make_trace_dir() {
    char tmpl[] = "/tmp/libe3_bench_full_loop_trace_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    return std::string(d);
}

E3LinkLayer parse_link(const char* s) {
    if (std::strcmp(s, "posix") == 0) return E3LinkLayer::POSIX;
    if (std::strcmp(s, "zmq")   == 0) return E3LinkLayer::ZMQ;
    std::fprintf(stderr, "Unknown link layer '%s'; using zmq\n", s);
    return E3LinkLayer::ZMQ;
}

E3TransportLayer parse_transport(const char* s) {
    if (std::strcmp(s, "tcp")  == 0) return E3TransportLayer::TCP;
    if (std::strcmp(s, "sctp") == 0) return E3TransportLayer::SCTP;
    if (std::strcmp(s, "ipc")  == 0) return E3TransportLayer::IPC;
    std::fprintf(stderr, "Unknown transport '%s'; using ipc\n", s);
    return E3TransportLayer::IPC;
}

EncodingFormat parse_encoding(const char* s) {
    if (std::strcmp(s, "json")     == 0) return EncodingFormat::JSON;
    if (std::strcmp(s, "asn1")     == 0) return EncodingFormat::ASN1;
    if (std::strcmp(s, "protobuf") == 0) return EncodingFormat::PROTOBUF;
    std::fprintf(stderr, "Unknown encoding '%s'; using asn1\n", s);
    return EncodingFormat::ASN1;
}

const char* encoding_str(EncodingFormat e) {
    switch (e) {
        case EncodingFormat::JSON:     return "JSON";
        case EncodingFormat::ASN1:     return "ASN.1 APER";
        case EncodingFormat::PROTOBUF: return "Protocol Buffers";
        default:                       return "unknown";
    }
}

double percentile(std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(p * static_cast<double>(v.size() - 1));
    return v[idx];
}

double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (auto x : v) sum += x;
    return sum / static_cast<double>(v.size());
}

// Reconstructs every complete round trip from the rings under `dir`. See the
// file header comment for why LF0 is paired positionally rather than by key.
std::vector<RoundTrip> reconstruct(const std::string& dir) {
    bool wrapped = false;
    const std::vector<latrec_rec> recs = latrec_test::read_ring_dir(dir, &wrapped);
    if (wrapped) {
        std::fprintf(stderr,
                      "WARNING: a latrec ring wrapped; some records were lost\n");
    }

    std::map<uint64_t, std::map<uint8_t, uint64_t>> by_seq;  // seq -> stage -> t_ns
    std::vector<uint64_t> lf0_times;
    for (const auto& r : recs) {
        const uint8_t stage = latrec_test::stage_of(r);
        switch (stage) {
            case LATREC_EX0_COLLECT_BEGIN:
            case LATREC_EX1_ENCODE_BEGIN:
            case LATREC_EX2_SEND_INDICATION:
            case LATREC_EX3_CTRL_RECV:
            case LATREC_EX4_CTRL_DONE:
            case LATREC_BD1_DECODED:
            case LATREC_BD2_CTRL_ENCODE_BEGIN:
            case LATREC_BD3_CTRL_ENCODE_DONE:
                by_seq[latrec_test::seq_of(r)][stage] = r.t_ns;
                break;
            case LATREC_LF0_FILTER_PASSED:
                lf0_times.push_back(r.t_ns);
                break;
            default:
                break;
        }
    }
    std::sort(lf0_times.begin(), lf0_times.end());

    std::vector<RoundTrip> out;
    size_t lf0_idx = 0;
    // std::map iterates in ascending key order, i.e. ascending business seq,
    // i.e. emission order -- the same order PingPong pacing guarantees LF0
    // times were recorded in, since round trips never overlap.
    for (const auto& [seq, stages] : by_seq) {
        auto has = [&](uint8_t s) { return stages.count(s) != 0; };
        if (!(has(LATREC_EX0_COLLECT_BEGIN) && has(LATREC_EX1_ENCODE_BEGIN) &&
              has(LATREC_EX2_SEND_INDICATION) && has(LATREC_BD1_DECODED) &&
              has(LATREC_BD2_CTRL_ENCODE_BEGIN) && has(LATREC_BD3_CTRL_ENCODE_DONE) &&
              has(LATREC_EX3_CTRL_RECV) && has(LATREC_EX4_CTRL_DONE))) {
            continue;  // incomplete tail: the run stopped mid round-trip
        }
        if (lf0_idx >= lf0_times.size()) break;
        RoundTrip rt;
        rt.seq = seq;
        rt.t1 = stages.at(LATREC_EX0_COLLECT_BEGIN);
        rt.t2 = stages.at(LATREC_EX1_ENCODE_BEGIN);
        rt.t3 = stages.at(LATREC_EX2_SEND_INDICATION);
        rt.t4 = lf0_times[lf0_idx++];
        rt.t5 = stages.at(LATREC_BD1_DECODED);
        rt.t6 = stages.at(LATREC_BD2_CTRL_ENCODE_BEGIN);
        rt.t7 = stages.at(LATREC_BD3_CTRL_ENCODE_DONE);
        rt.t8 = stages.at(LATREC_EX3_CTRL_RECV);
        rt.t9 = stages.at(LATREC_EX4_CTRL_DONE);
        out.push_back(rt);
    }
    return out;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Defaults.
    E3LinkLayer     link     = E3LinkLayer::ZMQ;
    E3TransportLayer transport = E3TransportLayer::IPC;
    EncodingFormat  encoding = EncodingFormat::ASN1;

    static const struct option long_opts[] = {
        {"link",      required_argument, nullptr, 'l'},
        {"transport", required_argument, nullptr, 't'},
        {"encoding",  required_argument, nullptr, 'e'},
        {"help",      no_argument,       nullptr, 'h'},
        {nullptr,     0,                 nullptr,  0},
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "l:t:e:h", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'l': link      = parse_link(optarg);      break;
            case 't': transport = parse_transport(optarg); break;
            case 'e': encoding  = parse_encoding(optarg);  break;
            case 'h':
                std::printf("Usage: %s [--link zmq|posix] [--transport ipc|tcp|sctp]"
                            " [--encoding asn1|json|protobuf]\n", argv[0]);
                return 0;
            default:
                std::fprintf(stderr, "Unknown option; use --help\n");
                return 1;
        }
    }

    // Own capture directory: this benchmark's whole purpose is now to read
    // its own trace back, so it always self-configures LATREC_DIR (the
    // process's threads have not opened any ring yet, since nothing has
    // called any latrec_* function before this point). A small ring is
    // plenty: at most a few thousand records per thread for this iteration
    // count, against the default 4M-record capacity.
    const std::string trace_dir = make_trace_dir();
    setenv("LATREC_DIR", trace_dir.c_str(), 1);
    setenv("LATREC_ENTRIES_LOG2", "16", 1);

    E3Config ran_cfg;
    ran_cfg.role = E3Role::RAN;
    ran_cfg.ran_identifier = "bench-ran";
    ran_cfg.link_layer = link;
    ran_cfg.transport_layer = transport;
    ran_cfg.encoding = encoding;
    ran_cfg.log_level = 0;

    // IPC transport: use a private tmpdir so the benchmark is self-contained.
    // TCP/SCTP: both sides run in the same process on localhost; the default
    // ports (9990/9991/9999) are used.
    std::string ipc_dir;
    if (transport == E3TransportLayer::IPC) {
        ipc_dir = make_ipc_dir();
        ran_cfg.setup_endpoint      = "ipc://" + ipc_dir + "/setup";
        ran_cfg.subscriber_endpoint = "ipc://" + ipc_dir + "/dapp_socket";
        ran_cfg.publisher_endpoint  = "ipc://" + ipc_dir + "/e3_socket";
    }

    auto dapp_cfg = ran_cfg;
    dapp_cfg.role = E3Role::DAPP;
    dapp_cfg.dapp_name = "BenchDApp";
    // For IPC the dApp inherits the same explicit endpoints from ran_cfg.
    // For TCP/SCTP the dApp connects to localhost on the default ports,
    // which is correct since both sides run in the same process.

    E3Agent ran(ran_cfg);
    // Drive the shipped Simple SM in PingPong mode (one indication in flight,
    // next emit gated on the control round trip); its RAN-side stamps
    // (LATREC_EX0..EX4) are now built into the SM itself. period_us=0 makes
    // the SM quiet, so stdout carries only the markdown table this benchmark
    // prints.
    auto sm = std::make_unique<libe3_examples::SimpleServiceModel>(
        /*period_us=*/0, encoding,
        libe3_examples::SimpleServiceModel::PacingMode::PingPong);
    if (ran.register_sm(std::move(sm)) != ErrorCode::SUCCESS) {
        std::cerr << "register_sm failed\n";
        return 1;
    }
    if (ran.start() != ErrorCode::SUCCESS) {
        std::cerr << "ran start failed\n";
        return 1;
    }

    E3Agent dapp(dapp_cfg);
    std::atomic<int> handled{0};
    dapp.set_indication_handler([&](const IndicationMessage& msg) {
        libe3_examples::SimpleIndication si;
        if (!libe3_examples::decode_simple_indication(msg.protocol_data, si, encoding)) return;
        const uint32_t seq = si.data1;
        latrec_tstamp(seq, LATREC_BD1_DECODED, 0, 0);

        latrec_tstamp(seq, LATREC_BD2_CTRL_ENCODE_BEGIN, 0, 0);
        std::vector<uint8_t> ctrl;
        if (!libe3_examples::encode_simple_control(static_cast<int>(si.data1 % 101), ctrl, encoding)) return;
        latrec_tstamp(seq, LATREC_BD3_CTRL_ENCODE_DONE, 0, 0);

        // Bridges this handler's business seq to libe3's Pdu::enqueue_seq for
        // the control leg, the same way the SM does for the indication leg.
        latrec_ctx_set(seq);
        (void)dapp.send_control(kRanFunctionId, kControlId, ctrl);

        handled.fetch_add(1, std::memory_order_relaxed);
    });

    if (dapp.start() != ErrorCode::SUCCESS) {
        std::cerr << "dapp start failed\n";
        return 1;
    }
    if (dapp.wait_for_setup(std::chrono::milliseconds(5000)) != ErrorCode::SUCCESS) {
        std::cerr << "dapp setup failed\n";
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // PUB/SUB settle
    if (dapp.subscribe(kRanFunctionId, {1}, {kControlId}) != ErrorCode::SUCCESS) {
        std::cerr << "subscribe failed\n";
        return 1;
    }

    // Drive ITERATIONS round-trips. The SM is paced to emit one indication per
    // received control ack, so we just wait for the dApp to have handled the
    // target count, then stop immediately: nothing else gates the SM from
    // emitting further indications once the next control ack lands, so any
    // pause here just lets more round trips run rather than settling the
    // last one. handled increments once the dApp has decoded, encoded and
    // sent the control -- before that control's own EX3/EX4 are necessarily
    // stamped on the RAN side -- so the very last round trip's record may be
    // an incomplete tail; reconstruct() below drops it rather than treat it
    // as a real sample, which costs at most one sample out of the total.
    const int total = kIterations + kWarmupIterations;
    auto deadline = clk::now() + std::chrono::seconds(60);
    while (true) {
        if (handled.load(std::memory_order_relaxed) >= total) break;
        if (clk::now() > deadline) {
            std::cerr << "bench deadline exceeded; handled="
                      << handled.load() << "/" << total << "\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    dapp.stop();
    ran.stop();
    unsetenv("LATREC_DIR");
    unsetenv("LATREC_ENTRIES_LOG2");
    if (!ipc_dir.empty()) {
        // Remove the IPC socket files created by the benchmark.
        for (const char* name : {"setup", "dapp_socket", "e3_socket"}) {
            std::string path = ipc_dir + "/" + name;
            ::unlink(path.c_str());
        }
        ::rmdir(ipc_dir.c_str());
    }

    const std::vector<RoundTrip> round_trips = reconstruct(trace_dir);
    {
        const std::string rm = "rm -rf '" + trace_dir + "'";
        if (std::system(rm.c_str()) != 0) { /* best effort */ }
    }

    // Compute per-phase deltas (fractional microseconds, dropping warmup).
    // Several phases are sub-microsecond; integer us would truncate them to 0.
    std::vector<double> p1, p2, p3, p4, p5, p6, p7, p8, total_us;
    for (const auto& t : round_trips) {
        if (t.seq < static_cast<uint64_t>(kWarmupIterations)) continue;
        auto us = [](uint64_t a, uint64_t b) {
            return static_cast<double>(b - a) / 1000.0;
        };
        p1.push_back(us(t.t1, t.t2));
        p2.push_back(us(t.t2, t.t3));
        p3.push_back(us(t.t3, t.t4));
        p4.push_back(us(t.t4, t.t5));
        p5.push_back(us(t.t5, t.t6));
        p6.push_back(us(t.t6, t.t7));
        p7.push_back(us(t.t7, t.t8));
        p8.push_back(us(t.t8, t.t9));
        total_us.push_back(us(t.t1, t.t9));
    }

    // Emit markdown (fractional us, two decimals).
    auto emit_row = [](const char* label, std::vector<double>& v) {
        if (v.empty()) {
            std::printf("| %-35s |    -    |    -    |    -    |     -    |\n", label);
            return;
        }
        std::printf("| %-35s | %7.2f | %7.2f | %7.2f | %8.2f |\n",
                    label,
                    mean(v),
                    percentile(v, 0.50),
                    percentile(v, 0.99),
                    *std::max_element(v.begin(), v.end()));
    };

    std::printf("## Full-loop latency benchmark (N=%d after %d warmup)\n\n",
                static_cast<int>(p1.size()), kWarmupIterations);
    std::printf("All values in microseconds (us). Link: %s, transport: %s, encoding: %s.\n\n",
                link_layer_to_string(link), transport_layer_to_string(transport),
                encoding_str(encoding));
    std::printf("| Phase                               |  mean   |   p50   |   p99   |    max   |\n");
    std::printf("|-------------------------------------|--------:|--------:|--------:|---------:|\n");
    emit_row("1. Collect indication data",          p1);
    emit_row("2. Create & encode indication",       p2);
    emit_row("3. Deliver indication (RAN -> dApp)", p3);
    emit_row("4. Decode indication",                p4);
    emit_row("5. Process data",                     p5);
    emit_row("6. Create & encode control",          p6);
    emit_row("7. Deliver control (dApp -> RAN)",    p7);
    emit_row("8. Decode & handle control",          p8);
    emit_row("**Total round-trip**",                total_us);
    std::printf("\n");
    return p1.empty() ? 1 : 0;
}
