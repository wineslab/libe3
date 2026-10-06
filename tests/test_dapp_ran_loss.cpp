/**
 * @file test_dapp_ran_loss.cpp
 * @brief A dApp must notice that the RAN went away.
 *
 * Before, a RAN that closed the connection or restarted left the dApp with its
 * old dApp id and subscriptions and no event: on POSIX the closed peer made the
 * receive return 0 (read as "nothing yet", so the loop spun), and on ZMQ the
 * socket silently reconnected to a RAN that did not know the dApp.
 *
 * Properties verified against a real RAN E3Agent, for ZMQ and POSIX over IPC:
 *   (1) stopping the RAN produces exactly one CONNECTION_LOST callback;
 *   (2) afterwards dapp_id() is empty and subscribe() refuses to send;
 *   (3) stop() then start() on the dApp registers it with a new RAN.
 *
 * And against a raw ZMQ peer standing in for the RAN, a ReleaseMessage is acted
 * on only when it names this dApp: the RAN's PUB reaches every dApp, so a release
 * for another one must leave this dApp running.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#if LIBE3_HAS_ZMQ
#include <zmq.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
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
    oss << "ipc:///tmp/dapps/ranloss_test_" << getpid() << "_"
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

struct Endpoints {
    std::string setup, sub, pub;
};

E3Config make_config(E3Role role, E3LinkLayer link, const Endpoints& ep) {
    E3Config c;
    c.role                = role;
    c.link_layer          = link;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = pick_encoding();
    c.log_level           = 0;
    c.ran_identifier      = "ranloss-test";
    c.dapp_name           = "RanLossDApp";
    c.dapp_version        = "1.0.0";
    return c;
}

/// Records disconnect callbacks and lets the test wait for them.
struct DisconnectLog {
    std::mutex mu;
    std::condition_variable cv;
    int count = 0;
    DisconnectReason last = DisconnectReason::RELEASED_BY_RAN;

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
};

void run_ran_loss(E3LinkLayer link) {
    (void)system("mkdir -p /tmp/dapps");
    const Endpoints ep{unique_ipc("setup"), unique_ipc("sub"), unique_ipc("pub")};

    auto ran = std::make_unique<E3Agent>(make_config(E3Role::RAN, link, ep));
    ASSERT_EQ(static_cast<int>(ran->start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    DisconnectLog log;
    E3Agent dapp(make_config(E3Role::DAPP, link, ep));
    dapp.set_disconnect_handler([&](DisconnectReason r) { log.record(r); });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(dapp.dapp_id().has_value());

    // (1) The RAN goes away: exactly one callback, with the right reason.
    ran->stop();
    ran.reset();
    ASSERT_TRUE(log.wait_for(1, 8000ms));
    {
        std::lock_guard<std::mutex> lk(log.mu);
        ASSERT_EQ(static_cast<int>(log.last),
                  static_cast<int>(DisconnectReason::CONNECTION_LOST));
    }

    // (2) The session is gone and nothing is sent to the dead peer.
    ASSERT_TRUE(!dapp.dapp_id().has_value());
    ASSERT_EQ(static_cast<int>(dapp.subscribe(1, {1}, {1})),
              static_cast<int>(ErrorCode::NOT_INITIALIZED));
    std::this_thread::sleep_for(1500ms);
    {
        std::lock_guard<std::mutex> lk(log.mu);
        ASSERT_EQ(log.count, 1);
    }

    // (3) A new RAN and a restart: the dApp registers again.
    ran = std::make_unique<E3Agent>(make_config(E3Role::RAN, link, ep));
    ASSERT_EQ(static_cast<int>(ran->start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);
    dapp.stop();
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(dapp.dapp_id().has_value());

    dapp.stop();
    ran->stop();
}

#if LIBE3_HAS_ZMQ
/// A raw ZMQ REP + PUB pair standing in for the RAN: answers one setup request
/// with `assigned_id`, and publishes encoded PDUs to the dApp's SUB.
struct RawRan {
    void* ctx = nullptr;
    void* rep = nullptr;
    void* pub = nullptr;
    std::thread setup_thread;
    std::unique_ptr<E3Encoder> encoder;

    RawRan(const Endpoints& ep, EncodingFormat enc, uint32_t assigned_id)
        : encoder(create_encoder(enc)) {
        (void)system("mkdir -p /tmp/dapps");
        ctx = zmq_ctx_new();
        rep = zmq_socket(ctx, ZMQ_REP);
        pub = zmq_socket(ctx, ZMQ_PUB);
        int zero = 0, rcv_timeout = 10000;
        zmq_setsockopt(rep, ZMQ_LINGER, &zero, sizeof(zero));
        zmq_setsockopt(pub, ZMQ_LINGER, &zero, sizeof(zero));
        zmq_setsockopt(rep, ZMQ_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));
        zmq_bind(rep, ep.setup.c_str());
        zmq_bind(pub, ep.pub.c_str());
        setup_thread = std::thread([this, assigned_id]() {
            uint8_t buf[4096];
            int n = zmq_recv(rep, buf, sizeof(buf), 0);
            if (n <= 0) return;
            auto req = encoder->decode(buf, static_cast<size_t>(n));
            auto resp = encoder->encode_setup_response(
                1, req ? req->message_id : 1, ResponseCode::POSITIVE,
                std::string("1.0.0"), assigned_id, "raw-ran");
            if (resp) zmq_send(rep, resp->buffer.data(), resp->buffer.size(), 0);
        });
    }
    ~RawRan() {
        if (setup_thread.joinable()) setup_thread.join();
        zmq_close(rep);
        zmq_close(pub);
        zmq_ctx_destroy(ctx);
    }
    void publish(const Pdu& pdu) {
        auto enc = encoder->encode(pdu);
        if (enc) zmq_send(pub, enc->buffer.data(), enc->buffer.size(), 0);
    }
    Pdu release_for(uint32_t dapp_id) {
        Pdu pdu(PduType::RELEASE_MESSAGE);
        ReleaseMessage rel;
        rel.dapp_identifier = dapp_id;
        pdu.choice = rel;
        pdu.message_id = 5;
        return pdu;
    }
    Pdu ack() {
        Pdu pdu(PduType::MESSAGE_ACK);
        MessageAck a;
        a.request_id = 9;
        a.response_code = ResponseCode::POSITIVE;
        pdu.choice = a;
        pdu.message_id = 6;
        return pdu;
    }
};

void run_release(EncodingFormat enc) {
    const Endpoints ep{unique_ipc("rsetup"), unique_ipc("rsub"), unique_ipc("rpub")};
    constexpr uint32_t kOurId = 5;
    RawRan ran(ep, enc, kOurId);

    auto cfg = make_config(E3Role::DAPP, E3LinkLayer::ZMQ, ep);
    cfg.encoding = enc;
    DisconnectLog log;
    std::atomic<int> acks{0};
    E3Agent dapp(std::move(cfg));
    dapp.set_disconnect_handler([&](DisconnectReason r) { log.record(r); });
    dapp.set_message_ack_handler([&](const MessageAck&) { ++acks; });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(5000ms)),
              static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(*dapp.dapp_id(), kOurId);

    // Publish until the dApp has seen one: a PUB drops what it sends before the SUB joined.
    for (int i = 0; i < 50 && acks.load() == 0; ++i) {
        ran.publish(ran.ack());
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_TRUE(acks.load() > 0);

    // A release for another dApp changes nothing.
    ran.publish(ran.release_for(kOurId + 1));
    std::this_thread::sleep_for(500ms);
    ASSERT_TRUE(dapp.dapp_id().has_value());
    ASSERT_TRUE(!log.wait_for(1, 0ms));

    // A release for this dApp ends the session, once.
    ran.publish(ran.release_for(kOurId));
    ASSERT_TRUE(log.wait_for(1, 5000ms));
    {
        std::lock_guard<std::mutex> lk(log.mu);
        ASSERT_EQ(static_cast<int>(log.last),
                  static_cast<int>(DisconnectReason::RELEASED_BY_RAN));
    }
    ASSERT_TRUE(!dapp.dapp_id().has_value());
    ran.publish(ran.release_for(kOurId));
    std::this_thread::sleep_for(300ms);
    {
        std::lock_guard<std::mutex> lk(log.mu);
        ASSERT_EQ(log.count, 1);
    }
    dapp.stop();
}
#endif  // LIBE3_HAS_ZMQ

}  // namespace

#if LIBE3_HAS_ZMQ
TEST(DAppRanLoss_zmq_stoppedRanReportsConnectionLost) {
    run_ran_loss(E3LinkLayer::ZMQ);
}
#endif


TEST(DAppRanLoss_posix_stoppedRanReportsConnectionLost) {
    run_ran_loss(E3LinkLayer::POSIX);
}

#if LIBE3_HAS_ZMQ && defined(LIBE3_ENABLE_JSON)
TEST(DAppRelease_json_onlyOwnReleaseEndsSession) {
    run_release(EncodingFormat::JSON);
}
#endif

#if LIBE3_HAS_ZMQ && defined(LIBE3_ENABLE_ASN1)
TEST(DAppRelease_asn1_onlyOwnReleaseEndsSession) {
    run_release(EncodingFormat::ASN1);
}
#endif

int main() {
    return RUN_ALL_TESTS();
}
