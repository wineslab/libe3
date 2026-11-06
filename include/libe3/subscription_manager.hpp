/**
 * @file subscription_manager.hpp
 * @brief E3 Subscription Manager for tracking dApp registrations
 *
 * This module manages the associations between dApps and RAN functions.
 * Ported from the original C implementation's e3_subscription_manager.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SUBSCRIPTION_MANAGER_HPP
#define LIBE3_SUBSCRIPTION_MANAGER_HPP

#include "types.hpp"
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace libe3 {

/**
 * @brief Per-subscription metadata stored by the SubscriptionManager.
 */
struct SubscriptionDetails {
    std::vector<uint32_t> telemetry_ids;  ///< Telemetry identifiers the dApp asked for
    std::vector<uint32_t> control_ids;    ///< Control identifiers the dApp asked for
    uint32_t periodicity_us{0};  ///< 0 = use SM default
    uint32_t subscription_time_s{0};  ///< How long the subscription lasts, 0 = no limit
    /// When the subscription ends. Unset when `subscription_time_s` is 0.
    std::optional<std::chrono::steady_clock::time_point> expires_at;
};

/**
 * @brief A subscription ended because its subscriptionTime ran out.
 */
struct ExpiredSubscription {
    uint32_t dapp_id{0};          ///< dApp whose subscription ended
    uint32_t ran_function_id{0};  ///< RAN function it was for
    uint32_t subscription_id{0};  ///< Id of the subscription that ended
};

/// Source of "now" for subscription expiry. Empty means std::chrono::steady_clock.
using SubscriptionClock = std::function<std::chrono::steady_clock::time_point()>;

/**
 * @brief Callback type for SM lifecycle events
 *
 * Called when a RAN function gains its first subscriber or loses
 * its last subscriber. This allows the E3Agent to start/stop
 * Service Models as needed.
 *
 * @param ran_function_id The RAN function ID
 * @param should_start True if SM should start, false if it should stop
 */
using SmLifecycleCallback = std::function<void(uint32_t ran_function_id, bool should_start)>;

/**
 * @brief E3 Subscription Manager
 *
 * Thread-safe manager for dApp registrations and RAN function subscriptions.
 * Handles:
 * - dApp registration from E3 Setup requests
 * - Subscription management from E3 Subscription requests
 * - RAN function lifecycle coordination
 *
 * This class is ported from the original C e3_subscription_manager
 * with modern C++ idioms for thread safety and memory management.
 */
class SubscriptionManager {
public:
    /**
     * @brief Construct a new Subscription Manager
     *
     * @param clock Source of "now" for subscription expiry, so tests can move
     *        time without sleeping. Defaults to std::chrono::steady_clock.
     */
    explicit SubscriptionManager(SubscriptionClock clock = {});

    /**
     * @brief Destructor
     */
    ~SubscriptionManager();

    // Non-copyable, non-movable (contains mutex)
    SubscriptionManager(const SubscriptionManager&) = delete;
    SubscriptionManager& operator=(const SubscriptionManager&) = delete;
    SubscriptionManager(SubscriptionManager&&) = delete;
    SubscriptionManager& operator=(SubscriptionManager&&) = delete;

    /**
     * @brief Set callback for SM lifecycle events
     * @param callback Called when a RAN function gains its first or loses its last subscriber
     */
    void set_sm_lifecycle_callback(SmLifecycleCallback callback);

    // =========================================================================
    // dApp Registration Management (E3 Setup flow)
    // =========================================================================

    /**
     * @brief Register a dApp from E3 Setup Request
     *
     * The SubscriptionManager assigns a unique dApp ID automatically.
     *
     * @return std::pair containing:
     *         - ErrorCode::SUCCESS on success, or error code on failure
     *         - The assigned dApp ID (valid only if ErrorCode::SUCCESS)
     * @return ErrorCode::INTERNAL_ERROR if no IDs available (max 101 dApps)
     */
    std::pair<ErrorCode, uint32_t> register_dapp();

    /**
     * @brief Unregister a dApp and clean up all its subscriptions
     *
     * @param dapp_id dApp identifier
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::DAPP_NOT_REGISTERED if dApp not found
     */
    ErrorCode unregister_dapp(uint32_t dapp_id);

