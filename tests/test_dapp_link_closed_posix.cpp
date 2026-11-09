/**
 * @file test_dapp_link_closed_posix.cpp
 * @brief A RAN removes a dApp whose connection closed without a release.
 *
 * A real RAN E3Agent and real dApp E3Agents over POSIX IPC. A dApp that is
 * stopped without release() closes its sockets, which is all a crash does.
 * Properties verified:
 *   (1) the RAN removes it at once, with its idle limit off, so it is the
 *       closed connection and not the idle time that does it: no registration,
 *       no subscription, its RAN function stopped, the status handler told;
 *   (2) a second dApp on the same RAN keeps its subscription and its RAN
 *       function;
 *   (3) a dApp that sent nothing after its setup request is removed at once as
 *       well, because it is tied to its connection at setup and not by its
 *       first message;
 *   (4) a dApp that released first is not removed again;
 *   (5) a dApp that took the id of one that is gone is not removed when the
 *       old connection finally closes.
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
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t FIRST_RF = 100;
constexpr uint32_t SECOND_RF = 101;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/linkclosed_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
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
    c.link_layer          = E3LinkLayer::POSIX;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = pick_encoding();
    c.log_level           = 0;
    c.ran_identifier      = "linkclosed-test";
    c.dapp_name           = "LinkClosedDApp";
    c.dapp_version        = "1.0.0";
    c.dapp_idle_timeout_s = idle_s;
    return c;
}

/// A RAN with two counting RAN functions, started.
struct Ran {
    explicit Ran(const Endpoints& ep, uint32_t idle_s) : agent(make_config(E3Role::RAN, ep, idle_s)) {
        auto first_sm = std::make_unique<CountingSM>(FIRST_RF);
        auto second_sm = std::make_unique<CountingSM>(SECOND_RF);
        first = first_sm.get();
        second = second_sm.get();
        agent.register_sm(std::move(first_sm));
        agent.register_sm(std::move(second_sm));
        agent.set_dapp_status_changed_handler([this] { ++status_changes; });
        started = agent.start() == ErrorCode::SUCCESS;
        std::this_thread::sleep_for(100ms);
    }
    ~Ran() { agent.stop(); }

    E3Agent agent;
    CountingSM* first{nullptr};
    CountingSM* second{nullptr};
    std::atomic<int> status_changes{0};
    bool started{false};
};

/// A dApp that has completed setup.
std::unique_ptr<E3Agent> start_dapp(const Endpoints& ep) {
    auto dapp = std::make_unique<E3Agent>(make_config(E3Role::DAPP, ep, 0));
    if (dapp->start() != ErrorCode::SUCCESS) return nullptr;
    if (dapp->wait_for_setup(15000ms) != ErrorCode::SUCCESS) return nullptr;
    return dapp;
}

}  // namespace

TEST(DAppLinkClosed_posix_aStoppedDAppIsRemovedAtOnceAndANeighbourStays) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const Endpoints ep;
    Ran ran(ep, 0);  // idle limit off: only the closed connection can remove a dApp
    ASSERT_TRUE(ran.started);

    auto gone = start_dapp(ep);
    auto stays = start_dapp(ep);
    ASSERT_TRUE(gone != nullptr);
    ASSERT_TRUE(stays != nullptr);
    ASSERT_EQ(static_cast<int>(gone->subscribe(FIRST_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(stays->subscribe(SECOND_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.agent.subscription_count() == 2u; }, 15000ms));
    ASSERT_TRUE(ran.first->is_running());
    ASSERT_TRUE(ran.second->is_running());
    std::this_thread::sleep_for(100ms);
    const int changes_before = ran.status_changes.load();
    const uint32_t stays_id = *stays->dapp_id();

    // (1) No release: closing its sockets is all it does.
    gone->stop();
    ASSERT_TRUE(wait_until([&] { return ran.agent.dapp_count() == 1u; }, 10000ms));
    ASSERT_EQ(ran.agent.subscription_count(), 1u);
    ASSERT_TRUE(ran.agent.get_ran_function_subscribers(FIRST_RF).empty());
    ASSERT_TRUE(wait_until([&] { return !ran.first->is_running(); }, 10000ms));
    ASSERT_EQ(ran.first->stops.load(), 1);
    ASSERT_TRUE(ran.status_changes.load() > changes_before);

    // (2) The other one is as it was.
    ASSERT_TRUE(stays->dapp_id().has_value());
    ASSERT_EQ(*stays->dapp_id(), stays_id);
    ASSERT_EQ(ran.agent.get_ran_function_subscribers(SECOND_RF).size(), 1u);
    ASSERT_TRUE(ran.second->is_running());
    ASSERT_EQ(ran.second->stops.load(), 0);

    stays->stop();
}

TEST(DAppLinkClosed_posix_aDAppThatNeverSpokeIsRemovedAtOnce) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    Ran ran(ep, 0);  // idle limit off: only the closed connection can remove a dApp
    ASSERT_TRUE(ran.started);

    auto silent = start_dapp(ep);
    ASSERT_TRUE(silent != nullptr);
    ASSERT_EQ(ran.agent.dapp_count(), 1u);

    // (3) It sent only its setup request, and the RAN tied it to that connection.
    silent->stop();
    ASSERT_TRUE(wait_until([&] { return ran.agent.dapp_count() == 0u; }, 10000ms));
}

TEST(DAppLinkClosed_posix_aDAppThatReleasedIsNotRemovedAgainAndAnIdIsNotTakenOver) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    Ran ran(ep, 0);
    ASSERT_TRUE(ran.started);

    auto old_dapp = start_dapp(ep);
    ASSERT_TRUE(old_dapp != nullptr);
    const uint32_t id = *old_dapp->dapp_id();
    ASSERT_EQ(static_cast<int>(old_dapp->subscribe(FIRST_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.agent.subscription_count() == 1u; }, 15000ms));

    // (4) It releases, so the RAN removes it, and its connection closes later.
    ASSERT_EQ(static_cast<int>(old_dapp->release()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.agent.dapp_count() == 0u; }, 10000ms));
    ASSERT_TRUE(wait_until([&] { return !ran.first->is_running(); }, 10000ms));

    // (5) The next dApp is handed the same id and tied to its own connection at
    // setup. The old connection closing must not touch it.
    auto next = start_dapp(ep);
    ASSERT_TRUE(next != nullptr);
    ASSERT_EQ(*next->dapp_id(), id);
    ASSERT_EQ(static_cast<int>(next->subscribe(SECOND_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.agent.subscription_count() == 1u; }, 15000ms));
    ASSERT_TRUE(ran.second->is_running());

    old_dapp->stop();
    std::this_thread::sleep_for(1000ms);
    ASSERT_EQ(ran.agent.dapp_count(), 1u);
    ASSERT_EQ(ran.agent.get_ran_function_subscribers(SECOND_RF).size(), 1u);
    ASSERT_TRUE(ran.second->is_running());
    ASSERT_EQ(ran.second->stops.load(), 0);
    ASSERT_EQ(ran.first->stops.load(), 1);

    next->stop();
}

int main() {
    return RUN_ALL_TESTS();
}
