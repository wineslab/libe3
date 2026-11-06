/**
 * @file test_c_api_dapp_idle.cpp
 * @brief e3_config_t carries the RAN's idle time for a dApp.
 *
 * A RAN created through the C API over POSIX IPC and a dApp created through the
 * C++ API. Properties verified:
 *   (1) with set_dapp_idle_timeout and a time, a dApp that sends nothing is
 *       released and removed after about that time;
 *   (2) with set_dapp_idle_timeout and 0, it is never removed for being quiet;
 *   (3) without the flag the time is ignored and the default stays, so a
 *       zeroed struct, which sets neither, changes nothing.
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

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/capi_idle_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

#if defined(LIBE3_ENABLE_JSON)
constexpr int C_ENCODING = 1;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::JSON;
#else
constexpr int C_ENCODING = 0;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::ASN1;
#endif

struct Endpoints {
    std::string setup = unique_ipc("setup");
    std::string sub = unique_ipc("sub");
    std::string pub = unique_ipc("pub");
};

/// A RAN made from C. The caller sets the two idle fields.
struct CRan {
    CRan(const Endpoints& ep, int set_flag, uint32_t idle_s) {
        e3_config_t cfg{};
        cfg.ran_identifier = "capi-idle-test";
        cfg.link_layer = 1;       // POSIX
        cfg.transport_layer = 2;  // IPC
        cfg.setup_endpoint = ep.setup.c_str();
        cfg.subscriber_endpoint = ep.sub.c_str();
        cfg.publisher_endpoint = ep.pub.c_str();
        cfg.encoding = C_ENCODING;
        cfg.log_level = 0;
        cfg.set_dapp_idle_timeout = set_flag;
        cfg.dapp_idle_timeout_s = idle_s;
        agent = e3_agent_create_with_config(&cfg);
        ok = agent && e3_agent_init(agent) == 0 && e3_agent_start(agent) == 0;
        std::this_thread::sleep_for(100ms);
    }
    ~CRan() {
        if (agent) {
            e3_agent_stop(agent);
            e3_agent_destroy(agent);
        }
    }
    e3_agent_handle_t* agent{nullptr};
    bool ok{false};
};

E3Config dapp_config(const Endpoints& ep) {
    E3Config c;
    c.role                = E3Role::DAPP;
    c.link_layer          = E3LinkLayer::POSIX;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = CPP_ENCODING;
    c.log_level           = 0;
    c.dapp_name           = "CApiIdleDApp";
    c.dapp_version        = "1.0.0";
    c.reconnect_interval_ms = 0;
    return c;
}

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

/// Starts a dApp against `ran`, waits `watch`, and says whether the RAN still holds it.
bool dapp_still_registered_after(const Endpoints& ep, CRan& ran, std::chrono::milliseconds watch) {
    E3Agent dapp(dapp_config(ep));
    if (dapp.start() != ErrorCode::SUCCESS) return false;
    if (dapp.wait_for_setup(15000ms) != ErrorCode::SUCCESS) return false;
    if (!wait_until([&] { return e3_agent_dapp_count(ran.agent) == 1u; }, 10000ms)) return false;
    std::this_thread::sleep_for(watch);
    const bool held = e3_agent_dapp_count(ran.agent) == 1u;
    dapp.stop();
    return held;
}

}  // namespace

TEST(CApiDAppIdle_aTimeSetFromCRemovesAQuietDApp) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const Endpoints ep;
    CRan ran(ep, 1, 2);
    ASSERT_TRUE(ran.ok);

    E3Agent dapp(dapp_config(ep));
    std::mutex mu;
    std::condition_variable cv;
    bool released = false;
    dapp.set_disconnect_handler([&](DisconnectReason r) {
        std::lock_guard<std::mutex> lk(mu);
        released = (r == DisconnectReason::RELEASED_BY_RAN);
        cv.notify_all();
    });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return e3_agent_dapp_count(ran.agent) == 1u; }, 10000ms));

    {
        std::unique_lock<std::mutex> lk(mu);
        ASSERT_TRUE(cv.wait_for(lk, 30s, [&] { return released; }));
    }
    ASSERT_TRUE(wait_until([&] { return e3_agent_dapp_count(ran.agent) == 0u; }, 10000ms));
    dapp.stop();
}

TEST(CApiDAppIdle_zeroSetFromCNeverRemovesAQuietDApp) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    CRan ran(ep, 1, 0);
    ASSERT_TRUE(ran.ok);
    // Longer than the 2 s the first test needs.
    ASSERT_TRUE(dapp_still_registered_after(ep, ran, 6000ms));
}

TEST(CApiDAppIdle_aTimeWithoutTheFlagIsIgnored) {
    mkdir("/tmp/dapps", 0777);
    const Endpoints ep;
    CRan ran(ep, 0, 1);  // would remove the dApp after 1 s if it were applied
    ASSERT_TRUE(ran.ok);
    ASSERT_TRUE(dapp_still_registered_after(ep, ran, 6000ms));
}

int main() {
    return RUN_ALL_TESTS();
}
