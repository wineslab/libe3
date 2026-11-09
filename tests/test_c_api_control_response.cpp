/**
 * @file test_c_api_control_response.cpp
 * @brief The C API answers a dApp control and asks a dApp to answer an xApp control.
 *
 * A RAN created through the C API over POSIX IPC, with a Service Model written
 * in C, and a dApp created through the C++ API. Properties verified:
 *   (1) e3_service_model_emit_control_response reaches a dApp that asked, with
 *       the code, the cause, the detail and the outcome it was given, under the
 *       id of the control and with the RAN function and control identifier of
 *       the control;
 *   (2) for a dApp that did not ask it returns NOT_FOUND and sends nothing;
 *   (3) it refuses a NULL handle and an outcome over 32768 bytes;
 *   (4) e3_agent_send_xapp_control returns the id of the control and carries
 *       request_ack, and the dApp's answer reaches the callback set with
 *       e3_agent_set_control_response_handler with every field;
 *   (5) both functions refuse a NULL agent.
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
#include <cstring>
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

constexpr uint32_t RF = 5;
constexpr uint32_t CONTROL = 3;
const char OUTCOME[] = "{\"ok\":1}";

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/capi_ctlresp_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

#if defined(LIBE3_ENABLE_JSON)
constexpr int C_ENCODING = 1;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::JSON;
const uint8_t FUNCTION_DATA[] = {'{', '}'};
#else
constexpr int C_ENCODING = 0;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::ASN1;
const uint8_t FUNCTION_DATA[] = {'x'};
#endif

/// What the C Service Model does, and what it saw.
struct SmState {
    std::atomic<bool> running{false};
    std::atomic<int> mode{0};  // 0 positive with outcome, 1 negative with reason
    std::atomic<int> controls{0};
    std::atomic<int> emit_rc{0};
    e3_service_model_handle_t* handle{nullptr};
};

SmState g_sm;

e3_error_t sm_init(void*) { return 0; }
void sm_destroy(void*) {}
e3_error_t sm_start(void*) { g_sm.running = true; return 0; }
void sm_stop(void*) { g_sm.running = false; }
int sm_is_running(void*) { return g_sm.running ? 1 : 0; }

e3_error_t sm_process_control(e3_service_model_handle_t* sm, void*, uint32_t request_id,
                              uint32_t dapp_id, uint32_t, uint32_t, const uint8_t*, size_t, uint32_t) {
    ++g_sm.controls;
    if (g_sm.mode == 0) {
        g_sm.emit_rc = e3_service_model_emit_control_response(
            sm, dapp_id, request_id, 0, -1, nullptr,
            reinterpret_cast<const uint8_t*>(OUTCOME), sizeof(OUTCOME) - 1);
    } else {
        g_sm.emit_rc = e3_service_model_emit_control_response(
            sm, dapp_id, request_id, 1, 7 /* invalidRequest */, "the scheduler refused it", nullptr, 0);
    }
    return 0;
}

/// What the RAN's callback heard from a dApp.
struct RanHeard {
    std::mutex mu;
    struct Item {
        uint32_t dapp_id, request_id, ran_function_id, control_id;
        int response_code, cause;
        std::string detail;
        bool has_detail;
        std::vector<uint8_t> outcome;
        bool has_outcome;
    };
    std::vector<Item> items;
    size_t count() {
        std::lock_guard<std::mutex> lk(mu);
        return items.size();
    }
    Item at(size_t i) {
        std::lock_guard<std::mutex> lk(mu);
        return items.at(i);
    }
};
RanHeard g_ran;

void on_response(uint32_t dapp_id, uint32_t request_id, uint32_t ran_function_id, uint32_t control_id,
                 int response_code, int cause, const char* detail, const uint8_t* outcome,
                 size_t outcome_len) {
    RanHeard::Item item{dapp_id, request_id, ran_function_id, control_id, response_code, cause,
                        detail ? detail : "", detail != nullptr,
                        outcome ? std::vector<uint8_t>(outcome, outcome + outcome_len) : std::vector<uint8_t>{},
                        outcome != nullptr};
    std::lock_guard<std::mutex> lk(g_ran.mu);
    g_ran.items.push_back(std::move(item));
}

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

std::mutex g_dapp_mu;
std::vector<ControlResponse> g_dapp_heard;
std::vector<XAppControlAction> g_dapp_xapp;

}  // namespace

