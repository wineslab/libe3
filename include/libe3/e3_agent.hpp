/**
 * @file e3_agent.hpp
 * @brief E3Agent - Main façade class for RAN vendors
 *
 * This is the ONLY class RAN vendors should interact with. It provides
 * a clean, stable public API while hiding all internal implementation
 * details (connectors, encoders, threads, state machines).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_AGENT_HPP
#define LIBE3_E3_AGENT_HPP

#include "types.hpp"
#include "sm_interface.hpp"
#include <memory>
#include <functional>
#include <vector>

namespace libe3 {

// Forward declaration - implementation hidden from users
class E3Interface;

/** Callback for incoming dApp reports (dApp → RAN). Set before start() to handle reports. */
using DAppReportHandler = std::function<void(const DAppReport&)>;

/** Callback when dApp status changes (connect, disconnect, subscribe, unsubscribe). */
using DAppStatusChangedHandler = std::function<void()>;

/**
 * @brief E3Agent - Main façade for RAN vendor integration
 *
 * This class is the primary entry point for integrating E3AP functionality
 * into a RAN function (DU, CU-CP, CU-UP). It provides:
 *
 * - Simple lifecycle management (init, start, stop, destroy)
 * - Service Model registration
 * - Automatic routing of control actions to the correct Service Model
 * - Thread-safe operation
 *
 * **Usage Example:**
 * ```cpp
 * // Create agent with configuration
 * libe3::E3Config config;
 * config.link_layer = libe3::E3LinkLayer::ZMQ;
 * config.transport_layer = libe3::E3TransportLayer::IPC;
 * config.encoding = libe3::EncodingFormat::JSON;
 * // Set custom ports if needed
 * config.setup_port = 9990;
 * config.subscriber_port = 9999;
 * config.publisher_port = 9991;

 * libe3::E3Agent agent(config);

 * // Register Service Models
 * agent.register_sm(std::make_unique<MySpectrumSM>());

 * // Start the agent
 * if (agent.start() != libe3::ErrorCode::SUCCESS) {
 *     // Handle error
 * }

 * // ... agent runs, handling dApp connections and messages ...

 * // Shutdown
 * agent.stop();
 * ```
 *
 * **Thread Safety:**
 * All public methods are thread-safe. The agent manages its own internal
 * threads for protocol handling.
 */
class E3Agent {
public:
    /**
     * @brief Construct an E3Agent with configuration
     *
     * @param config Agent configuration
     */
    explicit E3Agent(E3Config config = E3Config{});

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~E3Agent();

    // Non-copyable, movable
    E3Agent(const E3Agent&) = delete;
    E3Agent& operator=(const E3Agent&) = delete;
    E3Agent(E3Agent&&) noexcept;
    E3Agent& operator=(E3Agent&&) noexcept;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Initialize the agent
     *
     * Initializes internal components but does not start processing.
     * Must be called before start().
     *
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::ALREADY_INITIALIZED if already initialized
     */
    ErrorCode init();

    /**
     * @brief Start the agent
     *
     * Starts the internal processing threads and begins accepting
     * dApp connections. If not already initialized, calls init() first.
     *
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode start();

    /**
     * @brief Stop the agent
     *
     * Stops all processing threads and closes connections.
     * The agent can be restarted with start().
     */
    void stop();

    /**
     * @brief Get current agent state
     */
    AgentState state() const noexcept;

    /**
     * @brief Check if agent is running
     */
    bool is_running() const noexcept;

    // =========================================================================
    // Service Model Registration
    // =========================================================================

    /**
     * @brief Register a Service Model
     *
     * Service Models must be registered before start() is called.
     * Each SM handles one or more RAN function IDs.
     *
     * @param sm Service Model to register (ownership transferred)
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SM_ALREADY_REGISTERED if RAN function already has SM
     * @return ErrorCode::STATE_ERROR if agent is already running
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Get available RAN function IDs
     *
     * Returns the RAN function IDs from all registered Service Models.
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Set callback for incoming dApp reports (dApp → RAN).
     * Call before start(). When a dApp sends a DApp report, this callback is invoked.
     */
    void set_dapp_report_handler(DAppReportHandler handler);

    /**
     * @brief Set callback for dApp status changes.
     * Called when a dApp connects, disconnects, subscribes, or unsubscribes.
     * Call before start().
     */
    void set_dapp_status_changed_handler(DAppStatusChangedHandler handler);

    // =========================================================================
    // Manual Operations
    // =========================================================================

    /**
     * @brief Send an indication message to a specific dApp
     *
     * @param dapp_id Target dApp identifier
     * @param ran_function_id RAN function identifier
     * @param data E3SM-encoded indication data
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode send_indication(
        uint32_t dapp_id,
        uint32_t ran_function_id,
        const std::vector<uint8_t>& data
    );

    /**
     * @brief Send an xApp control action to a specific dApp coming from the E2SM-DAPP.
     *
     * @param dapp_id Target dApp identifier
     * @param ran_function_id RAN function identifier
     * @param control_data E3SM-encoded control payload
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode send_xapp_control(
        uint32_t dapp_id,
        uint32_t ran_function_id,
        const std::vector<uint8_t>& control_data
    );

    /**
     * @brief Send a message acknowledgment (e.g. ack a control/request from dApp).
     *
     * @param request_id ID of the request being acknowledged
     * @param response_code ResponseCode::POSITIVE (0) or NEGATIVE (1)
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode send_message_ack(uint32_t request_id, ResponseCode response_code);

    /**
     * @brief Get list of registered dApp IDs
     */
    std::vector<uint32_t> get_registered_dapps() const;

    /**
     * @brief Get subscriptions for a dApp
     *
     * @param dapp_id dApp identifier
     * @return List of RAN function IDs the dApp is subscribed to
     */
    std::vector<uint32_t> get_dapp_subscriptions(uint32_t dapp_id) const;

    /**
     * @brief Get subscribers for a RAN function
     *
     * @param ran_function_id RAN function identifier
     * @return List of dApp IDs subscribed to this RAN function
     */
    std::vector<uint32_t> get_ran_function_subscribers(
        uint32_t ran_function_id
    ) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const E3Config& config() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of registered dApps
     */
    size_t dapp_count() const;

    /**
     * @brief Get total number of active subscriptions
     */
    size_t subscription_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace libe3

#endif // LIBE3_E3_AGENT_HPP
