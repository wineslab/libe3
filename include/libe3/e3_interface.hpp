/**
 * @file e3_interface.hpp
 * @brief E3Interface - Internal protocol coordination layer
 *
 * This class manages the E3AP lifecycle and coordinates between
 * the E3Agent facade and the protocol handling components. It is
 * NOT exposed to library users directly.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
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
#include <unordered_map>

namespace libe3 {

// Forward declaration
class E3Agent;

/**
 * @brief Callback types for E3Interface events
 */
using SetupRequestHandler = std::function<ResponseCode(const SetupRequest&, SetupResponse&)>;
/// Called for a SubscriptionRequest; the returned code is the answer. @param request The request @return POSITIVE to grant it
using SubscriptionRequestHandler = std::function<ResponseCode(const SubscriptionRequest&)>;
using DAppReportHandler = std::function<void(const DAppReport&)>;
using DAppStatusChangedHandler = std::function<void()>;

// dApp-side handlers (RAN -> dApp PDU dispatch)
using SetupResponseHandler = std::function<void(const SetupResponse&)>;
using SubscriptionResponseHandler = std::function<void(const SubscriptionResponse&)>;
using IndicationHandler = std::function<void(const IndicationMessage&)>;
using XAppControlHandler = std::function<void(const XAppControlAction&)>;
/// Called for a MessageAck. DEPRECATED TO BE REMOVED: libe3 does not send one, it reads one from another RAN.
using MessageAckHandler = std::function<void(const MessageAck&)>;
/// Called for a ControlResponse: on a dApp, the RAN's answer to its control; on a RAN, the dApp's answer to an xApp control.
using ControlResponseHandler = std::function<void(const ControlResponse&)>;
using DisconnectHandler = std::function<void(DisconnectReason)>;

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
     * @param config Role, transport, encoding and tuning for this interface
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
     * Signals all threads to stop and waits for them to finish. It also ends
     * the dApp's reconnecting, so the dApp stays down. Not to be called from
     * the disconnect handler, which runs on a thread stop() joins.
     */
    void stop();

    /**
     * @brief Get current state
     * @return The lifecycle state of the interface
     */
    AgentState state() const noexcept { return state_.load(); }

    /**
     * @brief Check if interface is running
     * @return True while the state is RUNNING
     */
    bool is_running() const noexcept { 
        return state_.load() == AgentState::RUNNING; 
    }

    /**
     * @brief Get the subscription manager
     * @return The manager that holds the dApps and their subscriptions
     */
    SubscriptionManager& subscription_manager() noexcept { 
        return *subscription_manager_; 
    }

    /**
     * @brief Get the response queue for outbound messages
     * @return The queue the outbound loop drains
     */
    LockFreeQueue<Pdu>& response_queue() noexcept {
        return *response_queue_;
    }

    /**
     * @brief Queue a PDU for outbound transmission
     * @param pdu PDU to send; it is moved into the queue
     * @return SUCCESS once queued; the queueing error otherwise
     */
    ErrorCode queue_outbound(Pdu pdu);

    /**
     * @brief Get available RAN functions
     * @return The identifiers of the registered Service Models, sorted
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Register a Service Model
     * @param sm Service Model to register; ownership moves to the registry
     * @return SUCCESS, or the registry's error
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    // =========================================================================
    // Event Handlers - Set by E3Agent
    // =========================================================================

    /// Set the callback for a dApp report that reaches the RAN. @param handler Callback to call
    void set_dapp_report_handler(DAppReportHandler handler) {
        dapp_report_handler_ = std::move(handler);
    }

    /// Set the callback for a dApp that connects, subscribes, unsubscribes or leaves. @param handler Callback to call
    void set_dapp_status_changed_handler(DAppStatusChangedHandler handler) {
        dapp_status_changed_handler_ = std::move(handler);
    }

    // dApp-side handlers (set before start() when role==DAPP)
    /// Set the callback for a SetupResponse. @param handler Callback to call
    void set_setup_response_handler(SetupResponseHandler handler) {
        setup_response_handler_ = std::move(handler);
    }
    /// Set the callback for a SubscriptionResponse. @param handler Callback to call
    void set_subscription_response_handler(SubscriptionResponseHandler handler) {
        subscription_response_handler_ = std::move(handler);
    }
    /// Set the callback for an IndicationMessage. @param handler Callback to call
    void set_indication_handler(IndicationHandler handler) {
        indication_handler_ = std::move(handler);
    }
    /// Set the callback for an XAppControlAction. @param handler Callback to call
    void set_xapp_control_handler(XAppControlHandler handler) {
        xapp_control_handler_ = std::move(handler);
    }
    /// Set the callback for a MessageAck. DEPRECATED TO BE REMOVED. @param handler Callback to call
    void set_message_ack_handler(MessageAckHandler handler) {
        message_ack_handler_ = std::move(handler);
    }
    /// Set the callback for a ControlResponse (either role). @param handler Callback to call
    void set_control_response_handler(ControlResponseHandler handler) {
        control_response_handler_ = std::move(handler);
    }
    /// Called once when the dApp's session with the RAN ends (see E3Agent::set_disconnect_handler).
    /// @param handler Callback to call
    void set_disconnect_handler(DisconnectHandler handler) {
        disconnect_handler_ = std::move(handler);
    }

    // dApp-side accessors
    /// @return The dApp identifier the RAN assigned, or nothing before setup completes
    std::optional<uint32_t> dapp_id() const noexcept;
    /// @return The subscription identifiers the RAN granted and that have not ended
    std::vector<uint32_t> active_subscription_ids() const;
    /// @return The RAN functions this dApp holds a subscription to
    std::vector<uint32_t> subscribed_ran_functions() const;
    /// @return The RAN functions the RAN advertised in its setup response
    std::vector<RanFunctionDef> remote_ran_functions() const;

    // dApp-side outbound helpers (queue PDUs that the outbound loop will encode + send)
    /**
     * @brief Queue a SubscriptionRequest for the outbound loop to send.
     * @param ran_function_id RAN function to subscribe to
     * @param telemetry_ids telemetry identifiers requested
     * @param control_ids control identifiers requested
     * @param sub_time how long to keep the subscription, in seconds; unset leaves it to the RAN
     * @param periodicity microseconds between indications, 0 = every event
     * @param out_request_id when non-null, receives the assigned E3-MessageID on
     *        success, so the caller can correlate the eventual SubscriptionResponse
     *        (which echoes request_id) back to this request.
     * @return SUCCESS once queued; STATE_ERROR on a RAN-role agent; NOT_INITIALIZED
     *         before setup has assigned a dApp id; otherwise the queueing error.
     */
    ErrorCode queue_subscription_request(uint32_t ran_function_id,
                                         std::vector<uint32_t> telemetry_ids,
                                         std::vector<uint32_t> control_ids,
                                         std::optional<uint32_t> sub_time = std::nullopt,
                                         std::optional<uint32_t> periodicity = std::nullopt,
                                         uint32_t* out_request_id = nullptr);
    /// Queue a SubscriptionDelete for the outbound loop to send.
    /// @param ran_function_id RAN function whose subscription ends
    /// @return SUCCESS once queued; STATE_ERROR on a RAN-role agent; NOT_INITIALIZED before setup has assigned a dApp id;
    ///         SUBSCRIPTION_NOT_FOUND when the dApp holds none for it; otherwise the queueing error
    ErrorCode queue_subscription_delete(uint32_t ran_function_id);
    /// Queue a dApp control action for the outbound loop to send.
    /// @param ran_function_id RAN function the control is for
    /// @param control_id control identifier granted by the RAN
    /// @param action_data opaque payload of the Service Model
    /// @param sequence_id correlation id of the xApp procedure this control re-issues; 0 for none
    /// @param out_message_id when non-null, receives the assigned E3-MessageID on
    ///        success, so the caller can correlate a later response back to this send
    /// @param request_ack true to ask the RAN for a ControlResponse
    /// @return SUCCESS once queued; otherwise the queueing error
    ErrorCode queue_dapp_control_action(uint32_t ran_function_id,
                                        uint32_t control_id,
                                        std::vector<uint8_t> action_data,
                                        uint32_t sequence_id = 0,
                                        uint32_t* out_message_id = nullptr,
                                        bool request_ack = false);
    /// Queue a ControlResponse that answers an xApp control (dApp role).
    /// @param request_id message id of the xApp control this answers
    /// @param ran_function_id RAN function of the control
    /// @param control_id control the answer is about
    /// @param result code, reason and outcome to send
    /// @return SUCCESS once queued; STATE_ERROR on a RAN-role agent; NOT_INITIALIZED before setup has
    ///         assigned a dApp id; INVALID_PARAM for an outcome over MAX_CONTROL_OUTCOME_SIZE; otherwise the queueing error
    ErrorCode queue_control_response(uint32_t request_id,
                                     uint32_t ran_function_id,
                                     uint32_t control_id,
                                     const ControlResult& result);
    /// Answer a dApp control the RAN holds an answer for (RAN role, called by Service Models).
    /// @param dapp_id dApp whose control is answered
    /// @param request_id message id of the dApp control
    /// @param result code, reason and outcome to send
    /// @return SUCCESS once queued; NOT_FOUND when no answer is owed: the dApp did not ask for one, or
    ///         the control was answered already; INVALID_PARAM for an outcome over MAX_CONTROL_OUTCOME_SIZE
    ErrorCode respond_to_control(uint32_t dapp_id, uint32_t request_id, const ControlResult& result);
    /// Queue a dApp report for the outbound loop to send.
    /// @param ran_function_id RAN function the report is for
    /// @param report_data opaque payload of the Service Model
    /// @param sequence_id correlation id the dApp assigns to this detection; mandatory, not 0
    /// @param out_message_id when non-null, receives the assigned E3-MessageID on success
    /// @return SUCCESS once queued; INVALID_PARAM for a sequence id of 0; otherwise the queueing error
    ErrorCode queue_dapp_report(uint32_t ran_function_id,
                                std::vector<uint8_t> report_data,
                                uint32_t sequence_id,
                                uint32_t* out_message_id = nullptr);
    /// Queue a ReleaseMessage that ends this dApp's session.
    /// @return SUCCESS once queued; otherwise the queueing error
    ErrorCode queue_release_message();

    /// Block until the setup handshake completes (or times out / fails).
    /// Only meaningful on dApp role.
    /// @param timeout How long to wait
    /// @return SUCCESS when setup completed; TIMEOUT when it did not complete in time; CONNECTION_FAILED when it failed;
    ///         INVALID_PARAM on a RAN-role interface
    ErrorCode wait_for_setup(std::chrono::milliseconds timeout);

    /// Tell the status handler that a dApp connected, subscribed, unsubscribed or left.
    void notify_dapp_status_changed();

