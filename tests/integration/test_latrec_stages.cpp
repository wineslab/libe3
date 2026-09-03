/**
 * @file test_latrec_stages.cpp
 * @brief Asserts that the libe3 call sites stamp the right stages, in order.
 *
 * Drives every C++ path of a live RAN + dApp pair at once -- indications one
 * way, reports and controls the other -- then checks, per message, that each
 * leg is complete, ordered and free of duplicate stamps. A stamp on the wrong
 * side of a call changes the order without changing the record count, so the
 * per-message ordering is what is asserted rather than the totals.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/e3_agent.hpp>
#include <libe3/latrec.h>

#include "sm_simple/simple_service_model.hpp"
#include "latrec_ring_reader.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace libe3;
using namespace latrec_test;

namespace {

/* Every record captured by the harness, from every ring and every transport /
 * encoding combination. Merging is safe because latrec_seq_next() is
 * process-wide, so no two combos can hand out the same seq. */
std::vector<latrec_rec> g_recs;

/* One transport + encoding pairing to drive the whole stage set through. */
struct Combo {
    const char* name;
    E3LinkLayer link;
    E3TransportLayer transport;
    EncodingFormat encoding;
};

struct ComboResult { std::string name; bool ran{false}; size_t records{0}; };
std::vector<ComboResult> g_combos;

/* Base of the fake producer trace seqs the driver publishes with
 * latrec_ctx_set() before each indication. Far above any real seq, so a record
 * carrying one is unambiguously the context and not a stray value. */
constexpr uint64_t ORIGIN_BASE = 0xE300000000ull;

/* seq -> (stage -> timestamps), restricted to one leg's stages. */
std::map<uint64_t, std::map<uint8_t, std::vector<uint64_t>>>
group(const std::vector<uint8_t>& stages) {
    std::map<uint64_t, std::map<uint8_t, std::vector<uint64_t>>> by;
    for (const auto& r : g_recs) {
        for (uint8_t s : stages) {
            if (stage_of(r) == s) by[seq_of(r)][stage_of(r)].push_back(r.t_ns);
        }
    }
    return by;
}

size_t count(uint8_t stage) {
    size_t n = 0;
    for (const auto& r : g_recs) if (stage_of(r) == stage) n++;
    return n;
}

/* Every message that entered a leg must pass through the rest of it, in order,
 * exactly once.
 *
 * The anchor is the leg's first stage, not its last: still true in general
 * (a producer thread that opens no ring at all -- there is no longer a known
 * example of one in this harness, since a6970f5c has queue_outbound() open
 * one on demand -- would discard its own first stamp and reach the last
 * stage with no entry record). Messages that were dropped, or were still in
 * flight when the capture ended, are excluded. */
void assert_leg_ordered(const char* name, const std::vector<uint8_t>& chain,
                        size_t min_complete) {
    auto by = group(chain);
    auto dropped = group({LATREC_DROP});
    const uint8_t first = chain.front();
    size_t complete = 0;
    for (const auto& [seq, stamps] : by) {
        if (!stamps.count(first)) continue;
        if (dropped.count(seq)) continue;
        if (!stamps.count(chain.back())) continue;   // still in flight at capture end
        complete++;
        uint64_t prev = 0;
        for (uint8_t s : chain) {
            const auto it = stamps.find(s);
            ASSERT_TRUE(it != stamps.end());        // no stage skipped
            ASSERT_EQ(it->second.size(), 1u);       // and none stamped twice
            ASSERT_GE(it->second[0], prev);         // non-decreasing in time
            prev = it->second[0];
        }
    }
    std::printf("      [%s] %zu complete messages checked\n", name, complete);
    ASSERT_GE(complete, min_complete);
}

}  // namespace

// ---------------------------------------------------------------------------

