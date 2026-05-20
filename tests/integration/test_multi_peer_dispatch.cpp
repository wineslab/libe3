/**
 * @file test_multi_peer_dispatch.cpp
 * @brief Verify that TWO dApp-role E3Agent instances in ONE process do not
 *        share mutable state — each has its own dapp_id, its own subscription
 *        state, its own handlers; subscriptions to distinct RAN functions
 *        route indications to the right dApp instance with no crosstalk.
 *
 * This exercises the multi-peer capability added by issue #15: a dApp
 * application can hold multiple E3Agent instances (typically one per remote
 * RAN). In CI we can't easily fork two RAN binaries, so this test uses ONE
 * RAN with TWO service models (different RAN function ids) and TWO dApps
 * subscribed to disjoint SMs. The isolation we validate (per-instance
 * dapp_state_, per-instance handlers, per-instance subscription tracking)
 * is the same property that matters when each E3Agent connects to a
 * different remote RAN.
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
    char tmpl[] = "/tmp/libe3_multi_peer_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

struct Endpoints {
    std::string setup, subscriber, publisher;
};
Endpoints make_endpoints(const std::string& dir) {
    return {"ipc://" + dir + "/setup",
            "ipc://" + dir + "/dapp_socket",
            "ipc://" + dir + "/e3_socket"};
}

// SM that tags every indication so we can detect cross-talk.
// rf_id varies per instance; tag is encoded into the indication payload.
class TaggedSM : public ServiceModel {
public:
    TaggedSM(uint32_t rf_id, uint32_t tag) : rf_id_(rf_id), tag_(tag) {}
    std::string name() const override { return "TAGGED-" + std::to_string(rf_id_); }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return rf_id_; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { stop(); }
    bool is_running() const override { return running_; }

    std::vector<uint8_t> ran_function_data() const override {
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data(name(), out)) return out;
        return {0x01};
    }

    ErrorCode start() override {
        if (running_) return ErrorCode::SUCCESS;
        running_ = true;
        worker_ = std::thread([this]() {
            uint32_t seq = 0;
            while (running_) {
                std::this_thread::sleep_for(150ms);
                auto subs = get_subscribers();
                if (subs.empty()) continue;
                libe3_examples::SimpleIndication si{tag_ * 1000 + seq, 0};
                std::vector<uint8_t> enc;
                if (!libe3_examples::encode_simple_indication(si, enc)) continue;
                for (auto did : subs) {
                    Pdu pdu = make_indication_pdu(did, rf_id_, enc);
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
        int sampling = 0;
        if (libe3_examples::decode_simple_control(a.action_data, sampling)) {
            last_action_tag = static_cast<uint32_t>(sampling);
        }
        Pdu ack = make_message_ack_pdu(request_message_id, ResponseCode::POSITIVE);
        return emit_outbound(std::move(ack));
    }
    std::atomic<int> control_count{0};
    std::atomic<uint32_t> last_action_tag{0};

private:
    uint32_t rf_id_;
    uint32_t tag_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

}  // namespace

TEST(multi_peer_two_dapps_distinct_rfs_no_crosstalk) {
    const std::string dir = make_tmpdir();
    const auto ep = make_endpoints(dir);

    E3Config ran_cfg;
    ran_cfg.role = E3Role::RAN;
    ran_cfg.ran_identifier = "test-ran";
    ran_cfg.link_layer = E3LinkLayer::ZMQ;
    ran_cfg.transport_layer = E3TransportLayer::IPC;
    ran_cfg.encoding = EncodingFormat::ASN1;
    ran_cfg.log_level = 0;
    ran_cfg.setup_endpoint = ep.setup;
    ran_cfg.subscriber_endpoint = ep.subscriber;
    ran_cfg.publisher_endpoint = ep.publisher;

    auto make_dapp_cfg = [&]() {
        auto c = ran_cfg;
        c.role = E3Role::DAPP;
        c.dapp_name = "MultiPeerDApp";
        return c;
    };

    // ONE RAN, TWO SMs (different rf_ids)
    E3Agent ran(ran_cfg);
    auto* sm1 = new TaggedSM(/*rf_id=*/1, /*tag=*/1);
    auto* sm2 = new TaggedSM(/*rf_id=*/2, /*tag=*/2);
    ASSERT_TRUE(ran.register_sm(std::unique_ptr<ServiceModel>(sm1)) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.register_sm(std::unique_ptr<ServiceModel>(sm2)) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.start() == ErrorCode::SUCCESS);

    // TWO dApp E3Agent instances in this same process — each subscribes
    // to a DIFFERENT SM (rf_id 1 vs 2). Validates per-instance dispatch.
    std::mutex mu;
    std::condition_variable cv;
    std::atomic<int> a_indications{0};
    std::atomic<int> b_indications{0};
    std::atomic<bool> a_saw_b_tag{false};
    std::atomic<bool> b_saw_a_tag{false};
    std::atomic<bool> a_sub_ok{false};
    std::atomic<bool> b_sub_ok{false};

    E3Agent dapp_a(make_dapp_cfg());
    dapp_a.set_indication_handler([&](const IndicationMessage& msg) {
        libe3_examples::SimpleIndication si;
        if (libe3_examples::decode_simple_indication(msg.protocol_data, si)) {
            if (si.data1 / 1000 != 1) a_saw_b_tag = true;  // crosstalk
            ++a_indications;
            cv.notify_all();
        }
    });
    dapp_a.set_subscription_response_handler([&](const SubscriptionResponse& r) {
        a_sub_ok = (r.response_code == ResponseCode::POSITIVE);
        cv.notify_all();
    });

    E3Agent dapp_b(make_dapp_cfg());
    dapp_b.set_indication_handler([&](const IndicationMessage& msg) {
        libe3_examples::SimpleIndication si;
        if (libe3_examples::decode_simple_indication(msg.protocol_data, si)) {
            if (si.data1 / 1000 != 2) b_saw_a_tag = true;  // crosstalk
            ++b_indications;
            cv.notify_all();
        }
    });
    dapp_b.set_subscription_response_handler([&](const SubscriptionResponse& r) {
        b_sub_ok = (r.response_code == ResponseCode::POSITIVE);
        cv.notify_all();
    });

    ASSERT_TRUE(dapp_a.start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp_b.start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp_a.wait_for_setup(3000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp_b.wait_for_setup(3000ms) == ErrorCode::SUCCESS);

    // Per-instance state isolation: distinct dapp_ids
    ASSERT_TRUE(dapp_a.dapp_id().has_value());
    ASSERT_TRUE(dapp_b.dapp_id().has_value());
    ASSERT_NE(*dapp_a.dapp_id(), *dapp_b.dapp_id());

    std::this_thread::sleep_for(500ms);  // ZMQ PUB/SUB settle

    // dApp A subscribes to rf=1; dApp B subscribes to rf=2.
    ASSERT_TRUE(dapp_a.subscribe(1, {1}, {1}) == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp_b.subscribe(2, {1}, {1}) == ErrorCode::SUCCESS);

    // Wait for indications on both peers (or 5 s).
    {
        std::unique_lock<std::mutex> lk(mu);
        bool ok = cv.wait_for(lk, 5s, [&]() {
            return a_indications.load() >= 1 && b_indications.load() >= 1;
        });
        if (!ok) {
            // Re-subscribe in case the very-first PUB/SUB message was dropped.
            lk.unlock();
            std::this_thread::sleep_for(500ms);
            if (!a_sub_ok) (void)dapp_a.subscribe(1, {1}, {1});
            if (!b_sub_ok) (void)dapp_b.subscribe(2, {1}, {1});
            lk.lock();
            ASSERT_TRUE(cv.wait_for(lk, 5s, [&]() {
                return a_indications.load() >= 1 && b_indications.load() >= 1;
            }));
        }
    }
    ASSERT_FALSE(a_saw_b_tag.load());
    ASSERT_FALSE(b_saw_a_tag.load());

    // Send a control through dapp_a (rf=1) → should reach sm1 only.
    std::vector<uint8_t> ctrl;
    ASSERT_TRUE(libe3_examples::encode_simple_control(77, ctrl));
    ASSERT_TRUE(dapp_a.send_control(1, 1, ctrl) == ErrorCode::SUCCESS);
    for (int i = 0; i < 40 && sm1->control_count.load() == 0; ++i) {
        std::this_thread::sleep_for(50ms);
    }
    ASSERT_GE(sm1->control_count.load(), 1);
    ASSERT_EQ(sm2->control_count.load(), 0);  // No crosstalk

    dapp_a.stop();
    dapp_b.stop();
    ran.stop();
}

int main() {
    return RUN_ALL_TESTS();
}