    /**
     * @brief Check if a dApp is registered
     *
     * @param dapp_id dApp identifier
     * @return true if registered
     */
    bool is_dapp_registered(uint32_t dapp_id) const;

    /**
     * @brief Get list of registered dApp IDs
     * @return The dApp IDs
     */
    std::vector<uint32_t> get_registered_dapps() const;

    // =========================================================================
    // Subscription Management (E3 Subscription flow)
    // =========================================================================

    /**
     * @brief Add a subscription between dApp and RAN function
     *
     * @param dapp_id dApp identifier
     * @param ran_function_id RAN function identifier (0-255)
     * @param telemetry_ids Telemetry IDs the dApp wants
     * @param control_ids Control IDs the dApp wants
     * @param periodicity_us Reporting periodicity in microseconds (0 = SM default)
     * @param subscription_time_s Seconds the subscription lasts (0 = no limit).
     *        When it runs out the subscription is removed as if the dApp had
     *        deleted it, see expire_subscriptions().
     * @return std::pair containing:
     *         - ErrorCode::SUCCESS on success, or error code on failure
     *         - The assigned subscription ID (valid only if ErrorCode::SUCCESS)
     * @return ErrorCode::DAPP_NOT_REGISTERED if dApp not registered
     * @return ErrorCode::SUBSCRIPTION_EXISTS if already subscribed. A subscription
     *         that has run out but not yet been removed does not count: the
     *         request starts a new one under the same subscription ID.
     */
    std::pair<ErrorCode, uint32_t> add_subscription(
        uint32_t dapp_id,
        uint32_t ran_function_id,
        const std::vector<uint32_t>& telemetry_ids = {},
        const std::vector<uint32_t>& control_ids = {},
        uint32_t periodicity_us = 0,
        uint32_t subscription_time_s = 0);

    /**
     * @brief Remove every subscription whose time has run out
     *
     * Removal is silent towards the dApp: E3AP has no "subscription expired"
     * PDU. The SM lifecycle callback fires for a RAN function that loses its
     * last subscriber. Call this periodically; get_subscribed_dapps() already
     * leaves out an expired subscription in between.
     *
     * @return the subscriptions that were removed
     */
    std::vector<ExpiredSubscription> expire_subscriptions();

    /**
     * @brief Remove a subscription between dApp and RAN function
     *
     * @param dapp_id dApp identifier
     * @param ran_function_id RAN function identifier
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SUBSCRIPTION_NOT_FOUND if subscription doesn't exist
     */
    ErrorCode remove_subscription(uint32_t dapp_id, uint32_t ran_function_id);

    /**
     * @brief Remove a subscription by its ID
     *
     * @param dapp_id dApp identifier
     * @param subscription_id Subscription ID
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SUBSCRIPTION_NOT_FOUND if subscription doesn't exist
     */
    ErrorCode remove_subscription_by_id(uint32_t dapp_id, uint32_t subscription_id);

    /**
     * @brief Check if dApp is subscribed to a RAN function
     * @param dapp_id The dApp
     * @param ran_function_id The RAN function
     * @return true if the dApp is subscribed to the RAN function
     */
    bool is_subscribed(uint32_t dapp_id, uint32_t ran_function_id) const;

    /**
     * @brief Get all RAN functions a dApp is subscribed to
     * @param dapp_id The dApp
     * @return The RAN function IDs
     */
    std::vector<uint32_t> get_dapp_subscriptions(uint32_t dapp_id) const;

    /**
     * @brief Record that a message from a dApp has just arrived
     *
     * Only messages the dApp sends count. A message to the dApp proves nothing:
     * the RAN broadcasts, so it is sent whether or not the dApp is still there.
     *
     * @param dapp_id dApp identifier; an id that is not registered is ignored
     */
    void touch_dapp(uint32_t dapp_id);

    /**
     * @brief Which registration of a dApp id is current
     *
     * An id is handed out again after its dApp is removed, so a record that
     * names a dApp by id alone can end up naming a later one. Remember the
     * generation next to the id and compare before acting on it.
     *
     * @return The registration's generation, or nothing if the id is not registered
     * @param dapp_id The dApp
     */
    std::optional<uint64_t> dapp_generation(uint32_t dapp_id) const;

