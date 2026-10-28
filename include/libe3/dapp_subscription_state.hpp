/**
 * @file dapp_subscription_state.hpp
 * @brief Per-instance state for the dApp role.
 *
 * Holds the dApp-assigned identifier (received from the RAN in the
 * SetupResponse), the list of RAN functions the RAN advertised, and the
 * subscriptions this dApp has actively negotiated.
 *
 * A dApp may hold MULTIPLE concurrent subscriptions — one per distinct
 * RAN function. At most one subscription per (dapp, ran_function) pair,
 * mirroring the RAN-side uniqueness in SubscriptionManager.
 *
 * This struct is held by E3Interface when config.role == E3Role::DAPP.
 * It is the dApp-role counterpart of SubscriptionManager, which stays
 * strictly RAN-side. Exactly one of `subscription_manager_` / `dapp_state_`
 * is non-null on a given E3Interface instance.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_DAPP_SUBSCRIPTION_STATE_HPP
#define LIBE3_DAPP_SUBSCRIPTION_STATE_HPP

#include "types.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace libe3 {

class DAppSubscriptionState {
public:
    DAppSubscriptionState() = default;

    DAppSubscriptionState(const DAppSubscriptionState&) = delete;
    DAppSubscriptionState& operator=(const DAppSubscriptionState&) = delete;

    /**
     * @brief Store the assignment from a SetupResponse.
     * Called once after a positive SetupResponse arrives.
     */
    void record_setup_response(uint32_t dapp_id,
                               std::vector<RanFunctionDef> functions) {
        std::lock_guard<std::mutex> lock(mu_);
        assigned_dapp_id = dapp_id;
        remote_ran_functions = std::move(functions);
    }

    /**
     * @brief Forget the RAN's assignment and every subscription (session ended).
     */
    void clear_session() {
        std::lock_guard<std::mutex> lock(mu_);
        assigned_dapp_id.reset();
        remote_ran_functions.clear();
        ran_function_to_subscription_id.clear();
        subscription_id_to_ran_function.clear();
        ran_function_expiry.clear();
    }

    /**
     * @brief Record a successful SubscriptionResponse.
     * Idempotent: re-recording the same (rf_id, sub_id) is fine.
     *
     * @param expires_at When the subscriptionTime the dApp asked for runs out,
     *        unset for no limit. The RAN ends the subscription without a
     *        message, so the dApp forgets it too, see expire_subscriptions().
     *        Counted from the response, so it is never earlier than the RAN's.
     */
    void record_subscription(
            uint32_t ran_function_id, uint32_t subscription_id,
            std::optional<std::chrono::steady_clock::time_point> expires_at = std::nullopt) {
        std::lock_guard<std::mutex> lock(mu_);
        ran_function_to_subscription_id[ran_function_id] = subscription_id;
        subscription_id_to_ran_function[subscription_id] = ran_function_id;
        if (expires_at) {
            ran_function_expiry[ran_function_id] = *expires_at;
        } else {
            ran_function_expiry.erase(ran_function_id);
        }
    }

    /**
     * @brief Forget every subscription whose time is up at `now`.
     * @return the RAN functions whose subscription was forgotten
     */
    std::vector<uint32_t> expire_subscriptions(std::chrono::steady_clock::time_point now) {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<uint32_t> expired;
        for (auto it = ran_function_expiry.begin(); it != ran_function_expiry.end();) {
            if (now < it->second) {
                ++it;
                continue;
            }
            const uint32_t rf_id = it->first;
            auto sub_it = ran_function_to_subscription_id.find(rf_id);
            if (sub_it != ran_function_to_subscription_id.end()) {
                subscription_id_to_ran_function.erase(sub_it->second);
                ran_function_to_subscription_id.erase(sub_it);
            }
            expired.push_back(rf_id);
            it = ran_function_expiry.erase(it);
        }
        return expired;
    }

    /**
     * @brief Remove the subscription for a given RAN function.
     * @return true if a subscription was removed; false if none existed.
     */
    bool remove_subscription_by_rf(uint32_t ran_function_id) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = ran_function_to_subscription_id.find(ran_function_id);
        if (it == ran_function_to_subscription_id.end()) {
            return false;
        }
        const uint32_t sub_id = it->second;
        ran_function_to_subscription_id.erase(it);
        subscription_id_to_ran_function.erase(sub_id);
        ran_function_expiry.erase(ran_function_id);
        return true;
    }

    /**
     * @brief Remove the subscription with a given subscription ID.
     * @return true if a subscription was removed; false if none existed.
     */
    bool remove_subscription_by_id(uint32_t subscription_id) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = subscription_id_to_ran_function.find(subscription_id);
        if (it == subscription_id_to_ran_function.end()) {
            return false;
        }
        const uint32_t rf_id = it->second;
        subscription_id_to_ran_function.erase(it);
        ran_function_to_subscription_id.erase(rf_id);
        ran_function_expiry.erase(rf_id);
        return true;
    }

    /**
     * @brief Snapshot of active subscription IDs.
     */
    std::vector<uint32_t> active_subscriptions() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<uint32_t> out;
        out.reserve(subscription_id_to_ran_function.size());
        for (const auto& [sub_id, rf_id] : subscription_id_to_ran_function) {
            (void)rf_id;
            out.push_back(sub_id);
        }
        return out;
    }

    /**
     * @brief Snapshot of RAN function IDs this dApp is currently subscribed to.
     */
    std::vector<uint32_t> subscribed_ran_functions() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<uint32_t> out;
        out.reserve(ran_function_to_subscription_id.size());
        for (const auto& [rf_id, sub_id] : ran_function_to_subscription_id) {
            (void)sub_id;
            out.push_back(rf_id);
        }
        return out;
    }

    /**
     * @brief Look up subscription ID for a RAN function.
     */
    std::optional<uint32_t> subscription_id_for(uint32_t ran_function_id) const {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = ran_function_to_subscription_id.find(ran_function_id);
        if (it == ran_function_to_subscription_id.end()) return std::nullopt;
        return it->second;
    }

    // -----------------------------------------------------------------------
    // Public state members (kept public for direct inspection in tests and
    // for handlers that need to update multiple fields atomically inside
    // E3Interface). Callers MUST hold `mu` when reading or writing these
    // directly; the helpers above already take the lock.
    // -----------------------------------------------------------------------

    mutable std::mutex mu;

    /// Assigned by the RAN in SetupResponse. nullopt until setup completes.
    std::optional<uint32_t> assigned_dapp_id;

    /// RAN functions advertised by the RAN in SetupResponse.
    std::vector<RanFunctionDef> remote_ran_functions;

    /// rf_id -> sub_id
    std::unordered_map<uint32_t, uint32_t> ran_function_to_subscription_id;
    /// sub_id -> rf_id (reverse, used for SubscriptionDelete)
    std::unordered_map<uint32_t, uint32_t> subscription_id_to_ran_function;
    /// rf_id -> when its subscriptionTime runs out; absent for no limit
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> ran_function_expiry;

private:
    // Single mutex; the public-facing `mu` (above) is an alias for
    // documentation and direct lock_guard use from E3Interface. We keep
    // a single instance so this->mu_ and the public this->mu refer to the
    // same lock. Internal helpers use mu_.
    std::mutex& mu_ = mu;
};

} // namespace libe3

#endif // LIBE3_DAPP_SUBSCRIPTION_STATE_HPP
