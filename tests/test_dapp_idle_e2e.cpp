/**
 * @file test_dapp_idle_e2e.cpp
 * @brief A RAN releases and removes a dApp that has sent nothing for too long.
 *
 * A real RAN E3Agent and real dApp E3Agents over ZMQ IPC, with the RAN's idle
 * time at 3 s. Properties verified:
 *   (1) a dApp that goes quiet after subscribing is told with a release, so it
 *       reports RELEASED_BY_RAN, and no sooner than the idle time after its last
 *       message;
 *   (2) the RAN then holds nothing for it: no registration, no subscription,
 *       and the RAN function it alone used is stopped, with the status handler
 *       told;
 *   (3) a second dApp that keeps sending reports is untouched, though the RAN
 *       sends it nothing and it receives nothing either;
 *   (4) an idle time of 0 never removes a quiet dApp.
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
#include <condition_variable>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t QUIET_RF = 100;
constexpr uint32_t CHATTY_RF = 101;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/dappidle_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
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

    std::atomic<int> starts{0};
    std::atomic<int> stops{0};

private:
    uint32_t rf_;
    std::atomic<bool> running_{false};
};

/// Records disconnect callbacks and lets the test wait for them.
struct DisconnectLog {
    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    DisconnectReason last = DisconnectReason::CONNECTION_LOST;

    void record(DisconnectReason r) {
        std::lock_guard<std::mutex> lk(mu);
        ++count;
        last = r;
        cv.notify_all();
    }
    bool wait_for(int n, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lk(mu);
        return cv.wait_for(lk, timeout, [&] { return count >= n; });
    }
    int seen() {
        std::lock_guard<std::mutex> lk(mu);
        return count;
    }
};

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

struct Endpoints {
    std::string setup = unique_ipc("setup");
    std::string sub = unique_ipc("sub");
    std::string pub = unique_ipc("pub");
};

E3Config make_config(E3Role role, const Endpoints& ep, uint32_t idle_s) {
    E3Config c;
    c.role                = role;
    c.link_layer          = E3LinkLayer::ZMQ;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = pick_encoding();
    c.log_level           = 0;
    c.ran_identifier      = "dappidle-test";
    c.dapp_name           = "IdleDApp";
    c.dapp_version        = "1.0.0";
    c.dapp_idle_timeout_s = idle_s;
    // A released dApp would register again and be counted again;
    // test_dapp_reconnect.cpp covers that.
    c.reconnect_interval_ms = 0;
    return c;
}

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(DAppIdle_zmq_quietDAppIsReleasedAndRemovedAndAChattyOneStays) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const Endpoints ep;

    E3Agent ran(make_config(E3Role::RAN, ep, 3));
    auto quiet_sm = std::make_unique<CountingSM>(QUIET_RF);
    auto chatty_sm = std::make_unique<CountingSM>(CHATTY_RF);
    CountingSM* quiet_rf = quiet_sm.get();
    CountingSM* chatty_rf = chatty_sm.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(quiet_sm))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(chatty_sm))),
              static_cast<int>(ErrorCode::SUCCESS));
    std::atomic<int> status_changes{0};
    ran.set_dapp_status_changed_handler([&] { ++status_changes; });
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent quiet(make_config(E3Role::DAPP, ep, 0));
    E3Agent chatty(make_config(E3Role::DAPP, ep, 0));
    DisconnectLog quiet_log;
    DisconnectLog chatty_log;
    quiet.set_disconnect_handler([&](DisconnectReason r) { quiet_log.record(r); });
    chatty.set_disconnect_handler([&](DisconnectReason r) { chatty_log.record(r); });
    for (E3Agent* d : {&quiet, &chatty}) {
        ASSERT_EQ(static_cast<int>(d->start()), static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_EQ(static_cast<int>(d->wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    }
    const uint32_t chatty_id = *chatty.dapp_id();

    ASSERT_EQ(static_cast<int>(quiet.subscribe(QUIET_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    const auto last_message = std::chrono::steady_clock::now();
    ASSERT_EQ(static_cast<int>(chatty.subscribe(CHATTY_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 2u; }, 15000ms));
    ASSERT_TRUE(quiet_rf->is_running());
    ASSERT_TRUE(chatty_rf->is_running());
    std::this_thread::sleep_for(100ms);
    const int changes_before = status_changes.load();

    // The chatty dApp sends a report every 250 ms for as long as the test runs.
    std::atomic<bool> keep_sending{true};
    std::thread talker([&] {
        uint32_t sequence = 1;
        while (keep_sending.load()) {
            chatty.send_report(CHATTY_RF, {'{', '}'}, sequence++);
            std::this_thread::sleep_for(250ms);
        }
    });

    // (1) The quiet dApp is told, and not before the idle time has passed.
    const bool released = quiet_log.wait_for(1, 30000ms);
    const auto released_after = std::chrono::steady_clock::now() - last_message;
    keep_sending = false;
    talker.join();
    ASSERT_TRUE(released);
    ASSERT_TRUE(released_after >= 3s);
    ASSERT_TRUE(quiet_log.last == DisconnectReason::RELEASED_BY_RAN);
    ASSERT_TRUE(wait_until([&] { return !quiet.dapp_id().has_value(); }, 2000ms));

    // (2) The RAN holds nothing for it, and its RAN function is stopped.
    ASSERT_EQ(ran.dapp_count(), 1u);
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_TRUE(ran.get_ran_function_subscribers(QUIET_RF).empty());
    ASSERT_TRUE(wait_until([&] { return !quiet_rf->is_running(); }, 2000ms));
    ASSERT_EQ(quiet_rf->stops.load(), 1);
    ASSERT_TRUE(status_changes.load() > changes_before);

    // (3) The chatty one is as it was, and never heard of a release.
    ASSERT_TRUE(chatty.dapp_id().has_value());
    ASSERT_EQ(*chatty.dapp_id(), chatty_id);
    ASSERT_EQ(ran.get_ran_function_subscribers(CHATTY_RF).size(), 1u);
    ASSERT_TRUE(chatty_rf->is_running());
    ASSERT_EQ(chatty_rf->stops.load(), 0);
    ASSERT_EQ(chatty_log.seen(), 0);

    chatty.stop();
    quiet.stop();
    ran.stop();
}

TEST(DAppIdle_zmq_zeroNeverRemovesAQuietDApp) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;

    E3Agent ran(make_config(E3Role::RAN, ep, 0));
    auto sm = std::make_unique<CountingSM>(QUIET_RF);
    CountingSM* rf = sm.get();
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::move(sm))), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP, ep, 0));
    DisconnectLog log;
    dapp.set_disconnect_handler([&](DisconnectReason r) { log.record(r); });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.subscribe(QUIET_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 1u; }, 15000ms));

    // More than the 3 s the other test needs to remove a quiet dApp.
    ASSERT_FALSE(log.wait_for(1, 5000ms));
    ASSERT_EQ(ran.dapp_count(), 1u);
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_TRUE(rf->is_running());
    ASSERT_TRUE(dapp.dapp_id().has_value());

    dapp.stop();
    ran.stop();
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
