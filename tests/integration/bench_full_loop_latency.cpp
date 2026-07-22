/**
 * @file bench_full_loop_latency.cpp
 * @brief Full-loop control-loop latency benchmark for libe3.
 *
 * Drives N round-trip iterations of the Simple service-model control loop
 * through a colocated RAN + dApp E3Agent pair (over ZMQ IPC, single process)
 * and captures per-phase wall-clock timings. The result is a markdown table
 * suitable for posting as a PR comment, mirroring the existing MPMC queue
 * benchmark in tests/bench_mpmc_queue.cpp.
 *
 * Phases captured (all on the same monotonic clock):
 *   1. Collect indication data   — SM worker wake -> just before encode
 *   2. Create & encode indication — encode call -> bytes ready
 *   3. Deliver indication         — RAN connector send -> dApp handler entry
 *   4. Decode indication          — handler entry -> SM-specific decode done
 *   5. Process data               — decode done -> control encode start
 *   6. Create & encode control    — encode start -> bytes ready
 *   7. Deliver control            — dApp send -> RAN SM handler entry
 *   8. Decode & return control    — RAN SM handler entry -> handler return
 *
 * Phases 1, 2, 6 are local to one process side; phases 3, 4, 7, 8 cross the
 * IPC boundary but both sides run in the same OS process for this benchmark
 * so the steady_clock readings are coherent.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include "sm_simple/e3sm_simple_wrapper.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <mutex>
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

// One trace record per round-trip. Timestamps are in steady_clock nanoseconds.
struct Trace {
    uint32_t seq = 0;
    int64_t t1_collect_begin = 0;
    int64_t t2_encode_begin = 0;
    int64_t t3_send_indication = 0;
    int64_t t4_recv_indication = 0;
    int64_t t5_decode_done = 0;
    int64_t t6_control_encode_begin = 0;
    int64_t t7_send_control = 0;
    int64_t t8_recv_control = 0;
    int64_t t9_control_handler_done = 0;
    bool complete = false;
};

inline int64_t now_ns() {
    return duration_cast<nanoseconds>(clk::now().time_since_epoch()).count();
}

// Shared trace buffer between RAN-side SM and dApp-side handlers. The SM
// thread emits indications and the control_action handler is on the RAN
// inbound thread; the dApp's indication_handler is on the dApp inbound
// thread. All access is guarded by mu.
struct SharedTraces {
    std::mutex mu;
    std::vector<Trace> traces;
    Trace current;  // in-flight trace being filled
    int next_seq = 0;
    int completed = 0;
};

class BenchSM : public ServiceModel {
public:
    BenchSM(SharedTraces& s, EncodingFormat enc) : shared_(s), enc_(enc) {}
    std::string name() const override { return "BENCH"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return kRanFunctionId; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {kControlId}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { stop(); }
    bool is_running() const override { return running_; }

    std::vector<uint8_t> ran_function_data() const override {
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data("BENCH", out, enc_)) return out;
        return {0x01};
    }

    ErrorCode start() override {
        if (running_) return ErrorCode::SUCCESS;
        running_ = true;
        worker_ = std::thread([this]() {
            while (running_) {
                // Wait for the dApp's previous control to land before
                // emitting the next indication (back-to-back).
                {
                    std::unique_lock<std::mutex> lk(emit_mu_);
                    emit_cv_.wait_for(lk, std::chrono::milliseconds(50),
                                      [this]() { return !running_ || may_emit_; });
                    if (!running_) break;
                    may_emit_ = false;
                }
                auto subs = get_subscribers();
                if (subs.empty()) continue;

                // Phase 1: collect indication data
                int64_t t1 = now_ns();
                uint32_t seq;
                {
                    std::lock_guard<std::mutex> lk(shared_.mu);
                    shared_.current = Trace{};
                    seq = static_cast<uint32_t>(shared_.next_seq++);
                    shared_.current.seq = seq;
                    shared_.current.t1_collect_begin = t1;
                }
                libe3_examples::SimpleIndication si{seq, 0};

                // Phase 2: encode
                int64_t t2 = now_ns();
                std::vector<uint8_t> enc;
                if (!libe3_examples::encode_simple_indication(si, enc, enc_)) continue;
                int64_t t3 = now_ns();
                {
                    std::lock_guard<std::mutex> lk(shared_.mu);
                    shared_.current.t2_encode_begin = t2;
                    shared_.current.t3_send_indication = t3;
                }

                for (auto did : subs) {
                    Pdu pdu = make_indication_pdu(did, kRanFunctionId, enc);
                    (void)emit_outbound(std::move(pdu));
                }
            }
        });
        return ErrorCode::SUCCESS;
    }

    void stop() override {
        if (!running_) return;
        running_ = false;
        {
            std::lock_guard<std::mutex> lk(emit_mu_);
            may_emit_ = true;
        }
        emit_cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    // Allow the benchmark driver to gate emission to back-to-back.
    void allow_next_emit() {
        std::lock_guard<std::mutex> lk(emit_mu_);
        may_emit_ = true;
        emit_cv_.notify_one();
    }

    ErrorCode handle_control_action(uint32_t request_message_id,
                                    const DAppControlAction& a) override {
        int64_t t8 = now_ns();
        {
            std::lock_guard<std::mutex> lk(shared_.mu);
            shared_.current.t8_recv_control = t8;
        }
        int sampling = 0;
        (void)libe3_examples::decode_simple_control(a.action_data, sampling, enc_);
        int64_t t9 = now_ns();
        {
            std::lock_guard<std::mutex> lk(shared_.mu);
            shared_.current.t9_control_handler_done = t9;
            shared_.current.complete = true;
            shared_.traces.push_back(shared_.current);
            ++shared_.completed;
        }
        // Send ack and signal the worker to emit the next indication.
        Pdu ack = make_message_ack_pdu(request_message_id, ResponseCode::POSITIVE);
        ErrorCode rc = emit_outbound(std::move(ack));
        allow_next_emit();
        return rc;
    }

private:
    SharedTraces& shared_;
    EncodingFormat enc_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex emit_mu_;
    std::condition_variable emit_cv_;
    bool may_emit_{true};
};

std::string make_ipc_dir() {
    char tmpl[] = "/tmp/libe3_bench_full_loop_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
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

    SharedTraces shared;
    E3Agent ran(ran_cfg);
    auto* sm = new BenchSM(shared, encoding);
    if (ran.register_sm(std::unique_ptr<ServiceModel>(sm)) != ErrorCode::SUCCESS) {
        std::cerr << "register_sm failed\n";
        return 1;
    }
    if (ran.start() != ErrorCode::SUCCESS) {
        std::cerr << "ran start failed\n";
        return 1;
    }

    E3Agent dapp(dapp_cfg);
    dapp.set_indication_handler([&](const IndicationMessage& msg) {
        int64_t t4 = now_ns();
        libe3_examples::SimpleIndication si;
        if (!libe3_examples::decode_simple_indication(msg.protocol_data, si, encoding)) return;
        int64_t t5 = now_ns();

        int64_t t6 = now_ns();
        std::vector<uint8_t> ctrl;
        if (!libe3_examples::encode_simple_control(static_cast<int>(si.data1 % 101), ctrl, encoding)) return;
        int64_t t7 = now_ns();

        {
            std::lock_guard<std::mutex> lk(shared.mu);
            if (shared.current.seq == si.data1) {
                shared.current.t4_recv_indication = t4;
                shared.current.t5_decode_done = t5;
                shared.current.t6_control_encode_begin = t6;
                shared.current.t7_send_control = t7;
            }
        }
        (void)dapp.send_control(kRanFunctionId, kControlId, ctrl);
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
    // received control ack, so we just wait for `shared.completed` to reach
    // the target.
    const int total = kIterations + kWarmupIterations;
    auto deadline = clk::now() + std::chrono::seconds(60);
    while (true) {
        std::unique_lock<std::mutex> lk(shared.mu);
        if (shared.completed >= total) break;
        lk.unlock();
        if (clk::now() > deadline) {
            std::cerr << "bench deadline exceeded; completed="
                      << shared.completed << "/" << total << "\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    dapp.stop();
    ran.stop();
    if (!ipc_dir.empty()) {
        // Remove the IPC socket files created by the benchmark.
        for (const char* name : {"setup", "dapp_socket", "e3_socket"}) {
            std::string path = ipc_dir + "/" + name;
            ::unlink(path.c_str());
        }
        ::rmdir(ipc_dir.c_str());
    }

    // Compute per-phase deltas (fractional microseconds, dropping warmup).
    // Several phases are sub-microsecond; integer us would truncate them to 0.
    std::vector<double> p1, p2, p3, p4, p5, p6, p7, p8, total_us;
    {
        std::lock_guard<std::mutex> lk(shared.mu);
        for (const auto& t : shared.traces) {
            if (!t.complete) continue;
            if (t.seq < static_cast<uint32_t>(kWarmupIterations)) continue;
            auto us = [](int64_t a, int64_t b) {
                return static_cast<double>(b - a) / 1000.0;
            };
            p1.push_back(us(t.t1_collect_begin, t.t2_encode_begin));
            p2.push_back(us(t.t2_encode_begin, t.t3_send_indication));
            p3.push_back(us(t.t3_send_indication, t.t4_recv_indication));
            p4.push_back(us(t.t4_recv_indication, t.t5_decode_done));
            p5.push_back(us(t.t5_decode_done, t.t6_control_encode_begin));
            p6.push_back(us(t.t6_control_encode_begin, t.t7_send_control));
            p7.push_back(us(t.t7_send_control, t.t8_recv_control));
            p8.push_back(us(t.t8_recv_control, t.t9_control_handler_done));
            total_us.push_back(us(t.t1_collect_begin, t.t9_control_handler_done));
        }
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
    return 0;
}
