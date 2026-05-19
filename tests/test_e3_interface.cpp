/**
 * @file test_e3_interface.cpp
 * @brief Unit tests for E3Interface: DAppStatusChangedHandler fires on dApp lifecycle events
 *
 * Exercises the full callback chain by manipulating E3Interface's
 * SubscriptionManager directly (simulating what the internal message handlers
 * do), then calling notify_dapp_status_changed() as those handlers do.
 *
 * This approach avoids ZMQ transport while still testing the real
 * E3Interface code paths (set_dapp_status_changed_handler wiring,
 * notify_dapp_status_changed dispatch, and subscription_manager integration).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_interface.hpp"

using namespace libe3;

// ============================================================================
// Helpers
// ============================================================================

inline int error_to_int(ErrorCode e) { return static_cast<int>(e); }

static E3Config make_test_config() {
    E3Config cfg;
    cfg.ran_identifier = "integ-test";
    cfg.log_level = 0;
    return cfg;
}

// Minimal SM so the interface has a RAN function to subscribe to
class IntegTestSM : public ServiceModel {
public:
    std::string name() const override { return "IntegTestSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return 100; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {10}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }
private:
    bool running_ = false;
};

// ============================================================================
// Tests
// ============================================================================

/**
 * Verify that the handler fires on dApp connect (register_dapp succeeds).
 * Mirrors the code path in E3Interface::handle_setup_request().
 */
TEST(DAppStatusChanged_fires_on_connect) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    // Simulate: dApp sends SetupRequest → register_dapp() succeeds
    auto [result, dapp_id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::SUCCESS));

    // The real handle_setup_request() calls this after register_dapp() succeeds
    iface.notify_dapp_status_changed();

    ASSERT_EQ(count, 1);
    ASSERT_GT(dapp_id, 0u);
}

/**
 * Verify that the handler fires on dApp subscribe (add_subscription succeeds).
 * Mirrors the code path in E3Interface::handle_subscription_request().
 */
