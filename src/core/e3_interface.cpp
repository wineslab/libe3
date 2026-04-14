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
#include <random>
#include <signal.h>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "E3Iface";
constexpr auto SM_POLL_INTERVAL = std::chrono::milliseconds(10);

/**
 * @brief Apply CPU-affinity and niceness to the calling thread.
 *
 * Invoked at the very start of each I/O thread so that the settings take
 * effect before any real work begins.
 *
 * @param affinity  Logical CPU core to pin to, or -1 to skip pinning.
 * @param niceness  Nice value in [-20, 19], or 0 to skip.
 */
void apply_thread_config(int affinity, int niceness) noexcept {
#ifdef __linux__
    if (affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<size_t>(affinity), &cpuset);
        // Non-fatal if this fails (e.g. core index out of range).
        (void)pthread_setaffinity_np(pthread_self(),
                                     sizeof(cpu_set_t), &cpuset);
    }
    if (niceness != 0) {
        // Use the kernel TID so only this thread's scheduling priority changes.
        // syscall(SYS_gettid) returns the kernel thread ID as a long; cast it
        // to pid_t first, then to the id_t expected by setpriority(PRIO_PROCESS).
        auto tid = static_cast<id_t>(static_cast<pid_t>(syscall(SYS_gettid)));
        // Non-fatal: negative values require CAP_SYS_NICE.
        (void)setpriority(PRIO_PROCESS, tid, niceness);
    }
#else
    (void)affinity;
    (void)niceness;
#endif
}
} // anonymous namespace

uint32_t E3Interface::generate_message_id() {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(1, 1000);
    return dist(rng);
}

E3Interface::E3Interface(const E3Config& config)
    : config_(config)
{
    Logger::instance().set_log_file("/tmp/e3_agent.log");
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
        config_.setup_port,
        config_.subscriber_port,
        config_.publisher_port,
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
    setup_thread_ = std::make_unique<std::thread>(&E3Interface::setup_loop, this);
    subscriber_thread_ = std::make_unique<std::thread>(&E3Interface::subscriber_loop, this);
    publisher_thread_ = std::make_unique<std::thread>(&E3Interface::publisher_loop, this);
    
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
    
    // Interrupt blocking socket operations
    if (connector_) {
        connector_->shutdown();
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
    return response_queue_->push(std::move(pdu));
}

std::vector<uint32_t> E3Interface::get_available_ran_functions() const {
    return SmRegistry::instance().get_available_ran_functions();
}

ErrorCode E3Interface::register_sm(std::unique_ptr<ServiceModel> sm) {
    if (!sm) {
        return ErrorCode::INVALID_PARAM;
    }
    const uint32_t ran_function_id = sm->ran_function_id();
    sm->set_subscribers_provider([this, ran_function_id]() {
        if (!subscription_manager_) {
            return std::vector<uint32_t>{};
        }
        return subscription_manager_->get_subscribed_dapps(ran_function_id);
    });
    sm->set_outbound_emitter([this](Pdu&& pdu) {
        if (pdu.message_id == 0) {
            pdu.message_id = generate_message_id();
        }
        return queue_outbound(std::move(pdu));
    });
    return SmRegistry::instance().register_sm(std::move(sm));
}

void E3Interface::notify_dapp_status_changed() {
    if (dapp_status_changed_handler_) {
        dapp_status_changed_handler_();
    }
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
            continue;
        }

        E3_LOG_INFO(LOG_TAG) << "Setup request received: " << ret << " bytes";
        
        // Decode the setup request
        auto decode_result = encoder_->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to decode setup request; ret=" << ret;
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
        
        handle_setup_request(*request, pdu.message_id);
    }
    
    E3_LOG_INFO(LOG_TAG) << "Setup loop stopped";
}