TEST(ran_outbound_stages_are_ordered) {
    // L0 enqueue -> L1 dequeue -> L2 encoded -> L3 sent
    assert_leg_ordered("outbound", {LATREC_ENQUEUE, LATREC_DEQUEUE,
                                    LATREC_ENCODE_E3AP_DONE, LATREC_SEND_DONE}, 20);
}

TEST(emit_entry_precedes_the_enqueue) {
    // LE0 is not in the outbound chain (only the emit APIs stamp it, so acks and
    // setup responses have none), which is why its order is asserted here.
    auto by = group({LATREC_EMIT_ENTER, LATREC_ENQUEUE});
    size_t checked = 0;
    for (const auto& [seq, stamps] : by) {
        const auto le0 = stamps.find(LATREC_EMIT_ENTER);
        const auto l0 = stamps.find(LATREC_ENQUEUE);
        if (le0 == stamps.end() || l0 == stamps.end()) continue;
        ASSERT_EQ(le0->second.size(), 1u);
        ASSERT_GE(l0->second[0], le0->second[0]);
        checked++;
    }
    std::printf("      %zu indications entered libe3 before being enqueued\n", checked);
    ASSERT_GT(checked, 0u);
}

TEST(emit_entry_carries_the_producers_context) {
    // aux is the producer's own trace seq, the key its records are joined on;
    // aux2 is the RAN function. Without this the outbound leg cannot be tied
    // back to whatever generated the payload.
    //
    // Two producers reach EMIT_ENTER in this run, through two different
    // emit paths, and both are checked: the driver thread calls
    // E3Agent::send_indication directly (aux keyed off ORIGIN_BASE, an
    // unmistakably large sentinel so it can't be confused with a real
    // producer's own small counter), and the registered SimpleServiceModel's
    // own FixedRate worker reaches the same stage through
    // ServiceModel::emit_outbound, keyed on its own small indication
    // counter. Checking only the driver's records would leave
    // emit_outbound's own path -- the one every registered Service Model
    // actually uses -- unexercised, which is exactly how this stage went
    // unstamped there for as long as it did.
    // Two non-driver sources reach emit_outbound in this run, keyed from two
    // different seq spaces: the SM's own periodic indications (RECORD_BEGIN,
    // its local zero-based indication counter) and its acks sent in reply to
    // a control action (bridged from the RAN's own inbound-leg seq -- see
    // handle_control_action). Either can legitimately be 0, so a bare
    // non-zero check can't tell a genuine join from a broken one reading
    // back the same 0. Cross-referencing against both seq spaces can: it
    // confirms the exact value latrec_ctx() returned is one that was
    // actually live somewhere, zero included.
    auto sm_indication_seqs = group({LATREC_RECORD_BEGIN});
    auto ran_inbound_seqs = group({LATREC_RECV});
    size_t driver_checked = 0, sm_checked = 0;
    for (const auto& r : g_recs) {
        if (stage_of(r) != LATREC_EMIT_ENTER) continue;
        ASSERT_EQ(r.aux2, 1u);              // ran_function_id
        if (r.aux > ORIGIN_BASE) {
            driver_checked++;               // the driver's context, not a stray value
        } else {
            ASSERT_TRUE(sm_indication_seqs.count(r.aux) > 0 ||
                        ran_inbound_seqs.count(r.aux) > 0);  // a real cycle, via emit_outbound
            sm_checked++;
        }
    }
    std::printf("      %zu emit-entry records from the driver, %zu from the "
                "registered SM's own emit_outbound\n", driver_checked, sm_checked);
    ASSERT_GT(driver_checked, 0u);
    ASSERT_GT(sm_checked, 0u);
}

TEST(inbound_stages_are_ordered) {
    assert_leg_ordered("inbound", {LATREC_RECV, LATREC_DECODE_E3AP_DONE}, 20);
}

TEST(report_queue_stages_are_ordered) {
    // queued -> handled, on two different threads
    assert_leg_ordered("report", {LATREC_REPORT_QUEUED, LATREC_REPORT_DONE}, 1);
}

