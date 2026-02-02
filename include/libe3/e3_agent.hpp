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

/**
 * @brief Callback type for receiving indication data
 *
 * @param dapp_id The dApp ID the indication is for
 * @param ran_function_id The RAN function providing the data
 * @param data E3SM-encoded indication data
 */
using IndicationCallback = std::function<void(
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const std::vector<uint8_t>& data
)>;

/**
 * @brief Callback type for receiving control actions
 *
 * @param dapp_id The dApp ID sending the control
 * @param ran_function_id The target RAN function
 * @param data E3SM-encoded control data
 * @return ErrorCode::SUCCESS to acknowledge, error code to reject
 */
using ControlCallback = std::function<ErrorCode(
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const std::vector<uint8_t>& data
)>;

/**
 * @brief E3Agent - Main façade for RAN vendor integration
 *
 * This class is the primary entry point for integrating E3AP functionality
 * into a RAN function (DU, CU-CP, CU-UP). It provides:
 *
 * - Simple lifecycle management (init, start, stop, destroy)
 * - Service Model registration
 * - Event callbacks for control actions and indications
 * - Thread-safe operation
 *
 * **Usage Example:**
 * ```cpp
 * // Create agent with configuration
 * libe3::E3Config config;
 * config.link_layer = libe3::E3LinkLayer::ZMQ;
 * config.transport_layer = libe3::E3TransportLayer::IPC;
 * config.encoding = libe3::EncodingFormat::JSON;
 *
 * libe3::E3Agent agent(config);
 *
 * // Register Service Models
 * agent.register_sm(std::make_unique<MySpectrumSM>());
 *
 * // Set callbacks
 * agent.set_control_callback([](uint32_t dapp, uint32_t rf, auto& data) {
 *     // Handle control action
 *     return libe3::ErrorCode::SUCCESS;
 * });
 *
 * // Start the agent
 * if (agent.start() != libe3::ErrorCode::SUCCESS) {
 *     // Handle error
 * }
 *
 * // ... agent runs, handling dApp connections and messages ...
 *
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
    [[nodiscard]] ErrorCode init();

    /**
     * @brief Start the agent
     *
     * Starts the internal processing threads and begins accepting
     * dApp connections. If not already initialized, calls init() first.
     *
     * @return ErrorCode::SUCCESS on success
     */
    [[nodiscard]] ErrorCode start();

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
    [[nodiscard]] AgentState state() const noexcept;

    /**
     * @brief Check if agent is running
     */
    [[nodiscard]] bool is_running() const noexcept;

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
    [[nodiscard]] ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Get available RAN function IDs
     *
     * Returns the RAN function IDs from all registered Service Models.
     */
    [[nodiscard]] std::vector<uint32_t> get_available_ran_functions() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for control actions from dApps
     *
     * This callback is invoked when a dApp sends a control action.
     * The callback runs in the subscriber thread context.
     *
     * @param callback Control action callback
     */
    void set_control_callback(ControlCallback callback);

    /**
     * @brief Set callback for indication data ready events
     *
     * This callback is invoked when indication data is ready to be
     * sent to subscribed dApps. Use this for custom indication routing.
     *
     * @param callback Indication callback
     */
    void set_indication_callback(IndicationCallback callback);

    // =========================================================================
    // Manual Operations (for advanced use cases)
    // =========================================================================

    /**
     * @brief Send an indication message to a specific dApp
     *
     * @param dapp_id Target dApp identifier
     * @param data E3SM-encoded indication data
     * @return ErrorCode::SUCCESS on success
     */
    [[nodiscard]] ErrorCode send_indication(
        uint32_t dapp_id,
        const std::vector<uint8_t>& data
    );

    /**
     * @brief Get list of registered dApp IDs
     */
    [[nodiscard]] std::vector<uint32_t> get_registered_dapps() const;

    /**
     * @brief Get subscriptions for a dApp
     *
     * @param dapp_id dApp identifier
     * @return List of RAN function IDs the dApp is subscribed to
     */
    [[nodiscard]] std::vector<uint32_t> get_dapp_subscriptions(uint32_t dapp_id) const;

    /**
     * @brief Get subscribers for a RAN function
     *
     * @param ran_function_id RAN function identifier
     * @return List of dApp IDs subscribed to this RAN function
     */
    [[nodiscard]] std::vector<uint32_t> get_ran_function_subscribers(
        uint32_t ran_function_id
    ) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const E3Config& config() const noexcept;

    /**
     * @brief Check if running in simulation mode
     *
     * In simulation mode, the agent operates without a real transport
     * connection, useful for unit testing.
     */
    [[nodiscard]] bool is_simulation_mode() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of registered dApps
     */
    [[nodiscard]] size_t dapp_count() const;

    /**
     * @brief Get total number of active subscriptions
     */
    [[nodiscard]] size_t subscription_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace libe3

#endif // LIBE3_E3_AGENT_HPP