void E3Interface::subscriber_loop() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
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
        
        auto decode_result = encoder_->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to decode PDU in subscriber";
            continue;
        }
        
        Pdu& pdu = *decode_result;
        
        switch (pdu.type) {
            case PduType::SUBSCRIPTION_REQUEST: {
                auto* request = std::get_if<SubscriptionRequest>(&pdu.choice);
                if (request) {
                    handle_subscription_request(*request, pdu.message_id);
                }
                break;
            }
            
            case PduType::SUBSCRIPTION_DELETE: {
                auto* del = std::get_if<SubscriptionDelete>(&pdu.choice);
                if (del) {
                    handle_subscription_delete(*del, pdu.message_id);
                }
                break;
            }
            
            case PduType::DAPP_CONTROL_ACTION: {
                auto* action = std::get_if<DAppControlAction>(&pdu.choice);
                if (action) {
                    handle_control_action(*action, pdu.message_id);
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
            
            case PduType::RELEASE_MESSAGE: {
                auto* release = std::get_if<ReleaseMessage>(&pdu.choice);
                if (release) {
                    handle_release_message(*release);
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
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
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
        auto pdu_opt = response_queue_->pop(std::chrono::milliseconds(10));
        
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

void E3Interface::handle_setup_request(const SetupRequest& request, uint32_t request_message_id) {
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
        notify_dapp_status_changed();
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to register dApp '" << request.dapp_name << "': " << error_code_to_string(result);
    }
    
    // Create and send response
    // Get available RAN functions and convert to RanFunctionDef list
    auto available_ran_function_ids = SmRegistry::instance().get_available_ran_functions();
    E3_LOG_DEBUG(LOG_TAG) << "Available RAN function ids count: " << available_ran_function_ids.size();
    std::vector<RanFunctionDef> ran_function_list;
    for (auto id : available_ran_function_ids) {
        RanFunctionDef func;
        func.ran_function_identifier = id;
        ServiceModel* sm = SmRegistry::instance().get_by_ran_function(id);
        if (sm) {
            func.telemetry_identifier_list = sm->telemetry_ids();
            func.control_identifier_list = sm->control_ids();
            // Include optional RAN-function-specific opaque data provided by the SM
            func.ran_function_data = sm->ran_function_data();
        }
        ran_function_list.push_back(func);
    }
    E3_LOG_DEBUG(LOG_TAG) << "Constructed ran_function_list size: " << ran_function_list.size();
    for (const auto &rf : ran_function_list) {
        E3_LOG_DEBUG(LOG_TAG) << "  RAN function id=" << rf.ran_function_identifier
                               << ", telemetry_count=" << rf.telemetry_identifier_list.size()
                               << ", control_count=" << rf.control_identifier_list.size()
                               << ", ran_function_data_len=" << rf.ran_function_data.size();
    }
    
        auto encode_result = encoder_->encode_setup_response(
        generate_message_id(),
        request_message_id,
        response_code,
        config_.e3ap_version,
        assigned_dapp_id,  // dapp_identifier
        config_.ran_identifier,  // ran_identifier
        ran_function_list
    );
    
    if (!encode_result) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to encode setup response for request id " << request_message_id;
        return;
    }
    
    ErrorCode send_result = connector_->send_response(encode_result->buffer);
    if (send_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send setup response for request id " << request_message_id
                              << "; error=" << error_code_to_string(send_result);
    } else {
        E3_LOG_INFO(LOG_TAG) << "Sent setup response for request id " << request_message_id;
    }
}

void E3Interface::handle_subscription_request(const SubscriptionRequest& request, uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling subscription request from dApp " << request.dapp_identifier
                         << " for RAN function " << request.ran_function_identifier;
    
    ResponseCode response_code = ResponseCode::NEGATIVE;
    uint32_t subscription_id = 1;
    
    // Check if dApp is registered
    if (!subscription_manager_->is_dapp_registered(request.dapp_identifier)) {
        E3_LOG_ERROR(LOG_TAG) << "dApp " << request.dapp_identifier << " not registered";
    } else {
        auto [result, sub_id] = subscription_manager_->add_subscription(
            request.dapp_identifier,
            request.ran_function_identifier
        );
        subscription_id = sub_id;
        
        if (result == ErrorCode::SUCCESS || result == ErrorCode::SUBSCRIPTION_EXISTS) {
            response_code = ResponseCode::POSITIVE;
            E3_LOG_INFO(LOG_TAG) << "Subscription added: dApp " << request.dapp_identifier
                                 << " -> RAN function " << request.ran_function_identifier
                                 << " (subscription_id=" << subscription_id << ")";
            if (result == ErrorCode::SUCCESS) {
                notify_dapp_status_changed();
            }
        } else {
            E3_LOG_ERROR(LOG_TAG) << "Failed to add subscription: " 
                                  << error_code_to_string(result);
        }
    }
    
    // Create and queue response
    Pdu response_pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    response_pdu.message_id = generate_message_id();
    resp.request_id = request_message_id;
    resp.dapp_identifier = request.dapp_identifier;
    resp.response_code = response_code;
    if (response_code == ResponseCode::POSITIVE) {
        resp.subscription_id = subscription_id;
    }
    response_pdu.choice = resp;
    
    queue_outbound(std::move(response_pdu));
}

void E3Interface::handle_subscription_delete(const SubscriptionDelete& del, uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling subscription delete from dApp " << del.dapp_identifier
                         << " for subscription " << del.subscription_id;
    
    ResponseCode response_code = ResponseCode::NEGATIVE;
    
    ErrorCode result = subscription_manager_->remove_subscription_by_id(
        del.dapp_identifier,
        del.subscription_id
    );
    
    if (result == ErrorCode::SUCCESS) {
        response_code = ResponseCode::POSITIVE;
        E3_LOG_INFO(LOG_TAG) << "Subscription " << del.subscription_id << " removed for dApp " << del.dapp_identifier;
        notify_dapp_status_changed();
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to remove subscription: "
                              << error_code_to_string(result);
    }
    
    // Create and queue ack response
    Pdu response_pdu(PduType::MESSAGE_ACK);
    MessageAck ack;
    response_pdu.message_id = generate_message_id();
    ack.request_id = request_message_id;
    ack.response_code = response_code;
    response_pdu.choice = ack;
    
    queue_outbound(std::move(response_pdu));
}

void E3Interface::handle_control_action(const DAppControlAction& action, uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling control action from dApp " << action.dapp_identifier
                         << " for RAN function " << action.ran_function_identifier
                         << " control " << action.control_identifier
                         << " (" << action.action_data.size() << " bytes)";

    ServiceModel* sm = SmRegistry::instance().get_by_ran_function(action.ran_function_identifier);

    if (sm && sm->is_running()) {
        ErrorCode result = sm->handle_control_action(request_message_id, action);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "SM failed to process control action: "
                                  << error_code_to_string(result);
            return;
        }
        E3_LOG_INFO(LOG_TAG) << "Control action processed by SM";
    } else {
        E3_LOG_ERROR(LOG_TAG) << "No running SM found for RAN function " 
                              << action.ran_function_identifier;
        return;
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

void E3Interface::handle_release_message(const ReleaseMessage& release) {
    E3_LOG_INFO(LOG_TAG) << "Handling release message from dApp " << release.dapp_identifier;

    handle_dapp_disconnection(release.dapp_identifier);
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
        notify_dapp_status_changed();
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