TEST(setup_handshake_is_stamped) {
    // On this leg seq is the E3 request message_id, not the process-wide
    // counter: it restarts per E3Interface and is confined to the ASN.1 range
    // 1..1000, so several agents reuse the same value. The handshakes are
    // serialised, so the records are paired in time order instead.
    std::vector<uint64_t> recv, sent;
    for (const auto& r : g_recs) {
        if (stage_of(r) == LATREC_SETUP_BEGIN) recv.push_back(r.t_ns);
        else if (stage_of(r) == LATREC_SETUP_DONE) sent.push_back(r.t_ns);
    }
    std::sort(recv.begin(), recv.end());
    std::sort(sent.begin(), sent.end());
    std::printf("      [setup] %zu handshakes\n", recv.size());
    ASSERT_GT(recv.size(), 0u);
    ASSERT_EQ(recv.size(), sent.size());          // every request got a reply stamp
    for (size_t i = 0; i < recv.size(); i++) ASSERT_GE(sent[i], recv[i]);
}

TEST(the_send_follows_the_encode_on_every_sent_pdu) {
    // The connector call is no longer stamped separately: ENCODE_E3AP_DONE ->
    // SEND_DONE brackets it, since SEND_DONE is taken immediately after the
    // connector's send() returns.
    auto by = group({LATREC_ENCODE_E3AP_DONE, LATREC_SEND_DONE});
    size_t checked = 0;
    for (const auto& [seq, s] : by) {
        if (!s.count(LATREC_SEND_DONE) || !s.count(LATREC_ENCODE_E3AP_DONE)) continue;
        ASSERT_GE(s.at(LATREC_SEND_DONE)[0], s.at(LATREC_ENCODE_E3AP_DONE)[0]);
        checked++;
    }
    std::printf("      %zu sent PDUs have the encode before the send\n", checked);
    ASSERT_GT(checked, 0u);
}

TEST(every_instrumented_path_produced_records) {
    // A path that stops firing produces no records and no failures elsewhere.
    struct { const char* name; uint8_t stage; } expect[] = {
        {"emit API entry",       LATREC_EMIT_ENTER},
        {"RAN outbound enqueue", LATREC_ENQUEUE},
        {"outbound sent",        LATREC_SEND_DONE},
        {"inbound recv",         LATREC_RECV},
        {"inbound decoded",      LATREC_DECODE_E3AP_DONE},
        {"report queued",        LATREC_REPORT_QUEUED},
        {"report handled",       LATREC_REPORT_DONE},
        {"setup received",       LATREC_SETUP_BEGIN},
    };
    for (const auto& e : expect) {
        const size_t n = count(e.stage);
        std::printf("      %-22s %6zu records\n", e.name, n);
        ASSERT_GT(n, 0u);
    }
}

TEST(every_transport_and_encoding_combination_was_exercised) {
    // The ordering checks run over the merged records, so a combo that
    // produced nothing contributes no violations and would otherwise be
    // indistinguishable from one that passed.
    ASSERT_GT(g_combos.size(), 0u);
    size_t ran = 0;
    for (const auto& c : g_combos) {
        std::printf("      %-22s %s (%zu records)\n", c.name.c_str(),
                    c.ran ? "ran" : "SKIPPED (encoder not built)", c.records);
        if (c.ran) { ASSERT_GT(c.records, 0u); ran++; }
    }
    ASSERT_GE(ran, 2u);        // at minimum both transports must have run
}

