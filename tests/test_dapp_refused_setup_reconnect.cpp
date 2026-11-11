/**
 * @file test_dapp_refused_setup_reconnect.cpp
 * @brief A dApp whose setup is refused registers again by itself, and is served once the RAN accepts.
 *
 * A fake RAN, built from the connector and the encoder, refuses the first setup requests with
 * a negative SetupResponse. Then it goes away and a real RAN takes the endpoint. A real dApp
 * runs throughout, with reconnect_interval_ms set. Properties verified, for each link and
 * transport:
 *   (1) the dApp sends a setup request again after each refusal, with no call from the program,
 *       at least reconnect_interval_ms after the one before;
 *   (2) it never has a dApp id while it is refused;
 *   (3) once a RAN accepts, it registers: it has an id, and the RAN counts it.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "link_under_test.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_connector.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr int REFUSALS = 3;
constexpr uint32_t INTERVAL_MS = 300;

EncodingFormat pick_encoding() {
#if defined(LIBE3_ENABLE_JSON)
    return EncodingFormat::JSON;
#else
    return EncodingFormat::ASN1;
#endif
}

uint16_t unique_port() { return libe3_test::free_port(); }

std::string make_tmpdir() {
    char tmpl[] = "/tmp/libe3_refused_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

class QuietSM : public ServiceModel {
public:
    std::string name() const override { return "QuietSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return 1; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override {}
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override { return ErrorCode::SUCCESS; }

private:
    std::atomic<bool> running_{false};
};

struct Where {
    E3LinkLayer link;
    E3TransportLayer transport;
    std::string endpoint;   ///< setup endpoint (IPC path or URI); empty for TCP and SCTP
    uint16_t port;
};

E3Config make_config(E3Role role, const Where& where) {
    E3Config c;
    c.role = role;
    c.link_layer = where.link;
    c.transport_layer = where.transport;
    if (!where.endpoint.empty()) {
        c.setup_endpoint = where.endpoint;
        c.subscriber_endpoint = where.endpoint + "_sub";
        c.publisher_endpoint = where.endpoint + "_pub";
    }
    c.setup_port = where.port;
    c.subscriber_port = static_cast<uint16_t>(where.port + 1);
    c.publisher_port = static_cast<uint16_t>(where.port + 2);
    c.encoding = pick_encoding();
    c.log_level = 0;
    c.ran_identifier = "refusal-test";
    c.dapp_name = "RefusedDApp";
    c.dapp_version = "1.0.0";
    c.dapp_idle_timeout_s = 0;
    c.reconnect_interval_ms = INTERVAL_MS;
    return c;
}

/// What the dApp is told about its setup, with the time of each answer.
struct SetupLog {
    struct Entry {
        std::chrono::steady_clock::time_point at;
        ResponseCode code;
    };
    std::mutex mu;
    std::vector<Entry> entries;

    void attach(E3Agent& dapp) {
        dapp.set_setup_response_handler([this](const SetupResponse& r) {
            std::lock_guard<std::mutex> lk(mu);
            entries.push_back({std::chrono::steady_clock::now(), r.response_code});
        });
    }
    size_t count() {
        std::lock_guard<std::mutex> lk(mu);
        return entries.size();
    }
    std::vector<Entry> copy() {
        std::lock_guard<std::mutex> lk(mu);
        return entries;
    }
};

/// A RAN that refuses every setup request it is sent, until it is stopped.
class RefusingRan {
public:
    explicit RefusingRan(const Where& where)
        : encoder_(create_encoder(pick_encoding())) {
        connector_ = create_connector(where.link, where.transport, where.endpoint,
                                      where.endpoint.empty() ? "" : where.endpoint + "_sub",
                                      where.endpoint.empty() ? "" : where.endpoint + "_pub",
                                      where.port, static_cast<uint16_t>(where.port + 1),
                                      static_cast<uint16_t>(where.port + 2), 1, E3Role::RAN);
    }

    bool start() {
        if (!connector_ || connector_->setup_initial_connection() != ErrorCode::SUCCESS) return false;
        connector_->setup_inbound_connection();
        connector_->setup_outbound_connection();
        thread_ = std::thread([this] { run(); });
        return true;
    }

    void stop() {
        stop_.store(true);
        if (connector_) connector_->shutdown();
        if (thread_.joinable()) thread_.join();
        if (connector_) connector_->dispose();
    }

    int refused() const { return refused_.load(); }

private:
    void run() {
        uint32_t next_id = 1;
        while (!stop_.load()) {
            std::vector<uint8_t> buf;
            const int n = connector_->recv_setup_request(buf);
            if (n <= 0) continue;
            auto request = encoder_->decode(buf.data(), static_cast<size_t>(n));
            if (!request || request->type != PduType::SETUP_REQUEST) continue;
            auto reply = encoder_->encode_setup_response(
                next_id++, request->message_id, ResponseCode::NEGATIVE, "1.0.0", std::nullopt,
                "refusal-test", std::nullopt, Cause::RESOURCE_EXHAUSTED, std::string("refused by the test"));
            if (reply && connector_->send_response(reply->buffer) == ErrorCode::SUCCESS) {
                ++refused_;
            }
        }
    }

    std::unique_ptr<E3Encoder> encoder_;
    std::unique_ptr<E3Connector> connector_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> refused_{0};
};

void run(const Where& where) {
    RefusingRan fake(where);
    ASSERT_TRUE(fake.start());

    E3Agent dapp(make_config(E3Role::DAPP, where));
    SetupLog log;
    log.attach(dapp);
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));

    // (1) It asks again by itself, as often as the RAN refuses.
    ASSERT_TRUE(wait_until([&] { return log.count() >= static_cast<size_t>(REFUSALS); }, 30000ms));
    // (2) Refused: no id.
    ASSERT_FALSE(dapp.dapp_id().has_value());
    const auto refusals = log.copy();
    for (int i = 0; i < REFUSALS; ++i) {
        ASSERT_TRUE(refusals[static_cast<size_t>(i)].code == ResponseCode::NEGATIVE);
    }
    for (int i = 1; i < REFUSALS; ++i) {
        const auto gap = refusals[static_cast<size_t>(i)].at - refusals[static_cast<size_t>(i - 1)].at;
        ASSERT_TRUE(gap >= std::chrono::milliseconds(INTERVAL_MS * 9 / 10));  // never faster than the interval
        ASSERT_TRUE(gap < 10s);
    }

    // The RAN that accepts takes the endpoint.
    fake.stop();
    E3Agent ran(make_config(E3Role::RAN, where));
    ASSERT_EQ(static_cast<int>(ran.register_sm(std::make_unique<QuietSM>())), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));

    // (3) The dApp registers, with no call from the program.
    ASSERT_TRUE(wait_until([&] { return dapp.dapp_id().has_value() && ran.dapp_count() == 1u; }, 30000ms));
    const auto all = log.copy();
    ASSERT_TRUE(all.back().code == ResponseCode::POSITIVE);

    dapp.stop();
    ran.stop();
}

}  // namespace

TEST(refused_setup_is_retried_posix_ipc) {
    mkdir("/tmp/dapps", 0777);
    run({E3LinkLayer::POSIX, E3TransportLayer::IPC, "ipc://" + make_tmpdir() + "/e3", 0});
}

TEST(refused_setup_is_retried_posix_tcp) {
    run({E3LinkLayer::POSIX, E3TransportLayer::TCP, "", unique_port()});
}

TEST(refused_setup_is_retried_uring_ipc) {
    if (!libe3_test::uring_runs_here()) return;
    mkdir("/tmp/dapps", 0777);
    run({E3LinkLayer::URING, E3TransportLayer::IPC, "ipc://" + make_tmpdir() + "/e3", 0});
}

TEST(refused_setup_is_retried_uring_tcp) {
    if (!libe3_test::uring_runs_here()) return;
    run({E3LinkLayer::URING, E3TransportLayer::TCP, "", unique_port()});
}

#if LIBE3_HAS_ZMQ
TEST(refused_setup_is_retried_zmq_ipc) {
    mkdir("/tmp/dapps", 0777);
    run({E3LinkLayer::ZMQ, E3TransportLayer::IPC, "ipc://" + make_tmpdir() + "/setup", 0});
}
#endif

TEST(refused_setup_is_retried_posix_sctp) {
    int probe = socket(AF_INET, SOCK_STREAM, 132 /* IPPROTO_SCTP */);
    if (probe < 0) {
        const char* require = std::getenv("LIBE3_REQUIRE_SCTP");
        ASSERT_FALSE(require != nullptr && *require != '\0' && *require != '0');
        std::cout << "SCTP NOT RUN: this host cannot open an SCTP socket\n";
        return;
    }
    close(probe);
#ifdef LIBE3_ENABLE_SCTP
    run({E3LinkLayer::POSIX, E3TransportLayer::SCTP, "", unique_port()});
#else
    ASSERT_FALSE(std::getenv("LIBE3_REQUIRE_SCTP") != nullptr && *std::getenv("LIBE3_REQUIRE_SCTP") != '0');
    std::cout << "SCTP NOT RUN: libe3 was built without LIBE3_ENABLE_SCTP\n";
#endif
}

int main() {
    return RUN_ALL_TESTS();
}
