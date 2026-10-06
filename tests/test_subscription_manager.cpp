/**
 * @file test_subscription_manager.cpp
 * @brief Unit tests for SubscriptionManager
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/subscription_manager.hpp"
#include <thread>
#include <atomic>

using namespace libe3;

TEST(SubscriptionManager_initial_state) {
    SubscriptionManager mgr;
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_EQ(mgr.subscription_count(), 0u);
    ASSERT_TRUE(mgr.get_registered_dapps().empty());
}

TEST(SubscriptionManager_register_dapp) {
    SubscriptionManager mgr;
    
    auto [result, dapp_id] = mgr.register_dapp();
    ASSERT_TRUE(result == ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.dapp_count(), 1u);
    ASSERT_TRUE(mgr.is_dapp_registered(dapp_id));
}

TEST(SubscriptionManager_register_multiple_dapps_assigns_unique_ids) {
    SubscriptionManager mgr;
    
    auto [result1, id1] = mgr.register_dapp();
    auto [result2, id2] = mgr.register_dapp();
    auto [result3, id3] = mgr.register_dapp();
    
    ASSERT_TRUE(result1 == ErrorCode::SUCCESS);
    ASSERT_TRUE(result2 == ErrorCode::SUCCESS);
    ASSERT_TRUE(result3 == ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.dapp_count(), 3u);
    
    // All IDs should be unique
    ASSERT_NE(id1, id2);
    ASSERT_NE(id2, id3);
    ASSERT_NE(id1, id3);
}

TEST(SubscriptionManager_unregister_dapp) {
    SubscriptionManager mgr;
    
    auto [reg_result, dapp_id] = mgr.register_dapp();
    ASSERT_TRUE(reg_result == ErrorCode::SUCCESS);
    
    auto result = mgr.unregister_dapp(dapp_id);
    ASSERT_TRUE(result == ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_FALSE(mgr.is_dapp_registered(dapp_id));
}

TEST(SubscriptionManager_unregister_nonexistent) {
    SubscriptionManager mgr;
    auto result = mgr.unregister_dapp(999);
    ASSERT_TRUE(result == ErrorCode::DAPP_NOT_REGISTERED);
}

TEST(SubscriptionManager_add_subscription) {
    SubscriptionManager mgr;
    
    auto [reg_result, dapp_id] = mgr.register_dapp();
    auto [result, sub_id] = mgr.add_subscription(dapp_id, 200);
    ASSERT_TRUE(result == ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.subscription_count(), 1u);
    ASSERT_TRUE(mgr.is_subscribed(dapp_id, 200));
}

TEST(SubscriptionManager_add_subscription_unregistered_dapp) {
    SubscriptionManager mgr;
    auto [result, sub_id] = mgr.add_subscription(100, 200);
    ASSERT_TRUE(result == ErrorCode::DAPP_NOT_REGISTERED);
}

TEST(SubscriptionManager_add_subscription_duplicate) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();
    mgr.add_subscription(dapp_id, 200);
    auto [result, sub_id] = mgr.add_subscription(dapp_id, 200);
    ASSERT_TRUE(result == ErrorCode::SUBSCRIPTION_EXISTS);
}

TEST(SubscriptionManager_remove_subscription) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();
    mgr.add_subscription(dapp_id, 200);
    
    auto result = mgr.remove_subscription(dapp_id, 200);
    ASSERT_TRUE(result == ErrorCode::SUCCESS);
    ASSERT_FALSE(mgr.is_subscribed(dapp_id, 200));
    ASSERT_EQ(mgr.subscription_count(), 0u);
}

TEST(SubscriptionManager_remove_subscription_not_found) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();
    auto result = mgr.remove_subscription(dapp_id, 999);
    ASSERT_TRUE(result == ErrorCode::SUBSCRIPTION_NOT_FOUND);
}

TEST(SubscriptionManager_get_subscribed_dapps) {
    SubscriptionManager mgr;
    auto [res1, dapp1] = mgr.register_dapp();
    auto [res2, dapp2] = mgr.register_dapp();
    auto [res3, dapp3] = mgr.register_dapp();
    
    mgr.add_subscription(dapp1, 500);
    mgr.add_subscription(dapp2, 500);
    mgr.add_subscription(dapp3, 600);
    
    auto subscribers = mgr.get_subscribed_dapps(500);
    ASSERT_EQ(subscribers.size(), 2u);
    
    auto sub600 = mgr.get_subscribed_dapps(600);
    ASSERT_EQ(sub600.size(), 1u);
    
    auto sub999 = mgr.get_subscribed_dapps(999);
    ASSERT_TRUE(sub999.empty());
}

TEST(SubscriptionManager_get_dapp_subscriptions) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();
    mgr.add_subscription(dapp_id, 200);
    mgr.add_subscription(dapp_id, 300);
    mgr.add_subscription(dapp_id, 400);
    
    auto subs = mgr.get_dapp_subscriptions(dapp_id);
    ASSERT_EQ(subs.size(), 3u);
}

TEST(SubscriptionManager_unregister_clears_subscriptions) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();
    mgr.add_subscription(dapp_id, 200);
    mgr.add_subscription(dapp_id, 300);
    
    mgr.unregister_dapp(dapp_id);
    ASSERT_EQ(mgr.subscription_count(), 0u);
    
    // Re-register and verify subscriptions are gone
    auto [reg_result2, new_dapp_id] = mgr.register_dapp();
    ASSERT_TRUE(mgr.get_dapp_subscriptions(new_dapp_id).empty());
}

TEST(SubscriptionManager_multiple_dapps) {
    SubscriptionManager mgr;
    std::vector<uint32_t> dapp_ids;
    
    for (uint32_t i = 0; i < 10; ++i) {
        auto [result, id] = mgr.register_dapp();
        ASSERT_TRUE(result == ErrorCode::SUCCESS);
        dapp_ids.push_back(id);
        mgr.add_subscription(id, 100);
        mgr.add_subscription(id, 200);
    }
    
    ASSERT_EQ(mgr.dapp_count(), 10u);
    ASSERT_EQ(mgr.subscription_count(), 20u);
    ASSERT_EQ(mgr.get_subscribed_dapps(100).size(), 10u);
}

TEST(SubscriptionManager_has_subscribers) {
    SubscriptionManager mgr;
    
    ASSERT_FALSE(mgr.has_subscribers(100));
    
    auto [reg_result, dapp_id] = mgr.register_dapp();
    ASSERT_FALSE(mgr.has_subscribers(100));
    
    mgr.add_subscription(dapp_id, 100);
    ASSERT_TRUE(mgr.has_subscribers(100));
}

TEST(SubscriptionManager_sm_lifecycle_callback) {
    SubscriptionManager mgr;
    
    std::atomic<int> start_count{0};
    std::atomic<int> stop_count{0};
    uint32_t last_started_sm = 0;
    uint32_t last_stopped_sm = 0;
    
    mgr.set_sm_lifecycle_callback(
        [&](uint32_t sm_id, bool should_start) {
            if (should_start) {
                ++start_count;
                last_started_sm = sm_id;
            } else {
                ++stop_count;
                last_stopped_sm = sm_id;
            }
        }
    );
    
    auto [reg1, dapp1] = mgr.register_dapp();
    
    // First subscription should trigger start
    mgr.add_subscription(dapp1, 100);
    ASSERT_EQ(start_count.load(), 1);
    ASSERT_EQ(last_started_sm, 100u);
    
    // Second subscription to same SM should not trigger start
    auto [reg2, dapp2] = mgr.register_dapp();
    mgr.add_subscription(dapp2, 100);
    ASSERT_EQ(start_count.load(), 1);
    
    // Remove one subscription - SM still has subscribers
    mgr.remove_subscription(dapp1, 100);
    ASSERT_EQ(stop_count.load(), 0);
    
    // Remove last subscription - should trigger stop
    mgr.remove_subscription(dapp2, 100);
    ASSERT_EQ(stop_count.load(), 1);
    ASSERT_EQ(last_stopped_sm, 100u);
}

TEST(SubscriptionManager_thread_safety) {
    SubscriptionManager mgr;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    // Pre-register some dApps and collect their IDs
    std::vector<uint32_t> dapp_ids;
    for (uint32_t i = 0; i < 10; ++i) {
        auto [result, id] = mgr.register_dapp();
        dapp_ids.push_back(id);
    }
    
    // Concurrent operations
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; ++i) {
                uint32_t dapp_id = dapp_ids[static_cast<size_t>(t)];
                uint32_t sm_id = static_cast<uint32_t>(i % 5);
                
                auto [add_err, sub_id] = mgr.add_subscription(dapp_id, sm_id);
                if (add_err == ErrorCode::SUCCESS || add_err == ErrorCode::SUBSCRIPTION_EXISTS) {
                    ++success_count;
                } else {
                    ++error_count;
                }
                
                mgr.get_subscribed_dapps(sm_id);
                mgr.get_dapp_subscriptions(dapp_id);
                
                mgr.remove_subscription(dapp_id, sm_id);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // No crashes or deadlocks means success
    ASSERT_GT(success_count.load(), 0);
}

TEST(SubscriptionManager_clear) {
    SubscriptionManager mgr;
    auto [res1, id1] = mgr.register_dapp();
    auto [res2, id2] = mgr.register_dapp();
    mgr.add_subscription(id1, 100);
    mgr.add_subscription(id2, 200);
    
    mgr.clear();
    
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_EQ(mgr.subscription_count(), 0u);
}

TEST(SubscriptionManager_subscription_details_stored) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();

    std::vector<uint32_t> tids = {10, 20, 30};
    std::vector<uint32_t> cids = {7, 8};
    auto [result, sub_id] = mgr.add_subscription(dapp_id, 200, tids, cids, 5000);
    ASSERT_TRUE(result == ErrorCode::SUCCESS);

    const auto* details = mgr.get_subscription_details(dapp_id, 200);
    ASSERT_TRUE(details != nullptr);
    ASSERT_EQ(details->periodicity_us, 5000u);
    ASSERT_EQ(details->telemetry_ids.size(), 3u);
    ASSERT_EQ(details->telemetry_ids[0], 10u);
    ASSERT_EQ(details->telemetry_ids[1], 20u);
    ASSERT_EQ(details->telemetry_ids[2], 30u);
    ASSERT_EQ(details->control_ids.size(), 2u);
    ASSERT_EQ(details->control_ids[0], 7u);
    ASSERT_EQ(details->control_ids[1], 8u);
}

TEST(SubscriptionManager_subscription_details_defaults) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();

    auto [result, sub_id] = mgr.add_subscription(dapp_id, 200);
    ASSERT_TRUE(result == ErrorCode::SUCCESS);

    const auto* details = mgr.get_subscription_details(dapp_id, 200);
    ASSERT_TRUE(details != nullptr);
    ASSERT_EQ(details->periodicity_us, 0u);
    ASSERT_TRUE(details->telemetry_ids.empty());
    ASSERT_TRUE(details->control_ids.empty());
}

TEST(SubscriptionManager_subscription_details_removed_on_delete) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();

    std::vector<uint32_t> tids = {1};
    mgr.add_subscription(dapp_id, 200, tids, {}, 1000);
    mgr.remove_subscription(dapp_id, 200);

    const auto* details = mgr.get_subscription_details(dapp_id, 200);
    ASSERT_TRUE(details == nullptr);
}

TEST(SubscriptionManager_subscription_details_removed_on_unregister) {
    SubscriptionManager mgr;
    auto [reg_result, dapp_id] = mgr.register_dapp();

    std::vector<uint32_t> tids = {5, 6};
    mgr.add_subscription(dapp_id, 300, tids, {}, 2000);
    mgr.unregister_dapp(dapp_id);

    const auto* details = mgr.get_subscription_details(dapp_id, 300);
    ASSERT_TRUE(details == nullptr);
}

TEST(SubscriptionManager_dapp_ids_are_lowest_free) {
    SubscriptionManager mgr;
    auto [r1, id1] = mgr.register_dapp();
    auto [r2, id2] = mgr.register_dapp();
    auto [r3, id3] = mgr.register_dapp();
    ASSERT_EQ(id1, 1u);
    ASSERT_EQ(id2, 2u);
    ASSERT_EQ(id3, 3u);

    // A freed id is handed out again before any higher one.
    ASSERT_TRUE(mgr.unregister_dapp(id2) == ErrorCode::SUCCESS);
    auto [r4, id4] = mgr.register_dapp();
    ASSERT_EQ(id4, 2u);
    auto [r5, id5] = mgr.register_dapp();
    ASSERT_EQ(id5, 4u);
}

TEST(SubscriptionManager_dapp_ids_do_not_climb_with_reconnects) {
    SubscriptionManager mgr;
    // OCUDU's libe3-compatible peers only accept 1..100, so churn alone must
    // not push ids past it.
    for (int i = 0; i < 500; ++i) {
        auto [rc, id] = mgr.register_dapp();
        ASSERT_TRUE(rc == ErrorCode::SUCCESS);
        ASSERT_EQ(id, 1u);
        ASSERT_TRUE(mgr.unregister_dapp(id) == ErrorCode::SUCCESS);
    }
}

TEST(SubscriptionManager_dapp_ids_go_past_100_and_run_out_at_65535) {
    SubscriptionManager mgr;
    for (uint32_t expected = 1; expected <= 65535; ++expected) {
        auto [rc, id] = mgr.register_dapp();
        ASSERT_TRUE(rc == ErrorCode::SUCCESS);
        ASSERT_EQ(id, expected);
    }
    ASSERT_EQ(mgr.dapp_count(), 65535u);

    auto [rc, id] = mgr.register_dapp();
    ASSERT_TRUE(rc == ErrorCode::INTERNAL_ERROR);
    ASSERT_EQ(mgr.dapp_count(), 65535u);

    // Exhaustion clears as soon as one id is freed.
    ASSERT_TRUE(mgr.unregister_dapp(4242) == ErrorCode::SUCCESS);
    auto [rc2, id2] = mgr.register_dapp();
    ASSERT_TRUE(rc2 == ErrorCode::SUCCESS);
    ASSERT_EQ(id2, 4242u);
}

TEST(SubscriptionManager_subscription_ids_are_lowest_free_and_not_capped_at_100) {
    SubscriptionManager mgr;
    auto [reg, dapp_id] = mgr.register_dapp();

    for (uint32_t ran_function = 1; ran_function <= 150; ++ran_function) {
        auto [rc, sub_id] = mgr.add_subscription(dapp_id, ran_function, {1}, {}, 0);
        ASSERT_TRUE(rc == ErrorCode::SUCCESS);
        ASSERT_EQ(sub_id, ran_function);
    }

    // The freed id is reused, and nothing live is overwritten.
    ASSERT_TRUE(mgr.remove_subscription(dapp_id, 7) == ErrorCode::SUCCESS);
    auto [rc, sub_id] = mgr.add_subscription(dapp_id, 1000, {1}, {}, 0);
    ASSERT_TRUE(rc == ErrorCode::SUCCESS);
    ASSERT_EQ(sub_id, 7u);
    ASSERT_TRUE(mgr.is_subscribed(dapp_id, 1));
    ASSERT_TRUE(mgr.is_subscribed(dapp_id, 150));
    ASSERT_EQ(mgr.subscription_count(), 150u);
}

TEST(SubscriptionManager_subscription_ids_run_out_without_overwriting) {
    SubscriptionManager mgr;
    auto [reg, dapp_id] = mgr.register_dapp();

    for (uint32_t ran_function = 1; ran_function <= 65535; ++ran_function) {
        auto [rc, sub_id] = mgr.add_subscription(dapp_id, ran_function, {1}, {}, 0);
        ASSERT_TRUE(rc == ErrorCode::SUCCESS);
        ASSERT_EQ(sub_id, ran_function);
    }
    ASSERT_EQ(mgr.subscription_count(), 65535u);

    // The 65536th gets no id and leaves every live subscription as it was.
    auto [rc, sub_id] = mgr.add_subscription(dapp_id, 70000, {1}, {}, 0);
    ASSERT_TRUE(rc == ErrorCode::INTERNAL_ERROR);
    ASSERT_FALSE(mgr.is_subscribed(dapp_id, 70000));
    ASSERT_EQ(mgr.subscription_count(), 65535u);
    ASSERT_TRUE(mgr.is_subscribed(dapp_id, 1));

    // Removing by id frees exactly that id.
    ASSERT_TRUE(mgr.remove_subscription_by_id(dapp_id, 65535) == ErrorCode::SUCCESS);
    auto [rc2, sub_id2] = mgr.add_subscription(dapp_id, 70000, {1}, {}, 0);
    ASSERT_TRUE(rc2 == ErrorCode::SUCCESS);
    ASSERT_EQ(sub_id2, 65535u);
}

TEST(SubscriptionManager_unregister_frees_subscription_ids) {
    SubscriptionManager mgr;
    auto [reg1, dapp1] = mgr.register_dapp();
    auto [reg2, dapp2] = mgr.register_dapp();
    mgr.add_subscription(dapp1, 10, {1}, {}, 0);
    mgr.add_subscription(dapp1, 11, {1}, {}, 0);

    mgr.unregister_dapp(dapp1);
    auto [rc, sub_id] = mgr.add_subscription(dapp2, 12, {1}, {}, 0);
    ASSERT_TRUE(rc == ErrorCode::SUCCESS);
    ASSERT_EQ(sub_id, 1u);
}

int main() {
    return RUN_ALL_TESTS();
}
