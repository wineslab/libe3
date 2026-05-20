/**
 * @file test_role_pair_posix.cpp
 * @brief End-to-end RAN + dApp pair over POSIX sockets (IPC + TCP).
 *
 * Same scenario as test_role_pair_zmq_ipc.cpp but using the POSIX
 * connector. SCTP is not exercised (CI runners lack the sctp kernel
 * module).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include <libe3/libe3.hpp>
#include "sm_simple/e3sm_simple_wrapper.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

std::string make_tmpdir() {
    char tmpl[] = "/tmp/libe3_posix_pair_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

class PosixSM : public ServiceModel {
public:
    std::string name() const override { return "POSIX-TEST"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return 1; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { stop(); }
    bool is_running() const override { return running_; }

    std::vector<uint8_t> ran_function_data() const override {
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data("POSIXSM", out)) return out;
        return {0x01};
    }

    ErrorCode start() override {
        if (running_) return ErrorCode::SUCCESS;
        running_ = true;
        worker_ = std::thread([this]() {
            uint32_t seq = 0;
            while (running_) {
                std::this_thread::sleep_for(200ms);
                auto subs = get_subscribers();
                if (subs.empty()) continue;
                libe3_examples::SimpleIndication si{seq, 0};
                std::vector<uint8_t> enc;
                if (!libe3_examples::encode_simple_indication(si, enc)) continue;
                for (auto did : subs) {
                    Pdu pdu = make_indication_pdu(did, 1, enc);
                    (void)emit_outbound(std::move(pdu));
                }
                ++seq;
            }
        });
        return ErrorCode::SUCCESS;
    }
    void stop() override {
        if (!running_) return;
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }
    ErrorCode handle_control_action(uint32_t request_message_id,
                                    const DAppControlAction& a) override {
        ++control_count;
        Pdu ack = make_message_ack_pdu(request_message_id, ResponseCode::POSITIVE);
        return emit_outbound(std::move(ack));
    }
    std::atomic<int> control_count{0};

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
};

void run_pair(E3TransportLayer transport, uint16_t base_port,
              const std::string& ipc_dir) {
    auto make_ran_cfg = [&]() {
        E3Config c;
        c.role = E3Role::RAN;
        c.ran_identifier = "posix-test-ran";
        c.link_layer = E3LinkLayer::POSIX;
        c.transport_layer = transport;
        c.encoding = EncodingFormat::ASN1;
        c.log_level = 0;
        if (transport == E3TransportLayer::IPC) {
            c.setup_endpoint = "ipc://" + ipc_dir + "/setup";
            c.subscriber_endpoint = "ipc://" + ipc_dir + "/dapp_socket";
            c.publisher_endpoint = "ipc://" + ipc_dir + "/e3_socket";
        }
        c.setup_port = base_port;
        c.subscriber_port = base_port + 1;
        c.publisher_port = base_port + 2;
        return c;
    };
    auto ran_cfg = make_ran_cfg();
    auto dapp_cfg = ran_cfg;
    dapp_cfg.role = E3Role::DAPP;
    dapp_cfg.dapp_name = "PosixTestDApp";

    E3Agent ran(ran_cfg);
    auto* sm_ptr = new PosixSM();
    ASSERT_TRUE(ran.register_sm(std::unique_ptr<ServiceModel>(sm_ptr)) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.start() == ErrorCode::SUCCESS);

    std::mutex mu;
    std::condition_variable cv;
    int indications = 0;
    bool sub_resp_ok = false;

    E3Agent dapp(dapp_cfg);
    dapp.set_indication_handler([&](const IndicationMessage& msg) {
        libe3_examples::SimpleIndication si;
        if (libe3_examples::decode_simple_indication(msg.protocol_data, si)) {
            std::lock_guard<std::mutex> lk(mu);
            ++indications;
            cv.notify_all();
        }
    });
    dapp.set_subscription_response_handler([&](const SubscriptionResponse& r) {
        std::lock_guard<std::mutex> lk(mu);
        sub_resp_ok = (r.response_code == ResponseCode::POSITIVE);
        cv.notify_all();
    });

    ASSERT_TRUE(dapp.start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp.wait_for_setup(5000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp.dapp_id().has_value());

    // POSIX inbound/outbound use blocking accept on the RAN side, so the
    // dApp needs to actually connect before the RAN can dispatch its
    // SubscriptionResponse. Give it a moment.
    std::this_thread::sleep_for(500ms);

    ASSERT_TRUE(dapp.subscribe(1, {1}, {1}) == ErrorCode::SUCCESS);
    {
        std::unique_lock<std::mutex> lk(mu);
        ASSERT_TRUE(cv.wait_for(lk, 5s, [&]() { return sub_resp_ok; }));
    }
    {
        std::unique_lock<std::mutex> lk(mu);
        ASSERT_TRUE(cv.wait_for(lk, 5s, [&]() { return indications >= 2; }));
    }

    dapp.stop();
    ran.stop();
}

}  // namespace

TEST(role_pair_posix_ipc_end_to_end) {
    run_pair(E3TransportLayer::IPC, /*base_port=*/0, make_tmpdir());
}

TEST(role_pair_posix_tcp_end_to_end) {
    run_pair(E3TransportLayer::TCP, /*base_port=*/24990, "");
}

int main() {
    return RUN_ALL_TESTS();
}
