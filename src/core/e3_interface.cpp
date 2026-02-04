/**
 * @file e3_interface.cpp
 * @brief E3Interface implementation
 *
 * Ported from the original C implementation's e3_agent.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_interface.hpp"
#include "libe3/logger.hpp"
#include <chrono>
#include <signal.h>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "E3Iface";
constexpr auto SM_POLL_INTERVAL = std::chrono::milliseconds(10);
}

E3Interface::E3Interface(const E3Config& config)
    : config_(config)
{
    Logger::instance().set_level(config.log_level);
    E3_LOG_INFO(LOG_TAG) << "E3Interface created";
}

E3Interface::~E3Interface() {
    stop();
    E3_LOG_INFO(LOG_TAG) << "E3Interface destroyed";
}

ErrorCode E3Interface::init() {
    if (state_.load() != AgentState::UNINITIALIZED) {
        E3_LOG_WARN(LOG_TAG) << "Interface already initialized";
        return ErrorCode::ALREADY_INITIALIZED;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Initializing E3Interface";
    
    // Create encoder
    encoder_ = create_encoder(config_.encoding);
    if (!encoder_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create encoder";
        return ErrorCode::INTERNAL_ERROR;
    }
    
    // Create subscription manager
    subscription_manager_ = std::make_unique<SubscriptionManager>();
    
    // Set up SM lifecycle callback
    subscription_manager_->set_sm_lifecycle_callback(
        [this](uint32_t ran_function_id, bool should_start) {
            on_sm_lifecycle_change(ran_function_id, should_start);
        }
    );
    
    // Create response queue
    response_queue_ = std::make_unique<ResponseQueue>();
    
    // Create connector
    connector_ = create_connector(
        config_.link_layer,
        config_.transport_layer,
        config_.setup_endpoint,
        config_.subscriber_endpoint,
        config_.publisher_endpoint,
        config_.io_threads
    );
    
    if (!connector_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create connector";
        return ErrorCode::INTERNAL_ERROR;
    }
    
    state_.store(AgentState::INITIALIZED);
    E3_LOG_INFO(LOG_TAG) << "E3Interface initialized successfully";
    
    return ErrorCode::SUCCESS;
}

ErrorCode E3Interface::start() {
    AgentState expected = AgentState::INITIALIZED;
    if (!state_.compare_exchange_strong(expected, AgentState::CONNECTING)) {
        E3_LOG_ERROR(LOG_TAG) << "Cannot start from state: " 
                              << agent_state_to_string(expected);
        return ErrorCode::STATE_ERROR;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Starting E3Interface";
    should_stop_.store(false);
    
    // Set up initial connection
    ErrorCode conn_result = connector_->setup_initial_connection();
    if (conn_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup initial connection";
        state_.store(AgentState::ERROR);
        return conn_result;
    }
    
    state_.store(AgentState::CONNECTED);
    
    // Start threads
    subscriber_thread_ = std::make_unique<std::thread>(&E3Interface::subscriber_loop, this);
    publisher_thread_ = std::make_unique<std::thread>(&E3Interface::publisher_loop, this);
    sm_data_thread_ = std::make_unique<std::thread>(&E3Interface::sm_data_handler_loop, this);
    setup_thread_ = std::make_unique<std::thread>(&E3Interface::setup_loop, this);
    
    state_.store(AgentState::RUNNING);
    E3_LOG_INFO(LOG_TAG) << "E3Interface started successfully";
    
    return ErrorCode::SUCCESS;
}

void E3Interface::stop() {
    if (state_.load() == AgentState::UNINITIALIZED ||
        state_.load() == AgentState::STOPPING) {
        return;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Stopping E3Interface";
    state_.store(AgentState::STOPPING);
    should_stop_.store(true);
    
    // Wake up response queue
    if (response_queue_) {
        response_queue_->shutdown();
    }
    
    // Join threads
    if (setup_thread_ && setup_thread_->joinable()) {
        setup_thread_->join();
    }
    if (subscriber_thread_ && subscriber_thread_->joinable()) {
        subscriber_thread_->join();
    }
    if (publisher_thread_ && publisher_thread_->joinable()) {
        publisher_thread_->join();
    }
    if (sm_data_thread_ && sm_data_thread_->joinable()) {
        sm_data_thread_->join();
    }
    
    // Clean up SM registry
    SmRegistry::instance().clear();
    
    // Dispose connector
    if (connector_) {
        connector_->dispose();
    }
    
    state_.store(AgentState::INITIALIZED);
    E3_LOG_INFO(LOG_TAG) << "E3Interface stopped";
}

ErrorCode E3Interface::queue_outbound(Pdu pdu) {
    if (!response_queue_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    return response_queue_->push(pdu);
}

std::vector<uint32_t> E3Interface::get_available_ran_functions() const {
    return SmRegistry::instance().get_available_ran_functions();
}

ErrorCode E3Interface::register_sm(std::unique_ptr<ServiceModel> sm) {
    return SmRegistry::instance().register_sm(std::move(sm));
}

// =========================================================================
// Thread Entry Points
// =========================================================================

void E3Interface::setup_loop() {
    E3_LOG_INFO(LOG_TAG) << "Setup loop started";
    
    auto available_ran_functions = SmRegistry::instance().get_available_ran_functions();
    
    while (!should_stop_.load()) {
        std::vector<uint8_t> buffer;
        int ret = connector_->recv_setup_request(buffer);
        
        if (ret <= 0) {
            if (should_stop_.load()) break;
            E3_LOG_DEBUG(LOG_TAG) << "No setup request received";
            continue;
        }
        
        // Decode the setup request
        auto decode_result = encoder_->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to decode setup request";
            continue;
        }
        
        Pdu& pdu = *decode_result;
        if (pdu.type != PduType::SETUP_REQUEST) {
            E3_LOG_ERROR(LOG_TAG) << "Unexpected PDU type in setup: " 
                                  << pdu_type_to_string(pdu.type);
            continue;
        }
        
        auto* request = std::get_if<SetupRequest>(&pdu.choice);
        if (!request) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to get SetupRequest from PDU";
            continue;
        }
        
        handle_setup_request(*request);
    }
    
    E3_LOG_INFO(LOG_TAG) << "Setup loop stopped";
}

void E3Interface::subscriber_loop() {
    E3_LOG_INFO(LOG_TAG) << "Subscriber loop started";
    
    ErrorCode result = connector_->setup_inbound_connection();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup inbound connection";
        return;
    }
    
    std::vector<uint8_t> buffer;
    
    while (!should_stop_.load()) {
        int ret = connector_->receive(buffer);
        
        if (ret <= 0) {
            if (should_stop_.load()) break;
            continue;
        }
        
        auto decode_result = encoder_->decode(buffer.data(), ret);
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to decode PDU in subscriber";
            continue;
        }
        
        Pdu& pdu = *decode_result;
        
        switch (pdu.type) {
            case PduType::SUBSCRIPTION_REQUEST: {
                auto* request = std::get_if<SubscriptionRequest>(&pdu.choice);
                if (request) {
                    handle_subscription_request(*request);
                }
                break;
            }
            
            case PduType::CONTROL_ACTION: {
                auto* action = std::get_if<ControlAction>(&pdu.choice);
                if (action) {
                    handle_control_action(*action);
                }
                break;
            }
            
            case PduType::DAPP_REPORT: {
                auto* report = std::get_if<DAppReport>(&pdu.choice);
                if (report) {
                    handle_dapp_report(*report);
                }
                break;
            }
            
            default:
                E3_LOG_WARN(LOG_TAG) << "Received unexpected PDU type: " 
                                     << pdu_type_to_string(pdu.type);
                break;
        }
    }
    
    E3_LOG_INFO(LOG_TAG) << "Subscriber loop stopped";
}

void E3Interface::publisher_loop() {
    E3_LOG_INFO(LOG_TAG) << "Publisher loop started";
    
    // Ignore SIGPIPE to handle closed connections gracefully
    signal(SIGPIPE, SIG_IGN);
    
    // Retry outbound connection setup
    ErrorCode result;
    do {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (should_stop_.load()) return;
        
        E3_LOG_INFO(LOG_TAG) << "Trying to setup outbound connection";
        result = connector_->setup_outbound_connection();
    } while (result != ErrorCode::SUCCESS && !should_stop_.load());
    
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup outbound connection";
        return;
    }
    E3_LOG_INFO(LOG_TAG) << "Outbound connection established";
    
    while (!should_stop_.load()) {
        // Pop from queue (blocking)
        auto pdu_opt = response_queue_->pop(std::chrono::milliseconds(100));
        
        if (!pdu_opt) {
            continue;
        }
        
        // Encode and send
        auto encode_result = encoder_->encode(*pdu_opt);
        if (!encode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to encode PDU for sending";
            continue;
        }
        
        ErrorCode send_result = connector_->send(encode_result->buffer);
        if (send_result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to send PDU";
        } else {
            E3_LOG_DEBUG(LOG_TAG) << "Sent PDU: " << pdu_type_to_string(pdu_opt->type);
        }
    }
    
    E3_LOG_INFO(LOG_TAG) << "Publisher loop stopped";
}

void E3Interface::sm_data_handler_loop() {
    E3_LOG_INFO(LOG_TAG) << "SM data handler started";
    
    while (!should_stop_.load()) {
        bool data_processed = false;
        
        // Get active RAN functions
        auto ran_functions = subscription_manager_->get_active_ran_functions();
        
        if (ran_functions.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        for (uint32_t ran_func : ran_functions) {
            // Get subscribers for this RAN function
            auto subscribers = subscription_manager_->get_subscribed_dapps(ran_func);
            
            if (subscribers.empty()) {
                continue;
            }
            
            // Get SM for this RAN function
            ServiceModel* sm = SmRegistry::instance().get_by_ran_function(ran_func);
            
            if (!sm || !sm->is_running()) {
                continue;
            }
            
            // In a full implementation, we would poll the SM for indication data here
            // For now, indication data is delivered through callbacks
        }
        
        if (!data_processed) {
            std::this_thread::sleep_for(SM_POLL_INTERVAL);
        }
    }
    
    E3_LOG_INFO(LOG_TAG) << "SM data handler stopped";
}

// =========================================================================
// Message Handlers
// =========================================================================

void E3Interface::handle_setup_request(const SetupRequest& request) {
    E3_LOG_INFO(LOG_TAG) << "Handling setup request from dApp '" << request.dapp_name 
                         << "' (version=" << request.dapp_version 
                         << ", vendor=" << request.vendor 
                         << ", e3ap_version=" << request.e3ap_protocol_version << ")";
    
    ResponseCode response_code = ResponseCode::NEGATIVE;
    uint32_t assigned_dapp_id = 0;
    
    // Register the dApp
    auto [result, dapp_id] = subscription_manager_->register_dapp();
    assigned_dapp_id = dapp_id;
    
    if (result == ErrorCode::SUCCESS) {
        response_code = ResponseCode::POSITIVE;
        E3_LOG_INFO(LOG_TAG) << "dApp '" << request.dapp_name << "' registered with assigned ID " << assigned_dapp_id;
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to register dApp '" << request.dapp_name << "': " << error_code_to_string(result);
    }
    
    // Create and send response
    // Get available RAN functions and convert to RanFunctionDef list
    auto available_ran_function_ids = SmRegistry::instance().get_available_ran_functions();
    std::vector<RanFunctionDef> ran_function_list;
    for (auto id : available_ran_function_ids) {
        RanFunctionDef func;
        func.ran_function_identifier = id;
        // TODO: Get actual RAN function data from SM registry
        ran_function_list.push_back(func);
    }
    
    auto encode_result = encoder_->encode_setup_response(
        request.id,
        response_code,
        std::nullopt,  // e3ap_protocol_version
        assigned_dapp_id,  // dapp_identifier
        ran_function_list
    );
    
    if (!encode_result) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to encode setup response";
        return;
    }
    
    ErrorCode send_result = connector_->send_response(encode_result->buffer);
    if (send_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send setup response";
    } else {
        E3_LOG_INFO(LOG_TAG) << "Sent setup response";
    }
}

void E3Interface::handle_subscription_request(const SubscriptionRequest& request) {
    E3_LOG_INFO(LOG_TAG) << "Handling subscription request from dApp " << request.dapp_identifier
                         << " for RAN function " << request.ran_function_identifier
                         << " (action=" << action_type_to_string(request.type) << ")";
    
    ResponseCode response_code = ResponseCode::NEGATIVE;
    
    switch (request.type) {
        case ActionType::INSERT: {
            // Check if dApp is registered
            if (!subscription_manager_->is_dapp_registered(request.dapp_identifier)) {
                E3_LOG_ERROR(LOG_TAG) << "dApp " << request.dapp_identifier << " not registered";
                break;
            }
            
            ErrorCode result = subscription_manager_->add_subscription(
                request.dapp_identifier,
                request.ran_function_identifier
            );
            
            if (result == ErrorCode::SUCCESS || result == ErrorCode::SUBSCRIPTION_EXISTS) {
                response_code = ResponseCode::POSITIVE;
                E3_LOG_INFO(LOG_TAG) << "Subscription added: dApp " << request.dapp_identifier
                                     << " -> RAN function " << request.ran_function_identifier;
            } else {
                E3_LOG_ERROR(LOG_TAG) << "Failed to add subscription: " 
                                      << error_code_to_string(result);
            }
            break;
        }
        
        case ActionType::DELETE: {
            ErrorCode result = subscription_manager_->remove_subscription(
                request.dapp_identifier,
                request.ran_function_identifier
            );
            
            if (result == ErrorCode::SUCCESS) {
                response_code = ResponseCode::POSITIVE;
                E3_LOG_INFO(LOG_TAG) << "Subscription removed: dApp " << request.dapp_identifier
                                     << " -> RAN function " << request.ran_function_identifier;
            } else {
                E3_LOG_ERROR(LOG_TAG) << "Failed to remove subscription: "
                                      << error_code_to_string(result);
            }
            break;
        }
        
        default:
            E3_LOG_WARN(LOG_TAG) << "Unsupported action type in subscription request";
            break;
    }
    
    // Create and queue response
    Pdu response_pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.id = 0; // Will be set by encoder
    resp.request_id = request.id;
    resp.response_code = response_code;
    response_pdu.choice = resp;
    
    queue_outbound(std::move(response_pdu));
}

void E3Interface::handle_control_action(const ControlAction& action) {
    E3_LOG_INFO(LOG_TAG) << "Handling control action from dApp " << action.dapp_identifier
                         << " for RAN function " << action.ran_function_identifier
                         << " (" << action.action_data.size() << " bytes)";
    
    // Find SM for this RAN function
    ServiceModel* sm = SmRegistry::instance().get_by_ran_function(action.ran_function_identifier);
    
    if (sm && sm->is_running()) {
        ErrorCode result = sm->process_control_action(
            action.ran_function_identifier,
            action.action_data
        );
        
        if (result == ErrorCode::SUCCESS) {
            E3_LOG_INFO(LOG_TAG) << "Control action processed by SM";
        } else {
            E3_LOG_ERROR(LOG_TAG) << "SM failed to process control action: "
                                  << error_code_to_string(result);
        }
    } else {
        E3_LOG_ERROR(LOG_TAG) << "No running SM found for RAN function " 
                              << action.ran_function_identifier;
    }
    
    // Also invoke user callback if set
    if (control_action_handler_) {
        control_action_handler_(action);
    }
}

void E3Interface::handle_dapp_report(const DAppReport& report) {
    E3_LOG_INFO(LOG_TAG) << "Handling dApp report from dApp " << report.dapp_identifier
                         << " for RAN function " << report.ran_function_identifier;
    
    // Forward to callback if set
    if (dapp_report_handler_) {
        dapp_report_handler_(report);
    }
}

void E3Interface::handle_dapp_disconnection(uint32_t dapp_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling dApp disconnection: " << dapp_id;
    
    // Get subscriptions before unregistering
    auto subscriptions = subscription_manager_->get_dapp_subscriptions(dapp_id);
    
    // Unregister dApp (this will also clean up subscriptions)
    ErrorCode result = subscription_manager_->unregister_dapp(dapp_id);
    
    if (result == ErrorCode::SUCCESS) {
        E3_LOG_INFO(LOG_TAG) << "dApp " << dapp_id << " unregistered, " 
                             << subscriptions.size() << " subscriptions removed";
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to unregister dApp " << dapp_id << ": "
                              << error_code_to_string(result);
    }
}

// =========================================================================
// SM Lifecycle Management
// =========================================================================

void E3Interface::on_sm_lifecycle_change(uint32_t ran_function_id, bool should_start) {
    if (should_start) {
        ErrorCode result = SmRegistry::instance().start_sm(ran_function_id);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to start SM for RAN function " 
                                  << ran_function_id << ": " << error_code_to_string(result);
        }
    } else {
        ErrorCode result = SmRegistry::instance().stop_sm(ran_function_id);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_WARN(LOG_TAG) << "Failed to stop SM for RAN function " 
                                 << ran_function_id << ": " << error_code_to_string(result);
        }
    }
}

} // namespace libe3
