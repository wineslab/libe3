/**
 * @file test_subscription_manager.cpp
 * @brief Unit tests for SubscriptionManager
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/subscription_manager.hpp"
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>

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

TEST(SubscriptionManager_dapp_ids_are_the_lowest_free_ones) {
    SubscriptionManager mgr;
    auto [r1, id1] = mgr.register_dapp();
    auto [r2, id2] = mgr.register_dapp();
    auto [r3, id3] = mgr.register_dapp();
    ASSERT_EQ(id1, 1u);
    ASSERT_EQ(id2, 2u);
    ASSERT_EQ(id3, 3u);

    // A dApp that disconnects and reconnects gets its id back, however often it does.
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(mgr.unregister_dapp(2) == ErrorCode::SUCCESS);
        auto [r, id] = mgr.register_dapp();
        ASSERT_TRUE(r == ErrorCode::SUCCESS);
        ASSERT_EQ(id, 2u);
    }
}

TEST(SubscriptionManager_serves_more_than_100_dapps_with_distinct_ids) {
    SubscriptionManager mgr;
    std::unordered_set<uint32_t> dapp_ids, sub_ids;
    for (int i = 0; i < 300; ++i) {
        auto [r, dapp_id] = mgr.register_dapp();
        ASSERT_TRUE(r == ErrorCode::SUCCESS);
        ASSERT_TRUE(dapp_ids.insert(dapp_id).second);
        auto [sr, sub_id] = mgr.add_subscription(dapp_id, 1);
        ASSERT_TRUE(sr == ErrorCode::SUCCESS);
        ASSERT_TRUE(sub_ids.insert(sub_id).second);   // a live subscription id is never handed out again
    }
    ASSERT_EQ(mgr.dapp_count(), 300u);
    ASSERT_EQ(*std::max_element(dapp_ids.begin(), dapp_ids.end()), 300u);
    ASSERT_EQ(*std::max_element(sub_ids.begin(), sub_ids.end()), 300u);
}

TEST(SubscriptionManager_subscription_id_of_a_deleted_subscription_is_reused) {
    SubscriptionManager mgr;
    auto [r, dapp_id] = mgr.register_dapp();
    auto [r1, s1] = mgr.add_subscription(dapp_id, 1);
    auto [r2, s2] = mgr.add_subscription(dapp_id, 2);
    ASSERT_EQ(s1, 1u);
    ASSERT_EQ(s2, 2u);
    ASSERT_TRUE(mgr.remove_subscription(dapp_id, 1) == ErrorCode::SUCCESS);
    auto [r3, s3] = mgr.add_subscription(dapp_id, 3);
    ASSERT_EQ(s3, 1u);
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

// =========================================================================
// subscriptionTime
// =========================================================================

namespace {

// A clock the test moves by hand, so nothing here sleeps.
struct FakeClock {
    std::chrono::steady_clock::time_point t{std::chrono::steady_clock::time_point{} +
                                            std::chrono::hours(1)};
    SubscriptionClock fn() {
        return [this] { return t; };
    }
    void advance(std::chrono::milliseconds d) { t += d; }
};

using namespace std::chrono_literals;

}  // namespace

TEST(SubscriptionManager_time_zero_means_no_limit) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    mgr.add_subscription(dapp, 100, {1}, {}, 0, 0);

    const auto* details = mgr.get_subscription_details(dapp, 100);
    ASSERT_TRUE(details != nullptr);
    ASSERT_EQ(details->subscription_time_s, 0u);
    ASSERT_FALSE(details->expires_at.has_value());

    clock.advance(24h * 365);
    ASSERT_TRUE(mgr.expire_subscriptions().empty());
    ASSERT_TRUE(mgr.is_subscribed(dapp, 100));
}

TEST(SubscriptionManager_subscription_ends_when_its_time_is_up) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    auto [add, sub_id] = mgr.add_subscription(dapp, 100, {1}, {}, 0, 60);

    clock.advance(59999ms);
    ASSERT_TRUE(mgr.expire_subscriptions().empty());
    ASSERT_TRUE(mgr.is_subscribed(dapp, 100));

    clock.advance(1ms);
    auto expired = mgr.expire_subscriptions();
    ASSERT_EQ(expired.size(), 1u);
    ASSERT_EQ(expired[0].dapp_id, dapp);
    ASSERT_EQ(expired[0].ran_function_id, 100u);
    ASSERT_EQ(expired[0].subscription_id, sub_id);

    ASSERT_FALSE(mgr.is_subscribed(dapp, 100));
    ASSERT_FALSE(mgr.has_subscribers(100));
    ASSERT_EQ(mgr.subscription_count(), 0u);
    ASSERT_TRUE(mgr.get_subscription_details(dapp, 100) == nullptr);
    // Only the subscription goes; the dApp stays registered.
    ASSERT_TRUE(mgr.is_dapp_registered(dapp));
    ASSERT_TRUE(mgr.expire_subscriptions().empty());
}

TEST(SubscriptionManager_expired_subscription_gets_no_indications_before_the_sweep) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [r1, limited] = mgr.register_dapp();
    auto [r2, unlimited] = mgr.register_dapp();
    mgr.add_subscription(limited, 100, {1}, {}, 0, 10);
    mgr.add_subscription(unlimited, 100, {1}, {}, 0, 0);
    ASSERT_EQ(mgr.get_subscribed_dapps(100).size(), 2u);

    clock.advance(10s);
    // The sweep has not run, but the dispatch view already leaves the dApp out.
    auto dapps = mgr.get_subscribed_dapps(100);
    ASSERT_EQ(dapps.size(), 1u);
    ASSERT_EQ(dapps[0], unlimited);
}

TEST(SubscriptionManager_expiry_stops_the_sm_with_its_last_subscriber) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    int starts = 0, stops = 0;
    mgr.set_sm_lifecycle_callback([&](uint32_t, bool should_start) {
        should_start ? ++starts : ++stops;
    });
    auto [r1, short_lived] = mgr.register_dapp();
    auto [r2, long_lived] = mgr.register_dapp();
    mgr.add_subscription(short_lived, 100, {}, {}, 0, 5);
    mgr.add_subscription(long_lived, 100, {}, {}, 0, 50);
    ASSERT_EQ(starts, 1);

    clock.advance(5s);
    ASSERT_EQ(mgr.expire_subscriptions().size(), 1u);
    ASSERT_EQ(stops, 0);  // the other dApp still needs the SM

    clock.advance(45s);
    ASSERT_EQ(mgr.expire_subscriptions().size(), 1u);
    ASSERT_EQ(stops, 1);
    ASSERT_EQ(starts, 1);
}

TEST(SubscriptionManager_expiry_is_per_subscription) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    mgr.add_subscription(dapp, 100, {}, {}, 0, 5);
    mgr.add_subscription(dapp, 200, {}, {}, 0, 0);
    mgr.add_subscription(dapp, 300, {}, {}, 0, 50);

    clock.advance(5s);
    auto expired = mgr.expire_subscriptions();
    ASSERT_EQ(expired.size(), 1u);
    ASSERT_EQ(expired[0].ran_function_id, 100u);
    ASSERT_FALSE(mgr.is_subscribed(dapp, 100));
    ASSERT_TRUE(mgr.is_subscribed(dapp, 200));
    ASSERT_TRUE(mgr.is_subscribed(dapp, 300));
}

TEST(SubscriptionManager_expired_subscription_id_is_reusable) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    auto [r1, first] = mgr.add_subscription(dapp, 100, {}, {}, 0, 5);
    clock.advance(5s);
    mgr.expire_subscriptions();

    auto [r2, second] = mgr.add_subscription(dapp, 200);
    ASSERT_EQ(second, first);
}

TEST(SubscriptionManager_resubscribe_after_expiry_starts_a_new_subscription) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    int starts = 0, stops = 0;
    mgr.set_sm_lifecycle_callback([&](uint32_t, bool should_start) {
        should_start ? ++starts : ++stops;
    });
    auto [reg, dapp] = mgr.register_dapp();
    auto [r1, sub_id] = mgr.add_subscription(dapp, 100, {1}, {}, 0, 5);

    // Time is up but the sweep has not run yet. A second request must not be
    // answered with the dying subscription.
    clock.advance(5s);
    auto [r2, again] = mgr.add_subscription(dapp, 100, {2}, {}, 0, 30);
    ASSERT_TRUE(r2 == ErrorCode::SUCCESS);
    ASSERT_EQ(again, sub_id);

    ASSERT_TRUE(mgr.expire_subscriptions().empty());
    ASSERT_TRUE(mgr.is_subscribed(dapp, 100));
    const auto* details = mgr.get_subscription_details(dapp, 100);
    ASSERT_TRUE(details != nullptr);
    ASSERT_EQ(details->subscription_time_s, 30u);
    ASSERT_EQ(details->telemetry_ids.size(), 1u);
    ASSERT_EQ(details->telemetry_ids[0], 2u);
    // The SM was never without a subscriber.
    ASSERT_EQ(starts, 1);
    ASSERT_EQ(stops, 0);

    clock.advance(30s);
    ASSERT_EQ(mgr.expire_subscriptions().size(), 1u);
}

TEST(SubscriptionManager_resubscribe_before_expiry_keeps_the_original) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    auto [r1, sub_id] = mgr.add_subscription(dapp, 100, {}, {}, 0, 60);

    clock.advance(30s);
    auto [r2, again] = mgr.add_subscription(dapp, 100, {}, {}, 0, 600);
    ASSERT_TRUE(r2 == ErrorCode::SUBSCRIPTION_EXISTS);
    ASSERT_EQ(again, sub_id);
    ASSERT_EQ(mgr.get_subscription_details(dapp, 100)->subscription_time_s, 60u);

    clock.advance(30s);
    ASSERT_EQ(mgr.expire_subscriptions().size(), 1u);
}

TEST(SubscriptionManager_deleting_before_expiry_leaves_nothing_to_expire) {
    FakeClock clock;
    SubscriptionManager mgr(clock.fn());
    auto [reg, dapp] = mgr.register_dapp();
    mgr.add_subscription(dapp, 100, {}, {}, 0, 5);
    mgr.remove_subscription(dapp, 100);

    clock.advance(1h);
    ASSERT_TRUE(mgr.expire_subscriptions().empty());
}

TEST(SubscriptionManager_expiry_uses_steady_clock_by_default) {
    SubscriptionManager mgr;
    auto [reg, dapp] = mgr.register_dapp();
    mgr.add_subscription(dapp, 100, {}, {}, 0, 3600);
    ASSERT_TRUE(mgr.expire_subscriptions().empty());
    const auto* details = mgr.get_subscription_details(dapp, 100);
    ASSERT_TRUE(details->expires_at.has_value());
    ASSERT_TRUE(*details->expires_at > std::chrono::steady_clock::now());
}

int main() {
    return RUN_ALL_TESTS();
}
