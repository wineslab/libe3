/**
 * @file e3_interface.hpp
 * @brief E3Interface - Internal protocol coordination layer
 *
 * This class manages the E3AP lifecycle and coordinates between
 * the E3Agent facade and the protocol handling components. It is
 * NOT exposed to library users directly.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_INTERFACE_HPP
#define LIBE3_E3_INTERFACE_HPP

#include "types.hpp"
#include "e3_connector.hpp"
#include "e3_encoder.hpp"
#include "subscription_manager.hpp"
#include "lockfree_queue.hpp"
#include "sm_interface.hpp"
#include "dapp_subscription_state.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace libe3 {

// Forward declaration
class E3Agent;

/**
 * @brief Callback types for E3Interface events
 */
using SetupRequestHandler = std::function<ResponseCode(const SetupRequest&, SetupResponse&)>;
using SubscriptionRequestHandler = std::function<ResponseCode(const SubscriptionRequest&)>;
using DAppReportHandler = std::function<void(const DAppReport&)>;
using DAppStatusChangedHandler = std::function<void()>;

// dApp-side handlers (RAN -> dApp PDU dispatch)
using SetupResponseHandler = std::function<void(const SetupResponse&)>;
using SubscriptionResponseHandler = std::function<void(const SubscriptionResponse&)>;
using IndicationHandler = std::function<void(const IndicationMessage&)>;
using XAppControlHandler = std::function<void(const XAppControlAction&)>;
using MessageAckHandler = std::function<void(const MessageAck&)>;

/**
 * @brief E3Interface - Internal protocol coordination
 *
 * This class is the internal coordination layer that:
 * - Manages E3AP lifecycle (Setup, Subscription, Control, Release)
 * - Owns inbound/outbound processing threads
 * - Coordinates between connector, encoder, and subscription manager
 * - Supports simulation mode for testing without real RAN
 *
 * This class is NOT directly exposed to library users. Users interact
 * with the E3Agent facade which owns an E3Interface instance.
 */
class E3Interface {
public:
    /**
     * @brief Construct with configuration
     */
    explicit E3Interface(const E3Config& config);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~E3Interface();

    // Non-copyable, non-movable
    E3Interface(const E3Interface&) = delete;
    E3Interface& operator=(const E3Interface&) = delete;
    E3Interface(E3Interface&&) = delete;
    E3Interface& operator=(E3Interface&&) = delete;

    /**
     * @brief Initialize the interface
     *
     * Creates connector, encoder, and initializes internal state.
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode init();

    /**
     * @brief Start the interface processing loops
     *
     * Spawns the setup, inbound, and outbound threads, plus the report worker on the RAN role.
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode start();

    /**
     * @brief Stop the interface
     *
     * Signals all threads to stop and waits for them to finish.
     */
    void stop();

    /**
     * @brief Get current state
     */
    AgentState state() const noexcept { return state_.load(); }

    /**
     * @brief Check if interface is running
     */
    bool is_running() const noexcept { 
        return state_.load() == AgentState::RUNNING; 
    }

    /**
     * @brief Get the subscription manager
     */
    SubscriptionManager& subscription_manager() noexcept { 
        return *subscription_manager_; 
    }

    /**
     * @brief Get the response queue for outbound messages
     */
    LockFreeQueue<Pdu>& response_queue() noexcept {
        return *response_queue_;
    }

    /**
     * @brief Queue a PDU for outbound transmission
     */
    ErrorCode queue_outbound(Pdu pdu);