TEST(DAppStatusChanged_fires_on_subscribe) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));
    iface.register_sm(std::make_unique<IntegTestSM>());

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    // Register a dApp first
    auto [reg_result, dapp_id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(reg_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();  // connect event

    // Simulate: dApp sends SubscriptionRequest → add_subscription() succeeds
    auto [sub_result, sub_id] = iface.subscription_manager().add_subscription(dapp_id, 100);
    ASSERT_EQ(error_to_int(sub_result), error_to_int(ErrorCode::SUCCESS));

    // The real handle_subscription_request() calls this when result == SUCCESS
    iface.notify_dapp_status_changed();

    ASSERT_EQ(count, 2);  // connect + subscribe
}

/**
 * Verify that the handler fires on dApp unsubscribe (remove_subscription succeeds).
 * Mirrors the code path in E3Interface::handle_subscription_delete().
 */
TEST(DAppStatusChanged_fires_on_unsubscribe) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));
    iface.register_sm(std::make_unique<IntegTestSM>());

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    auto [reg_result, dapp_id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(reg_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();  // connect

    auto [sub_result, sub_id] = iface.subscription_manager().add_subscription(dapp_id, 100);
    ASSERT_EQ(error_to_int(sub_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();  // subscribe

    // Simulate: dApp sends SubscriptionDelete → remove_subscription succeeds
    ErrorCode unsub_result = iface.subscription_manager().remove_subscription_by_id(dapp_id, sub_id);
    ASSERT_EQ(error_to_int(unsub_result), error_to_int(ErrorCode::SUCCESS));

    // The real handle_subscription_delete() calls this when result == SUCCESS
    iface.notify_dapp_status_changed();

    ASSERT_EQ(count, 3);  // connect + subscribe + unsubscribe
}

/**
 * Verify that the handler fires on dApp disconnect (unregister_dapp succeeds).
 * Mirrors the code path in E3Interface::handle_dapp_disconnection().
 */
TEST(DAppStatusChanged_fires_on_disconnect) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    auto [reg_result, dapp_id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(reg_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();  // connect

    // Simulate: dApp sends ReleaseMessage → unregister_dapp() succeeds
    ErrorCode unreg_result = iface.subscription_manager().unregister_dapp(dapp_id);
    ASSERT_EQ(error_to_int(unreg_result), error_to_int(ErrorCode::SUCCESS));

    // The real handle_dapp_disconnection() calls this when result == SUCCESS
    iface.notify_dapp_status_changed();

    ASSERT_EQ(count, 2);  // connect + disconnect
}

/**
 * Full lifecycle in one sequence: connect → subscribe → unsubscribe → disconnect.
 * Verifies the callback counter increments correctly at each step.
 */
TEST(DAppStatusChanged_full_lifecycle) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));
    iface.register_sm(std::make_unique<IntegTestSM>());

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    // Connect
    auto [reg_result, dapp_id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(reg_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();
    ASSERT_EQ(count, 1);

    // Subscribe
    auto [sub_result, sub_id] = iface.subscription_manager().add_subscription(dapp_id, 100);
    ASSERT_EQ(error_to_int(sub_result), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();
    ASSERT_EQ(count, 2);

    // Unsubscribe
    ErrorCode unsub = iface.subscription_manager().remove_subscription_by_id(dapp_id, sub_id);
    ASSERT_EQ(error_to_int(unsub), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();
    ASSERT_EQ(count, 3);

    // Disconnect
    ErrorCode unreg = iface.subscription_manager().unregister_dapp(dapp_id);
    ASSERT_EQ(error_to_int(unreg), error_to_int(ErrorCode::SUCCESS));
    iface.notify_dapp_status_changed();
    ASSERT_EQ(count, 4);
}

/**
 * Verify no crash and no spurious callback when no dApp is registered.
 */
TEST(DAppStatusChanged_no_spurious_calls) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    int count = 0;
    iface.set_dapp_status_changed_handler([&count]() { ++count; });

    // No lifecycle events — callback must not have fired
    ASSERT_EQ(count, 0);
}

// ============================================================================
// get_dapp_encoding(): SMs use this to pick the encoder matching the
// dApp's channel. Tested without ZMQ — SubscriptionManager::register_dapp
// records the channel index, and E3Interface combines that with the
// channel list to surface the encoding.
// ============================================================================

TEST(DappEncoding_single_channel_asn1_returns_asn1) {
    E3Config cfg = make_test_config();
    cfg.encoding = EncodingFormat::ASN1;
    E3Interface iface(cfg);
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    auto [res, id] = iface.subscription_manager().register_dapp(/*channel_index=*/0);
    ASSERT_EQ(error_to_int(res), error_to_int(ErrorCode::SUCCESS));

    auto enc = iface.get_dapp_encoding(id);
    ASSERT_TRUE(enc.has_value());
    ASSERT_EQ(static_cast<int>(*enc), static_cast<int>(EncodingFormat::ASN1));
}

TEST(DappEncoding_single_channel_json_returns_json) {
    E3Config cfg = make_test_config();
    cfg.encoding = EncodingFormat::JSON;
    E3Interface iface(cfg);
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    auto [res, id] = iface.subscription_manager().register_dapp(/*channel_index=*/0);
    ASSERT_EQ(error_to_int(res), error_to_int(ErrorCode::SUCCESS));

    auto enc = iface.get_dapp_encoding(id);
    ASSERT_TRUE(enc.has_value());
    ASSERT_EQ(static_cast<int>(*enc), static_cast<int>(EncodingFormat::JSON));
}

TEST(DappEncoding_dual_channel_returns_per_dapp_encoding) {
    E3Config cfg = make_test_config();
    cfg.encoding              = EncodingFormat::ASN1;        // primary
    cfg.enable_dual_encoding  = true;
    cfg.secondary_encoding    = EncodingFormat::JSON;        // secondary
    // Distinct ports so init() doesn't reject a collision.
    cfg.setup_port            = 19990;
    cfg.subscriber_port       = 19999;
    cfg.publisher_port        = 19991;
    cfg.secondary_setup_port  = 15555;
    cfg.secondary_subscriber_port = 15557;
    cfg.secondary_publisher_port  = 15556;

    E3Interface iface(cfg);
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    auto [res1, id_a] = iface.subscription_manager().register_dapp(/*channel_index=*/0);
    auto [res2, id_b] = iface.subscription_manager().register_dapp(/*channel_index=*/1);
    ASSERT_EQ(error_to_int(res1), error_to_int(ErrorCode::SUCCESS));
    ASSERT_EQ(error_to_int(res2), error_to_int(ErrorCode::SUCCESS));

    auto enc_a = iface.get_dapp_encoding(id_a);
    auto enc_b = iface.get_dapp_encoding(id_b);
    ASSERT_TRUE(enc_a.has_value());
    ASSERT_TRUE(enc_b.has_value());
    ASSERT_EQ(static_cast<int>(*enc_a), static_cast<int>(EncodingFormat::ASN1));
    ASSERT_EQ(static_cast<int>(*enc_b), static_cast<int>(EncodingFormat::JSON));
}

TEST(DappEncoding_unregistered_dapp_returns_nullopt) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    auto enc = iface.get_dapp_encoding(/*never-registered*/ 42);
    ASSERT_FALSE(enc.has_value());
}

TEST(DappEncoding_returns_nullopt_after_unregister) {
    E3Interface iface(make_test_config());
    ASSERT_EQ(error_to_int(iface.init()), error_to_int(ErrorCode::SUCCESS));

    auto [res, id] = iface.subscription_manager().register_dapp();
    ASSERT_EQ(error_to_int(res), error_to_int(ErrorCode::SUCCESS));
    ASSERT_TRUE(iface.get_dapp_encoding(id).has_value());

    ASSERT_EQ(error_to_int(iface.subscription_manager().unregister_dapp(id)),
              error_to_int(ErrorCode::SUCCESS));
    ASSERT_FALSE(iface.get_dapp_encoding(id).has_value());
}

int main() {
    return RUN_ALL_TESTS();
}
