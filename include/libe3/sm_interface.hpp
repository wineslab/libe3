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

namespace libe3 {

// Forward declarations
class ServiceModel;

/**
 * @brief SM indication data for passing data from SM to publisher
 */
struct SmIndicationData {
    uint32_t ran_function_id{0};
    std::vector<uint8_t> encoded_data;
    uint64_t timestamp{0};
    bool ready{false};
};

/**
 * @brief Callback type for processing control actions from dApps
 *
 * @param ran_function_id The RAN function this action is for
 * @param action_data The control action data (E3SM-encoded)
 * @return ErrorCode::SUCCESS on success, error code on failure
 */
using ControlActionHandler = std::function<ErrorCode(
    uint32_t ran_function_id,
    const std::vector<uint8_t>& action_data
)>;

/**
 * @brief Callback type for receiving indication data from SM
 *
 * @param ran_function_id The RAN function providing data
 * @param encoded_data E3SM-encoded indication data
 * @param timestamp Timestamp of the data
 */
using IndicationDataCallback = std::function<void(
    uint32_t ran_function_id,
    std::vector<uint8_t> encoded_data,
    uint64_t timestamp
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
    [[nodiscard]] virtual std::string name() const = 0;

    /**
     * @brief Get SM version
     */
    [[nodiscard]] virtual uint32_t version() const = 0;

    /**
     * @brief Get RAN function IDs handled by this SM
     */
    [[nodiscard]] virtual std::vector<uint32_t> ran_function_ids() const = 0;

    /**
     * @brief Check if SM handles a specific RAN function
     */
    [[nodiscard]] bool handles_ran_function(uint32_t ran_function_id) const {
        auto ids = ran_function_ids();
        return std::find(ids.begin(), ids.end(), ran_function_id) != ids.end();
    }

    /**
     * @brief Initialize the SM
     *
     * Called when the SM is first registered.
     * @return ErrorCode::SUCCESS on success
     */
    [[nodiscard]] virtual ErrorCode init() = 0;

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
    [[nodiscard]] virtual ErrorCode start() = 0;

    /**
     * @brief Stop the SM processing
     *
     * Called when the last dApp unsubscribes from this SM's RAN function.
     */
    virtual void stop() = 0;

    /**
     * @brief Check if SM is currently running
     */
    [[nodiscard]] virtual bool is_running() const = 0;

    /**
     * @brief Process a control action from a dApp
     *
     * @param ran_function_id The RAN function this action is for
     * @param action_data E3SM-encoded control action data
     * @return ErrorCode::SUCCESS on success
     */
    [[nodiscard]] virtual ErrorCode process_control_action(
        uint32_t ran_function_id,
        const std::vector<uint8_t>& action_data
    ) = 0;

    /**
     * @brief Set callback for delivering indication data
     *
     * The SM implementation should call this callback when it has
     * indication data ready to be sent to subscribers.
     */
    void set_indication_callback(IndicationDataCallback callback) {
        indication_callback_ = std::move(callback);
    }

protected:
    ServiceModel() = default;

    /**
     * @brief Deliver indication data to the E3 agent
     *
     * Call this from your SM implementation when indication data is ready.
     */
    void deliver_indication(uint32_t ran_function_id,
                           std::vector<uint8_t> encoded_data,
                           uint64_t timestamp = 0) {
        if (indication_callback_) {
            indication_callback_(ran_function_id, std::move(encoded_data), timestamp);
        }
    }

private:
    IndicationDataCallback indication_callback_;
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
    [[nodiscard]] ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Register a Service Model factory
     *
     * Use this to defer SM creation until needed.
     */
    [[nodiscard]] ErrorCode register_sm_factory(uint32_t ran_function_id, SmFactory factory);

    /**
     * @brief Unregister a Service Model by RAN function ID
     */
    [[nodiscard]] ErrorCode unregister_sm(uint32_t ran_function_id);

    /**
     * @brief Get SM by RAN function ID
     *
     * @param ran_function_id RAN function ID to look up
     * @return Pointer to SM, nullptr if not found
     */
    [[nodiscard]] ServiceModel* get_by_ran_function(uint32_t ran_function_id);

    /**
     * @brief Get all available RAN function IDs
     */
    [[nodiscard]] std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Start SM for a RAN function
     */
    [[nodiscard]] ErrorCode start_sm(uint32_t ran_function_id);

    /**
     * @brief Stop SM for a RAN function
     */
    [[nodiscard]] ErrorCode stop_sm(uint32_t ran_function_id);

    /**
     * @brief Check if SM is running
     */
    [[nodiscard]] bool is_sm_running(uint32_t ran_function_id) const;

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
