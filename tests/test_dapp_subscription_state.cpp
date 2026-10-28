/**
 * @file test_dapp_subscription_state.cpp
 * @brief Unit tests for the per-dApp-instance subscription state.
 *
 * Part of issue #15: extend libe3 to support both RAN and dApp roles.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/dapp_subscription_state.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace libe3;

TEST(DAppSubscriptionState_initially_empty) {
    DAppSubscriptionState s;
    ASSERT_FALSE(s.assigned_dapp_id.has_value());
    ASSERT_TRUE(s.active_subscriptions().empty());
    ASSERT_TRUE(s.remote_ran_functions.empty());
}

TEST(DAppSubscriptionState_record_setup_response_sets_id_and_functions) {
    DAppSubscriptionState s;

    std::vector<RanFunctionDef> fns;
    RanFunctionDef rf;
    rf.ran_function_identifier = 1;
    rf.telemetry_identifier_list = {1};
    rf.control_identifier_list = {1};
    fns.push_back(rf);

    s.record_setup_response(42, fns);

    ASSERT_TRUE(s.assigned_dapp_id.has_value());
    ASSERT_EQ(*s.assigned_dapp_id, 42u);
    ASSERT_EQ(s.remote_ran_functions.size(), size_t{1});
    ASSERT_EQ(s.remote_ran_functions[0].ran_function_identifier, 1u);
}

TEST(DAppSubscriptionState_record_subscription_indexes_both_ways) {
    DAppSubscriptionState s;

    s.record_subscription(/*rf_id=*/10, /*sub_id=*/5);

    ASSERT_EQ(s.ran_function_to_subscription_id.at(10), 5u);
    ASSERT_EQ(s.subscription_id_to_ran_function.at(5), 10u);
}

TEST(DAppSubscriptionState_supports_multiple_concurrent_subscriptions) {
    // A dApp can hold N subscriptions, one per distinct RAN function.
    DAppSubscriptionState s;

    s.record_subscription(10, 1);
    s.record_subscription(20, 2);
    s.record_subscription(30, 3);

    auto subs = s.active_subscriptions();
    ASSERT_EQ(subs.size(), size_t{3});

    // Reverse-lookups all resolve
    ASSERT_EQ(s.subscription_id_to_ran_function.at(1), 10u);
    ASSERT_EQ(s.subscription_id_to_ran_function.at(2), 20u);
    ASSERT_EQ(s.subscription_id_to_ran_function.at(3), 30u);
}

TEST(DAppSubscriptionState_remove_by_rf_clears_both_maps) {
    DAppSubscriptionState s;
    s.record_subscription(10, 5);
    s.record_subscription(20, 6);

    ASSERT_TRUE(s.remove_subscription_by_rf(10));

    ASSERT_EQ(s.ran_function_to_subscription_id.count(10), size_t{0});
    ASSERT_EQ(s.subscription_id_to_ran_function.count(5), size_t{0});
    // The unrelated subscription survives
    ASSERT_EQ(s.ran_function_to_subscription_id.at(20), 6u);
    ASSERT_EQ(s.subscription_id_to_ran_function.at(6), 20u);

    // Removing a non-existent rf returns false
    ASSERT_FALSE(s.remove_subscription_by_rf(99));
}

TEST(DAppSubscriptionState_remove_by_id_clears_both_maps) {
    DAppSubscriptionState s;
    s.record_subscription(10, 5);
    s.record_subscription(20, 6);

    ASSERT_TRUE(s.remove_subscription_by_id(5));

    ASSERT_EQ(s.ran_function_to_subscription_id.count(10), size_t{0});
    ASSERT_EQ(s.subscription_id_to_ran_function.count(5), size_t{0});

    ASSERT_FALSE(s.remove_subscription_by_id(99));
}

TEST(DAppSubscriptionState_concurrent_record_subscription_is_thread_safe) {
    // Smoke test: many threads recording disjoint subscriptions in parallel
    // must converge to the right size with no data races / crashes.
    DAppSubscriptionState s;
    constexpr int N = 200;
    std::vector<std::thread> threads;
    threads.reserve(N);
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&s, i]() {
            s.record_subscription(static_cast<uint32_t>(i + 1),
                                  static_cast<uint32_t>(i + 1));
        });
    }
    for (auto& t : threads) t.join();

    auto subs = s.active_subscriptions();
    ASSERT_EQ(subs.size(), size_t{N});
}

TEST(DAppSubscriptionState_forgets_a_subscription_when_its_time_is_up) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::now();
    DAppSubscriptionState s;
    s.record_subscription(10, 5, start + 60s);
    s.record_subscription(20, 6, start + 120s);
    s.record_subscription(30, 7);  // no limit

    ASSERT_TRUE(s.expire_subscriptions(start + 59s).empty());
    ASSERT_EQ(s.active_subscriptions().size(), size_t{3});

    auto expired = s.expire_subscriptions(start + 60s);
    ASSERT_EQ(expired.size(), size_t{1});
    ASSERT_EQ(expired[0], 10u);
    ASSERT_EQ(s.ran_function_to_subscription_id.count(10), size_t{0});
    ASSERT_EQ(s.subscription_id_to_ran_function.count(5), size_t{0});
    ASSERT_FALSE(s.subscription_id_for(10).has_value());
    ASSERT_EQ(s.ran_function_expiry.count(10), size_t{0});

    expired = s.expire_subscriptions(start + 24h);
    ASSERT_EQ(expired.size(), size_t{1});
    ASSERT_EQ(expired[0], 20u);
    // The one with no limit is never forgotten.
    ASSERT_EQ(s.subscription_id_for(30).value_or(0), 7u);
}

TEST(DAppSubscriptionState_removing_a_subscription_drops_its_deadline) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::now();
    DAppSubscriptionState s;
    s.record_subscription(10, 5, start + 60s);
    s.record_subscription(20, 6, start + 60s);
    s.remove_subscription_by_rf(10);
    s.remove_subscription_by_id(6);
    ASSERT_TRUE(s.ran_function_expiry.empty());

    // A new subscription to the same RAN function is not cut short by the old deadline.
    s.record_subscription(10, 8);
    ASSERT_TRUE(s.expire_subscriptions(start + 24h).empty());
    ASSERT_EQ(s.subscription_id_for(10).value_or(0), 8u);

    s.record_subscription(10, 8, start + 60s);
    s.clear_session();
    ASSERT_TRUE(s.ran_function_expiry.empty());
}

TEST(DAppSubscriptionState_resubscribing_without_a_time_clears_the_old_deadline) {
    using namespace std::chrono_literals;
    const auto start = std::chrono::steady_clock::now();
    DAppSubscriptionState s;
    s.record_subscription(10, 5, start + 60s);
    s.record_subscription(10, 5);
    ASSERT_TRUE(s.expire_subscriptions(start + 24h).empty());
    ASSERT_EQ(s.subscription_id_for(10).value_or(0), 5u);
}

int main() {
    return RUN_ALL_TESTS();
}
