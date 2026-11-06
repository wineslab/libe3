/**
 * @file test_dapp_reconnect.cpp
 * @brief A dApp that lost its RAN registers with it again by itself.
 *
 * A real RAN E3Agent and a real dApp E3Agent, with the dApp's retry interval
 * at 200 ms. Properties verified:
 *   (1) after the RAN goes away and a new one comes up on the same endpoints,
 *       the dApp is registered with it again, without stop() and start(), and
 *       hears of it through the setup response handler, and can subscribe again;
 *   (2) after the RAN releases the dApp for being idle, the dApp is registered
 *       again, and had been told with RELEASED_BY_RAN first;
 *   (3) a dApp that started before any RAN exists registers when one comes up;
 *   (4) a dApp that released itself is not registered again;
 *   (5) stop() ends the retrying, and does not wait for a RAN;
 *   (6) with the interval at 0 the dApp stays down, as before.
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
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t RF = 100;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/reconnect_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
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
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }

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

struct Endpoints {
    std::string setup = unique_ipc("setup");
    std::string sub = unique_ipc("sub");
    std::string pub = unique_ipc("pub");
};

E3Config make_config(E3Role role, E3LinkLayer link, const Endpoints& ep, uint32_t idle_s,
                     uint32_t reconnect_ms) {
    E3Config c;
    c.role                = role;
    c.link_layer          = link;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = pick_encoding();
    c.log_level           = 0;
    c.ran_identifier      = "reconnect-test";
    c.dapp_name           = "ReconnectDApp";
    c.dapp_version        = "1.0.0";
    c.dapp_idle_timeout_s = idle_s;
    c.reconnect_interval_ms = reconnect_ms;
    return c;
}

std::unique_ptr<E3Agent> start_ran(E3LinkLayer link, const Endpoints& ep, uint32_t idle_s = 0) {
    auto ran = std::make_unique<E3Agent>(make_config(E3Role::RAN, link, ep, idle_s, 0));
    ran->register_sm(std::make_unique<CountingSM>());
    if (ran->start() != ErrorCode::SUCCESS) return nullptr;
    std::this_thread::sleep_for(100ms);
    return ran;
}

/// Records what the dApp is told, from whichever thread tells it.
struct DAppLog {
    std::mutex mu;
    std::condition_variable cv;
    int disconnects = 0;
    int setups = 0;
    DisconnectReason last = DisconnectReason::CONNECTION_LOST;

    void attach(E3Agent& dapp) {
        dapp.set_disconnect_handler([this](DisconnectReason r) {
            std::lock_guard<std::mutex> lk(mu);
            ++disconnects;
            last = r;
            cv.notify_all();
        });
        dapp.set_setup_response_handler([this](const SetupResponse&) {
            std::lock_guard<std::mutex> lk(mu);
            ++setups;
            cv.notify_all();
        });
    }
    bool wait_for_disconnects(int n, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lk(mu);
        return cv.wait_for(lk, timeout, [&] { return disconnects >= n; });
    }
    bool wait_for_setups(int n, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lk(mu);
        return cv.wait_for(lk, timeout, [&] { return setups >= n; });
    }
    int disconnect_count() {
        std::lock_guard<std::mutex> lk(mu);
        return disconnects;
    }
};

void run_ran_restart(E3LinkLayer link) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const Endpoints ep;
    auto ran = start_ran(link, ep);
    ASSERT_TRUE(ran != nullptr);

    E3Agent dapp(make_config(E3Role::DAPP, link, ep, 0, 200));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(log.wait_for_setups(1, 10000ms));

    ran->stop();
    ran.reset();
    ASSERT_TRUE(log.wait_for_disconnects(1, 20000ms));
    ASSERT_TRUE(wait_until([&] { return !dapp.dapp_id().has_value(); }, 10000ms));

    // (1) A new RAN, and nobody calls stop() or start().
    ran = start_ran(link, ep);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(wait_until([&] { return dapp.dapp_id().has_value(); }, 30000ms));
    ASSERT_TRUE(wait_until([&] { return ran->dapp_count() == 1u; }, 15000ms));
    ASSERT_TRUE(log.wait_for_setups(2, 10000ms));
    ASSERT_EQ(log.disconnect_count(), 1);

    // Its old subscriptions went with the RAN; it can ask for a new one.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran->subscription_count() == 1u; }, 15000ms));

    dapp.stop();
    ran->stop();
}

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(DAppReconnect_zmq_registersWithANewRanByItself) {
    run_ran_restart(E3LinkLayer::ZMQ);
}

TEST(DAppReconnect_zmq_registersAgainAfterTheRanReleasedItForBeingIdle) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    auto ran = start_ran(E3LinkLayer::ZMQ, ep, 2);
    ASSERT_TRUE(ran != nullptr);

    E3Agent dapp(make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep, 0, 200));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(log.wait_for_setups(1, 10000ms));

    // (2) It sends nothing, so the RAN releases it, and it comes back.
    ASSERT_TRUE(log.wait_for_disconnects(1, 30000ms));
    {
        std::lock_guard<std::mutex> lk(log.mu);
        ASSERT_TRUE(log.last == DisconnectReason::RELEASED_BY_RAN);
    }
    ASSERT_TRUE(log.wait_for_setups(2, 20000ms));
    ASSERT_TRUE(wait_until([&] { return dapp.dapp_id().has_value(); }, 15000ms));
    ASSERT_TRUE(wait_until([&] { return ran->dapp_count() == 1u; }, 15000ms));

    dapp.stop();
    ran->stop();
}

TEST(DAppReconnect_zmq_aDAppStartedBeforeAnyRanRegistersWhenOneComesUp) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;

    E3Agent dapp(make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep, 0, 200));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(500ms);
    ASSERT_FALSE(dapp.dapp_id().has_value());

    // (3) Setup gives up after about ten seconds and starts over; the RAN
    // arrives in between.
    auto ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(wait_until([&] { return dapp.dapp_id().has_value(); }, 40000ms));
    ASSERT_TRUE(wait_until([&] { return ran->dapp_count() == 1u; }, 15000ms));

    dapp.stop();
    ran->stop();
}

TEST(DAppReconnect_zmq_aDAppThatReleasedItselfStaysReleased) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    auto ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);

    E3Agent dapp(make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep, 0, 200));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));

    // (4)
    ASSERT_EQ(static_cast<int>(dapp.release()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran->dapp_count() == 0u; }, 10000ms));
    std::this_thread::sleep_for(2000ms);
    ASSERT_EQ(ran->dapp_count(), 0u);
    ASSERT_EQ(log.disconnect_count(), 0);

    dapp.stop();
    ran->stop();
}

TEST(DAppReconnect_zmq_stopEndsTheRetryingWithoutWaitingForARan) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    auto ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);

    E3Agent dapp(make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep, 0, 200));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));

    ran->stop();
    ran.reset();
    ASSERT_TRUE(log.wait_for_disconnects(1, 20000ms));
    std::this_thread::sleep_for(500ms);  // it is retrying against nothing now

    // (5)
    const auto before = std::chrono::steady_clock::now();
    dapp.stop();
    ASSERT_TRUE(std::chrono::steady_clock::now() - before < 5s);

    // A RAN that comes up afterwards finds no dApp.
    ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);
    std::this_thread::sleep_for(2000ms);
    ASSERT_EQ(ran->dapp_count(), 0u);

    ran->stop();
}

TEST(DAppReconnect_zmq_zeroKeepsTheDAppDown) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    auto ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);

    E3Agent dapp(make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep, 0, 0));
    DAppLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));

    ran->stop();
    ran.reset();
    ASSERT_TRUE(log.wait_for_disconnects(1, 20000ms));

    // (6)
    ran = start_ran(E3LinkLayer::ZMQ, ep);
    ASSERT_TRUE(ran != nullptr);
    std::this_thread::sleep_for(3000ms);
    ASSERT_FALSE(dapp.dapp_id().has_value());
    ASSERT_EQ(ran->dapp_count(), 0u);

    dapp.stop();
    ran->stop();
}
#endif

TEST(DAppReconnect_posix_registersWithANewRanByItself) {
    run_ran_restart(E3LinkLayer::POSIX);
}

int main() {
    return RUN_ALL_TESTS();
}
