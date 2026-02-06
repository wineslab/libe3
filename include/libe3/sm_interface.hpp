/**
 * @file sm_interface.hpp
 * @brief Service Model Interface - Extension point for RAN vendors
 *
 * Defines the interface that Service Models must implement to integrate
 * with the E3 agent. E3SM logic is treated as opaque to E3AP - this
 * interface provides extension points without embedding SM-specific logic.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SM_INTERFACE_HPP
#define LIBE3_SM_INTERFACE_HPP

#include "types.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace libe3 {

// Forward declarations
class ServiceModel;

/**
 * @brief Callback type for handling a specific control action within an SM
 *
 * @param action_data The control action data (E3SM-encoded)
 * @return ErrorCode::SUCCESS on success, error code on failure
 */
using ControlActionCallback = std::function<ErrorCode(
    const std::vector<uint8_t>& action_data
)>;

/**
 * @brief Abstract Service Model interface
 *
 * This is the extension point for RAN vendors to implement their
 * Service Models. The E3 agent treats E3SM as opaque - all SM-specific
 * encoding/decoding happens in the implementations of this interface.
 *
 * RAN vendors should:
 * 1. Derive from this class for each supported SM
 * 2. Implement the required virtual methods
 * 3. Register their SMs with the E3Agent
 */
class ServiceModel {
public:
    virtual ~ServiceModel() = default;

    /**
     * @brief Get SM name
     */
    virtual std::string name() const = 0;

    /**
     * @brief Get SM version
     */
    virtual uint32_t version() const = 0;

    /**
     * @brief Get RAN function ID for this SM
     */
    virtual uint32_t ran_function_id() const = 0;

    /**
     * @brief Get telemetry IDs supported by this SM
     */
    virtual std::vector<uint32_t> telemetry_ids() const = 0;

    /**
     * @brief Get control IDs supported by this SM
     */
    virtual std::vector<uint32_t> control_ids() const = 0;

    /**
     * @brief Initialize the SM
     *
     * Called when the SM is first registered.
     * @return ErrorCode::SUCCESS on success
     */
    virtual ErrorCode init() = 0;

    /**
     * @brief Destroy the SM and release resources
     *
     * Called when the SM is being unregistered.
     */
    virtual void destroy() = 0;

    /**
     * @brief Start the SM processing
     *
     * Called when the first dApp subscribes to this SM's RAN function.
     * @return ErrorCode::SUCCESS on success
     */
    virtual ErrorCode start() = 0;

    /**
     * @brief Stop the SM processing
     *
     * Called when the last dApp unsubscribes from this SM's RAN function.
     */
    virtual void stop() = 0;

    /**
     * @brief Check if SM is currently running
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Process a control action from a dApp
     *
     * Dispatches to the registered control callback for the given control ID.
     *
     * @param control_id The control action ID
     * @param action_data E3SM-encoded control action data
     * @return ErrorCode::SUCCESS on success, ErrorCode::NOT_FOUND if no callback registered
     */
    ErrorCode process_control_action(
        uint32_t control_id,
        const std::vector<uint8_t>& action_data
    ) {
        auto it = control_callbacks_.find(control_id);
        if (it != control_callbacks_.end()) {
            return it->second(action_data);
        }
        return ErrorCode::NOT_FOUND;
    }

protected:
    ServiceModel() = default;

    /**
     * @brief Register a control action callback for a specific control ID
     *
     * @param control_id The control ID to register the callback for
     * @param callback The callback function to handle this control action
     */
    void register_control_callback(uint32_t control_id, ControlActionCallback callback) {
        control_callbacks_[control_id] = std::move(callback);
    }

    /**
     * @brief Unregister a control action callback
     *
     * @param control_id The control ID to unregister
     */
    void unregister_control_callback(uint32_t control_id) {
        control_callbacks_.erase(control_id);
    }

private:
    std::unordered_map<uint32_t, ControlActionCallback> control_callbacks_;
};

/**
 * @brief Factory function type for creating SM instances
 */
using SmFactory = std::function<std::unique_ptr<ServiceModel>()>;

/**
 * @brief SM Registry for managing registered Service Models
 *
 * This class provides a central registry for Service Models.
 * It's used by the E3Agent to find and manage SMs.
 */
class SmRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static SmRegistry& instance();

    /**
     * @brief Register a Service Model
     *
     * @param sm Service Model to register
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SM_ALREADY_REGISTERED if SM for this RAN function exists
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Register a Service Model factory
     *
     * Use this to defer SM creation until needed.
     */
    ErrorCode register_sm_factory(uint32_t ran_function_id, SmFactory factory);

    /**
     * @brief Unregister a Service Model by RAN function ID
     */
    ErrorCode unregister_sm(uint32_t ran_function_id);

    /**
     * @brief Get SM by RAN function ID
     *
     * @param ran_function_id RAN function ID to look up
     * @return Pointer to SM, nullptr if not found
     */
    ServiceModel* get_by_ran_function(uint32_t ran_function_id);

    /**
     * @brief Get all available RAN function IDs
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Start SM for a RAN function
     */
    ErrorCode start_sm(uint32_t ran_function_id);

    /**
     * @brief Stop SM for a RAN function
     */
    ErrorCode stop_sm(uint32_t ran_function_id);

    /**
     * @brief Check if SM is running
     */
    bool is_sm_running(uint32_t ran_function_id) const;

    /**
     * @brief Clear all registered SMs
     */
    void clear();

private:
    SmRegistry() = default;
    ~SmRegistry() = default;
    SmRegistry(const SmRegistry&) = delete;
    SmRegistry& operator=(const SmRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::unique_ptr<ServiceModel>> sms_;
    std::unordered_map<uint32_t, SmFactory> factories_;
};

} // namespace libe3

#endif // LIBE3_SM_INTERFACE_HPP
