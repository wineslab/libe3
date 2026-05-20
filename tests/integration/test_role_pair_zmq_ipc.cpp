/**
 * @file test_role_pair_zmq_ipc.cpp
 * @brief End-to-end integration test for a RAN + dApp E3Agent pair in one
 *        process over ZMQ + IPC.
 *
 * Drives the full protocol from a unit-test harness: setup → subscribe →
 * indication → control → report → release. Validates that the role-aware
 * E3Interface dispatches PDUs correctly in both directions.
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
    char tmpl[] = "/tmp/libe3_role_pair_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

struct Endpoints {
    std::string setup;
    std::string subscriber;
    std::string publisher;
};

Endpoints make_endpoints(const std::string& dir) {
    return Endpoints{
        "ipc://" + dir + "/setup",
        "ipc://" + dir + "/dapp_socket",
        "ipc://" + dir + "/e3_socket",
    };
}

class TestSimpleSM : public ServiceModel {
public:
    static constexpr uint32_t RAN_FUNCTION_ID = 1;
    std::string name() const override { return "TEST"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return RAN_FUNCTION_ID; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { stop(); }

    std::vector<uint8_t> ran_function_data() const override {
        // ASN.1 OCTET STRING (SIZE (1..32768)) — must be non-empty.
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data("TEST", out)) return out;
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
                libe3_examples::SimpleIndication si{seq, static_cast<uint32_t>(std::time(nullptr))};
                std::vector<uint8_t> enc;
                if (!libe3_examples::encode_simple_indication(si, enc)) continue;
                for (auto dapp_id : subs) {
                    Pdu pdu = make_indication_pdu(dapp_id, RAN_FUNCTION_ID, enc);
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

    bool is_running() const override { return running_; }

    ErrorCode handle_control_action(uint32_t request_message_id,
                                    const DAppControlAction& action) override {
        ++control_count;
        last_control_id = action.control_identifier;
        last_action_size = action.action_data.size();
        Pdu ack = make_message_ack_pdu(request_message_id, ResponseCode::POSITIVE);
        return emit_outbound(std::move(ack));
    }

    std::atomic<int> control_count{0};
    std::atomic<uint32_t> last_control_id{0};
    std::atomic<size_t> last_action_size{0};

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
};

E3Config make_ran_config(const Endpoints& ep) {
    E3Config cfg;
    cfg.role = E3Role::RAN;
    cfg.ran_identifier = "test-ran";
    cfg.link_layer = E3LinkLayer::ZMQ;
    cfg.transport_layer = E3TransportLayer::IPC;
    cfg.encoding = EncodingFormat::ASN1;
    cfg.log_level = 0;  // silence logger to avoid file collision
    cfg.setup_endpoint = ep.setup;
    cfg.subscriber_endpoint = ep.subscriber;
    cfg.publisher_endpoint = ep.publisher;
    return cfg;
}

E3Config make_dapp_config(const Endpoints& ep) {
    E3Config cfg = make_ran_config(ep);
    cfg.role = E3Role::DAPP;
    cfg.dapp_name = "TestSimpleDApp";
    cfg.dapp_version = "1.0.0";
    cfg.vendor = "WinesLab";
    return cfg;
}

}  // namespace

TEST(role_pair_full_handshake_indication_control_report_release) {
    const std::string dir = make_tmpdir();
    const Endpoints ep = make_endpoints(dir);

    E3Agent ran(make_ran_config(ep));
    auto* sm_ptr = new TestSimpleSM();
    std::unique_ptr<ServiceModel> sm(sm_ptr);
    ASSERT_TRUE(ran.register_sm(std::move(sm)) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.start() == ErrorCode::SUCCESS);

    std::mutex mu;
    std::condition_variable cv;
    int indications = 0;
    bool sub_resp_ok = false;

    E3Agent dapp(make_dapp_config(ep));
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

    // Wait for setup
    ASSERT_TRUE(dapp.wait_for_setup(3000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp.dapp_id().has_value());

    // Settle delay: lets the PUB/SUB sockets on both sides finish binding/
    // connecting before the first subscription is sent. Without this, ZMQ's
    // slow-joiner problem can drop the first SubscriptionRequest.
    std::this_thread::sleep_for(500ms);

    // Subscribe and wait for response (retry if needed — sub/pub can drop
    // the very first message if SUB hasn't bound yet).
    ASSERT_TRUE(dapp.subscribe(1, {1}, {1}) == ErrorCode::SUCCESS);
    {
        std::unique_lock<std::mutex> lk(mu);
        bool ok = cv.wait_for(lk, 2s, [&]() { return sub_resp_ok; });
        if (!ok) {
            // Retry once after a longer settle.
            lk.unlock();
            std::this_thread::sleep_for(500ms);
            (void)dapp.subscribe(1, {1}, {1});
            lk.lock();
            ASSERT_TRUE(cv.wait_for(lk, 3s, [&]() { return sub_resp_ok; }));
        }
    }

    // Wait for at least 2 indications
    {
        std::unique_lock<std::mutex> lk(mu);
        ASSERT_TRUE(cv.wait_for(lk, 5s, [&]() { return indications >= 2; }));
    }

    // Send a control action and verify the SM saw it
    std::vector<uint8_t> ctrl;
    ASSERT_TRUE(libe3_examples::encode_simple_control(42, ctrl));
    ASSERT_TRUE(dapp.send_control(1, 1, ctrl) == ErrorCode::SUCCESS);

    // Allow the SM up to 2s to receive and ack the control
    for (int i = 0; i < 40 && sm_ptr->control_count.load() == 0; ++i) {
        std::this_thread::sleep_for(50ms);
    }
    ASSERT_GE(sm_ptr->control_count.load(), 1);
    ASSERT_EQ(sm_ptr->last_control_id.load(), 1u);

    // Release + stop
    ASSERT_TRUE(dapp.release() == ErrorCode::SUCCESS);
    std::this_thread::sleep_for(200ms);
    dapp.stop();
    ran.stop();
}

int main() {
    return RUN_ALL_TESTS();
}
