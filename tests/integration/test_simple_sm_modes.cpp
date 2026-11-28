/**
 * @file test_simple_sm_modes.cpp
 * @brief Unit-level tests for the shipped Simple Service Model's pacing modes
 *        and trace hook.
 *
 * Drives the SM directly through a harness that re-exposes the protected
 * ServiceModel wiring (outbound emitter + subscriber provider), so the pacing
 * and hook behavior are exercised without a real transport. Covers:
 *   - FixedRate flood (period 0) emits without pacing,
 *   - FixedRate paces at ~1/period,
 *   - PingPong is closed-loop (gated, not flooding) with no control answers,
 *   - each control answer releases the next PingPong emission, answers a valid
 *     control with a POSITIVE response code, and emits indications whose
 *     payload carries the monotonic seq in data1 with a populated timestamp,
 *   - an undecodable control payload is answered NEGATIVE (decode-failure branch),
 *   - the trace hook fires the RAN-side phases in order for a round trip,
 *   - the opaque payload the benchmarks use to give the SM a real SM's size
 *     (37 B indication, PRB-block control) survives every compiled encoding and
 *     stays absent when not asked for.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include <libe3/libe3.hpp>
#include <libe3/latrec.h>
#include "sm_simple/e3sm_simple_wrapper.hpp"
#include "sm_simple/simple_service_model.hpp"
#include "latrec_ring_reader.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace libe3;
using namespace std::chrono_literals;
using libe3_examples::SimpleServiceModel;

namespace {

// Re-expose the protected ServiceModel wiring so a test can feed the SM
// subscribers and capture its outbound PDUs without a real E3Interface.
class Harness : public SimpleServiceModel {
public:
    using SimpleServiceModel::SimpleServiceModel;  // inherit constructors
    using ServiceModel::set_outbound_emitter;
    using ServiceModel::set_subscribers_provider;
    using ServiceModel::set_control_responder;
};

// Counts outbound PDUs by type and notifies waiters on each indication. Also
// captures the raw indication payloads (in emit order) and the response code of
// each answer to a control, so tests can verify payload shape and POSITIVE/NEGATIVE answers.
struct EmitterCounts {
    std::mutex mu;
    std::condition_variable cv;
    int indications = 0;
    int responses = 0;
    std::vector<std::vector<uint8_t>> indication_payloads;  // SM payload bytes
    std::vector<ResponseCode> response_codes;               // one per answer to a control
};

// Wire a harness to a counting emitter with a single subscriber (dApp id 1).
void wire(Harness& sm, EmitterCounts& c) {
    sm.set_subscribers_provider([]() -> std::vector<uint32_t> { return {1}; });
    sm.set_outbound_emitter([&c](Pdu&& pdu) -> ErrorCode {
        std::lock_guard<std::mutex> lk(c.mu);
        if (pdu.type == PduType::INDICATION_MESSAGE) {
            if (const auto* ind = pdu.get_if<IndicationMessage>()) {
                c.indication_payloads.push_back(ind->protocol_data);
            }
            ++c.indications;
            c.cv.notify_all();
        }
        return ErrorCode::SUCCESS;
    });
    // The answer to a control, as the RAN would send it to a dApp that asked.
    sm.set_control_responder([&c](uint32_t, uint32_t, const ControlResult& result) -> ErrorCode {
        std::lock_guard<std::mutex> lk(c.mu);
        c.response_codes.push_back(result.response_code);
        ++c.responses;
        return ErrorCode::SUCCESS;
    });
}

int indication_count(EmitterCounts& c) {
    std::lock_guard<std::mutex> lk(c.mu);
    return c.indications;
}

// Feed one control action to the SM, as an E3Interface would on the RAN
// inbound thread, carrying a valid Simple-Control payload.
ErrorCode feed_control(Harness& sm) {
    DAppControlAction action;
    action.dapp_identifier = 1;
    action.ran_function_identifier = SimpleServiceModel::RAN_FUNCTION_ID;
    action.control_identifier = 1;
    std::vector<uint8_t> payload;
    libe3_examples::encode_simple_control(50, payload);  // ASN.1 default
    action.action_data = payload;
    return sm.handle_control_action(/*request_message_id=*/1, action);
}

// Feed one control action carrying an empty (undecodable) Simple-Control
// payload, exercising the SM's decode-failure -> NEGATIVE-ack branch.
ErrorCode feed_control_bad(Harness& sm) {
    DAppControlAction action;
    action.dapp_identifier = 1;
    action.ran_function_identifier = SimpleServiceModel::RAN_FUNCTION_ID;
    action.control_identifier = 1;
    action.action_data = {};  // no bytes: APER decode fails
    return sm.handle_control_action(/*request_message_id=*/2, action);
}

}  // namespace

