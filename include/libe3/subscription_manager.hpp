/**
 * @file subscription_manager.hpp
 * @brief E3 Subscription Manager for tracking dApp registrations
 *
 * This module manages the associations between dApps and RAN functions.
 * Ported from the original C implementation's e3_subscription_manager.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SUBSCRIPTION_MANAGER_HPP
#define LIBE3_SUBSCRIPTION_MANAGER_HPP

#include "types.hpp"
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace libe3 {

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
     */
    SubscriptionManager();

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
     * @return std::pair containing:
     *         - ErrorCode::SUCCESS on success, or error code on failure
     *         - The assigned subscription ID (valid only if ErrorCode::SUCCESS)
     * @return ErrorCode::DAPP_NOT_REGISTERED if dApp not registered
     * @return ErrorCode::SUBSCRIPTION_EXISTS if already subscribed
     */
    std::pair<ErrorCode, uint32_t> add_subscription(uint32_t dapp_id, uint32_t ran_function_id);

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
     */
    bool is_subscribed(uint32_t dapp_id, uint32_t ran_function_id) const;

    /**
     * @brief Get all RAN functions a dApp is subscribed to
     */
    std::vector<uint32_t> get_dapp_subscriptions(uint32_t dapp_id) const;

    /**
     * @brief Get all dApps subscribed to a RAN function
     */
    std::vector<uint32_t> get_subscribed_dapps(uint32_t ran_function_id) const;

    /**
     * @brief Get count of subscribers for a RAN function
     */
    size_t get_subscriber_count(uint32_t ran_function_id) const;

    // =========================================================================
    // RAN Function Management
    // =========================================================================

    /**
     * @brief Get all RAN functions that have at least one subscriber
     */
    std::vector<uint32_t> get_active_ran_functions() const;

    /**
     * @brief Check if any dApp is subscribed to a RAN function
     */
    bool has_subscribers(uint32_t ran_function_id) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of registered dApps
     */
    size_t dapp_count() const;

    /**
     * @brief Get total number of active subscriptions
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
    
    // dApp ID -> set of subscribed RAN function IDs
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> dapp_subscriptions_;
    
    // RAN function ID -> set of subscribed dApp IDs (reverse index for fast lookup)
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> ran_function_subscribers_;
    
    // Subscription ID -> (dApp ID, RAN function ID) mapping
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> subscription_ids_;
    
    // (dApp ID, RAN function ID) -> Subscription ID reverse mapping
    std::unordered_map<uint64_t, uint32_t> subscription_id_reverse_;
    
    // Callback for SM lifecycle events
    SmLifecycleCallback sm_lifecycle_callback_;
    
    // Next dApp ID to assign (1-100 per spec)
    uint32_t next_dapp_id_{1};
    
    // Next subscription ID to assign
    uint32_t next_subscription_id_{1};
    
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