    /**
     * @brief Registered dApps that have not sent a message for at least @p idle_for
     *
     * A dApp that registered and never spoke counts from its registration.
     * Nothing is removed; the caller decides, then calls unregister_dapp().
     *
     * @param idle_for Time without a message from the dApp
     * @return Identifiers of the idle dApps, lowest first
     */
    std::vector<uint32_t> idle_dapps(std::chrono::seconds idle_for) const;

    /**
     * @brief Get all dApps subscribed to a RAN function
     *
     * Leaves out a dApp whose subscription time has run out, even before
     * expire_subscriptions() removes it, so no indication goes past the deadline.
     *
     * @param ran_function_id RAN function to look up
     * @return ids of the dApps with a live subscription to it
     */
    std::vector<uint32_t> get_subscribed_dapps(uint32_t ran_function_id) const;

    /**
     * @brief Get subscription details for a specific dApp + RAN function pair
     *
     * @param dapp_id dApp that holds the subscription
     * @param ran_function_id RAN function it is for
     * @return A copy taken under the lock, or nullopt if no such subscription
     *         exists. A copy, not a pointer: the subscription can be removed or
     *         renewed by another thread (including the expiry sweep) at any time.
     */
    std::optional<SubscriptionDetails> get_subscription_details(uint32_t dapp_id, uint32_t ran_function_id) const;

    /**
     * @brief Get count of subscribers for a RAN function
     * @param ran_function_id The RAN function
     * @return The number of dApps subscribed to it
     */
    size_t get_subscriber_count(uint32_t ran_function_id) const;

    // =========================================================================
    // RAN Function Management
    // =========================================================================

    /**
     * @brief Get all RAN functions that have at least one subscriber
     * @return The RAN function IDs with a subscriber
     */
    std::vector<uint32_t> get_active_ran_functions() const;

    /**
     * @brief Check if any dApp is subscribed to a RAN function
     * @param ran_function_id The RAN function
     * @return true if a dApp is subscribed to it
     */
    bool has_subscribers(uint32_t ran_function_id) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of registered dApps
     * @return The number of registered dApps
     */
    size_t dapp_count() const;

    /**
     * @brief Get total number of active subscriptions
     * @return The number of subscriptions
     */
    size_t subscription_count() const;

    /**
     * @brief Clear all registrations and subscriptions
     */
    void clear();

private:
    // Using shared_mutex for read-heavy workloads
    mutable std::shared_mutex mutex_;
    
    // dApp ID -> registration entry
    std::unordered_map<uint32_t, DAppEntry> registered_dapps_;
    uint64_t last_generation_{0};  ///< Generation of the latest registration, under mutex_
    
    // dApp ID -> set of subscribed RAN function IDs
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> dapp_subscriptions_;
    
    // RAN function ID -> set of subscribed dApp IDs (reverse index for fast lookup)
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> ran_function_subscribers_;
    
    // Subscription ID -> (dApp ID, RAN function ID) mapping
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> subscription_ids_;
    
    // (dApp ID, RAN function ID) -> Subscription ID reverse mapping
    std::unordered_map<uint64_t, uint32_t> subscription_id_reverse_;

    // (dApp ID, RAN function ID) -> per-subscription metadata
    std::unordered_map<uint64_t, SubscriptionDetails> subscription_details_;
    
    // Callback for SM lifecycle events
    SmLifecycleCallback sm_lifecycle_callback_;

    SubscriptionClock clock_;

    std::chrono::steady_clock::time_point now() const {
        return clock_ ? clock_() : std::chrono::steady_clock::now();
    }

    static bool is_expired(const SubscriptionDetails& details,
                           std::chrono::steady_clock::time_point at) {
        return details.expires_at && at >= *details.expires_at;
    }
    
    // Helper to create composite key for subscription
    static uint64_t make_sub_key(uint32_t dapp_id, uint32_t ran_func_id) {
        return (static_cast<uint64_t>(dapp_id) << 32) | ran_func_id;
    }

    /**
     * @brief Check if SM should be started/stopped and invoke callback
     *
     * Called after subscription changes to notify the E3Agent about
     * RAN functions that need their SMs started or stopped.
     *
     * @param ran_function_id RAN function that changed
     * @param had_subscribers Whether it had subscribers before the change
     */
    void check_sm_lifecycle(uint32_t ran_function_id, bool had_subscribers);
};

} // namespace libe3

#endif // LIBE3_SUBSCRIPTION_MANAGER_HPP
