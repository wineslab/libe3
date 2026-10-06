/**
 * @file test_dapp_setup_retry_zmq.cpp
 * @brief Regression test: a dApp retries setup over ZMQ when the RAN is slow.
 *
 * The dApp setup channel is a ZMQ REQ socket, which refuses a second send
 * while it still waits for a reply. A RAN that needs longer than one receive
 * window (about 2 s) to answer used to leave every retry failing at the send,
 * so wait_for_setup ended in CONNECTION_FAILED although the RAN was up.
 *
 * A raw REP peer plays a slow RAN: it holds the first request past the first
 * window, then answers the retry. The dApp must complete setup.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <zmq.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

std::atomic<int> g_seq{0};

std::string unique_ipc(const char* tag) {
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/setupretry_test_" << getpid() << "_"
        << g_seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

EncodingFormat pick_encoding() {
#if defined(LIBE3_ENABLE_JSON)
    return EncodingFormat::JSON;
#else
    return EncodingFormat::ASN1;
#endif
}

}  // namespace

TEST(DAppSetup_slowRan_retryOnFreshSocketCompletes) {
    const std::string setup_ep = unique_ipc("setup");
    const EncodingFormat encoding = pick_encoding();

    // Mkdir happens inside the connector; the raw REP binds first, so make sure it exists.
    (void)system("mkdir -p /tmp/dapps");

    void* ctx = zmq_ctx_new();
    ASSERT_TRUE(ctx != nullptr);
    void* rep = zmq_socket(ctx, ZMQ_REP);
    ASSERT_TRUE(rep != nullptr);
    int linger = 0;
    zmq_setsockopt(rep, ZMQ_LINGER, &linger, sizeof(linger));
    int rcv_timeout = 10000;
    zmq_setsockopt(rep, ZMQ_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
    ASSERT_EQ(zmq_bind(rep, setup_ep.c_str()), 0);

    auto encoder = create_encoder(encoding);
    ASSERT_TRUE(encoder != nullptr);

    std::atomic<int> requests_seen{0};
    std::thread slow_ran([&]() {
        uint8_t buf[4096];
        // First request: hold it past the dApp's first receive window.
        int n = zmq_recv(rep, buf, sizeof(buf), 0);
        if (n <= 0) return;
        ++requests_seen;
        std::this_thread::sleep_for(2500ms);
        // This reply goes to a REQ socket the dApp has already replaced.
        auto first = encoder->decode(buf, static_cast<size_t>(n));
        uint32_t first_id = first ? first->message_id : 1;
        auto r1 = encoder->encode_setup_response(7, first_id, ResponseCode::POSITIVE,
                                                  std::string("1.0.0"), 1u, "slow-ran");
        if (r1) zmq_send(rep, r1->buffer.data(), r1->buffer.size(), 0);

        // The retry arrives next and is answered.
        n = zmq_recv(rep, buf, sizeof(buf), 0);
        if (n <= 0) return;
        ++requests_seen;
        auto second = encoder->decode(buf, static_cast<size_t>(n));
        uint32_t second_id = second ? second->message_id : 1;
        auto r2 = encoder->encode_setup_response(8, second_id, ResponseCode::POSITIVE,
                                                  std::string("1.0.0"), 2u, "slow-ran");
        if (r2) zmq_send(rep, r2->buffer.data(), r2->buffer.size(), 0);
    });

    // Joined on any exit so a failed ASSERT does not terminate on a joinable thread.
    struct Cleanup {
        std::thread& t; void* rep; void* ctx;
        ~Cleanup() {
            if (t.joinable()) t.join();
            zmq_close(rep);
            zmq_ctx_destroy(ctx);
        }
    } cleanup{slow_ran, rep, ctx};

    E3Config cfg;
    cfg.role                = E3Role::DAPP;
    cfg.link_layer          = E3LinkLayer::ZMQ;
    cfg.transport_layer     = E3TransportLayer::IPC;
    cfg.setup_endpoint      = setup_ep;
    cfg.subscriber_endpoint = unique_ipc("sub");
    cfg.publisher_endpoint  = unique_ipc("pub");
    cfg.encoding            = encoding;
    cfg.log_level           = 0;
    cfg.dapp_name           = "SetupRetryDApp";
    cfg.dapp_version        = "1.0.0";

    E3Agent dapp(std::move(cfg));
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));

    // Without the fix the retry send fails and this ends in CONNECTION_FAILED.
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(dapp.dapp_id().has_value());
    ASSERT_EQ(*dapp.dapp_id(), 2u);
    ASSERT_EQ(requests_seen.load(), 2);

    dapp.stop();
}

int main() {
    return RUN_ALL_TESTS();
}