private:
    // Configuration
    E3Config config_;

    // State
    std::atomic<AgentState> state_{AgentState::UNINITIALIZED};
    std::atomic<bool> should_stop_{false};
    bool started_before_{false};

    // Monotonic source for E3-MessageID (1..4294967295). A counter gives
    // unique ids, unlike the former random draw which could collide (birthday
    // hazard) and defeat request/response correlation.
    std::atomic<uint32_t> next_message_id_{0};

    // Core components
    std::unique_ptr<E3Connector> connector_;
    std::unique_ptr<E3Encoder> encoder_;
    // RAN-only state (nullptr when role==DAPP).
    std::unique_ptr<SubscriptionManager> subscription_manager_;
    // dApp-only state (nullptr when role==RAN).
    std::unique_ptr<DAppSubscriptionState> dapp_state_;

    // dApp-only: subscription requests awaiting their SubscriptionResponse.
    // E3-SubscriptionResponse echoes requestId but not ranFunctionIdentifier,
    // so the RAN function a granted subscription belongs to is only knowable
    // from the request that asked for it. Keyed by the E3-MessageID we sent;
    // entries are erased when the response arrives.
    struct PendingSubscriptionOp {
        uint32_t ran_function_id{0};
        bool is_delete{false};
        // subscriptionTime asked for, in seconds. The dApp's deadline starts when
        // the response arrives, never before the RAN's own.
        std::optional<uint32_t> subscription_time_s;
    };
    std::mutex pending_subscriptions_mutex_;
    std::unordered_map<uint32_t, PendingSubscriptionOp> pending_subscriptions_;
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
    std::unique_ptr<std::thread> expiry_thread_;
    std::mutex expiry_mu_;
    std::condition_variable expiry_cv_;  // woken by stop() so the thread exits at once

    // dApp reconnect. reconnect_thread_ restarts the interface (stop_locked, then
    // start_locked) when the RAN is gone or setup failed. lifecycle_mu_ keeps
    // that from overlapping a start() or stop() of the application;
    // reconnect_mu_ guards the two flags below and is never held across either.
    std::mutex lifecycle_mu_;
    std::mutex reconnect_mu_;
    std::condition_variable reconnect_cv_;
    bool reconnect_pending_{false};  ///< a restart has been asked for
    bool user_stopped_{false};       ///< stop() was called and start() has not been since
    std::unique_ptr<std::thread> reconnect_thread_;
