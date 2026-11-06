/**
 * @file test_subscription_repeat_e2e.cpp
 * @brief A RAN answers a repeated subscription with the one it already holds.
 *
 * A real RAN E3Agent and a real dApp E3Agent over ZMQ IPC. Properties verified:
 *   (1) a dApp that subscribes again to a RAN function gets the same
 *       subscription id, with the lists and the periodicity the RAN holds, not
 *       the ones of the second request; the RAN still has one subscription and
 *       its SM was started once;
 *   (2) a repeat the RAN refuses (an identifier the function does not
 *       advertise) leaves the subscription as it was;
 *   (3) a dApp that registers again gets a new registration, not the old
 *       subscription: its first request is answered with what it asked for.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
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

constexpr uint32_t RF = 100;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/subrepeat_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

EncodingFormat pick_encoding() {
#if defined(LIBE3_ENABLE_JSON)
    return EncodingFormat::JSON;
#else
    return EncodingFormat::ASN1;
#endif
}

class CountingSM : public ServiceModel {
public:
    std::string name() const override { return "CountingSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return RF; }
    std::vector<uint32_t> telemetry_ids() const override { return {1, 2, 3}; }
    std::vector<uint32_t> control_ids() const override { return {}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; ++starts; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }

    std::atomic<int> starts{0};

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

/// The subscription responses a dApp was sent, in order.
struct Responses {
    std::mutex mu;
    std::vector<SubscriptionResponse> all;

    void attach(E3Agent& dapp) {
        dapp.set_subscription_response_handler([this](const SubscriptionResponse& r) {
            std::lock_guard<std::mutex> lk(mu);
            all.push_back(r);
        });
    }
    size_t count() {
        std::lock_guard<std::mutex> lk(mu);
        return all.size();
    }
    SubscriptionResponse at(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return all.at(i);
    }
};

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(SubscriptionRepeat_zmq_ranAnswersWithTheSubscriptionItHolds) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const std::string setup = unique_ipc("setup");
    const std::string sub = unique_ipc("sub");
    const std::string pub = unique_ipc("pub");

    auto make_config = [&](E3Role role) {
        E3Config c;
        c.role                  = role;
        c.link_layer            = E3LinkLayer::ZMQ;
        c.transport_layer       = E3TransportLayer::IPC;
        c.setup_endpoint        = setup;
        c.subscriber_endpoint   = sub;
        c.publisher_endpoint    = pub;
        c.encoding              = pick_encoding();
        c.log_level             = 0;
        c.ran_identifier        = "subrepeat-test";
        c.dapp_name             = "SubRepeatDApp";
        c.dapp_version          = "1.0.0";
        c.reconnect_interval_ms = 0;
        return c;
    };

    E3Agent ran(make_config(E3Role::RAN));
    auto sm_owned = std::make_unique<CountingSM>();
    CountingSM* sm = sm_owned.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(sm_owned))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP));
    Responses responses;
    responses.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));

    // (1) The first request, then a second one that asks for other things.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {1, 2}, {}, std::nullopt, 5000u)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return responses.count() == 1u; }, 5000ms));
    const SubscriptionResponse first = responses.at(0);
    ASSERT_TRUE(first.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(first.subscription_id.has_value());

    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {3}, {}, std::nullopt, 9000u)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return responses.count() == 2u; }, 5000ms));
    const SubscriptionResponse again = responses.at(1);
    ASSERT_TRUE(again.response_code == ResponseCode::POSITIVE);
    ASSERT_EQ(*again.subscription_id, *first.subscription_id);
    ASSERT_TRUE(again.telemetry_identifier_list.has_value());
    ASSERT_EQ(again.telemetry_identifier_list->size(), 2u);
    ASSERT_EQ((*again.telemetry_identifier_list)[0], 1u);
    ASSERT_EQ((*again.telemetry_identifier_list)[1], 2u);
    ASSERT_TRUE(again.periodicity.has_value());
    ASSERT_EQ(*again.periodicity, 5000u);

    const uint32_t dapp_id = *dapp.dapp_id();
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_EQ(ran.get_subscription_periodicity(dapp_id, RF), 5000u);
    ASSERT_EQ(sm->starts.load(), 1);

    // (2) A repeat the RAN refuses changes nothing.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {77}, {}, std::nullopt, 1000u)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return responses.count() == 3u; }, 5000ms));
    const SubscriptionResponse refused = responses.at(2);
    ASSERT_TRUE(refused.response_code == ResponseCode::NEGATIVE);
    ASSERT_TRUE(refused.cause.has_value());
    ASSERT_TRUE(*refused.cause == Cause::UNSUPPORTED_IDENTIFIER);
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_EQ(ran.get_subscription_periodicity(dapp_id, RF), 5000u);

    // (3) The dApp registers again, with a new id and none of the old
    // subscriptions, and is answered with what it asks for.
    dapp.stop();
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {3}, {}, std::nullopt, 9000u)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return responses.count() == 4u; }, 5000ms));
    const SubscriptionResponse fresh = responses.at(3);
    ASSERT_TRUE(fresh.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(fresh.telemetry_identifier_list.has_value());
    ASSERT_EQ(fresh.telemetry_identifier_list->size(), 1u);
    ASSERT_EQ((*fresh.telemetry_identifier_list)[0], 3u);
    ASSERT_TRUE(fresh.periodicity.has_value());
    ASSERT_EQ(*fresh.periodicity, 9000u);

    dapp.stop();
    ran.stop();
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
