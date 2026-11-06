/**
 * @file test_c_api_granted_periodicity.cpp
 * @brief A Service Model written in C reports the interval it keeps.
 *
 * A RAN created through the C API over ZMQ IPC, with four C Service Models, and
 * a dApp created through the C++ API. Properties verified:
 *   (1) a model that sets no callback: the response has no periodicity;
 *   (2) a model whose callback answers 1000: the response says 1000, and the
 *       callback is given the periodicity the dApp asked for;
 *   (3) callbacks that answer 0 and 70000000: the response has none;
 *   (4) the setter refuses a NULL handle and a model that is already registered.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/c_api.h"
#include "libe3/e3_agent.hpp"
#include "libe3/types.hpp"

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

constexpr uint32_t PLAIN_RF = 110;
constexpr uint32_t FLOOR_RF = 111;
constexpr uint32_t ZERO_RF = 112;
constexpr uint32_t TOO_LONG_RF = 113;

std::atomic<uint32_t> g_requested_seen{0};

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/capi_grantper_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

#if defined(LIBE3_ENABLE_JSON)
constexpr int C_ENCODING = 1;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::JSON;
#else
constexpr int C_ENCODING = 0;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::ASN1;
#endif

e3_error_t sm_ok(void*) { return 0; }
void sm_void(void*) {}
int sm_running(void*) { return 1; }
e3_error_t sm_control(e3_service_model_handle_t*, void*, uint32_t, uint32_t, uint32_t, uint32_t,
                      const uint8_t*, size_t, uint32_t) {
    return 0;
}

uint32_t grant_floor(void*, uint32_t requested_us) {
    g_requested_seen.store(requested_us);
    return requested_us < 1000u ? 1000u : requested_us;
}
uint32_t grant_zero(void*, uint32_t) { return 0; }
uint32_t grant_too_long(void*, uint32_t) { return 70000000u; }

/// A C Service Model for `rf`. `cb` may be null: no hook is set.
e3_service_model_handle_t* make_sm(uint32_t rf, e3_sm_granted_periodicity_cb cb) {
    static const uint32_t telemetry[] = {1};
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    static const uint8_t rfd[] = {'{', '}'};
    e3_c_service_model_desc_t d{};
    d.name = "CPacedSM";
    d.version = 1;
    d.ran_function_id = rf;
    d.telemetry_ids = telemetry;
    d.telemetry_ids_len = 1;
    d.ran_function_data = rfd;
    d.ran_function_data_len = sizeof(rfd);
    d.sm_init = sm_ok;
    d.sm_destroy = sm_void;
    d.sm_start = sm_ok;
    d.sm_stop = sm_void;
    d.sm_is_running = sm_running;
    d.sm_process_control = sm_control;
    e3_service_model_handle_t* sm = e3_service_model_create_from_c(&d);
    if (sm && cb) {
        if (e3_service_model_set_granted_periodicity_cb(sm, cb) != 0) return nullptr;
    }
    return sm;
}

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

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(CApiGrantedPeriodicity_aCServiceModelReportsTheIntervalItKeeps) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const std::string setup = unique_ipc("setup");
    const std::string sub = unique_ipc("sub");
    const std::string pub = unique_ipc("pub");

    e3_config_t cfg{};
    cfg.ran_identifier = "capi-grantper-test";
    cfg.link_layer = 0;       // ZMQ
    cfg.transport_layer = 2;  // IPC
    cfg.setup_endpoint = setup.c_str();
    cfg.subscriber_endpoint = sub.c_str();
    cfg.publisher_endpoint = pub.c_str();
    cfg.encoding = C_ENCODING;
    cfg.log_level = 0;
    e3_agent_handle_t* ran = e3_agent_create_with_config(&cfg);
    ASSERT_TRUE(ran != nullptr);

    // (4) The setter refuses NULL, and a model that the agent already owns.
    ASSERT_EQ(e3_service_model_set_granted_periodicity_cb(nullptr, grant_floor),
              static_cast<int>(ErrorCode::INVALID_PARAM));

    struct Sm {
        uint32_t rf;
        e3_sm_granted_periodicity_cb cb;
    };
    const Sm sms[] = {{PLAIN_RF, nullptr},
                      {FLOOR_RF, grant_floor},
                      {ZERO_RF, grant_zero},
                      {TOO_LONG_RF, grant_too_long}};
    ASSERT_EQ(e3_agent_init(ran), 0);
    e3_service_model_handle_t* last = nullptr;
    for (const Sm& s : sms) {
        e3_service_model_handle_t* h = make_sm(s.rf, s.cb);
        ASSERT_TRUE(h != nullptr);
        ASSERT_EQ(e3_agent_register_sm(ran, h), 0);
        last = h;
    }
    ASSERT_EQ(e3_service_model_set_granted_periodicity_cb(last, grant_zero),
              static_cast<int>(ErrorCode::INVALID_PARAM));

    ASSERT_EQ(e3_agent_start(ran), 0);
    std::this_thread::sleep_for(100ms);

    E3Config dc;
    dc.role                = E3Role::DAPP;
    dc.link_layer          = E3LinkLayer::ZMQ;
    dc.transport_layer     = E3TransportLayer::IPC;
    dc.setup_endpoint      = setup;
    dc.subscriber_endpoint = sub;
    dc.publisher_endpoint  = pub;
    dc.encoding            = CPP_ENCODING;
    dc.log_level           = 0;
    dc.dapp_name           = "CApiGrantPerDApp";
    dc.dapp_version        = "1.0.0";
    dc.reconnect_interval_ms = 0;
    E3Agent dapp(dc);
    Responses responses;
    responses.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));

    auto ask = [&](uint32_t rf, uint32_t periodicity) {
        const size_t before = responses.count();
        ASSERT_EQ(static_cast<int>(dapp.subscribe(rf, {1}, {}, std::nullopt, periodicity)),
                  static_cast<int>(ErrorCode::SUCCESS));
        ASSERT_TRUE(wait_until([&] { return responses.count() == before + 1; }, 10000ms));
    };

    // (1) No callback: nothing is reported, whatever was asked.
    ask(PLAIN_RF, 5000u);
    SubscriptionResponse r = responses.at(0);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());

    // (2) A callback: the answer is the floor, and the callback saw the request.
    ask(FLOOR_RF, 500u);
    r = responses.at(1);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(r.periodicity.has_value());
    ASSERT_EQ(*r.periodicity, 1000u);
    ASSERT_EQ(g_requested_seen.load(), 500u);

    // (3) 0 and more than the grammar carries are left out; the response still arrives.
    ask(ZERO_RF, 5000u);
    r = responses.at(2);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());
    ask(TOO_LONG_RF, 5000u);
    r = responses.at(3);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_FALSE(r.periodicity.has_value());

    dapp.stop();
    e3_agent_stop(ran);
    e3_agent_destroy(ran);
}

#endif

int main() {
    return RUN_ALL_TESTS();
}