TEST(flood_emits_without_pacing) {
    // FixedRate + period 0 => flood. Over 300 ms the SM emits far more than any
    // paced rate could; the bound leaves a huge margin even on a slow host.
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::FixedRate);
    wire(sm, c);
    sm.start();
    std::this_thread::sleep_for(300ms);
    sm.stop();
    ASSERT_GT(indication_count(c), 30);
}

TEST(fixed_rate_paces_approximately) {
    // period 10 ms over ~500 ms => ~50 indications. Re-anchoring guarantees no
    // burst (safe upper bound); the wide lower bound tolerates slow CI hosts.
    EmitterCounts c;
    Harness sm(/*period_us=*/10'000, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::FixedRate);
    wire(sm, c);
    sm.start();
    std::this_thread::sleep_for(500ms);
    sm.stop();
    int n = indication_count(c);
    ASSERT_GT(n, 20);
    ASSERT_LT(n, 120);
}

TEST(fixed_rate_stop_returns_promptly) {
    // A FixedRate worker parked in the pacing wait must wake as soon as stop()
    // flips running_, not wait out the rest of the period. A multi-second period
    // makes a blocked stop() obvious without slowing CI: the test only runs as
    // long as stop() actually takes.
    EmitterCounts c;
    Harness sm(/*period_us=*/5'000'000, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::FixedRate);
    wire(sm, c);
    sm.start();
    std::this_thread::sleep_for(50ms);  // let the worker enter the pacing wait

    const auto t0 = std::chrono::steady_clock::now();
    sm.stop();
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    ASSERT_TRUE(elapsed < 200ms);
}

TEST(pingpong_is_gated_not_flooding) {
    // PingPong with no control answers: the loop is closed, so emissions are
    // bounded by the 50 ms liveness fallback (+ bootstrap), never a flood.
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong);
    wire(sm, c);
    sm.start();
    std::this_thread::sleep_for(300ms);
    sm.stop();
    ASSERT_LT(indication_count(c), 15);
}

TEST(pingpong_ack_releases_each_emission) {
    // Each control ack releases exactly the next emission. Attributing an
    // emission to the ack (not the liveness fallback) requires the observation
    // window to stay below the fallback; we widen both so a slow CI runner's
    // notify->wakeup latency cannot cause a false failure while the window still
    // cannot be satisfied by the fallback: a 2 s fallback with a 1 s window.
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong,
               /*pingpong_fallback_ms=*/2000);
    wire(sm, c);
    sm.start();

    {  // wait for the bootstrap indication
        std::unique_lock<std::mutex> lk(c.mu);
        ASSERT_TRUE(c.cv.wait_for(lk, 2s, [&] { return c.indications >= 1; }));
    }

    const int K = 5;
    for (int i = 0; i < K; ++i) {
        int before = indication_count(c);
        ASSERT_TRUE(feed_control(sm) == ErrorCode::SUCCESS);
        std::unique_lock<std::mutex> lk(c.mu);
        bool released = c.cv.wait_for(lk, 1s, [&] { return c.indications > before; });
        ASSERT_TRUE(released);
    }
    sm.stop();
    ASSERT_GE(indication_count(c), K + 1);

    std::lock_guard<std::mutex> lk(c.mu);
    // Every valid control action was acked POSITIVE.
    ASSERT_EQ(c.responses, K);
    ASSERT_EQ(c.response_codes.size(), static_cast<size_t>(K));
    for (ResponseCode rc : c.response_codes) {
        ASSERT_TRUE(rc == ResponseCode::POSITIVE);
    }
    // Each emitted indication carries the monotonic seq in data1 and a
    // populated timestamp (the shipped SM stamps a live wall-clock value).
    ASSERT_GE(c.indication_payloads.size(), static_cast<size_t>(K + 1));
    for (size_t j = 0; j < c.indication_payloads.size(); ++j) {
        libe3_examples::SimpleIndication si;
        ASSERT_TRUE(libe3_examples::decode_simple_indication(c.indication_payloads[j], si));
        ASSERT_EQ(si.data1, static_cast<uint32_t>(j));
        ASSERT_TRUE(si.timestamp.has_value());
    }
}

