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
 * Phases captured (all read from latrec's CLOCK_MONOTONIC records), aligned
 * with docs/path-a-e3-loop.md's box numbering (A1..A18). "SM" is the shipped
 * reference Service Model's own worker ring (sm_simple.<tid>); "dApp" is this
 * benchmark's handler, which runs on libe3's inbound thread
 * (libe3.inbound.<tid>); "libe3" is the library's own delivery stamp.
 *    1. Collect indication data     (A1+A2)  RECORD_BEGIN -> ENCODE_E3SM_BEGIN
 *    2. Create & encode indication  (A3)     ENCODE_E3SM_BEGIN -> ENCODE_E3SM_DONE
 *    3. Encode E3AP (indication)    (A4)     EMIT_ENTER -> ENQUEUE, then
 *                                             DEQUEUE -> ENCODE_E3AP_DONE
 *    4. Queuing (indication)        (A5)     ENQUEUE -> DEQUEUE
 *    5. Delivery (indication)       (A6)     ENCODE_E3AP_DONE -> SEND_DONE
 *    6. E3 wire (RAN -> dApp)       (--)     SEND_DONE -> RECV
 *    7. Decode E3AP (indication)    (A8)     RECV -> DECODE_E3AP_DONE
 *    8. libe3 dispatch (indication) (--)     DECODE_E3AP_DONE -> DELIVER_BEGIN
 *    9. Decode indication           (A9)     DELIVER_BEGIN -> DECODE_E3SM_DONE
 *   10. Process data                (A10+A11) DECODE_E3SM_DONE -> ENCODE_E3SM_BEGIN
 *   11. Create & encode control     (A12)    ENCODE_E3SM_BEGIN -> ENCODE_E3SM_DONE
 *   12. Encode E3AP (control)       (A13)    EMIT_ENTER -> ENQUEUE, then
 *                                             DEQUEUE -> ENCODE_E3AP_DONE
 *   13. Queuing (control)           (A14)    ENQUEUE -> DEQUEUE
 *   14. Delivery (control)          (A15)    ENCODE_E3AP_DONE -> SEND_DONE
 *   15. E3 wire (dApp -> RAN)       (--)     SEND_DONE -> RECV
 *   16. Decode E3AP (control)       (A17)    RECV -> DECODE_E3AP_DONE
 *   17. libe3 dispatch (control)    (--)     DECODE_E3AP_DONE -> DECODE_E3SM_BEGIN
 *   18. Decode & handle control     (A18)    DECODE_E3SM_BEGIN -> DECODE_E3SM_DONE
 *
 * The Service Model's stamps are built into examples/sm_simple; the dApp-side
 * ones are this benchmark's own, standing in for a real dApp's
 * decode/process/encode-control work. Both use the same catalog identifiers,
 * because they are the same operations -- which side performed one is read off
 * the ring, not the stage id.
 *
 * Both round-trip legs cross libE3's own instrumented outbound/inbound chain
 * (EMIT_ENTER, ENQUEUE..DECODE_E3AP_DONE, DELIVER_BEGIN). Every one of those
 * stamps shares a single value drawn from latrec_seq_next(), a process-wide
 * monotonic counter -- verified at every call site in src/core/e3_interface.cpp
 * -- distinct from Pdu::message_id, which is ASN.1-range-limited to 1..1000
 * and does wrap within this benchmark's iteration count. That means every
 * outbound leg-instance's quintuple (EMIT_ENTER..SEND_DONE) and every inbound
 * leg-instance's group (RECV, DECODE_E3AP_DONE, and DELIVER_BEGIN where
 * present) can be grouped by this seq with no collision risk at this
 * benchmark's call volume.
 *
 * The outbound quintuple on both legs carries a direct, key-based bridge back
 * to the business seq used everywhere else in this file: EMIT_ENTER.aux is
 * the producer's own latrec_ctx() value at the moment it emitted, and both the
 * SM (for indications) and this benchmark's own dApp handler (for controls)
 * call latrec_ctx_set(seq) immediately beforehand. The inbound side has no
 * such bridge (RECV's seq is allocated fresh on receipt, before the payload is
 * even decoded), so PingPong pacing -- exactly one round trip in flight at a
 * time -- is relied on to pair completed inbound groups with round trips by
 * chronological position instead, the same technique this file has always
 * used for DELIVER_BEGIN.
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