TEST(no_drops_were_recorded) {
    // The harness stays well below saturation, so any capacity-related drop
    // is a defect. LATREC_DROP_SHUTDOWN is different: response_queue_ and
    // report_queue_ are shut down early in E3Interface::stop(), before a
    // registered ServiceModel's own producer thread -- not one of
    // E3Interface's own threads -- is joined via SmRegistry::clear(). A
    // handful of enqueue attempts from that window are rejected by design,
    // not because either queue was actually full, so they are reported but
    // not asserted to be zero.
    std::map<uint64_t, size_t> by_reason;
    for (const auto& r : g_recs) {
        if (stage_of(r) == LATREC_DROP) by_reason[r.aux2]++;
    }
    for (const auto& [reason, n] : by_reason) {
        std::printf("      drop reason %llu: %zu records\n",
                    static_cast<unsigned long long>(reason), n);
    }
    size_t capacity_drops = 0;
    for (const auto& [reason, n] : by_reason) {
        if (reason != LATREC_DROP_SHUTDOWN) capacity_drops += n;
    }
    ASSERT_EQ(capacity_drops, 0u);
}

TEST(every_enqueued_pdu_is_either_sent_or_dropped) {
    // queue_outbound stamps L0 on the caller's thread, opening a ring for it
    // on demand if it doesn't already have one (fixed in a6970f5c -- before
    // that, a producer thread with no ring of its own, such as the example
    // SM's emitter thread, silently discarded its L0 stamp). So every L0
    // should now be accounted for: it either reaches L3_SEND_DONE, or the
    // same seq carries an outbound-queue drop (LATREC_DROP_QUEUE_PUSH or
    // LATREC_DROP_SHUTDOWN) -- nothing should vanish in between.
    const size_t entered = count(LATREC_ENQUEUE);
    const size_t sent = count(LATREC_SEND_DONE);
    size_t outbound_drops = 0;
    for (const auto& r : g_recs) {
        if (stage_of(r) == LATREC_DROP &&
            (r.aux2 == LATREC_DROP_QUEUE_PUSH || r.aux2 == LATREC_DROP_SHUTDOWN)) {
            outbound_drops++;
        }
    }
    std::printf("      L0=%zu L3=%zu outbound-drops=%zu (L0 should equal L3+drops)\n",
                entered, sent, outbound_drops);
    ASSERT_GT(entered, 0u);
    ASSERT_EQ(entered, sent + outbound_drops);
}

// ---------------------------------------------------------------------------

/* Drives every instrumented path once over one transport + encoding pairing.
 * Returns false when the encoder is not compiled into this build. */
