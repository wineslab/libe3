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
 *   - each control ack releases the next PingPong emission,
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

// Counts outbound PDUs by type and notifies waiters on each indication.
struct EmitterCounts {
    std::mutex mu;
    std::condition_variable cv;
    int indications = 0;
    int acks = 0;
};

// Wire a harness to a counting emitter with a single subscriber (dApp id 1).
void wire(Harness& sm, EmitterCounts& c) {
    sm.set_subscribers_provider([]() -> std::vector<uint32_t> { return {1}; });
    sm.set_outbound_emitter([&c](Pdu&& pdu) -> ErrorCode {
        std::lock_guard<std::mutex> lk(c.mu);
        if (pdu.type == PduType::INDICATION_MESSAGE) {
            ++c.indications;
            c.cv.notify_all();
        } else if (pdu.type == PduType::MESSAGE_ACK) {
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
    // Each control ack releases exactly the next emission. The ack path is
    // microseconds, well under the 50 ms liveness fallback, so an emission
    // observed within 40 ms proves the ack (not the fallback) drove it.
    EmitterCounts c;
    Harness sm(/*period_us=*/0, EncodingFormat::ASN1,
               SimpleServiceModel::PacingMode::PingPong);
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
        bool released = c.cv.wait_for(lk, 40ms, [&] { return c.indications > before; });
        ASSERT_TRUE(released);
    }
    sm.stop();
    ASSERT_GE(indication_count(c), K + 1);
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