// The 9 business-seq-keyed timestamps of one round trip (unchanged from
// before this rewrite), plus the 14 libE3-internal ones added by it: 7 per
// leg (5 outbound + 2 inbound), forward and return.
struct RoundTrip {
    uint64_t seq = 0;
    uint64_t t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0, t8 = 0, t9 = 0;

    // Forward leg (indication), outbound quintuple -- bridged by EMIT_ENTER.aux.
    uint64_t t_ind_emit_enter = 0, t_ind_enqueue = 0, t_ind_dequeue = 0,
             t_ind_encode_e3ap_done = 0, t_ind_send_done = 0;
    // Forward leg, inbound pair -- positionally paired alongside t4.
    uint64_t t_ind_recv = 0, t_ind_decode_e3ap_done = 0;

    // Return leg (control), outbound quintuple -- also bridged by EMIT_ENTER.aux.
    uint64_t t_ctrl_emit_enter = 0, t_ctrl_enqueue = 0, t_ctrl_dequeue = 0,
             t_ctrl_encode_e3ap_done = 0, t_ctrl_send_done = 0;
    // Return leg, inbound pair -- positionally paired.
    uint64_t t_ctrl_recv = 0, t_ctrl_decode_e3ap_done = 0;
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

// Reconstructs every complete round trip from the rings under `dir`.
//
// The stage catalog names operations, not components, so both ends of a round
// trip stamp the same ENCODE_E3SM / DECODE_E3SM identifiers against the same
// business seq. What separates them is aux2, which carries the PduType of the
// payload being coded -- exactly as the catalog documents. The RAN-side
// Service Model encodes an INDICATION_MESSAGE and decodes a
// DAPP_CONTROL_ACTION; the dApp-side handler does the mirror image. Ring names
// cannot do this job here: the SM's control handler and the dApp's indication
// handler both run on a libe3 inbound thread, so both write a "libe3.inbound"
// ring, differing only in tid.
//
// See the file header comment for the libE3-internal seq/bridging model.
std::vector<RoundTrip> reconstruct(const std::string& dir) {
    bool wrapped = false;
    const std::vector<latrec_rec> recs = latrec_test::read_ring_dir(dir, &wrapped);
    if (wrapped) {
        std::fprintf(stderr,
                      "WARNING: a latrec ring wrapped; some records were lost\n");
    }

    constexpr uint64_t kInd  = static_cast<uint64_t>(PduType::INDICATION_MESSAGE);
    constexpr uint64_t kCtrl = static_cast<uint64_t>(PduType::DAPP_CONTROL_ACTION);

    // (stage, aux2) -> t_ns, per business seq -- unchanged from before.
    std::map<uint64_t, std::map<std::pair<uint8_t, uint64_t>, uint64_t>> by_seq;

    // libE3-internal-seq -> stage -> full record. One shared seq per
    // leg-instance (see file header); storing the whole record, not just
    // t_ns, because EMIT_ENTER.aux is the business-seq bridge and
    // ENQUEUE/DECODE_E3AP_DONE.aux2 is the PduType classifier.
    std::map<uint64_t, std::map<uint8_t, latrec_rec>> by_libe3_seq;

    for (const auto& r : recs) {
        const uint8_t stage = latrec_test::stage_of(r);
        switch (stage) {
            case LATREC_RECORD_BEGIN:
            case LATREC_ENCODE_E3SM_BEGIN:
            case LATREC_ENCODE_E3SM_DONE:
            case LATREC_DECODE_E3SM_BEGIN:
            case LATREC_DECODE_E3SM_DONE:
                by_seq[latrec_test::seq_of(r)][{stage, r.aux2}] = r.t_ns;
                break;
            case LATREC_EMIT_ENTER:
            case LATREC_ENQUEUE:
            case LATREC_DEQUEUE:
            case LATREC_ENCODE_E3AP_DONE:
            case LATREC_SEND_DONE:
            case LATREC_RECV:
            case LATREC_DECODE_E3AP_DONE:
            case LATREC_DELIVER_BEGIN:
                by_libe3_seq[latrec_test::seq_of(r)][stage] = r;
                break;
            default:
                break;
        }
    }

    // Classify each libE3-internal group and dispatch it: outbound quintuples
    // (have EMIT_ENTER) bridge directly to a business seq via EMIT_ENTER.aux;
    // inbound groups (have RECV) have no such bridge and are collected for
    // positional pairing below, exactly as this file has always done for
    // DELIVER_BEGIN alone.
    struct Libe3Times {
        uint64_t emit_enter = 0, enqueue = 0, dequeue = 0,
                 encode_e3ap_done = 0, send_done = 0;
    };
    std::map<uint64_t, Libe3Times> ind_outbound;   // keyed by business seq
    std::map<uint64_t, Libe3Times> ctrl_outbound;  // keyed by business seq

    struct InboundTriple { uint64_t recv = 0, decode_e3ap_done = 0, deliver_begin = 0; };
    struct InboundPair   { uint64_t recv = 0, decode_e3ap_done = 0; };
    std::vector<InboundTriple> ind_inbound;
    std::vector<InboundPair> ctrl_inbound;

    for (const auto& [seq, stamps] : by_libe3_seq) {
        auto get = [&](uint8_t stage) -> const latrec_rec* {
            auto it = stamps.find(stage);
            return it == stamps.end() ? nullptr : &it->second;
        };
        if (const latrec_rec* emit = get(LATREC_EMIT_ENTER)) {
            const latrec_rec* enq = get(LATREC_ENQUEUE);
            const latrec_rec* deq = get(LATREC_DEQUEUE);
            const latrec_rec* enc = get(LATREC_ENCODE_E3AP_DONE);
            const latrec_rec* snd = get(LATREC_SEND_DONE);
            if (!(enq && deq && enc && snd)) continue;  // incomplete tail
            const Libe3Times t{emit->t_ns, enq->t_ns, deq->t_ns, enc->t_ns, snd->t_ns};
            const uint64_t business_seq = emit->aux;
            if (enq->aux2 == kInd)       ind_outbound[business_seq] = t;
            else if (enq->aux2 == kCtrl) ctrl_outbound[business_seq] = t;
            // else: an ack or other non-round-trip emission; ignore.
            continue;
        }
        if (const latrec_rec* recv = get(LATREC_RECV)) {
            const latrec_rec* dec = get(LATREC_DECODE_E3AP_DONE);
            if (!dec) continue;  // incomplete tail
            if (dec->aux2 == kInd) {
                const latrec_rec* dlv = get(LATREC_DELIVER_BEGIN);
                if (!dlv) continue;  // incomplete tail
                ind_inbound.push_back({recv->t_ns, dec->t_ns, dlv->t_ns});
            } else if (dec->aux2 == kCtrl) {
                ctrl_inbound.push_back({recv->t_ns, dec->t_ns});
            }
            // else: a subscription response / ack / setup reply; ignore.
        }
    }
    // std::map iterates in ascending key order above, i.e. ascending
    // libE3-internal seq -- but that is not chronological order across
    // different leg-instances, so both inbound vectors are sorted explicitly
    // before positional pairing, same as this file has always done for
    // DELIVER_BEGIN alone.
    std::sort(ind_inbound.begin(), ind_inbound.end(),
              [](const InboundTriple& a, const InboundTriple& b) {
                  return a.deliver_begin < b.deliver_begin;
              });
    std::sort(ctrl_inbound.begin(), ctrl_inbound.end(),
              [](const InboundPair& a, const InboundPair& b) {
                  return a.decode_e3ap_done < b.decode_e3ap_done;
              });

    std::vector<RoundTrip> out;
    size_t skipped_misaligned = 0;
    size_t ind_inbound_idx = 0, ctrl_inbound_idx = 0;
    // std::map iterates in ascending key order, i.e. ascending business seq,
    // i.e. emission order -- the same order PingPong pacing guarantees the
    // inbound groups were recorded in, since round trips never overlap.
    for (const auto& [seq, st] : by_seq) {
        auto at = [&](uint8_t stage, uint64_t pdu) -> const uint64_t* {
            auto it = st.find({stage, pdu});
            return it == st.end() ? nullptr : &it->second;
        };
        const uint64_t* t1 = at(LATREC_RECORD_BEGIN, 0);
        const uint64_t* t2 = at(LATREC_ENCODE_E3SM_BEGIN, kInd);
        const uint64_t* t3 = at(LATREC_ENCODE_E3SM_DONE, kInd);
        const uint64_t* t5 = at(LATREC_DECODE_E3SM_DONE, kInd);
        const uint64_t* t6 = at(LATREC_ENCODE_E3SM_BEGIN, kCtrl);
        const uint64_t* t7 = at(LATREC_ENCODE_E3SM_DONE, kCtrl);
        const uint64_t* t8 = at(LATREC_DECODE_E3SM_BEGIN, kCtrl);
        const uint64_t* t9 = at(LATREC_DECODE_E3SM_DONE, kCtrl);
        if (!(t1 && t2 && t3 && t5 && t6 && t7 && t8 && t9)) {
            continue;  // incomplete tail: the run stopped mid round-trip
        }
        auto ind_out_it = ind_outbound.find(seq);
        auto ctrl_out_it = ctrl_outbound.find(seq);
        if (ind_out_it == ind_outbound.end() || ctrl_out_it == ctrl_outbound.end()) {
            continue;  // this round trip's libE3-internal chain didn't fully land
        }
        if (ind_inbound_idx >= ind_inbound.size()) break;
        if (ctrl_inbound_idx >= ctrl_inbound.size()) break;

        const Libe3Times& io = ind_out_it->second;
        const Libe3Times& co = ctrl_out_it->second;
        const InboundTriple& ii = ind_inbound[ind_inbound_idx++];
        const InboundPair& ci = ctrl_inbound[ctrl_inbound_idx++];

        // ind_outbound/ctrl_outbound are joined to this round trip by exact
        // key (EMIT_ENTER.aux == business seq); ii/ci are joined by
        // chronological position instead, since the inbound side has no such
        // key (see file header). Position-based pairing silently drifts by
        // one for every round trip whose own inbound triple/pair didn't fully
        // land (most commonly right at shutdown, when the SM can emit one
        // more indication than the harness waits for a control reply to).
        // Once drifted, ii/ci belong to some *other* round trip, not this
        // one -- checking that the two joins agree on basic time ordering
        // catches that and drops just the affected round trip, the same
        // tolerance this file already extends to an incomplete tail.
        const bool ind_ok = io.send_done <= ii.recv && ii.deliver_begin <= *t5;
        const bool ctrl_ok = co.send_done <= ci.recv && ci.decode_e3ap_done <= *t8;
        if (!ind_ok || !ctrl_ok) {
            ++skipped_misaligned;
            continue;
        }

        RoundTrip rt;
        rt.seq = seq;
        rt.t1 = *t1;
        rt.t2 = *t2;
        rt.t3 = *t3;
        rt.t5 = *t5;
        rt.t6 = *t6;
        rt.t7 = *t7;
        rt.t8 = *t8;
        rt.t9 = *t9;

        rt.t_ind_emit_enter = io.emit_enter;
        rt.t_ind_enqueue = io.enqueue;
        rt.t_ind_dequeue = io.dequeue;
        rt.t_ind_encode_e3ap_done = io.encode_e3ap_done;
        rt.t_ind_send_done = io.send_done;
        rt.t4 = ii.deliver_begin;
        rt.t_ind_recv = ii.recv;
        rt.t_ind_decode_e3ap_done = ii.decode_e3ap_done;

        rt.t_ctrl_emit_enter = co.emit_enter;
        rt.t_ctrl_enqueue = co.enqueue;
        rt.t_ctrl_dequeue = co.dequeue;
        rt.t_ctrl_encode_e3ap_done = co.encode_e3ap_done;
        rt.t_ctrl_send_done = co.send_done;
        rt.t_ctrl_recv = ci.recv;
        rt.t_ctrl_decode_e3ap_done = ci.decode_e3ap_done;

        out.push_back(rt);
    }
    if (skipped_misaligned > 0) {
        std::fprintf(stderr,
                      "WARNING: dropped %zu round trip(s) with a misaligned "
                      "inbound position (see reconstruct()'s comment)\n",
                      skipped_misaligned);
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

    // Own capture directory: this benchmark's whole purpose is to read its own
    // trace back, and a shared default directory would mix it with any other
    // run's rings. Safe to set here because no thread has opened a ring yet --
    // nothing has called any latrec_* function before this point. A small ring
    // is plenty: at most a few thousand records per thread for this iteration
    // count, against the default 4M-record capacity.
    const std::string trace_dir = make_trace_dir();
    latrec_set_output_dir(trace_dir.c_str());
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
    // next emit gated on the control round trip); its RAN-side stamps are
    // built into the SM itself. period_us=0 makes
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
        latrec_tstamp(seq, LATREC_DECODE_E3SM_DONE, 0,
                      static_cast<uint64_t>(PduType::INDICATION_MESSAGE));

        latrec_tstamp(seq, LATREC_ENCODE_E3SM_BEGIN, 0,
                      static_cast<uint64_t>(PduType::DAPP_CONTROL_ACTION));
        std::vector<uint8_t> ctrl;
        if (!libe3_examples::encode_simple_control(static_cast<int>(si.data1 % 101), ctrl, encoding)) return;
        latrec_tstamp(seq, LATREC_ENCODE_E3SM_DONE, 0,
                      static_cast<uint64_t>(PduType::DAPP_CONTROL_ACTION));

        // Bridges this handler's business seq to libe3's Pdu::enqueue_seq for
        // the control leg, the same way the SM does for the indication leg:
        // queue_dapp_control_action now stamps EMIT_ENTER with aux =
        // latrec_ctx(), so this is a real join, not a no-op.
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
    latrec_set_output_dir(nullptr);
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
    std::vector<double> p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13,
        p14, p15, p16, p17, p18, total_us;
    for (const auto& t : round_trips) {
        if (t.seq < static_cast<uint64_t>(kWarmupIterations)) continue;
        auto us = [](uint64_t a, uint64_t b) {
            return static_cast<double>(b - a) / 1000.0;
        };
        p1.push_back(us(t.t1, t.t2));
        p2.push_back(us(t.t2, t.t3));
        p3.push_back(us(t.t_ind_emit_enter, t.t_ind_enqueue) +
                     us(t.t_ind_dequeue, t.t_ind_encode_e3ap_done));
        p4.push_back(us(t.t_ind_enqueue, t.t_ind_dequeue));
        p5.push_back(us(t.t_ind_encode_e3ap_done, t.t_ind_send_done));
        p6.push_back(us(t.t_ind_send_done, t.t_ind_recv));
        p7.push_back(us(t.t_ind_recv, t.t_ind_decode_e3ap_done));
        p8.push_back(us(t.t_ind_decode_e3ap_done, t.t4));
        p9.push_back(us(t.t4, t.t5));
        p10.push_back(us(t.t5, t.t6));
        p11.push_back(us(t.t6, t.t7));
        p12.push_back(us(t.t_ctrl_emit_enter, t.t_ctrl_enqueue) +
                      us(t.t_ctrl_dequeue, t.t_ctrl_encode_e3ap_done));
        p13.push_back(us(t.t_ctrl_enqueue, t.t_ctrl_dequeue));
        p14.push_back(us(t.t_ctrl_encode_e3ap_done, t.t_ctrl_send_done));
        p15.push_back(us(t.t_ctrl_send_done, t.t_ctrl_recv));
        p16.push_back(us(t.t_ctrl_recv, t.t_ctrl_decode_e3ap_done));
        p17.push_back(us(t.t_ctrl_decode_e3ap_done, t.t8));
        p18.push_back(us(t.t8, t.t9));
        total_us.push_back(us(t.t1, t.t9));
    }

    // Emit markdown (fractional us, two decimals).
    auto emit_row = [](const char* row_num, const char* desc, const char* tag,
                        std::vector<double>& v) {
        if (v.empty()) {
            std::printf("| %5s | %-30s | %-68s |    -    |    -    |    -    |    -    |\n",
                        row_num, desc, tag);
            return;
        }
        std::printf("| %5s | %-30s | %-68s | %7.2f | %7.2f | %7.2f | %7.2f |\n",
                    row_num, desc, tag,
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
    std::printf("| %5s | %-30s | %-68s | %7s | %7s | %7s | %7s |\n",
                "#", "Description", "Tags", "mean", "p50", "p99", "max");
    std::printf("|-------|--------------------------------|"
                "----------------------------------------------------------------------|"
                "--------:|--------:|--------:|--------:|\n");
    emit_row("1",  "Collect indication data",
             "`RECORD_BEGIN` to `ENCODE_E3SM_BEGIN`", p1);
    emit_row("2",  "Create & encode indication",
             "`ENCODE_E3SM_BEGIN` to `ENCODE_E3SM_DONE`", p2);
    emit_row("3",  "Encode E3AP (indication)",
             "`EMIT_ENTER` to `ENQUEUE`, then `DEQUEUE` to `ENCODE_E3AP_DONE`", p3);
    emit_row("4",  "Queuing (indication)",
             "`ENQUEUE` to `DEQUEUE`", p4);
    emit_row("5",  "Delivery (indication)",
             "`ENCODE_E3AP_DONE` to `SEND_DONE`", p5);
    emit_row("6",  "E3 wire (RAN -> dApp)",
             "`SEND_DONE` to `RECV`", p6);
    emit_row("7",  "Decode E3AP (indication)",
             "`RECV` to `DECODE_E3AP_DONE`", p7);
    emit_row("8",  "libe3 dispatch (indication)",
             "`DECODE_E3AP_DONE` to `DELIVER_BEGIN`", p8);
    emit_row("9",  "Decode indication",
             "`DELIVER_BEGIN` to `DECODE_E3SM_DONE`", p9);
    emit_row("10", "Process data",
             "`DECODE_E3SM_DONE` to `ENCODE_E3SM_BEGIN`", p10);
    emit_row("11", "Create & encode control",
             "`ENCODE_E3SM_BEGIN` to `ENCODE_E3SM_DONE`", p11);
    emit_row("12", "Encode E3AP (control)",
             "`EMIT_ENTER` to `ENQUEUE`, then `DEQUEUE` to `ENCODE_E3AP_DONE`", p12);
    emit_row("13", "Queuing (control)",
             "`ENQUEUE` to `DEQUEUE`", p13);
    emit_row("14", "Delivery (control)",
             "`ENCODE_E3AP_DONE` to `SEND_DONE`", p14);
    emit_row("15", "E3 wire (dApp -> RAN)",
             "`SEND_DONE` to `RECV`", p15);
    emit_row("16", "Decode E3AP (control)",
             "`RECV` to `DECODE_E3AP_DONE`", p16);
    emit_row("17", "libe3 dispatch (control)",
             "`DECODE_E3AP_DONE` to `DECODE_E3SM_BEGIN`", p17);
    emit_row("18", "Decode & handle control",
             "`DECODE_E3SM_BEGIN` to `DECODE_E3SM_DONE`", p18);
    emit_row("Total", "**Total round-trip**", "", total_us);
    std::printf("\n");
    return p1.empty() ? 1 : 0;
}
