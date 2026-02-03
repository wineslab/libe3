/**
 * @file subscription_manager.cpp
 * @brief E3 Subscription Manager implementation
 *
 * Ported from the original C implementation e3_subscription_manager.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/subscription_manager.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "SubMgr";
constexpr uint32_t MAX_DAPP_ID = 100;
}

SubscriptionManager::SubscriptionManager() {
    E3_LOG_DEBUG(LOG_TAG) << "Subscription manager created";
}

SubscriptionManager::~SubscriptionManager() {
    std::unique_lock lock(mutex_);
    registered_dapps_.clear();
    dapp_subscriptions_.clear();
    ran_function_subscribers_.clear();
    E3_LOG_DEBUG(LOG_TAG) << "Subscription manager destroyed";
}

void SubscriptionManager::set_sm_lifecycle_callback(SmLifecycleCallback callback) {
    std::unique_lock lock(mutex_);
    sm_lifecycle_callback_ = std::move(callback);
}

// =========================================================================
// dApp Registration Management
// =========================================================================

std::pair<ErrorCode, uint32_t> SubscriptionManager::register_dapp() {
    std::unique_lock lock(mutex_);

    // Find the next available dApp ID
    uint32_t assigned_id = next_dapp_id_;
    uint32_t attempts = 0;
    
    // Search for an available ID (handle wrap-around and gaps from unregistered dApps)
    while (registered_dapps_.count(assigned_id) > 0 && attempts <= MAX_DAPP_ID) {
        assigned_id = (assigned_id + 1) % (MAX_DAPP_ID + 1);
        attempts++;
    }
    
    if (attempts > MAX_DAPP_ID) {
        E3_LOG_ERROR(LOG_TAG) << "No available dApp IDs (max " << MAX_DAPP_ID + 1 << " dApps reached)";
        return {ErrorCode::INTERNAL_ERROR, 0};
    }

    DAppEntry entry;
    entry.dapp_identifier = assigned_id;
    entry.registered_time = std::chrono::steady_clock::now();
    registered_dapps_[assigned_id] = entry;

    // Initialize empty subscription set for this dApp
    dapp_subscriptions_[assigned_id] = {};
    
    // Update next_dapp_id_ for the next registration
    next_dapp_id_ = (assigned_id + 1) % (MAX_DAPP_ID + 1);

    E3_LOG_INFO(LOG_TAG) << "dApp registered successfully with ID " << assigned_id;
    return {ErrorCode::SUCCESS, assigned_id};
}

ErrorCode SubscriptionManager::unregister_dapp(uint32_t dapp_id) {
    std::unique_lock lock(mutex_);

    auto it = registered_dapps_.find(dapp_id);
    if (it == registered_dapps_.end()) {
        E3_LOG_WARN(LOG_TAG) << "dApp " << dapp_id << " not found for unregistration";
        return ErrorCode::DAPP_NOT_REGISTERED;
    }

    // Track affected RAN functions before removing subscriptions
    std::vector<std::pair<uint32_t, bool>> affected_ran_functions;
    
    auto sub_it = dapp_subscriptions_.find(dapp_id);
    if (sub_it != dapp_subscriptions_.end()) {
        for (uint32_t ran_func : sub_it->second) {
            // Check if it currently has subscribers (before we remove this dApp)
            auto& subscribers = ran_function_subscribers_[ran_func];
            bool had_subscribers = !subscribers.empty();
            
            // Remove this dApp from the RAN function's subscriber list
            subscribers.erase(dapp_id);
            
            // Track whether this RAN function still has subscribers
            bool still_has_subscribers = !subscribers.empty();
            if (had_subscribers && !still_has_subscribers) {
                affected_ran_functions.emplace_back(ran_func, false); // should_start = false
            }
        }
        dapp_subscriptions_.erase(sub_it);
    }

    // Remove the dApp registration
    registered_dapps_.erase(it);

    size_t subscriptions_removed = affected_ran_functions.size();
    E3_LOG_INFO(LOG_TAG) << "dApp " << dapp_id << " unregistered, " 
                         << subscriptions_removed << " subscriptions removed";

    // Notify about SM lifecycle changes (outside the lock to avoid deadlock)
    lock.unlock();
    for (const auto& [ran_func, should_start] : affected_ran_functions) {
        check_sm_lifecycle(ran_func, true); // had_subscribers = true
    }

    return ErrorCode::SUCCESS;
}

bool SubscriptionManager::is_dapp_registered(uint32_t dapp_id) const {
    std::shared_lock lock(mutex_);
    return registered_dapps_.count(dapp_id) > 0;
}

std::vector<uint32_t> SubscriptionManager::get_registered_dapps() const {
    std::shared_lock lock(mutex_);
    std::vector<uint32_t> result;
    result.reserve(registered_dapps_.size());
    for (const auto& [id, entry] : registered_dapps_) {
        result.push_back(id);
    }
    return result;
}

// =========================================================================
// Subscription Management
// =========================================================================

ErrorCode SubscriptionManager::add_subscription(uint32_t dapp_id, uint32_t ran_function_id) {
    std::unique_lock lock(mutex_);

    // Check if dApp is registered
    if (registered_dapps_.count(dapp_id) == 0) {
        E3_LOG_ERROR(LOG_TAG) << "dApp " << dapp_id << " not registered, cannot add subscription";
        return ErrorCode::DAPP_NOT_REGISTERED;
    }

    // Check if subscription already exists
    auto& dapp_subs = dapp_subscriptions_[dapp_id];
    if (dapp_subs.count(ran_function_id) > 0) {
        E3_LOG_WARN(LOG_TAG) << "dApp " << dapp_id << " already subscribed to RAN function " 
                             << ran_function_id;
        return ErrorCode::SUBSCRIPTION_EXISTS;
    }

    // Check if this is the first subscriber for this RAN function
    bool had_subscribers = !ran_function_subscribers_[ran_function_id].empty();

    // Add subscription
    dapp_subs.insert(ran_function_id);
    ran_function_subscribers_[ran_function_id].insert(dapp_id);

    E3_LOG_INFO(LOG_TAG) << "Subscription added: dApp " << dapp_id 
                         << " -> RAN function " << ran_function_id;

    // Notify about SM lifecycle change if this is the first subscriber
    lock.unlock();
    if (!had_subscribers) {
        check_sm_lifecycle(ran_function_id, false); // had_subscribers = false
    }

    return ErrorCode::SUCCESS;
}

ErrorCode SubscriptionManager::remove_subscription(uint32_t dapp_id, uint32_t ran_function_id) {
    std::unique_lock lock(mutex_);

    auto dapp_it = dapp_subscriptions_.find(dapp_id);
    if (dapp_it == dapp_subscriptions_.end()) {
        return ErrorCode::SUBSCRIPTION_NOT_FOUND;
    }

    auto& dapp_subs = dapp_it->second;
    if (dapp_subs.erase(ran_function_id) == 0) {
        E3_LOG_WARN(LOG_TAG) << "Subscription not found for dApp " << dapp_id 
                             << " -> RAN function " << ran_function_id;
        return ErrorCode::SUBSCRIPTION_NOT_FOUND;
    }

    // Remove from reverse index
    auto& subscribers = ran_function_subscribers_[ran_function_id];
    bool had_subscribers = !subscribers.empty();
    subscribers.erase(dapp_id);
    bool still_has_subscribers = !subscribers.empty();

    E3_LOG_INFO(LOG_TAG) << "Subscription removed: dApp " << dapp_id 
                         << " -> RAN function " << ran_function_id;

    // Notify about SM lifecycle change if this was the last subscriber
    lock.unlock();
    if (had_subscribers && !still_has_subscribers) {
        check_sm_lifecycle(ran_function_id, true); // had_subscribers = true
    }

    return ErrorCode::SUCCESS;
}

bool SubscriptionManager::is_subscribed(uint32_t dapp_id, uint32_t ran_function_id) const {
    std::shared_lock lock(mutex_);
    
    auto it = dapp_subscriptions_.find(dapp_id);
    if (it == dapp_subscriptions_.end()) {
        return false;
    }
    return it->second.count(ran_function_id) > 0;
}

std::vector<uint32_t> SubscriptionManager::get_dapp_subscriptions(uint32_t dapp_id) const {
    std::shared_lock lock(mutex_);
    
    auto it = dapp_subscriptions_.find(dapp_id);
    if (it == dapp_subscriptions_.end()) {
        return {};
    }
    return std::vector<uint32_t>(it->second.begin(), it->second.end());
}

std::vector<uint32_t> SubscriptionManager::get_subscribed_dapps(uint32_t ran_function_id) const {
    std::shared_lock lock(mutex_);
    
    auto it = ran_function_subscribers_.find(ran_function_id);
    if (it == ran_function_subscribers_.end()) {
        return {};
    }
    return std::vector<uint32_t>(it->second.begin(), it->second.end());
}

size_t SubscriptionManager::get_subscriber_count(uint32_t ran_function_id) const {
    std::shared_lock lock(mutex_);
    
    auto it = ran_function_subscribers_.find(ran_function_id);
    if (it == ran_function_subscribers_.end()) {
        return 0;
    }
    return it->second.size();
}

// =========================================================================
// RAN Function Management
// =========================================================================

std::vector<uint32_t> SubscriptionManager::get_active_ran_functions() const {
    std::shared_lock lock(mutex_);
    
    std::vector<uint32_t> result;
    for (const auto& [ran_func, subscribers] : ran_function_subscribers_) {
        if (!subscribers.empty()) {
            result.push_back(ran_func);
        }
    }
    return result;
}

bool SubscriptionManager::has_subscribers(uint32_t ran_function_id) const {
    std::shared_lock lock(mutex_);
    
    auto it = ran_function_subscribers_.find(ran_function_id);
    return it != ran_function_subscribers_.end() && !it->second.empty();
}

// =========================================================================
// Statistics
// =========================================================================

size_t SubscriptionManager::dapp_count() const {
    std::shared_lock lock(mutex_);
    return registered_dapps_.size();
}

size_t SubscriptionManager::subscription_count() const {
    std::shared_lock lock(mutex_);
    
    size_t count = 0;
    for (const auto& [dapp_id, subs] : dapp_subscriptions_) {
        count += subs.size();
    }
    return count;
}

void SubscriptionManager::clear() {
    std::unique_lock lock(mutex_);
    registered_dapps_.clear();
    dapp_subscriptions_.clear();
    ran_function_subscribers_.clear();
    E3_LOG_INFO(LOG_TAG) << "All registrations and subscriptions cleared";
}

// =========================================================================
// Private Methods
// =========================================================================

void SubscriptionManager::check_sm_lifecycle(uint32_t ran_function_id, bool had_subscribers) {
    if (!sm_lifecycle_callback_) {
        return;
    }

    // Determine if we need to start or stop
    bool has_subscribers_now = has_subscribers(ran_function_id);
    
    if (!had_subscribers && has_subscribers_now) {
        // First subscriber - start SM
        E3_LOG_INFO(LOG_TAG) << "Starting SM for RAN function " << ran_function_id 
                             << " (first subscriber)";
        sm_lifecycle_callback_(ran_function_id, true);
    } else if (had_subscribers && !has_subscribers_now) {
        // Last subscriber gone - stop SM
        E3_LOG_INFO(LOG_TAG) << "Stopping SM for RAN function " << ran_function_id 
                             << " (no remaining subscribers)";
        sm_lifecycle_callback_(ran_function_id, false);
    }
}

} // namespace libe3
