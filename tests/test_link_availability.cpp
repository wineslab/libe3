/**
 * @file test_link_availability.cpp
 * @brief Each link layer is there exactly when the build has it, and the C API reaches all three.
 *
 * Built in every configuration, so the builds without ZeroMQ and without liburing run it too.
 * Properties verified:
 *   (1) the factory makes a ZeroMQ connector exactly when LIBE3_HAS_ZMQ is 1, and the POSIX one always;
 *   (2) without liburing, asking for the io_uring link gives no connector (the factory logs why)
 *       and an E3Agent configured with it fails to initialise instead of using another link;
 *   (3) with liburing, the factory makes an io_uring connector whose link and transport are the
 *       ones asked for, unless the kernel refuses io_uring here (then it says so);
 *   (4) link_layer 2 in e3_config_t is the io_uring link: a RAN made from C with it serves an
 *       io_uring dApp over TCP.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "link_under_test.hpp"
#include "libe3/libe3.hpp"
#include "libe3/c_api.h"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_connector.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

uint16_t unique_port() { return libe3_test::free_port(); }

std::unique_ptr<E3Connector> make(E3LinkLayer link, E3TransportLayer transport) {
    return create_connector(link, transport, "", "", "", 1, 2, 3, 1, E3Role::DAPP);
}

#if defined(LIBE3_ENABLE_JSON)
constexpr int C_ENCODING = 1;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::JSON;
#else
constexpr int C_ENCODING = 0;
constexpr EncodingFormat CPP_ENCODING = EncodingFormat::ASN1;
#endif

}  // namespace

TEST(the_zmq_and_posix_links_are_there_when_built) {
    ASSERT_TRUE(make(E3LinkLayer::POSIX, E3TransportLayer::TCP) != nullptr);
#if LIBE3_HAS_ZMQ
    ASSERT_TRUE(make(E3LinkLayer::ZMQ, E3TransportLayer::TCP) != nullptr);
#else
    ASSERT_TRUE(make(E3LinkLayer::ZMQ, E3TransportLayer::TCP) == nullptr);
#endif
}

TEST(the_uring_link_is_there_exactly_when_built_and_allowed) {
#if LIBE3_HAS_URING
    if (!libe3_test::uring_runs_here()) return;
    for (auto transport : {E3TransportLayer::TCP, E3TransportLayer::IPC, E3TransportLayer::SCTP}) {
        auto conn = make(E3LinkLayer::URING, transport);
        ASSERT_TRUE(conn != nullptr);
        ASSERT_TRUE(conn->link_layer() == E3LinkLayer::URING);
        ASSERT_TRUE(conn->transport_layer() == transport);
        ASSERT_FALSE(conn->is_connected());
        conn->dispose();
        conn->dispose();  // twice is harmless
    }
#else
    ASSERT_TRUE(make(E3LinkLayer::URING, E3TransportLayer::TCP) == nullptr);
    E3Config config;
    config.link_layer = E3LinkLayer::URING;
    config.log_level = 0;
    E3Agent agent(config);
    ASSERT_TRUE(agent.init() != ErrorCode::SUCCESS);  // no silent fallback to another link
#endif
}

TEST(link_layer_two_in_the_c_api_is_the_uring_link) {
    if (!libe3_test::uring_runs_here()) return;
    const uint16_t port = unique_port();
    e3_config_t cfg{};
    cfg.ran_identifier = "capi-uring-test";
    cfg.link_layer = 2;       // io_uring
    cfg.transport_layer = 1;  // TCP
    cfg.setup_port = port;
    cfg.encoding = C_ENCODING;
    cfg.log_level = 0;
    e3_agent_handle_t* ran = e3_agent_create_with_config(&cfg);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_EQ(e3_agent_init(ran), 0);
    ASSERT_EQ(e3_agent_start(ran), 0);

    // A ZeroMQ RAN (what an ignored link_layer would leave) does not frame a setup request
    // the way an io_uring dApp sends it, so the setup below would time out.
    E3Config dapp_cfg;
    dapp_cfg.role = E3Role::DAPP;
    dapp_cfg.link_layer = E3LinkLayer::URING;
    dapp_cfg.transport_layer = E3TransportLayer::TCP;
    dapp_cfg.setup_port = port;
    dapp_cfg.encoding = CPP_ENCODING;
    dapp_cfg.log_level = 0;
    dapp_cfg.reconnect_interval_ms = 0;
    E3Agent dapp(dapp_cfg);
    ASSERT_TRUE(dapp.start() == ErrorCode::SUCCESS);
    const bool set_up = dapp.wait_for_setup(10000ms) == ErrorCode::SUCCESS && dapp.dapp_id().has_value();
    dapp.stop();
    e3_agent_stop(ran);
    e3_agent_destroy(ran);
    ASSERT_TRUE(set_up);
}

int main() {
    return RUN_ALL_TESTS();
}
