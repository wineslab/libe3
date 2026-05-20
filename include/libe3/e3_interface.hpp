/**
 * @file e3_interface.hpp
 * @brief E3Interface - Internal protocol coordination layer
 *
 * This class manages the E3AP lifecycle and coordinates between
 * the E3Agent facade and the protocol handling components. It is
 * NOT exposed to library users directly.
 *
 * Supports either a single channel (one encoder + one ZMQ triplet) or
 * dual channels (e.g. ASN.1 on one port set, JSON on another) when
 * `E3Config::enable_dual_encoding` is true. SubscriptionManager and
 * SmRegistry are shared across channels; outbound PDUs are routed
 * back through the channel the target dApp registered on.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_INTERFACE_HPP
#define LIBE3_E3_INTERFACE_HPP

#include "types.hpp"
#include "e3_connector.hpp"
#include "e3_encoder.hpp"
#include "subscription_manager.hpp"
#include "response_queue.hpp"
#include "sm_interface.hpp"
#include <memory>
#include <optional>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

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

/**
 * @brief Per-encoding I/O channel.
 *
 * Bundles the encoder, connector, outbound queue, and three I/O threads
 * for one wire-format/port-triplet combination. E3Interface holds 1 or 2
 * of these depending on `E3Config::enable_dual_encoding`.
 */
struct Channel {
    EncodingFormat encoding{EncodingFormat::ASN1};
    uint16_t setup_port{0};
    uint16_t subscriber_port{0};
    uint16_t publisher_port{0};
    std::unique_ptr<E3Encoder> encoder;
    std::unique_ptr<E3Connector> connector;
    std::unique_ptr<ResponseQueue> response_queue;
    std::unique_ptr<std::thread> setup_thread;
    std::unique_ptr<std::thread> subscriber_thread;
    std::unique_ptr<std::thread> publisher_thread;
};

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
     * Spawns the subscriber, publisher, and SM data handler threads.
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
     * @brief Queue a PDU for outbound transmission
     *
     * Routes the PDU to the response queue of the channel that the target
     * dApp is bound to (recorded by SubscriptionManager at register_dapp).
     * Falls back to channel 0 if the target cannot be determined.
     */
    ErrorCode queue_outbound(Pdu pdu);

    /**
     * @brief Get available RAN functions
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Encoding used by the channel a given dApp is bound to.
     *
     * Combines SubscriptionManager::get_dapp_channel() (channel index by
     * dApp ID) with channels_[idx].encoding. Returns std::nullopt if the
     * dApp is not registered or the recorded channel index is stale.
     *
     * Service Models call this through the C API to pick which encoder
     * to use when emitting indication payloads.
     */
    std::optional<EncodingFormat> get_dapp_encoding(uint32_t dapp_id) const;

    struct SubscriberEncoding {
        uint32_t       dapp_id;
        EncodingFormat encoding;
    };

    /**
     * @brief All dApps subscribed to a RAN function paired with the
     *        encoding their channel speaks, under a single shared-lock
     *        acquire on the SubscriptionManager.
     *
     * Service Models call this once per emit-batch instead of pairing
     * get_ran_function_subscribers() with per-dApp get_dapp_encoding()
     * — same data, one third the lock acquires, and the contention with
     * setup-time register_dapp (writer) drops accordingly.
     */
    std::vector<SubscriberEncoding>
    get_subscribers_with_encoding(uint32_t ran_function_id) const;

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

    void notify_dapp_status_changed();

private:
    // Configuration
    E3Config config_;

    // State
    std::atomic<AgentState> state_{AgentState::UNINITIALIZED};
    std::atomic<bool> should_stop_{false};

    // Core components shared across channels
    std::unique_ptr<SubscriptionManager> subscription_manager_;

    // I/O channels (1 or 2 entries)
    std::vector<std::unique_ptr<Channel>> channels_;

    // Event handlers
    DAppReportHandler dapp_report_handler_;
    DAppStatusChangedHandler dapp_status_changed_handler_;

    // =========================================================================
    // Thread Entry Points (one set per channel)
    // =========================================================================

    void setup_loop(size_t channel_idx);
    void subscriber_loop(size_t channel_idx);
    void publisher_loop(size_t channel_idx);

    // =========================================================================
    // Message Handlers
    //
    // Subscription/control/report/release are channel-agnostic at the handler
    // level: they touch the shared SubscriptionManager / SM registry. The
    // channel that received the inbound PDU is threaded through to handle
    // outbound routing (setup response, subscription response) so the dApp
    // gets a reply on the same encoding it spoke.
    // =========================================================================

    void handle_setup_request(const SetupRequest& request,
                              uint32_t request_message_id,
                              size_t channel_idx);

    void handle_subscription_request(const SubscriptionRequest& request,
                                     uint32_t request_message_id);

    void handle_subscription_delete(const SubscriptionDelete& del,
                                    uint32_t request_message_id);

    void handle_control_action(const DAppControlAction& action,
                               uint32_t request_message_id);

    void handle_dapp_report(const DAppReport& report);

    void handle_release_message(const ReleaseMessage& release);

    void handle_dapp_disconnection(uint32_t dapp_id);

    // =========================================================================
    // SM Lifecycle Management
    // =========================================================================

    void on_sm_lifecycle_change(uint32_t ran_function_id, bool should_start);

    // =========================================================================
    // Outbound routing helpers
    // =========================================================================

    /**
     * @brief Resolve the channel a PDU should be sent through.
     *
     * Inspects the PDU's `choice` variant for `dapp_identifier` and looks
     * the dApp up in SubscriptionManager. For MessageAck (no dApp tag) the
     * thread-local "current inbound channel" set by subscriber_loop is
     * used. Returns 0 if no hint is available.
     */
    size_t resolve_outbound_channel(const Pdu& pdu) const noexcept;

public:
    /**
     * @brief Generate message ID (1-1000, randomized)
     */
    uint32_t generate_message_id();
};

} // namespace libe3

#endif // LIBE3_E3_INTERFACE_HPP