TEST(control_decode_failure_yields_negative_answer) {
    // The SM answers every control action: POSITIVE when the Simple-Control
    // payload decodes, NEGATIVE when it does not. handle_control_action is
    // synchronous and needs only the outbound emitter, so both branches are
    // exercised without starting the worker thread.
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::FixedRate);
    wire(sm, c);

    ASSERT_TRUE(feed_control(sm) == ErrorCode::SUCCESS);      // valid -> POSITIVE
    ASSERT_TRUE(feed_control_bad(sm) == ErrorCode::SUCCESS);  // undecodable -> NEGATIVE

    std::lock_guard<std::mutex> lk(c.mu);
    ASSERT_EQ(c.responses, 2);
    ASSERT_EQ(c.response_codes.size(), 2u);
    ASSERT_TRUE(c.response_codes[0] == ResponseCode::POSITIVE);
    ASSERT_TRUE(c.response_codes[1] == ResponseCode::NEGATIVE);
}

TEST(trace_hook_phase_order) {
    // The RAN-side phases must occur in order: CollectBegin, EncodeBegin,
    // SendIndication (worker thread), then ControlRecv, ControlDone (control
    // handler), with monotonic non-decreasing timestamps. Reads the
    // the SM's own stamps back from latrec instead of the retired ad hoc trace
    // hook -- this is only meaningful in a LIBE3_ENABLE_LATREC build, which
    // is why this whole test is skipped (see cmake/libe3Tests.cmake) when
    // that flag is off.
    char tmpl[] = "/tmp/libe3_test_sm_trace_XXXXXX";
    char* dir = mkdtemp(tmpl);
    ASSERT_TRUE(dir != nullptr);
    latrec_set_output_dir(dir);
    // handle_control_action below runs synchronously on this thread (there is
    // no real E3Interface inbound thread in this harness to have opened a
    // ring for it already), so its EX3/EX4 stamps need one opened here.
    latrec_tls_open_as("test.main");

    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong);
    wire(sm, c);
    sm.start();

    {  // wait for the bootstrap indication (fires the three worker-side phases)
        std::unique_lock<std::mutex> lk(c.mu);
        ASSERT_TRUE(c.cv.wait_for(lk, 2s, [&] { return c.indications >= 1; }));
    }
    ASSERT_TRUE(feed_control(sm) == ErrorCode::SUCCESS);  // fires the two control phases
    sm.stop();

    latrec_set_output_dir(nullptr);

    bool wrapped = false;
    std::vector<latrec_rec> recs = latrec_test::read_ring_dir(dir, &wrapped);
    std::vector<std::pair<uint8_t, uint64_t>> events;
    for (const auto& r : recs) {
        const uint8_t s = latrec_test::stage_of(r);
        if (s == LATREC_RECORD_BEGIN || s == LATREC_ENCODE_E3SM_BEGIN ||
            s == LATREC_ENCODE_E3SM_DONE || s == LATREC_DECODE_E3SM_BEGIN ||
            s == LATREC_DECODE_E3SM_DONE) {
            events.emplace_back(s, r.t_ns);
        }
    }
    // Rings are read back one at a time (read_ring_dir merges them in
    // directory order, not time order): the worker thread's record/encode
    // stamps and this thread's control-decode stamps land in two different
    // rings, so this must be sorted by timestamp before order is checked.
    std::sort(events.begin(), events.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // The bootstrap indication always produces the first three events in order.
    ASSERT_GE(events.size(), 5u);
    ASSERT_TRUE(events[0].first == LATREC_RECORD_BEGIN);
    ASSERT_TRUE(events[1].first == LATREC_ENCODE_E3SM_BEGIN);
    ASSERT_TRUE(events[2].first == LATREC_ENCODE_E3SM_DONE);
    // Locate the control round (the 50 ms fallback may inject worker triples
    // ahead of it, so search rather than assume a fixed index).
    bool found = false;
    for (size_t i = 3; i + 1 < events.size(); ++i) {
        if (events[i].first == LATREC_DECODE_E3SM_BEGIN) {
            ASSERT_TRUE(events[i + 1].first == LATREC_DECODE_E3SM_DONE);
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
    for (size_t i = 1; i < events.size(); ++i) {
        ASSERT_GE(events[i].second, events[i - 1].second);
    }

    const std::string rm = "rm -rf '" + std::string(dir) + "'";
    if (std::system(rm.c_str()) != 0) { /* best effort */ }
}

namespace {

// Every encoding this build has: ASN.1 always, the others when compiled in.
std::vector<EncodingFormat> compiled_encodings() {
    std::vector<EncodingFormat> v{EncodingFormat::ASN1};
#ifdef LIBE3_ENABLE_JSON
    v.push_back(EncodingFormat::JSON);
#endif
#ifdef LIBE3_ENABLE_PROTOBUF
    v.push_back(EncodingFormat::PROTOBUF);
#endif
    return v;
}

}  // namespace

TEST(indication_payload_round_trips_in_every_encoding) {
    const std::vector<uint8_t> payload = libe3_examples::make_pseudo_random_payload(37);
    ASSERT_EQ(payload.size(), 37u);
    for (EncodingFormat enc : compiled_encodings()) {
        libe3_examples::SimpleIndication in{7, 1234567u, payload};
        std::vector<uint8_t> wire_bytes;
        ASSERT_TRUE(libe3_examples::encode_simple_indication(in, wire_bytes, enc));

        libe3_examples::SimpleIndication out;
        ASSERT_TRUE(libe3_examples::decode_simple_indication(wire_bytes, out, enc));
        ASSERT_EQ(out.data1, 7u);
        ASSERT_TRUE(out.timestamp.has_value());
        ASSERT_EQ(*out.timestamp, 1234567u);
        ASSERT_TRUE(out.payload == payload);
    }
}

TEST(indication_without_payload_stays_empty_and_smaller) {
    for (EncodingFormat enc : compiled_encodings()) {
        libe3_examples::SimpleIndication bare{7, 1234567u, {}};
        libe3_examples::SimpleIndication loaded{7, 1234567u,
                                                libe3_examples::make_pseudo_random_payload(37)};
        std::vector<uint8_t> a, b;
        ASSERT_TRUE(libe3_examples::encode_simple_indication(bare, a, enc));
        ASSERT_TRUE(libe3_examples::encode_simple_indication(loaded, b, enc));
        ASSERT_GT(b.size(), a.size() + 36u);  // at least the 37 payload bytes more

        libe3_examples::SimpleIndication out;
        out.payload = {1, 2, 3};  // decode must clear a stale payload
        ASSERT_TRUE(libe3_examples::decode_simple_indication(a, out, enc));
        ASSERT_TRUE(out.payload.empty());
    }
}

TEST(control_prb_block_round_trips_in_every_encoding) {
    // 0 PRBs (clear), 50 (the r2 mock block), 273 (every PRB of 100 MHz): the
    // raw payload is 2 bytes per PRB, the PRB indices little-endian.
    for (unsigned prbs : {0u, 50u, 273u}) {
        const std::vector<uint8_t> payload = libe3_examples::make_prb_block_payload(prbs);
        ASSERT_EQ(payload.size(), static_cast<size_t>(prbs) * 2);
        if (prbs == 273u) {
            ASSERT_EQ(payload[2 * 272], 272 & 0xFF);
            ASSERT_EQ(payload[2 * 272 + 1], 272 >> 8);
        }
        for (EncodingFormat enc : compiled_encodings()) {
            std::vector<uint8_t> wire_bytes;
            ASSERT_TRUE(libe3_examples::encode_simple_control(42, wire_bytes, enc, payload));

            int threshold = -1;
            std::vector<uint8_t> out{9, 9};  // decode must replace, not append
            ASSERT_TRUE(libe3_examples::decode_simple_control(wire_bytes, threshold, enc, &out));
            ASSERT_EQ(threshold, 42);
            ASSERT_TRUE(out == payload);
        }
    }
}

TEST(sm_emits_payload_when_asked_and_accepts_prb_block_control) {
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong,
               /*pingpong_fallback_ms=*/50, /*indication_payload_bytes=*/37);
    wire(sm, c);
    sm.start();
    {
        std::unique_lock<std::mutex> lk(c.mu);
        ASSERT_TRUE(c.cv.wait_for(lk, 2s, [&] { return c.indications >= 1; }));
    }

    DAppControlAction action;
    action.dapp_identifier = 1;
    action.ran_function_identifier = SimpleServiceModel::RAN_FUNCTION_ID;
    action.control_identifier = 1;
    std::vector<uint8_t> encoded;
    ASSERT_TRUE(libe3_examples::encode_simple_control(
        50, encoded, EncodingFormat::ASN1, libe3_examples::make_prb_block_payload(273)));
    action.action_data = encoded;
    ASSERT_TRUE(sm.handle_control_action(/*request_message_id=*/1, action) == ErrorCode::SUCCESS);
    sm.stop();

    std::lock_guard<std::mutex> lk(c.mu);
    ASSERT_EQ(c.response_codes.size(), 1u);
    ASSERT_TRUE(c.response_codes[0] == ResponseCode::POSITIVE);
    ASSERT_GE(c.indication_payloads.size(), 1u);
    libe3_examples::SimpleIndication si;
    ASSERT_TRUE(libe3_examples::decode_simple_indication(c.indication_payloads[0], si));
    ASSERT_EQ(si.payload.size(), 37u);
}

int main() {
    return RUN_ALL_TESTS();
}
