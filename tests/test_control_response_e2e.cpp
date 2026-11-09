/**
 * @file test_control_response_e2e.cpp
 * @brief The answer to a dApp control and to an xApp control: E3-ControlResponse.
 *
 * A real RAN E3Agent and real dApp E3Agents over ZMQ IPC, once for every
 * encoding this build has. The RAN serves RAN function 100 with control 10; its
 * Service Model answers as the test tells it to. Properties verified:
 *   (1) a dApp control with requestAck gets one ControlResponse that names the
 *       control by the id send_control returned, carries the RAN function and
 *       control identifier, and carries the code, the reason and the outcome
 *       the Service Model gave;
 *   (2) a control without requestAck gets none from the Service Model, whatever
 *       the Service Model answers, and a control that asks gets the next one
 *       and no other;
 *   (3) a control the RAN refuses itself is answered negative with its cause
 *       whether it asked or not, and never reaches the Service Model;
 *   (4) a Service Model that returns an error gets a negative answer from the
 *       core, one that says nothing gets nothing, and one that answers twice
 *       gets one response;
 *   (5) a ControlResponse is read only by the dApp it names, though ZMQ
 *       broadcasts it to every dApp;
 *   (6) an xApp control reaches its dApp with requestAck as sent, the dApp
 *       answers it, the RAN hands the answer to its handler under the id
 *       send_xapp_control returned, and a dApp that does not act on xApp
 *       controls answers negative by itself;
 *   (7) libe3 sends no MessageAck: nothing but a ControlResponse answers a control.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t SERVED_RF = 100;
constexpr uint32_t SERVED_CONTROL = 10;
const std::vector<uint8_t> OUTCOME = {'{', '"', 'a', 'p', 'p', 'l', 'i', 'e', 'd', '"', ':', '1', '}'};

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/ctlresp_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

/// What the Service Model does with the next control it gets.
enum class Behavior { POSITIVE_WITH_OUTCOME, NEGATIVE_WITH_REASON, SILENT, RETURN_ERROR, ANSWER_TWICE };

class AnswerSM : public ServiceModel {
public:
    std::string name() const override { return "AnswerSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return SERVED_RF; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {SERVED_CONTROL}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }

    ErrorCode handle_control_action(uint32_t request_message_id,
                                    const DAppControlAction& action) override {
        ++controls;
        last_request_ack = action.request_ack;
        ControlResult result;
        switch (behavior.load()) {
            case Behavior::POSITIVE_WITH_OUTCOME:
                result.response_code = ResponseCode::POSITIVE;
                result.outcome = OUTCOME;
                first_rc = respond_to_control(action.dapp_identifier, request_message_id, result);
                break;
            case Behavior::NEGATIVE_WITH_REASON:
                result.response_code = ResponseCode::NEGATIVE;
                result.cause = Cause::INVALID_REQUEST;
                result.detail = "the scheduler refused it";
                first_rc = respond_to_control(action.dapp_identifier, request_message_id, result);
                break;
            case Behavior::SILENT:
                break;
            case Behavior::RETURN_ERROR:
                return ErrorCode::GENERIC_ERROR;
            case Behavior::ANSWER_TWICE:
                result.response_code = ResponseCode::POSITIVE;
                first_rc = respond_to_control(action.dapp_identifier, request_message_id, result);
                second_rc = respond_to_control(action.dapp_identifier, request_message_id, result);
                break;
        }
        return ErrorCode::SUCCESS;
    }

    std::atomic<Behavior> behavior{Behavior::POSITIVE_WITH_OUTCOME};
    std::atomic<int> controls{0};
    std::atomic<bool> last_request_ack{false};
    std::atomic<ErrorCode> first_rc{ErrorCode::SUCCESS};
    std::atomic<ErrorCode> second_rc{ErrorCode::SUCCESS};

private:
    std::atomic<bool> running_{false};
};

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

/// Every ControlResponse an agent hands to its handler, in order.
struct Heard {
    std::mutex mu;
    std::vector<ControlResponse> responses;

    void push(const ControlResponse& r) {
        std::lock_guard<std::mutex> lk(mu);
        responses.push_back(r);
    }
    size_t count() {
        std::lock_guard<std::mutex> lk(mu);
        return responses.size();
    }
    ControlResponse at(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return responses.at(i);
    }
};

/// Every xApp control a dApp handler sees.
struct XAppSeen {
    std::mutex mu;
    std::vector<XAppControlAction> controls;

    size_t count() {
        std::lock_guard<std::mutex> lk(mu);
        return controls.size();
    }
    XAppControlAction at(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return controls.at(i);
    }
};

bool has(const std::optional<std::string>& text, const std::string& part) {
    return text.has_value() && text->find(part) != std::string::npos;
}

}  // namespace

#if LIBE3_HAS_ZMQ
static void run_control_responses(EncodingFormat encoding, const char* tag) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const std::string setup = unique_ipc(tag);
    const std::string sub = unique_ipc(tag);
    const std::string pub = unique_ipc(tag);

    auto make_config = [&](E3Role role) {
        E3Config c;
        c.role                = role;
        c.link_layer          = E3LinkLayer::ZMQ;
        c.transport_layer     = E3TransportLayer::IPC;
        c.setup_endpoint      = setup;
        c.subscriber_endpoint = sub;
        c.publisher_endpoint  = pub;
        c.encoding            = encoding;
        c.log_level           = 0;
        c.ran_identifier      = "ctlresp-test";
        c.dapp_name           = "ControlResponseDApp";
        c.dapp_version        = "1.0.0";
        return c;
    };

    E3Agent ran(make_config(E3Role::RAN));
    auto sm_owner = std::make_unique<AnswerSM>();
    AnswerSM* sm = sm_owner.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(sm_owner))),
              static_cast<int>(ErrorCode::SUCCESS));
    Heard ran_heard;  // what the RAN hears from dApps: their answers to xApp controls
    ran.set_control_response_handler([&ran_heard](const ControlResponse& r) { ran_heard.push(r); });
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    // dApp A acts on everything; dApp B shares the links and acts on no xApp control.
    E3Agent dapp_a(make_config(E3Role::DAPP));
    Heard heard_a;
    XAppSeen xapp_a;
    dapp_a.set_control_response_handler([&heard_a](const ControlResponse& r) { heard_a.push(r); });
    dapp_a.set_xapp_control_handler([&xapp_a](const XAppControlAction& a) {
        std::lock_guard<std::mutex> lk(xapp_a.mu);
        xapp_a.controls.push_back(a);
    });
    ASSERT_EQ(static_cast<int>(dapp_a.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp_a.wait_for_setup(5000ms)), static_cast<int>(ErrorCode::SUCCESS));

    E3Agent dapp_b(make_config(E3Role::DAPP));
    Heard heard_b;
    dapp_b.set_control_response_handler([&heard_b](const ControlResponse& r) { heard_b.push(r); });
    ASSERT_EQ(static_cast<int>(dapp_b.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp_b.wait_for_setup(5000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(dapp_a.dapp_id().has_value() && dapp_b.dapp_id().has_value());
    const uint32_t id_a = *dapp_a.dapp_id();
    const uint32_t id_b = *dapp_b.dapp_id();
    ASSERT_TRUE(id_a != id_b);

    // Sends a control from dApp A and returns the message id that names it.
    auto control = [&](uint32_t rf, uint32_t control_id, bool ask) {
        uint32_t message_id = 0;
        ASSERT_EQ(static_cast<int>(dapp_a.send_control(rf, control_id, {'{', '}'}, 0, &message_id, ask)),
                  static_cast<int>(ErrorCode::SUCCESS));
        return message_id;
    };

    // (3) What the RAN refuses itself, before any subscription: answered whether the
    // control asked or not, never handed to the Service Model.
    for (bool ask : {false, true}) {
        const size_t before = heard_a.count();
        const uint32_t unknown_function = control(999, SERVED_CONTROL, ask);
        ASSERT_TRUE(wait_until([&] { return heard_a.count() > before; }, 5000ms));
        auto r = heard_a.at(before);
        ASSERT_EQ(r.request_id, unknown_function);
        ASSERT_EQ(r.dapp_identifier, id_a);
        ASSERT_EQ(r.ran_function_identifier, 999u);
        ASSERT_EQ(r.control_identifier, SERVED_CONTROL);
        ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::UNSUPPORTED_RAN_FUNCTION);
        ASSERT_TRUE(has(r.detail, "999"));
        ASSERT_FALSE(r.control_outcome.has_value());

        const uint32_t unknown_control = control(SERVED_RF, 55, ask);
        ASSERT_TRUE(wait_until([&] { return heard_a.count() > before + 1; }, 5000ms));
        r = heard_a.at(before + 1);
        ASSERT_EQ(r.request_id, unknown_control);
        ASSERT_EQ(r.control_identifier, 55u);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::UNSUPPORTED_IDENTIFIER);

        const uint32_t no_subscription = control(SERVED_RF, SERVED_CONTROL, ask);  // the SM is not running
        ASSERT_TRUE(wait_until([&] { return heard_a.count() > before + 2; }, 5000ms));
        r = heard_a.at(before + 2);
        ASSERT_EQ(r.request_id, no_subscription);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::NOT_FOUND);
    }
    ASSERT_EQ(sm->controls.load(), 0);
    // dApp B did not send them and does not read them (5).
    ASSERT_EQ(heard_b.count(), 0u);

    // A subscription starts the Service Model.
    ASSERT_EQ(static_cast<int>(dapp_a.subscribe(SERVED_RF, {1}, {SERVED_CONTROL})),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return sm->is_running(); }, 5000ms));

    // (1) A control that asks, answered positive with an outcome.
    size_t seen = heard_a.count();
    const uint32_t positive = control(SERVED_RF, SERVED_CONTROL, true);
    ASSERT_TRUE(wait_until([&] { return heard_a.count() > seen; }, 5000ms));
    {
        auto r = heard_a.at(seen);
        ASSERT_EQ(r.request_id, positive);
        ASSERT_EQ(r.dapp_identifier, id_a);
        ASSERT_EQ(r.ran_function_identifier, SERVED_RF);
        ASSERT_EQ(r.control_identifier, SERVED_CONTROL);
        ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
        ASSERT_FALSE(r.cause.has_value());
        ASSERT_FALSE(r.detail.has_value());
        ASSERT_TRUE(r.control_outcome.has_value());
        ASSERT_TRUE(*r.control_outcome == OUTCOME);
    }
    ASSERT_TRUE(sm->last_request_ack.load());
    ASSERT_TRUE(sm->first_rc.load() == ErrorCode::SUCCESS);
    // (5) dApp B shares the link and was sent the same bytes: it reads none of it.
    std::this_thread::sleep_for(150ms);
    ASSERT_EQ(heard_b.count(), 0u);

    // A control that asks, answered negative with a reason.
    sm->behavior = Behavior::NEGATIVE_WITH_REASON;
    seen = heard_a.count();
    const uint32_t negative = control(SERVED_RF, SERVED_CONTROL, true);
    ASSERT_TRUE(wait_until([&] { return heard_a.count() > seen; }, 5000ms));
    {
        auto r = heard_a.at(seen);
        ASSERT_EQ(r.request_id, negative);
        ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::INVALID_REQUEST);
        ASSERT_STREQ(*r.detail, "the scheduler refused it");
        ASSERT_FALSE(r.control_outcome.has_value());
    }

    // (2) A control that does not ask gets nothing from the Service Model, positive or
    // negative. A marker control that asks then gets exactly its own response next, so
    // nothing was held back or sent late.
    for (Behavior b : {Behavior::POSITIVE_WITH_OUTCOME, Behavior::NEGATIVE_WITH_REASON}) {
        sm->behavior = b;
        seen = heard_a.count();
        const int controls_before = sm->controls.load();
        const uint32_t quiet = control(SERVED_RF, SERVED_CONTROL, false);
        ASSERT_TRUE(wait_until([&] { return sm->controls.load() > controls_before; }, 5000ms));
        ASSERT_FALSE(sm->last_request_ack.load());
        ASSERT_TRUE(sm->first_rc.load() == ErrorCode::NOT_FOUND);  // no answer is owed
        const uint32_t marker = control(SERVED_RF, SERVED_CONTROL, true);
        ASSERT_TRUE(wait_until([&] { return heard_a.count() > seen; }, 5000ms));
        std::this_thread::sleep_for(150ms);
        ASSERT_EQ(heard_a.count(), seen + 1);
        ASSERT_EQ(heard_a.at(seen).request_id, marker);
        ASSERT_TRUE(marker != quiet);
    }

    // (4) A Service Model that says nothing: nothing is sent.
    sm->behavior = Behavior::SILENT;
    seen = heard_a.count();
    (void)control(SERVED_RF, SERVED_CONTROL, true);
    std::this_thread::sleep_for(400ms);
    ASSERT_EQ(heard_a.count(), seen);

    // A Service Model that returns an error: the core answers negative for it.
    sm->behavior = Behavior::RETURN_ERROR;
    seen = heard_a.count();
    const uint32_t failed = control(SERVED_RF, SERVED_CONTROL, true);
    ASSERT_TRUE(wait_until([&] { return heard_a.count() > seen; }, 5000ms));
    {
        auto r = heard_a.at(seen);
        ASSERT_EQ(r.request_id, failed);
        ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::UNSPECIFIED);
    }

    // A Service Model that answers twice: one response, and the second call is told so.
    sm->behavior = Behavior::ANSWER_TWICE;
    seen = heard_a.count();
    (void)control(SERVED_RF, SERVED_CONTROL, true);
    ASSERT_TRUE(wait_until([&] { return heard_a.count() > seen; }, 5000ms));
    std::this_thread::sleep_for(300ms);
    ASSERT_EQ(heard_a.count(), seen + 1);
    ASSERT_TRUE(sm->first_rc.load() == ErrorCode::SUCCESS);
    ASSERT_TRUE(sm->second_rc.load() == ErrorCode::NOT_FOUND);

    // (6) An xApp control that asks, to dApp A, which answers it.
    uint32_t xapp_id = 0;
    ASSERT_EQ(static_cast<int>(ran.send_xapp_control(id_a, SERVED_RF, {'{', '}'}, /*sequence_id=*/77,
                                                     &xapp_id, /*request_ack=*/true)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(xapp_id != 0);
    ASSERT_TRUE(wait_until([&] { return xapp_a.count() == 1; }, 5000ms));
    {
        auto x = xapp_a.at(0);
        ASSERT_TRUE(x.request_ack);
        ASSERT_EQ(x.message_id, xapp_id);
        ASSERT_EQ(x.sequence_id, 77u);
    }
    ControlResult applied;
    applied.response_code = ResponseCode::POSITIVE;
    applied.outcome = OUTCOME;
    ASSERT_EQ(static_cast<int>(dapp_a.send_control_response(xapp_id, SERVED_RF, SERVED_CONTROL, applied)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran_heard.count() == 1; }, 5000ms));
    {
        auto r = ran_heard.at(0);
        ASSERT_EQ(r.request_id, xapp_id);
        ASSERT_EQ(r.dapp_identifier, id_a);
        ASSERT_EQ(r.ran_function_identifier, SERVED_RF);
        ASSERT_EQ(r.control_identifier, SERVED_CONTROL);
        ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
        ASSERT_TRUE(r.control_outcome.has_value() && *r.control_outcome == OUTCOME);
    }

    // An xApp control that does not ask: the dApp sees it as sent and nothing goes back.
    uint32_t quiet_xapp_id = 0;
    ASSERT_EQ(static_cast<int>(ran.send_xapp_control(id_a, SERVED_RF, {'{', '}'}, 78, &quiet_xapp_id)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return xapp_a.count() == 2; }, 5000ms));
    ASSERT_FALSE(xapp_a.at(1).request_ack);
    std::this_thread::sleep_for(200ms);
    ASSERT_EQ(ran_heard.count(), 1u);

    // An xApp control for dApp B, which acts on none: B answers negative by itself, and
    // dApp A, which shares the link, neither sees it nor answers.
    uint32_t for_b = 0;
    ASSERT_EQ(static_cast<int>(ran.send_xapp_control(id_b, SERVED_RF, {'{', '}'}, 79, &for_b, true)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran_heard.count() == 2; }, 5000ms));
    {
        auto r = ran_heard.at(1);
        ASSERT_EQ(r.request_id, for_b);
        ASSERT_EQ(r.dapp_identifier, id_b);
        ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::UNSUPPORTED_RAN_FUNCTION);
        ASSERT_EQ(r.control_identifier, SERVED_CONTROL);  // the first the function advertises
    }
    std::this_thread::sleep_for(200ms);
    ASSERT_EQ(xapp_a.count(), 2u);
    ASSERT_EQ(ran_heard.count(), 2u);

    // The answer API refuses what it cannot send.
    ControlResult too_big;
    too_big.outcome.assign(MAX_CONTROL_OUTCOME_SIZE + 1, 'x');
    ASSERT_EQ(static_cast<int>(dapp_a.send_control_response(xapp_id, SERVED_RF, SERVED_CONTROL, too_big)),
              static_cast<int>(ErrorCode::INVALID_PARAM));
    ASSERT_EQ(static_cast<int>(ran.send_control_response(1, SERVED_RF, SERVED_CONTROL, applied)),
              static_cast<int>(ErrorCode::STATE_ERROR));  // a RAN answers through its Service Models

    dapp_b.stop();
    dapp_a.stop();
    ran.stop();
}

TEST(ControlResponse_zmq_aControlIsAnsweredByAControlResponseAndNothingElse) {
    struct Choice { EncodingFormat format; const char* tag; };
    bool any = false;
    for (const Choice& c : {Choice{EncodingFormat::ASN1, "asn1"}, Choice{EncodingFormat::JSON, "json"},
                            Choice{EncodingFormat::PROTOBUF, "pb"}}) {
        if (create_encoder(c.format) == nullptr) continue;
        run_control_responses(c.format, c.tag);
        any = true;
    }
    ASSERT_TRUE(any);
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
