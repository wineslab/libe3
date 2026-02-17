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
#include "response_queue.hpp"
#include "sm_interface.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace libe3 {

// Forward declaration
class E3Agent;

/**
 * @brief Callback types for E3Interface events
 */
using SetupRequestHandler = std::function<ResponseCode(const SetupRequest&, SetupResponse&)>;
using SubscriptionRequestHandler = std::function<ResponseCode(const SubscriptionRequest&)>;
using DAppReportHandler = std::function<void(const DAppReport&)>;

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
     * @brief Get the response queue for outbound messages
     */
    ResponseQueue& response_queue() noexcept { 
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

private:
    // Configuration
    E3Config config_;

    // State
    std::atomic<AgentState> state_{AgentState::UNINITIALIZED};
    std::atomic<bool> should_stop_{false};

    // Core components
    std::unique_ptr<E3Connector> connector_;
    std::unique_ptr<E3Encoder> encoder_;
    std::unique_ptr<SubscriptionManager> subscription_manager_;
    std::unique_ptr<ResponseQueue> response_queue_;

    // Threads
    std::unique_ptr<std::thread> setup_thread_;
    std::unique_ptr<std::thread> subscriber_thread_;
    std::unique_ptr<std::thread> publisher_thread_;
    std::unique_ptr<std::thread> sm_data_thread_;

    // Event handlers
    DAppReportHandler dapp_report_handler_;

    // =========================================================================
    // Thread Entry Points
    // =========================================================================

    /**
     * @brief Main setup loop - handles E3 Setup requests
     */
    void setup_loop();

    /**
     * @brief Subscriber thread - receives control actions from dApps
     */
    void subscriber_loop();

    /**
     * @brief Publisher thread - sends indication messages to dApps
     */
    void publisher_loop();

    /**
     * @brief SM data handler - polls SMs and queues indication messages
     */
    void sm_data_handler_loop();

    // =========================================================================
    // Message Handlers
    // =========================================================================

    /**
     * @brief Handle E3 Setup Request
     */
    void handle_setup_request(const SetupRequest& request, uint32_t request_message_id);

    /**
     * @brief Handle E3 Subscription Request
     */
    void handle_subscription_request(const SubscriptionRequest& request, uint32_t request_message_id);

    /**
     * @brief Handle E3 Subscription Delete
     */
    void handle_subscription_delete(const SubscriptionDelete& del, uint32_t request_message_id);

    /**
     * @brief Handle E3 Control Action
     * @param request_message_id Message ID from the incoming PDU (used for MessageAck)
     */
    void handle_control_action(const DAppControlAction& action, uint32_t request_message_id);

    /**
     * @brief Handle dApp Report
     */
    void handle_dapp_report(const DAppReport& report);

    /**
     * @brief Handle dApp Report
     */
    void handle_release_message(const ReleaseMessage &release);

    /**
     * @brief Handle dApp disconnection
     */
    void handle_dapp_disconnection(uint32_t dapp_id);

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
};

} // namespace libe3

#endif // LIBE3_E3_INTERFACE_HPP
