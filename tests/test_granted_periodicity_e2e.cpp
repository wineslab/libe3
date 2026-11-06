/**
 * @file test_granted_periodicity_e2e.cpp
 * @brief The periodicity of a subscription response is the one the SM keeps.
 *
 * A real RAN E3Agent and a real dApp E3Agent over ZMQ IPC, once per encoding
 * that is built. Four RAN functions, whose SMs answer granted_periodicity()
 * differently. Properties verified:
 *   (1) an SM that does not override it: the response has no periodicity,
 *       although the dApp asked for one;
 *   (2) an SM that keeps a floor of 1000 us: the dApp asks for 500 and is told
 *       1000, and a repeat that asks for 9000 is told 1000 again, since the
 *       subscription holds 500;
 *   (3) an SM that says 0 (it keeps no interval): the response has none;
 *   (4) an SM that says more than the grammar carries: the response has none,
 *       and is delivered.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/types.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t PLAIN_RF = 100;
constexpr uint32_t FLOOR_RF = 101;
constexpr uint32_t ZERO_RF = 102;
constexpr uint32_t TOO_LONG_RF = 103;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/grantper_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

/// One RAN function. `grant` is what granted_periodicity() answers; empty keeps the default.
class PacedSM : public ServiceModel {
public:
    using Grant = std::function<std::optional<uint32_t>(uint32_t)>;
    PacedSM(uint32_t rf, Grant grant) : rf_(rf), grant_(std::move(grant)) {}
    std::string name() const override { return "PacedSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return rf_; }
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
    std::optional<uint32_t> granted_periodicity(uint32_t requested_us) const override {
        if (!grant_) return ServiceModel::granted_periodicity(requested_us);
        return grant_(requested_us);
    }

private:
    uint32_t rf_;
    Grant grant_;
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

void run(EncodingFormat encoding) {
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
        c.encoding              = encoding;
        c.log_level             = 0;
        c.ran_identifier        = "grantper-test";
        c.dapp_name             = "GrantPerDApp";
        c.dapp_version          = "1.0.0";
        c.reconnect_interval_ms = 0;
        return c;
    };

    E3Agent ran(make_config(E3Role::RAN));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::make_unique<PacedSM>(PLAIN_RF, nullptr))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::make_unique<PacedSM>(
                  FLOOR_RF, [](uint32_t requested) -> std::optional<uint32_t> {
                      return std::max<uint32_t>(requested, 1000u);
                  }))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::make_unique<PacedSM>(
                  ZERO_RF, [](uint32_t) -> std::optional<uint32_t> { return 0u; }))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::make_unique<PacedSM>(
                  TOO_LONG_RF, [](uint32_t) -> std::optional<uint32_t> { return 70000000u; }))),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP));
    Responses responses;
    responses.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));

    auto ask = [&](uint32_t rf, uint32_t periodicity) {
        const size_t before = responses.count();
        ASSERT_EQ(static_cast<int>(dapp.subscribe(rf, {1}, {}, std::nullopt, periodicity)),
                  static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_TRUE(wait_until([&] { return responses.count() == before + 1; }, 5000ms));
    };

    // (1) No override: nothing is reported, whatever was asked.
    ask(PLAIN_RF, 5000u);
    SubscriptionResponse r = responses.at(0);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());

    // (2) A floor: the answer is the floor, and a repeat is answered from what is held.
    ask(FLOOR_RF, 500u);
    r = responses.at(1);
    ASSERT_TRUE(r.periodicity.has_value());
    ASSERT_EQ(*r.periodicity, 1000u);
    ask(FLOOR_RF, 9000u);
    r = responses.at(2);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(r.periodicity.has_value());
    ASSERT_EQ(*r.periodicity, 1000u);

    // (3) 0 keeps no interval, which is what a missing value says.
    ask(ZERO_RF, 5000u);
    r = responses.at(3);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());

    // (4) More than the grammar carries is left out; the response still arrives.
    ask(TOO_LONG_RF, 5000u);
    r = responses.at(4);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());

    dapp.stop();
    ran.stop();
}

}  // namespace

#if LIBE3_HAS_ZMQ
#if defined(LIBE3_ENABLE_JSON)
TEST(GrantedPeriodicity_zmq_json_isWhatTheSmKeeps) {
    run(EncodingFormat::JSON);
}
#endif

#if defined(LIBE3_ENABLE_ASN1)
TEST(GrantedPeriodicity_zmq_asn1_isWhatTheSmKeeps) {
    run(EncodingFormat::ASN1);
}
#endif
#endif

int main() {
    return RUN_ALL_TESTS();
}