    /**
     * @brief Get available RAN functions
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Register a Service Model
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    // =========================================================================
    // Event Handlers - Set by E3Agent
    // =========================================================================

    void set_dapp_report_handler(DAppReportHandler handler) {
        dapp_report_handler_ = std::move(handler);
    }

    void set_dapp_status_changed_handler(DAppStatusChangedHandler handler) {
        dapp_status_changed_handler_ = std::move(handler);
    }

    // dApp-side handlers (set before start() when role==DAPP)
    void set_setup_response_handler(SetupResponseHandler handler) {
        setup_response_handler_ = std::move(handler);
    }
    void set_subscription_response_handler(SubscriptionResponseHandler handler) {
        subscription_response_handler_ = std::move(handler);
    }
    void set_indication_handler(IndicationHandler handler) {
        indication_handler_ = std::move(handler);
    }
    void set_xapp_control_handler(XAppControlHandler handler) {
        xapp_control_handler_ = std::move(handler);
    }
    void set_message_ack_handler(MessageAckHandler handler) {
        message_ack_handler_ = std::move(handler);
    }

    // dApp-side accessors
    std::optional<uint32_t> dapp_id() const noexcept;
    std::vector<uint32_t> active_subscription_ids() const;
    std::vector<uint32_t> subscribed_ran_functions() const;
    std::vector<RanFunctionDef> remote_ran_functions() const;

    // dApp-side outbound helpers (queue PDUs that the outbound loop will encode + send)
    ErrorCode queue_subscription_request(uint32_t ran_function_id,
                                         std::vector<uint32_t> telemetry_ids,
                                         std::vector<uint32_t> control_ids,
                                         std::optional<uint32_t> sub_time = std::nullopt,
                                         std::optional<uint32_t> periodicity = std::nullopt);
    ErrorCode queue_subscription_delete(uint32_t ran_function_id);
    ErrorCode queue_dapp_control_action(uint32_t ran_function_id,
                                        uint32_t control_id,
                                        std::vector<uint8_t> action_data);
    ErrorCode queue_dapp_report(uint32_t ran_function_id,
                                std::vector<uint8_t> report_data);
    ErrorCode queue_release_message();

    // Block until the setup handshake completes (or times out / fails).
    // Only meaningful on dApp role.
    ErrorCode wait_for_setup(std::chrono::milliseconds timeout);

    void notify_dapp_status_changed();

private:
    // Configuration
    E3Config config_;

    // State
    std::atomic<AgentState> state_{AgentState::UNINITIALIZED};
    std::atomic<bool> should_stop_{false};

    // Core components
    std::unique_ptr<E3Connector> connector_;
    std::unique_ptr<E3Encoder> encoder_;
    // RAN-only state (nullptr when role==DAPP).
    std::unique_ptr<SubscriptionManager> subscription_manager_;
    // dApp-only state (nullptr when role==RAN).
    std::unique_ptr<DAppSubscriptionState> dapp_state_;
    std::unique_ptr<LockFreeQueue<Pdu>> response_queue_;

    // dApp-report queue: subscriber/inbound thread hands reports off to the
    // report worker thread so downstream (OAI / iApp) work never blocks reads.
    std::unique_ptr<LockFreeQueue<DAppReport>> report_queue_;

    // Threads. setup_thread_ runs the setup loop (RAN: serves; dApp: drives once).
    // inbound_thread_ / outbound_thread_ replace the old subscriber_thread_ /
    // publisher_thread_ names since on the dApp side what flows in is not a
    // subscription.
    std::unique_ptr<std::thread> setup_thread_;
    std::unique_ptr<std::thread> inbound_thread_;
    std::unique_ptr<std::thread> outbound_thread_;
    std::unique_ptr<std::thread> report_worker_thread_;

    // RAN-side handlers
    DAppReportHandler dapp_report_handler_;
    DAppStatusChangedHandler dapp_status_changed_handler_;

    // dApp-side handlers
    SetupResponseHandler setup_response_handler_;
    SubscriptionResponseHandler subscription_response_handler_;
    IndicationHandler indication_handler_;
    XAppControlHandler xapp_control_handler_;
    MessageAckHandler message_ack_handler_;

    // dApp-side setup-complete signal (notified by setup_loop_dapp)
    mutable std::mutex setup_complete_mu_;
    std::condition_variable setup_complete_cv_;
    bool setup_complete_{false};
    bool setup_succeeded_{false};

    // =========================================================================
    // Thread Entry Points
    // =========================================================================

    /**
     * @brief RAN-role main setup loop - handles E3 Setup requests
     */
    void setup_loop_ran();