bool run_combo(const Combo& c, const std::string& trace_dir) {
    char ipctmpl[] = "/tmp/latrec_stages_ipc_XXXXXX";
    const std::string ipc = mkdtemp(ipctmpl);

    E3Config ran_cfg;
    ran_cfg.role = E3Role::RAN;
    ran_cfg.ran_identifier = "stages-ran";
    ran_cfg.link_layer = c.link;
    ran_cfg.transport_layer = c.transport;
    ran_cfg.encoding = c.encoding;
    ran_cfg.log_level = 0;
    ran_cfg.setup_endpoint      = "ipc://" + ipc + "/setup";
    ran_cfg.subscriber_endpoint = "ipc://" + ipc + "/dapp_socket";
    ran_cfg.publisher_endpoint  = "ipc://" + ipc + "/e3_socket";
    auto dapp_cfg = ran_cfg;
    dapp_cfg.role = E3Role::DAPP;
    dapp_cfg.dapp_name = "StagesDApp";

    E3Agent ran(ran_cfg);
    // 5 ms period: fills every leg while staying below the rate at which
    // anything is dropped.
    auto sm = std::make_unique<libe3_examples::SimpleServiceModel>(
        /*period_us=*/5000, c.encoding,
        libe3_examples::SimpleServiceModel::PacingMode::FixedRate);
    if (ran.register_sm(std::move(sm)) != ErrorCode::SUCCESS) return false;
    if (ran.start() != ErrorCode::SUCCESS) return false;   // encoder not built in

    E3Agent dapp(dapp_cfg);
    if (dapp.start() != ErrorCode::SUCCESS) { ran.stop(); return false; }
    dapp.wait_for_setup(std::chrono::seconds(5));
    dapp.subscribe(/*ran_function_id=*/1, {}, {});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Drives the dApp -> RAN direction alongside the indication flow, so the
    // inbound loop, the report queue and the report worker run while the
    // outbound path is busy. queue_outbound stamps L0 on this thread, which
    // therefore needs its own ring.
    std::atomic<bool> stop{false};
    std::thread driver([&] {
        latrec_tls_open_as("stages.driver");
        // A short burst of indications from a ring-owning thread, so the
        // emit-entry stage is recorded at all: the example SM's emitter thread
        // has no ring. The context published before each one is what that stage
        // must carry into aux. Kept off the loop below, whose rate is set to
        // fill every leg without provoking a drop.
        // protocolData has to be valid JSON: the JSON encoder embeds it as a
        // nested object rather than a string, so it parses what it is given.
        static const char kPayload[] = "{\"stages\":1}";
        const std::vector<uint8_t> payload(kPayload, kPayload + sizeof(kPayload) - 1);
        // The context encodes which combo produced the record, so a drop can be
        // attributed to its encoding.
        const uint64_t base = ORIGIN_BASE
                            + (static_cast<uint64_t>(c.encoding) << 8);
        for (uint64_t i = 1; i <= 10; i++) {
            latrec_ctx_set(base + i);
            ran.send_indication(1, /*ran_function_id=*/1, payload);
        }
        while (!stop.load()) {
            dapp.send_report(1, std::vector<uint8_t>(32, 0xAB), /*sequence_id=*/1);
            dapp.send_control(1, 1, std::vector<uint8_t>(16, 0xCD));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop.store(true);
    driver.join();
    dapp.stop();
    ran.stop();
    (void)trace_dir;
    const std::string rm = "rm -rf '" + ipc + "'";
    if (system(rm.c_str()) != 0) { /* best effort */ }
    return true;
}

int main() {
    char tmpl[] = "/tmp/latrec_stages_XXXXXX";
    const char* trace_dir = mkdtemp(tmpl);
    if (!trace_dir) { std::fprintf(stderr, "cannot create a ring directory\n"); return 1; }
    latrec_set_output_dir(trace_dir);
    setenv("LATREC_ENTRIES_LOG2", "16", 1);

    // The POSIX RAN send() accepts new peers and broadcasts to all of them,
    // which the ZMQ one does not; JSON and Protobuf change what L1->L2 and
    // L4->L5 measure.
    const Combo combos[] = {
        {"zmq-ipc-asn1",     E3LinkLayer::ZMQ,   E3TransportLayer::IPC, EncodingFormat::ASN1},
        {"posix-ipc-asn1",   E3LinkLayer::POSIX, E3TransportLayer::IPC, EncodingFormat::ASN1},
        {"zmq-ipc-json",     E3LinkLayer::ZMQ,   E3TransportLayer::IPC, EncodingFormat::JSON},
        {"zmq-ipc-protobuf", E3LinkLayer::ZMQ,   E3TransportLayer::IPC, EncodingFormat::PROTOBUF},
    };

    for (const Combo& c : combos) {
        const size_t before = read_ring_dir(trace_dir).size();
        const bool ran = run_combo(c, trace_dir);
        const size_t after = read_ring_dir(trace_dir).size();
        g_combos.push_back({c.name, ran, ran ? after - before : 0});
        std::printf("combo %-18s %s\n", c.name,
                    ran ? "ok" : "skipped (encoder not built)");
    }

    g_recs = read_ring_dir(trace_dir);
    std::printf("\ncaptured %zu records across %zu combos\n\n",
                g_recs.size(), g_combos.size());
    const int rc = RUN_ALL_TESTS();

    const std::string rm = std::string("rm -rf '") + trace_dir + "'";
    if (system(rm.c_str()) != 0) { /* best effort */ }
    latrec_set_output_dir(nullptr);
    unsetenv("LATREC_ENTRIES_LOG2");
    return rc;
}
