/**
 * @file test_dapp_subscription_state.cpp
 * @brief Unit tests for the per-dApp-instance subscription state.
 *
 * Part of issue #15: extend libe3 to support both RAN and dApp roles.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/dapp_subscription_state.hpp"

#include <atomic>
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

int main() {
    return RUN_ALL_TESTS();
}