#ifdef LIBE3_ENABLE_LATREC
    // ~1 Hz slow-lane sampler stamping LATREC_CONTEXT (involuntary context
    // switches, current CPU frequency), to explain p99 tails. Exists only in
    // a latrec-enabled build: a normal build spawns no such thread at all.
    std::unique_ptr<std::thread> context_thread_;
#endif

    // RAN-side handlers
    DAppReportHandler dapp_report_handler_;
    DAppStatusChangedHandler dapp_status_changed_handler_;

    // dApp-side handlers
    SetupResponseHandler setup_response_handler_;
    SubscriptionResponseHandler subscription_response_handler_;
    IndicationHandler indication_handler_;
    XAppControlHandler xapp_control_handler_;
    MessageAckHandler message_ack_handler_;  // DEPRECATED TO BE REMOVED
    ControlResponseHandler control_response_handler_;
    DisconnectHandler disconnect_handler_;

    /// A dApp control that asked for an answer and has none yet: where the RAN function and the
    /// control are found when a Service Model answers. At most MAX_PENDING_CONTROLS; an entry
    /// goes when it is answered or when its dApp leaves.
    struct PendingControl {
        uint32_t ran_function_id{0};  ///< RAN function of the control
        uint32_t control_id{0};       ///< Control identifier of the control
    };
    std::mutex pending_controls_mu_;
    std::unordered_map<uint64_t, PendingControl> pending_controls_;  ///< Keyed by dApp id and request id

    // dApp-only: the RAN is gone (set once per session) / we sent our own release.
    std::atomic<bool> ran_gone_{false};
    std::atomic<bool> release_sent_{false};

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

    /**
     * @brief Thread that ends subscriptions whose subscriptionTime has run out.
     *        The RAN removes them; the dApp forgets them at the same moment.
     *        Neither tells the other: E3-ReleaseMessage ends the whole dApp
     *        session, not one subscription.
     */
    void subscription_expiry_loop();