    /**
     * @brief dApp-role setup loop - send SetupRequest, recv SetupResponse, idle.
     */
    void setup_loop_dapp();

    /**
     * @brief RAN-role inbound loop - receives subscribe/control/report/release.
     */
    void inbound_loop_ran();

    /**
     * @brief dApp-role inbound loop - receives subscribe-response/indication/
     *        xapp-control/message-ack.
     */
    void inbound_loop_dapp();

    /**
     * @brief RAN-role outbound loop - sends indications, subscription
     *        responses and acks back to dApps.
     */
    void outbound_loop_ran();

    /**
     * @brief dApp-role outbound loop - sends subscribe/delete/control/
     *        report/release back to the RAN.
     */
    void outbound_loop_dapp();

    /**
     * @brief Report worker thread - drains report_queue_ and invokes
     *        handle_dapp_report() off the subscriber thread so ZMQ reads
     *        are never blocked by downstream (OAI / iApp) work.
     */
    void report_worker_loop();

    // =========================================================================
    // Message Handlers
    // =========================================================================

    // RAN-role handlers
    void handle_setup_request(const SetupRequest& request, uint32_t request_message_id);
    void handle_subscription_request(const SubscriptionRequest& request, uint32_t request_message_id);
    void handle_subscription_delete(const SubscriptionDelete& del, uint32_t request_message_id);
    void handle_control_action(const DAppControlAction& action, uint32_t request_message_id);
    void handle_dapp_report(const DAppReport& report);
    void handle_release_message(const ReleaseMessage &release);
    void handle_dapp_disconnection(uint32_t dapp_id);

    /**
     * @brief Reply to a setup request that cannot be answered positively.
     *
     * The RAN side of the setup channel is a REQ/REP exchange: exactly one
     * reply must be sent per received request before the next one can be
     * received (a ZMQ REP socket enforces this in its state machine).
     * Bailing out without replying (undecodable bytes, wrong PDU type)
     * wedges the setup channel for every subsequent dApp until the agent
     * restarts. The encoding is fixed and known a priori by both peers, so
     * a negative SetupResponse can always be produced; the peer gets an
     * explicit rejection instead of a silent timeout.
     *
     * @param request_id Message ID of the offending request, or any id that
     *                   cannot be echoed (0 when the request bytes never
     *                   decoded, out of E3-MessageID range otherwise) — a
     *                   fresh id is then substituted on the wire (dApps do
     *                   not correlate setup replies by ID).
     */
    void send_negative_setup_reply(uint32_t request_id);

    // dApp-role handlers
    void handle_setup_response(const SetupResponse& response);
    void handle_subscription_response(const SubscriptionResponse& response);
    void handle_indication(const IndicationMessage& msg);
    void handle_xapp_control_action(const XAppControlAction& action);
    void handle_message_ack(const MessageAck& ack);

    // =========================================================================
    // SM Lifecycle Management
    // =========================================================================

    /**
     * @brief Callback for SM lifecycle changes
     */
    void on_sm_lifecycle_change(uint32_t ran_function_id, bool should_start);

public:
    /**
     * @brief Generate message ID (1-1000, randomized)
     */
    uint32_t generate_message_id();

    /**
     * @brief Return a request id that can be echoed into a response.
     *
     * The decoders do not enforce integer range constraints, so a
     * peer-supplied message id outside E3-MessageID's 1..1000 can reach
     * the handlers; echoing it into a response would fail the encode.
     * In-range ids pass through, anything else is replaced with a fresh
     * generated id (dApps do not correlate setup replies by ID).
     */
    uint32_t sanitize_request_message_id(uint32_t request_id);
};

} // namespace libe3

#endif // LIBE3_E3_INTERFACE_HPP
