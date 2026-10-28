/**
 * @file test_subscription_time_e2e.cpp
 * @brief A RAN ends a subscription when the dApp's subscriptionTime runs out.
 *
 * A real RAN E3Agent and a real dApp E3Agent over ZMQ IPC. The dApp asks for
 * RAN function 100 for 2 s and for RAN function 101 with no time at all.
 * Properties verified:
 *   (1) both subscriptions are in place right after they are granted;
 *   (2) the timed one is gone no sooner than 2 s after the request and soon
 *       after, its SM is stopped and the status handler is told;
 *   (3) the other subscription, its SM and the dApp's session are untouched;
 *   (4) the dApp forgets the expired subscription too, without a message from
 *       the RAN, and keeps the other;
 *   (5) the dApp can subscribe to the expired RAN function again, for a short
 *       time of its own, and keep renewing it: it forgets a subscription only
 *       after the RAN has dropped it, and the last one ends without a release.
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
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t TIMED_RF = 100;
constexpr uint32_t UNTIMED_RF = 101;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/subtime_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
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
    explicit CountingSM(uint32_t rf) : rf_(rf) {}
    std::string name() const override { return "CountingSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return rf_; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; ++starts; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; ++stops; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }

    /// The dApps this SM would address an indication to.
    std::vector<uint32_t> subscribers() const { return get_subscribers(); }

    std::atomic<int> starts{0};
    std::atomic<int> stops{0};

private:
    uint32_t rf_;
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

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(SubscriptionTime_zmq_ranEndsTheSubscriptionWhenItsTimeIsUp) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const std::string setup = unique_ipc("setup");
    const std::string sub = unique_ipc("sub");
    const std::string pub = unique_ipc("pub");

    auto make_config = [&](E3Role role) {
        E3Config c;
        c.role                = role;
        c.link_layer          = E3LinkLayer::ZMQ;
        c.transport_layer     = E3TransportLayer::IPC;
        c.setup_endpoint      = setup;
        c.subscriber_endpoint = sub;
        c.publisher_endpoint  = pub;
        c.encoding            = pick_encoding();
        c.log_level           = 0;
        c.ran_identifier      = "subtime-test";
        c.dapp_name           = "SubTimeDApp";
        c.dapp_version        = "1.0.0";
        return c;
    };

    E3Agent ran(make_config(E3Role::RAN));
    auto timed_sm = std::make_unique<CountingSM>(TIMED_RF);
    auto untimed_sm = std::make_unique<CountingSM>(UNTIMED_RF);
    CountingSM* timed = timed_sm.get();
    CountingSM* untimed = untimed_sm.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(timed_sm))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(untimed_sm))),
              static_cast<int>(ErrorCode::SUCCESS));

    std::atomic<int> status_changes{0};
    ran.set_dapp_status_changed_handler([&] { ++status_changes; });
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP));
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    const uint32_t dapp_id = *dapp.dapp_id();

    const auto requested = std::chrono::steady_clock::now();
    ASSERT_EQ(static_cast<int>(dapp.subscribe(TIMED_RF, {1}, {}, 2u)),
              static_cast<int>(ErrorCode::SUCCESS));
    // No subscriptionTime at all: the same as 0, no limit.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(UNTIMED_RF, {1}, {})),
              static_cast<int>(ErrorCode::SUCCESS));

    // (1) Both are granted, their SMs run, and the SM would address the dApp.
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 2u; }, 5000ms));
    ASSERT_TRUE(wait_until([&] { return dapp.subscribed_ran_functions().size() == 2u; }, 5000ms));
    ASSERT_TRUE(timed->is_running());
    ASSERT_TRUE(untimed->is_running());
    ASSERT_EQ(timed->subscribers().size(), 1u);
    // The handler fires just after a subscription is added; let both settle.
    std::this_thread::sleep_for(100ms);
    const int changes_before_expiry = status_changes.load();

    // (2) The timed one ends, and not before its time.
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 1u; }, 8000ms));
    ASSERT_TRUE(std::chrono::steady_clock::now() - requested >= 2s);
    ASSERT_TRUE(ran.get_ran_function_subscribers(TIMED_RF).empty());
    ASSERT_TRUE(wait_until([&] { return !timed->is_running(); }, 2000ms));
    ASSERT_EQ(timed->stops.load(), 1);
    ASSERT_TRUE(timed->subscribers().empty());
    // The status handler hears of it, as it would of a delete.
    ASSERT_TRUE(status_changes.load() > changes_before_expiry);

    // (3) The rest is as it was.
    ASSERT_EQ(ran.get_ran_function_subscribers(UNTIMED_RF).size(), 1u);
    ASSERT_TRUE(untimed->is_running());
    ASSERT_EQ(untimed->stops.load(), 0);
    ASSERT_EQ(ran.dapp_count(), 1u);
    ASSERT_TRUE(dapp.dapp_id().has_value());
    ASSERT_EQ(*dapp.dapp_id(), dapp_id);

    // (4) The dApp's own view agrees. It had no message to go by.
    ASSERT_TRUE(wait_until([&] { return dapp.subscribed_ran_functions().size() == 1u; }, 2000ms));
    ASSERT_EQ(dapp.subscribed_ran_functions()[0], UNTIMED_RF);
    ASSERT_EQ(static_cast<int>(dapp.unsubscribe(TIMED_RF)),
              static_cast<int>(ErrorCode::SUBSCRIPTION_NOT_FOUND));

    // (5) The same RAN function can be subscribed to again, each time for 1 s,
    // and a dApp that renews the moment its own deadline passes is granted a
    // fresh subscription every time. By the time the dApp has forgotten the
    // subscription the RAN must already have let go of it, else the renewal
    // would be answered with the dying one and cut short.
    for (int round = 0; round < 2; ++round) {
        const auto asked = std::chrono::steady_clock::now();
        ASSERT_EQ(static_cast<int>(dapp.subscribe(TIMED_RF, {1}, {}, 1u)),
                  static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_TRUE(wait_until(
            [&] { return ran.get_ran_function_subscribers(TIMED_RF).size() == 1u; }, 5000ms));
        ASSERT_TRUE(wait_until(
            [&] { return dapp.subscribed_ran_functions().size() == 2u; }, 5000ms));
        ASSERT_TRUE(timed->is_running());

        ASSERT_TRUE(wait_until(
            [&] { return dapp.subscribed_ran_functions().size() == 1u; }, 5000ms));
        ASSERT_TRUE(std::chrono::steady_clock::now() - asked >= 1s);
        ASSERT_TRUE(ran.get_ran_function_subscribers(TIMED_RF).empty());
    }

    // The last one ends by itself too, so nothing is left to release at the end
    // but the subscription that never had a time.
    ASSERT_TRUE(wait_until([&] { return !timed->is_running(); }, 2000ms));
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_TRUE(untimed->is_running());

    dapp.stop();
    ran.stop();
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