TEST(CApiControlResponse_aCServiceModelAnswersAndTheRanHearsTheDAppsAnswer) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const std::string setup = unique_ipc("setup"), sub = unique_ipc("sub"), pub = unique_ipc("pub");

    e3_config_t cfg{};
    cfg.ran_identifier = "capi-ctlresp-test";
    cfg.link_layer = 1;       // POSIX
    cfg.transport_layer = 2;  // IPC
    cfg.setup_endpoint = setup.c_str();
    cfg.subscriber_endpoint = sub.c_str();
    cfg.publisher_endpoint = pub.c_str();
    cfg.encoding = C_ENCODING;
    cfg.log_level = 0;
    e3_agent_handle_t* ran = e3_agent_create_with_config(&cfg);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_EQ(e3_agent_init(ran), 0);

    const uint32_t telemetry[] = {1};
    const uint32_t controls[] = {CONTROL};
    e3_c_service_model_desc_t desc{};
    desc.name = "CSm";
    desc.version = 1;
    desc.ran_function_id = RF;
    desc.telemetry_ids = telemetry;
    desc.telemetry_ids_len = 1;
    desc.control_ids = controls;
    desc.control_ids_len = 1;
    desc.ran_function_data = FUNCTION_DATA;
    desc.ran_function_data_len = sizeof(FUNCTION_DATA);
    desc.sm_init = sm_init;
    desc.sm_destroy = sm_destroy;
    desc.sm_start = sm_start;
    desc.sm_stop = sm_stop;
    desc.sm_is_running = sm_is_running;
    desc.sm_process_control = sm_process_control;
    g_sm.handle = e3_service_model_create_from_c(&desc);
    ASSERT_TRUE(g_sm.handle != nullptr);
    ASSERT_EQ(e3_agent_register_sm(ran, g_sm.handle), 0);
    ASSERT_EQ(e3_agent_set_control_response_handler(ran, on_response), 0);
    ASSERT_EQ(e3_agent_start(ran), 0);
    std::this_thread::sleep_for(100ms);

    E3Config c;
    c.role                = E3Role::DAPP;
    c.link_layer          = E3LinkLayer::POSIX;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = setup;
    c.subscriber_endpoint = sub;
    c.publisher_endpoint  = pub;
    c.encoding            = CPP_ENCODING;
    c.log_level           = 0;
    c.dapp_name           = "CApiControlResponseDApp";
    c.dapp_version        = "1.0.0";
    c.reconnect_interval_ms = 0;
    E3Agent dapp(c);
    dapp.set_control_response_handler([](const ControlResponse& r) {
        std::lock_guard<std::mutex> lk(g_dapp_mu);
        g_dapp_heard.push_back(r);
    });
    dapp.set_xapp_control_handler([&dapp](const XAppControlAction& a) {
        {
            std::lock_guard<std::mutex> lk(g_dapp_mu);
            g_dapp_xapp.push_back(a);
        }
        if (!a.request_ack) return;
        ControlResult result;
        result.response_code = ResponseCode::NEGATIVE;
        result.cause = Cause::INVALID_REQUEST;
        result.detail = "not now";
        result.outcome.assign(OUTCOME, OUTCOME + sizeof(OUTCOME) - 1);
        (void)dapp.send_control_response(a.message_id, a.ran_function_identifier, CONTROL, result);
    });
    auto heard = [] { std::lock_guard<std::mutex> lk(g_dapp_mu); return g_dapp_heard.size(); };
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.subscribe(RF, {1}, {CONTROL})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([] { return g_sm.running.load(); }, 10000ms));
    const uint32_t dapp_id = *dapp.dapp_id();

    // (1) positive, with an outcome.
    uint32_t message_id = 0;
    ASSERT_EQ(static_cast<int>(dapp.send_control(RF, CONTROL, {'{', '}'}, 0, &message_id, true)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return heard() == 1; }, 10000ms));
    {
        std::lock_guard<std::mutex> lk(g_dapp_mu);
        const auto& r = g_dapp_heard[0];
        ASSERT_EQ(r.request_id, message_id);
        ASSERT_EQ(r.dapp_identifier, dapp_id);
        ASSERT_EQ(r.ran_function_identifier, RF);
        ASSERT_EQ(r.control_identifier, CONTROL);
        ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
        ASSERT_FALSE(r.cause.has_value());
        ASSERT_FALSE(r.detail.has_value());
        ASSERT_TRUE(r.control_outcome.has_value());
        const std::string got(r.control_outcome->begin(), r.control_outcome->end());
        const std::string expected(OUTCOME);
        ASSERT_STREQ(got, expected);
    }
    ASSERT_EQ(g_sm.emit_rc.load(), 0);

    // negative, with a cause and a detail.
    g_sm.mode = 1;
    ASSERT_EQ(static_cast<int>(dapp.send_control(RF, CONTROL, {'{', '}'}, 0, &message_id, true)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return heard() == 2; }, 10000ms));
    {
        std::lock_guard<std::mutex> lk(g_dapp_mu);
        const auto& r = g_dapp_heard[1];
        ASSERT_EQ(r.request_id, message_id);
        ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
        ASSERT_TRUE(r.cause.has_value() && *r.cause == Cause::INVALID_REQUEST);
        ASSERT_STREQ(*r.detail, "the scheduler refused it");
        ASSERT_FALSE(r.control_outcome.has_value());
    }

    // (2) the dApp does not ask: NOT_FOUND, and nothing is sent.
    g_sm.mode = 0;
    const int controls_before = g_sm.controls.load();
    ASSERT_EQ(static_cast<int>(dapp.send_control(RF, CONTROL, {'{', '}'})),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return g_sm.controls.load() > controls_before; }, 10000ms));
    ASSERT_EQ(g_sm.emit_rc.load(), static_cast<int>(ErrorCode::NOT_FOUND));
    std::this_thread::sleep_for(300ms);
    ASSERT_EQ(heard(), 2u);

    // (3) what it refuses.
    ASSERT_EQ(e3_service_model_emit_control_response(nullptr, dapp_id, 1, 0, -1, nullptr, nullptr, 0),
              static_cast<int>(ErrorCode::INVALID_PARAM));
    ASSERT_EQ(e3_service_model_emit_control_response(g_sm.handle, dapp_id, 1, 0, -1, nullptr,
                                                     reinterpret_cast<const uint8_t*>(OUTCOME), 32769),
              static_cast<int>(ErrorCode::INVALID_PARAM));

    // (4) an xApp control that asks: its id comes back, and so does the dApp's answer.
    uint32_t xapp_id = 0;
    const uint8_t data[] = {'{', '}'};
    ASSERT_EQ(e3_agent_send_xapp_control(ran, dapp_id, RF, data, sizeof(data), 9, &xapp_id, 1), 0);
    ASSERT_TRUE(xapp_id != 0);
    ASSERT_TRUE(wait_until([] { return g_ran.count() == 1; }, 10000ms));
    {
        const auto item = g_ran.at(0);
        ASSERT_EQ(item.dapp_id, dapp_id);
        ASSERT_EQ(item.request_id, xapp_id);
        ASSERT_EQ(item.ran_function_id, RF);
        ASSERT_EQ(item.control_id, CONTROL);
        ASSERT_EQ(item.response_code, 1);
        ASSERT_EQ(item.cause, 7);
        ASSERT_TRUE(item.has_detail);
        ASSERT_STREQ(item.detail, "not now");
        ASSERT_TRUE(item.has_outcome);
        const std::string got(item.outcome.begin(), item.outcome.end());
        const std::string expected(OUTCOME);
        ASSERT_STREQ(got, expected);
    }
    {
        std::lock_guard<std::mutex> lk(g_dapp_mu);
        ASSERT_EQ(g_dapp_xapp.size(), 1u);
        ASSERT_TRUE(g_dapp_xapp[0].request_ack);
        ASSERT_EQ(g_dapp_xapp[0].message_id, xapp_id);
    }

    // one that does not ask is seen as sent, and is not answered.
    ASSERT_EQ(e3_agent_send_xapp_control(ran, dapp_id, RF, data, sizeof(data), 10, nullptr, 0), 0);
    ASSERT_TRUE(wait_until([] { std::lock_guard<std::mutex> lk(g_dapp_mu); return g_dapp_xapp.size() == 2; }, 10000ms));
    {
        std::lock_guard<std::mutex> lk(g_dapp_mu);
        ASSERT_FALSE(g_dapp_xapp[1].request_ack);
    }
    std::this_thread::sleep_for(300ms);
    ASSERT_EQ(g_ran.count(), 1u);

    // (5) a NULL agent.
    ASSERT_EQ(e3_agent_send_xapp_control(nullptr, dapp_id, RF, data, sizeof(data), 9, nullptr, 1),
              static_cast<int>(ErrorCode::INVALID_PARAM));
    ASSERT_EQ(e3_agent_set_control_response_handler(nullptr, on_response),
              static_cast<int>(ErrorCode::INVALID_PARAM));

    dapp.stop();
    e3_agent_stop(ran);
    e3_agent_destroy(ran);
}

int main() {
    return RUN_ALL_TESTS();
}
