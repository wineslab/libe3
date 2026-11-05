/**
 * @file test_refusal_e2e.cpp
 * @brief A RAN refuses a subscription or a control it cannot serve, and says why.
 *
 * A real RAN E3Agent and a real dApp E3Agent over ZMQ IPC, once for every
 * encoding this build has. The RAN serves RAN function 100 with telemetry 1 and
 * 2 and control 10. Properties verified:
 *   (1) a subscription to an unknown RAN function, with no identifier, with an
 *       unknown telemetry id or with an unknown control id is answered negative
 *       with the cause Aerial's and OCUDU's agents give, and leaves no
 *       subscription behind and the Service Model stopped;
 *   (2) a control for an unknown RAN function or control id, or with no
 *       subscription behind it, is answered with a negative acknowledgment
 *       and does not reach the Service Model;
 *   (3) a subscription that names what the function advertises is granted and
 *       a control for it reaches the Service Model with no refusal.
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

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/refusal_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

class ServedSM : public ServiceModel {
public:
    std::string name() const override { return "ServedSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return SERVED_RF; }
    std::vector<uint32_t> telemetry_ids() const override { return {1, 2}; }
    std::vector<uint32_t> control_ids() const override { return {SERVED_CONTROL}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; ++starts; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        ++controls;
        return ErrorCode::SUCCESS;
    }

    std::atomic<int> starts{0};
    std::atomic<int> controls{0};

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

/// What the dApp hears back, in the order it arrives.
struct Heard {
    std::mutex mu;
    std::vector<SubscriptionResponse> subscriptions;
    std::vector<MessageAck> acks;

    size_t subscription_count() {
        std::lock_guard<std::mutex> lk(mu);
        return subscriptions.size();
    }
    size_t ack_count() {
        std::lock_guard<std::mutex> lk(mu);
        return acks.size();
    }
    SubscriptionResponse subscription(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return subscriptions.at(i);
    }
    MessageAck ack(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return acks.at(i);
    }
};

bool has(const std::optional<std::string>& text, const std::string& part) {
    return text.has_value() && text->find(part) != std::string::npos;
}

}  // namespace

#if LIBE3_HAS_ZMQ
static void run_refusals(EncodingFormat encoding, const char* tag) {
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
        c.ran_identifier      = "refusal-test";
        c.dapp_name           = "RefusalDApp";
        c.dapp_version        = "1.0.0";
        return c;
    };

    E3Agent ran(make_config(E3Role::RAN));
    auto sm_owner = std::make_unique<ServedSM>();
    ServedSM* sm = sm_owner.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(sm_owner))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP));
    Heard heard;
    dapp.set_subscription_response_handler([&heard](const SubscriptionResponse& r) {
        std::lock_guard<std::mutex> lk(heard.mu);
        heard.subscriptions.push_back(r);
    });
    dapp.set_message_ack_handler([&heard](const MessageAck& a) {
        std::lock_guard<std::mutex> lk(heard.mu);
        heard.acks.push_back(a);
    });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));

    // One subscription request at a time, so the n-th response is the n-th request's.
    auto subscribe = [&](uint32_t rf, std::vector<uint32_t> telemetry,
                         std::vector<uint32_t> controls) {
        const size_t before = heard.subscription_count();
        ASSERT_EQ(static_cast<int>(dapp.subscribe(rf, std::move(telemetry), std::move(controls))),
                  static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_TRUE(wait_until([&] { return heard.subscription_count() > before; }, 5000ms));
    };
    auto control = [&](uint32_t rf, uint32_t control_id) {
        const size_t before = heard.ack_count();
        ASSERT_EQ(static_cast<int>(dapp.send_control(rf, control_id, {'{', '}'})),
                  static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_TRUE(wait_until([&] { return heard.ack_count() > before; }, 5000ms));
    };

    // (1) Subscriptions the RAN cannot serve.
    subscribe(999, {1}, {});
    auto unknown_function = heard.subscription(0);
    ASSERT_TRUE(unknown_function.response_code == ResponseCode::NEGATIVE);
    ASSERT_FALSE(unknown_function.subscription_id.has_value());
    ASSERT_TRUE(unknown_function.cause.has_value());
    ASSERT_TRUE(*unknown_function.cause == Cause::UNSUPPORTED_RAN_FUNCTION);
    ASSERT_TRUE(has(unknown_function.detail, "999"));

    subscribe(SERVED_RF, {}, {});
    auto nothing_asked = heard.subscription(1);
    ASSERT_TRUE(nothing_asked.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(nothing_asked.cause.has_value());
    ASSERT_TRUE(*nothing_asked.cause == Cause::INVALID_REQUEST);

    subscribe(SERVED_RF, {1, 7}, {});
    auto unknown_telemetry = heard.subscription(2);
    ASSERT_TRUE(unknown_telemetry.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(unknown_telemetry.cause.has_value());
    ASSERT_TRUE(*unknown_telemetry.cause == Cause::UNSUPPORTED_IDENTIFIER);
    ASSERT_TRUE(has(unknown_telemetry.detail, "telemetry identifier 7"));

    subscribe(SERVED_RF, {1}, {SERVED_CONTROL, 99});
    auto unknown_control = heard.subscription(3);
    ASSERT_TRUE(unknown_control.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(unknown_control.cause.has_value());
    ASSERT_TRUE(*unknown_control.cause == Cause::UNSUPPORTED_IDENTIFIER);
    ASSERT_TRUE(has(unknown_control.detail, "control identifier 99"));

    // None of them left anything behind.
    ASSERT_EQ(ran.subscription_count(), 0u);
    ASSERT_EQ(sm->starts.load(), 0);
    ASSERT_FALSE(sm->is_running());
    ASSERT_TRUE(dapp.subscribed_ran_functions().empty());

    // (2) Controls the RAN cannot hand on.
    control(999, SERVED_CONTROL);
    auto control_unknown_function = heard.ack(0);
    ASSERT_TRUE(control_unknown_function.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(control_unknown_function.cause.has_value());
    ASSERT_TRUE(*control_unknown_function.cause == Cause::UNSUPPORTED_RAN_FUNCTION);

    control(SERVED_RF, 55);
    auto control_unknown_id = heard.ack(1);
    ASSERT_TRUE(control_unknown_id.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(control_unknown_id.cause.has_value());
    ASSERT_TRUE(*control_unknown_id.cause == Cause::UNSUPPORTED_IDENTIFIER);

    control(SERVED_RF, SERVED_CONTROL);  // no subscription, so the SM is not running
    auto control_without_subscription = heard.ack(2);
    ASSERT_TRUE(control_without_subscription.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(control_without_subscription.cause.has_value());
    ASSERT_TRUE(*control_without_subscription.cause == Cause::NOT_FOUND);
    ASSERT_EQ(sm->controls.load(), 0);

    // (3) What the function advertises is granted, and its control gets through.
    subscribe(SERVED_RF, {1, 2}, {SERVED_CONTROL});
    auto granted = heard.subscription(4);
    ASSERT_TRUE(granted.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(granted.subscription_id.has_value());
    ASSERT_FALSE(granted.cause.has_value());
    ASSERT_FALSE(granted.detail.has_value());
    ASSERT_TRUE(wait_until([&] { return sm->is_running(); }, 2000ms));
    ASSERT_EQ(ran.subscription_count(), 1u);

    const size_t acks_before = heard.ack_count();
    ASSERT_EQ(static_cast<int>(dapp.send_control(SERVED_RF, SERVED_CONTROL, {'{', '}'})),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return sm->controls.load() == 1; }, 5000ms));
    // The SM took it and sent no ack of its own, and the RAN refused nothing.
    std::this_thread::sleep_for(200ms);
    ASSERT_EQ(heard.ack_count(), acks_before);

    dapp.stop();
    ran.stop();
}

TEST(Refusal_zmq_ranRefusesWhatItDoesNotServeAndSaysWhy) {
    // The encodings this build has are found by asking for them, as the
    // equivalence suite does, so the test follows the build options.
    struct Choice { EncodingFormat format; const char* tag; };
    bool any = false;
    for (const Choice& c : {Choice{EncodingFormat::ASN1, "asn1"}, Choice{EncodingFormat::JSON, "json"},
                            Choice{EncodingFormat::PROTOBUF, "pb"}}) {
        if (create_encoder(c.format) == nullptr) continue;
        run_refusals(c.format, c.tag);
        any = true;
    }
    ASSERT_TRUE(any);
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
