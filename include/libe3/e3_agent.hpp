/**
 * @file e3_agent.hpp
 * @brief E3Agent - Main façade class for RAN vendors
 *
 * This is the ONLY class RAN vendors should interact with. It provides
 * a clean, stable public API while hiding all internal implementation
 * details (connectors, encoders, threads, state machines).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_AGENT_HPP
#define LIBE3_E3_AGENT_HPP

#include "types.hpp"
#include "sm_interface.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <optional>
#include <chrono>

namespace libe3 {

// Forward declaration - implementation hidden from users
class E3Interface;

/** Callback for incoming dApp reports (dApp → RAN). Set before start() to handle reports. */
using DAppReportHandler = std::function<void(const DAppReport&)>;

/** Callback when dApp status changes (connect, disconnect, subscribe, unsubscribe). */
using DAppStatusChangedHandler = std::function<void()>;

// dApp-side handlers (used when role==DAPP).
/** Callback for a SetupResponse (RAN → dApp). */
using SetupResponseHandler = std::function<void(const SetupResponse&)>;
/** Callback for a SubscriptionResponse (RAN → dApp). */
using SubscriptionResponseHandler = std::function<void(const SubscriptionResponse&)>;
/** Callback for an IndicationMessage (RAN → dApp). */
using IndicationHandler = std::function<void(const IndicationMessage&)>;
/** Callback for an XAppControlAction (xApp → dApp via the RAN). */
using XAppControlHandler = std::function<void(const XAppControlAction&)>;
/** Callback for a MessageAck (RAN → dApp). DEPRECATED TO BE REMOVED: libe3 does not send one. */
using MessageAckHandler = std::function<void(const MessageAck&)>;
/** Callback for a ControlResponse: the RAN's answer to a dApp control (RAN → dApp), or a dApp's answer to an xApp control (dApp → RAN). */
using ControlResponseHandler = std::function<void(const ControlResponse&)>;
/** Callback for the end of the dApp's session with the RAN, with the reason. */
using DisconnectHandler = std::function<void(DisconnectReason)>;

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
    /** @brief Move constructor. */
    E3Agent(E3Agent&&) noexcept;
    /**
     * @brief Move assignment.
     * @return This agent
     */
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
     * @return The state the agent is in
     */
    AgentState state() const noexcept;

    /**
     * @brief Check if agent is running
     * @return true while the agent is running
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
     * @return The RAN function IDs
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Set callback for incoming dApp reports (dApp → RAN).
     * Call before start(). When a dApp sends a DApp report, this callback is invoked.
     * @param handler Called with each dApp report
     */
    void set_dapp_report_handler(DAppReportHandler handler);

    /**
     * @brief Set callback for dApp status changes.
     * Called when a dApp connects, disconnects, subscribes, or unsubscribes.
     * Call before start().
     * @param handler Called after each change
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
     * @param sequence_id Correlation id from the E2SM-DAPP control header, so
     *        the dApp can carry it back on the control it re-issues.
     * @param out_message_id When non-null, receives the E3-MessageID of the control on
     *        success: the `request_id` of the ControlResponse that answers it.
     * @param request_ack True to ask the dApp for a ControlResponse; it reaches the
     *        handler of set_control_response_handler().
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode send_xapp_control(
        uint32_t dapp_id,
        uint32_t ran_function_id,
        const std::vector<uint8_t>& control_data,
        uint32_t sequence_id,
        uint32_t* out_message_id = nullptr,
        bool request_ack = false
    );

    /**
     * @brief Send a message acknowledgment.
     *
     * DEPRECATED TO BE REMOVED. The answer to a control is a ControlResponse, which a
     * Service Model sends with ServiceModel::respond_to_control(). A libe3 RAN does not
     * read a MessageAck from a dApp.
     * @param request_id ID of the request being acknowledged
     * @param response_code ResponseCode::POSITIVE (0) or NEGATIVE (1)
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode send_message_ack(uint32_t request_id, ResponseCode response_code);

    // =========================================================================
    // dApp-role API (used when E3Config.role == E3Role::DAPP)
    //
    // Calling these on a RAN-role agent returns STATE_ERROR. Setters can be
    // called before start(); they're stored and forwarded to the interface
    // once init() runs.
    // =========================================================================

    /**
     * Set callback for incoming SetupResponse (RAN → dApp).
     * @param handler Called on every setup, so also when the dApp registers again
     */
    void set_setup_response_handler(SetupResponseHandler handler);
    /**
     * Set callback for incoming SubscriptionResponse (RAN → dApp).
     * @param handler Called with each response
     */
    void set_subscription_response_handler(SubscriptionResponseHandler handler);
    /**
     * Set callback for incoming IndicationMessage (RAN → dApp).
     * @param handler Called with each indication
     */
    void set_indication_handler(IndicationHandler handler);
    /**
     * Set callback for incoming XAppControlAction (xApp → dApp via RAN).
     * @param handler Called with each control action
     */
    void set_xapp_control_handler(XAppControlHandler handler);
    /**
     * Set callback for incoming MessageAck (RAN → dApp). DEPRECATED TO BE REMOVED:
     * libe3 does not send one; another RAN may.
     * @param handler Called with each acknowledgement
     */
    void set_message_ack_handler(MessageAckHandler handler);
    /**
     * Set callback for incoming ControlResponse, in either role. On a dApp it is the
     * RAN's answer to a control that asked for one, or the RAN's refusal of any control;
     * on a RAN it is a dApp's answer to an xApp control that asked for one.
     * @param handler Called with each response
     */
    void set_control_response_handler(ControlResponseHandler handler);
    /**
     * Set callback for the end of the dApp's session with the RAN: a release for
     * this dApp, or a closed or failed connection. Fires once, after dapp_id()
     * has been cleared. Unless E3Config::reconnect_interval_ms is 0 the dApp then
     * registers again by itself, with a new dapp_id() and no subscriptions: subscribe
     * again from the setup response handler. It does not after release() or stop().
     * @param handler Called once per ended session, with the reason
     */
    void set_disconnect_handler(DisconnectHandler handler);

    /**
     * @brief dApp identifier assigned by the RAN in SetupResponse.
     * @return nullopt until setup completes (or on RAN-role agents).
     */
    std::optional<uint32_t> dapp_id() const noexcept;

    /**
     * RAN functions this dApp is currently subscribed to.
     *
     * @return The RAN function IDs
     */
    std::vector<uint32_t> subscribed_ran_functions() const;

    /**
     * Subscription IDs this dApp currently holds.
     *
     * @return The subscription IDs
     */
    std::vector<uint32_t> active_subscription_ids() const;

    /**
     * @brief Block until the dApp setup handshake completes.
     * @return SUCCESS on positive SetupResponse, CONNECTION_FAILED on negative,
     *         TIMEOUT if the wait elapses, INVALID_PARAM on RAN-role.
     * @param timeout How long to wait at most
     */
    ErrorCode wait_for_setup(std::chrono::milliseconds timeout);

    /**
     * @brief Subscribe to a RAN function. Must be called after a positive setup.
     *
     * @param ran_function_id RAN function to subscribe to
     * @param telemetry_ids telemetry identifiers requested
     * @param control_ids control identifiers requested
     * @param sub_time seconds to keep the subscription. Over ASN.1 the range is 0 to 86400.
     * @param periodicity microseconds between indications, 0 = every event.
     *        Over ASN.1 the range is 0 to 60000000. An OCUDU RAN reads the
     *        same number as milliseconds, so pass it what it expects.
     * @param out_request_id when non-null, receives the assigned E3-MessageID on
     *        success, so the caller can correlate the SubscriptionResponse
     *        (which echoes request_id) back to this subscribe call.
     * @return SUCCESS once the request is queued; STATE_ERROR on a RAN-role
     *         agent; NOT_INITIALIZED if the agent is not running or setup has
     *         not completed; otherwise the error from queueing the message.
     */
    ErrorCode subscribe(uint32_t ran_function_id,
                        std::vector<uint32_t> telemetry_ids,
                        std::vector<uint32_t> control_ids,
                        std::optional<uint32_t> sub_time = std::nullopt,
                        std::optional<uint32_t> periodicity = std::nullopt,
                        uint32_t* out_request_id = nullptr);

    /**
     * Send a SubscriptionDelete for a previously subscribed RAN function.
     *
     * @param ran_function_id RAN function to unsubscribe from
     * @return ErrorCode::SUCCESS if the delete was queued, an error code otherwise
     */
    ErrorCode unsubscribe(uint32_t ran_function_id);

    /**
     * @brief Send a dApp control action to the RAN.
     *
     * @param ran_function_id RAN function the control is for
     * @param control_id control identifier granted by the RAN
     * @param action_data opaque E3SM control payload
     * @param sequence_id correlation id of the xApp procedure this control
     *        re-issues; leave 0 for a control the dApp decided on its own.
     * @param out_message_id when non-null, receives the assigned E3-MessageID on
     *        success: the `request_id` of the ControlResponse that answers it.
     * @param request_ack true to ask the RAN for a ControlResponse when the control
     *        has been dealt with. Without it the RAN answers only a control that it
     *        refuses itself (no setup, unknown function or control, no subscription).
     * @return ErrorCode::SUCCESS if the control was queued, an error code otherwise
     */
    ErrorCode send_control(uint32_t ran_function_id,
                           uint32_t control_id,
                           std::vector<uint8_t> action_data,
                           uint32_t sequence_id = 0,
                           uint32_t* out_message_id = nullptr,
                           bool request_ack = false);

    /**
     * @brief Answer an xApp control that asked for an answer (`XAppControlAction::request_ack`).
     *
     * @param request_id `XAppControlAction::message_id` of the control being answered
     * @param ran_function_id RAN function of the control
     * @param control_id control the dApp applied, from the list the RAN advertised for the function
     * @param result Whether the dApp applied the control, why not, and what it reports about it
     * @return ErrorCode::SUCCESS if the answer was queued; STATE_ERROR on a RAN-role agent;
     *         NOT_INITIALIZED if the agent is not running or setup has not completed;
     *         INVALID_PARAM if the outcome is over MAX_CONTROL_OUTCOME_SIZE
     */
    ErrorCode send_control_response(uint32_t request_id,
                                    uint32_t ran_function_id,
                                    uint32_t control_id,
                                    const ControlResult& result);

    /**
     * @brief Send a dApp report to the RAN.
     *
     * @param ran_function_id RAN function the report is for
     * @param report_data opaque E3SM report payload
     * @param sequence_id correlation id the dApp assigns to this detection. It
     *        is the head of the loop's chain and is mandatory on the wire.
     * @param out_message_id when non-null, receives the assigned E3-MessageID on
     *        success, so the caller can correlate a later ack/response back to
     *        this send.
     * @return ErrorCode::SUCCESS if the report was queued, an error code otherwise
     */
    ErrorCode send_report(uint32_t ran_function_id,
                          std::vector<uint8_t> report_data,
                          uint32_t sequence_id,
                          uint32_t* out_message_id = nullptr);

    /**
     * Send a ReleaseMessage. The dApp stays running until stop() is called.
     *
     * @return ErrorCode::SUCCESS if the release was queued, an error code otherwise
     */
    ErrorCode release();

    /**
     * @brief Get list of registered dApp IDs
     * @return The dApp IDs
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

    /**
     * @brief Get the RAN functions that currently have at least one subscriber
     *
     * RAN role only. Unlike get_available_ran_functions() (which lists every
     * registered Service Model), this returns only the RAN functions a dApp is
     * actively subscribed to.
     *
     * @return List of RAN function IDs with one or more subscribers
     */
    std::vector<uint32_t> get_active_ran_functions() const;

    /**
     * @brief Get the reporting periodicity a dApp requested for a RAN function
     *
     * @return periodicity in microseconds, 0 if not set or subscription not found
     * @param dapp_id The dApp
     * @param ran_function_id The RAN function
     */
    uint32_t get_subscription_periodicity(
        uint32_t dapp_id, uint32_t ran_function_id
    ) const;

    /**
     * @brief Get the telemetry IDs a dApp requested for a RAN function
     *
     * @return vector of telemetry IDs (empty if none requested or subscription not found)
     * @param dapp_id The dApp
     * @param ran_function_id The RAN function
     */
    std::vector<uint32_t> get_subscription_telemetry_ids(
        uint32_t dapp_id, uint32_t ran_function_id
    ) const;

    /**
     * @brief Get the control IDs a dApp requested for a RAN function
     *
     * @return vector of control IDs (empty if none requested or subscription not found)
     * @param dapp_id The dApp
     * @param ran_function_id The RAN function
     */
    std::vector<uint32_t> get_subscription_control_ids(
        uint32_t dapp_id, uint32_t ran_function_id
    ) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     * @return The configuration the agent was created with
     */
    const E3Config& config() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of registered dApps
     * @return The number of dApps
     */
    size_t dapp_count() const;

    /**
     * @brief Get total number of active subscriptions
     * @return The number of subscriptions
     */
    size_t subscription_count() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace libe3

#endif // LIBE3_E3_AGENT_HPP
