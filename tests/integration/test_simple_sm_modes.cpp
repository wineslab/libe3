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
 *   - PingPong is closed-loop (gated, not flooding) with no control acks,
 *   - each control ack releases the next PingPong emission, acks a valid
 *     control with a POSITIVE response code, and emits indications whose
 *     payload carries the monotonic seq in data1 with a populated timestamp,
 *   - an undecodable control payload is acked NEGATIVE (decode-failure branch),
 *   - the trace hook fires the RAN-side phases in order for a round trip.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include <libe3/libe3.hpp>
#include "sm_simple/e3sm_simple_wrapper.hpp"
#include "sm_simple/simple_service_model.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
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
};

// Counts outbound PDUs by type and notifies waiters on each indication. Also
// captures the raw indication payloads (in emit order) and the response code of
// each ack, so tests can verify payload shape and POSITIVE/NEGATIVE acking.
struct EmitterCounts {
    std::mutex mu;
    std::condition_variable cv;
    int indications = 0;
    int acks = 0;
    std::vector<std::vector<uint8_t>> indication_payloads;  // SM payload bytes
    std::vector<ResponseCode> ack_codes;                    // one per MESSAGE_ACK
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
        } else if (pdu.type == PduType::MESSAGE_ACK) {
            if (const auto* ack = pdu.get_if<MessageAck>()) {
                c.ack_codes.push_back(ack->response_code);
            }
            ++c.acks;
        }
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

TEST(pingpong_is_gated_not_flooding) {
    // PingPong with no control acks: the loop is closed, so emissions are
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
    ASSERT_EQ(c.acks, K);
    ASSERT_EQ(c.ack_codes.size(), static_cast<size_t>(K));
    for (ResponseCode rc : c.ack_codes) {
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

TEST(control_decode_failure_yields_negative_ack) {
    // The SM acks every control action: POSITIVE when the Simple-Control
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
    ASSERT_EQ(c.acks, 2);
    ASSERT_EQ(c.ack_codes.size(), 2u);
    ASSERT_TRUE(c.ack_codes[0] == ResponseCode::POSITIVE);
    ASSERT_TRUE(c.ack_codes[1] == ResponseCode::NEGATIVE);
}

TEST(trace_hook_phase_order) {
    // The trace hook must fire the RAN-side phases in order: CollectBegin,
    // EncodeBegin, SendIndication (worker), then ControlRecv, ControlDone
    // (control handler), with monotonic non-decreasing timestamps.
    using TP = SimpleServiceModel::TracePhase;
    std::mutex mu;
    std::vector<std::pair<TP, int64_t>> events;

    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong);
    sm.set_trace_hook([&](uint32_t, TP ph, int64_t ts) {
        std::lock_guard<std::mutex> lk(mu);
        events.emplace_back(ph, ts);
    });
    wire(sm, c);
    sm.start();

    {  // wait for the bootstrap indication (fires the three worker-side phases)
        std::unique_lock<std::mutex> lk(c.mu);
        ASSERT_TRUE(c.cv.wait_for(lk, 2s, [&] { return c.indications >= 1; }));
    }
    ASSERT_TRUE(feed_control(sm) == ErrorCode::SUCCESS);  // fires the two control phases
    sm.stop();

    std::lock_guard<std::mutex> lk(mu);
    // The bootstrap indication always produces the first three events in order.
    ASSERT_GE(events.size(), 5u);
    ASSERT_TRUE(events[0].first == TP::CollectBegin);
    ASSERT_TRUE(events[1].first == TP::EncodeBegin);
    ASSERT_TRUE(events[2].first == TP::SendIndication);
    // Locate the control round (the 50 ms fallback may inject worker triples
    // ahead of it, so search rather than assume a fixed index).
    bool found = false;
    for (size_t i = 3; i + 1 < events.size(); ++i) {
        if (events[i].first == TP::ControlRecv) {
            ASSERT_TRUE(events[i + 1].first == TP::ControlDone);
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
    for (size_t i = 1; i < events.size(); ++i) {
        ASSERT_GE(events[i].second, events[i - 1].second);
    }
}

int main() {
    return RUN_ALL_TESTS();
}