#ifdef LIBE3_ENABLE_LATREC
    /**
     * @brief ~1 Hz slow-lane sampler stamping LATREC_CONTEXT.
     */
    void context_monitor_loop();
#endif

    // =========================================================================
    // Message Handlers
    // =========================================================================

    // RAN-role handlers
    void handle_setup_request(const SetupRequest& request, uint32_t request_message_id);
    void handle_subscription_request(const SubscriptionRequest& request, uint32_t request_message_id);
    void handle_subscription_delete(const SubscriptionDelete& del, uint32_t request_message_id);
    void handle_control_action(const DAppControlAction& action, uint32_t request_message_id,
                               uint64_t latrec_seq);
    void handle_dapp_report(const DAppReport& report);
    void handle_release_message(const ReleaseMessage &release);
    void handle_dapp_disconnection(uint32_t dapp_id);
    void handle_control_response_from_dapp(const ControlResponse& response);
    /// Queue a negative ControlResponse for a dApp control the RAN refuses itself.
    void refuse_control(const DAppControlAction& action, uint32_t request_message_id,
                        Cause cause, std::string detail);

    /**
     * @brief Release and remove every dApp that has been quiet for dapp_idle_timeout_s
     *
     * Runs on the expiry thread. Each dApp gets a ReleaseMessage first, then the
     * same removal a release from the dApp gets.
     */
    void remove_idle_dapps();

    /// start() and stop() without the lifecycle lock or the reconnect thread.
    ErrorCode start_locked();
    void stop_locked();

    /// Ask the reconnect thread for a restart, unless it is off or stop() was called.
    void request_reconnect();

    /// Body of reconnect_thread_: wait for a request, pause, restart, repeat.
    void reconnect_loop();

    /// stop_locked() then start_locked() under the lifecycle lock, unless stop() was called.
    void restart_session();

    /**
     * @brief Which connection each dApp id belongs to, on a RAN that can tell its peers apart.
     *
     * A dApp id belongs to the connection that first sent a message naming it, for as
     * long as that registration lasts. Each entry carries the generation of the
     * registration (see SubscriptionManager::dapp_generation), because an id is handed
     * out again after its dApp is removed. Only the inbound thread uses it.
     */
    struct PeerBindings {
        /// Connection token, then the dApps seen on it, each with its registration's generation.
        std::unordered_map<uint64_t, std::unordered_map<uint32_t, uint64_t>> by_peer;
        /// dApp id, then its connection token and its registration's generation.
        std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> owner;
    };

    /**
     * @brief Decide whether a message that names a dApp may be acted on, and bind the dApp to its connection
     *
     * The first message that names a registered dApp binds the dApp to the connection it
     * came on. Later messages that name it must come on that connection: the E3AP messages
     * carry no authentication, so without this any dApp could release or control another.
     *
     * @param bindings The inbound thread's bindings
     * @param peer Token of the connection the message came on, 0 if the connector cannot tell
     * @param dapp_id The dApp the message names
     * @return false if the dApp is bound to another connection, so the message is to be dropped
     */
    bool admit_dapp_message(PeerBindings& bindings, uint64_t peer, uint32_t dapp_id);

    /**
     * @brief Remove every dApp that was seen on a connection that has since closed
     *
     * No release is sent, there is nobody to send it to. A dApp whose id was
     * removed and handed out again since it was seen is left alone.
     *
     * @param bindings The inbound thread's bindings
     */
    void remove_dapps_of_closed_peers(PeerBindings& bindings);

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
    void handle_indication(const IndicationMessage& msg, uint64_t latrec_seq);
    void handle_xapp_control_action(const XAppControlAction& action, uint64_t latrec_seq);
    void handle_message_ack(const MessageAck& ack);  // DEPRECATED TO BE REMOVED
    void handle_control_response_from_ran(const ControlResponse& response, uint64_t latrec_seq);
    void handle_release_from_ran(const ReleaseMessage& release);

    /// End the dApp session: clear its state and tell the application, once.
    void on_ran_gone(DisconnectReason reason);

    /// Build the transport connector from config_ (init, and again on restart).
    ErrorCode create_connector_from_config();

    // =========================================================================
    // SM Lifecycle Management
    // =========================================================================

    /**
     * @brief Callback for SM lifecycle changes
     */
    void on_sm_lifecycle_change(uint32_t ran_function_id, bool should_start);

    /**
     * @brief Record a subscribe/unsubscribe request awaiting its response.
     *
     * @param request_id      E3-MessageID the request was sent with
     * @param ran_function_id RAN function the request concerns
     * @param is_delete       true for a SubscriptionDelete, false for a request
     * @param subscription_time_s subscriptionTime the request asked for, if any
     */
    void remember_subscription_op(
        uint32_t request_id, uint32_t ran_function_id, bool is_delete,
        std::optional<uint32_t> subscription_time_s = std::nullopt);

    /// Drop a pending record, e.g. when the request could not be queued.
    void forget_subscription_op(uint32_t request_id);

public:
    /**
     * @brief Generate the next message ID, monotonic within 1..4294967295.
     *
     * Wraps a lock-free counter into the ASN.1 E3-MessageID range, skipping 0.
     * Monotonic (not random) so an id is unique, letting a caller correlate a
     * response by the id it was assigned.
     *
     * @return The next message id
     */
    uint32_t generate_message_id();

    /**
     * @brief Return a request id that can be echoed into a response.
     *
     * The decoders do not enforce integer range constraints, so a
     * peer-supplied message id of 0, outside E3-MessageID's 1..4294967295,
     * can reach the handlers; echoing it into a response would fail the
     * encode. Every non-zero id passes through unchanged, 0 is replaced
     * with a fresh generated id.
     *
     * @param request_id Message id a peer sent
     * @return The same id when it is not 0, a fresh one otherwise
     */
    uint32_t sanitize_request_message_id(uint32_t request_id);
};

} // namespace libe3

#endif // LIBE3_E3_INTERFACE_HPP
