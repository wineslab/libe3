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
#include <cctype>
#include <chrono>
#include <random>
#include <signal.h>
#include <string>

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
/**
 * @brief Build the default log-file path for a given role/identifier.
 *
 * Produces `/tmp/e3_<role>[_<sanitized-id>]_<euid>.log`. The id is sanitized
 * to `[A-Za-z0-9._-]` (other bytes become '_') and omitted when empty. The
 * effective uid is appended so the file is always owned by the opener,
 * sidestepping `fs.protected_regular` denials in sticky /tmp.
 */
std::string default_log_path(E3Role role, const std::string& id) {
    std::string san;
    san.reserve(id.size());
    for (char c : id) {
        san.push_back((std::isalnum(static_cast<unsigned char>(c)) ||
                       c == '.' || c == '_' || c == '-')
                          ? c
                          : '_');
    }
    std::string path = "/tmp/e3_";
    path += (role == E3Role::DAPP) ? "dapp" : "agent";
    if (!san.empty()) {
        path += '_';
        path += san;
    }
#ifdef __linux__
    path += '_';
    path += std::to_string(static_cast<unsigned long>(geteuid()));
#endif
    path += ".log";
    return path;
}

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
    // Suppress shared log-file writes when logging is disabled. This lets
    // multiple E3Interface instances (e.g. a RAN-role and a dApp-role one in
    // the same process — integration tests, the latency benchmark, or
    // production multi-peer dApps) coexist without racing on the same file.
    if (config.log_level > 0) {
        std::string log_path = config.log_path;
        if (log_path.empty()) {
            const std::string& id = (config.role == E3Role::DAPP)
                ? config.dapp_name
                : config.ran_identifier;
            log_path = default_log_path(config.role, id);
        }
        Logger::instance().set_log_file(log_path);
    }
    Logger::instance().set_level(config.log_level);
    E3_LOG_INFO(LOG_TAG) << "E3Interface created (role=" << role_to_string(config.role) << ")";
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
    
    // Allocate role-specific state. Exactly one is non-null.
    if (config_.role == E3Role::RAN) {
        subscription_manager_ = std::make_unique<SubscriptionManager>();
        subscription_manager_->set_sm_lifecycle_callback(
            [this](uint32_t ran_function_id, bool should_start) {
                on_sm_lifecycle_change(ran_function_id, should_start);
            }
        );
    } else {
        dapp_state_ = std::make_unique<DAppSubscriptionState>();
    }

    // Create response queue
    response_queue_ = std::make_unique<LockFreeQueue<Pdu>>();

    // Create dApp-report queue (RAN side drains it via the report worker)
    report_queue_ = std::make_unique<LockFreeQueue<DAppReport>>(1024);

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
        config_.io_threads,
        config_.role
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
    
    E3_LOG_INFO(LOG_TAG) << "Starting E3Interface (role=" << role_to_string(config_.role) << ")";
    should_stop_.store(false);

    // Set up the setup channel. RAN binds; dApp connects.
    ErrorCode conn_result = (config_.role == E3Role::RAN)
        ? connector_->setup_initial_connection()
        : connector_->setup_initial_connection_client();
    if (conn_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup initial connection: "
                              << error_code_to_string(conn_result);
        state_.store(AgentState::ERROR);
        return conn_result;
    }

    state_.store(AgentState::CONNECTED);

    // Start threads. RAN runs setup+inbound+outbound+sm_data; dApp runs
    // setup+inbound+outbound (no sm_data — dApps don't host SMs).
    if (config_.role == E3Role::RAN) {
        setup_thread_ = std::make_unique<std::thread>(&E3Interface::setup_loop_ran, this);
        inbound_thread_ = std::make_unique<std::thread>(&E3Interface::inbound_loop_ran, this);
        outbound_thread_ = std::make_unique<std::thread>(&E3Interface::outbound_loop_ran, this);
        // RAN receives dApp reports; drain them off the inbound thread.
        report_worker_thread_ = std::make_unique<std::thread>(&E3Interface::report_worker_loop, this);
    } else {
        setup_thread_ = std::make_unique<std::thread>(&E3Interface::setup_loop_dapp, this);
        inbound_thread_ = std::make_unique<std::thread>(&E3Interface::inbound_loop_dapp, this);
        outbound_thread_ = std::make_unique<std::thread>(&E3Interface::outbound_loop_dapp, this);
    }

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

    // Wake up the report queue so the report worker's blocking pop returns.
    if (report_queue_) {
        report_queue_->shutdown();
    }

    // Interrupt blocking socket operations
    if (connector_) {
        connector_->shutdown();
    }
    
    // Join threads
    if (setup_thread_ && setup_thread_->joinable()) {
        setup_thread_->join();
    }
    if (inbound_thread_ && inbound_thread_->joinable()) {
        inbound_thread_->join();
    }
    if (outbound_thread_ && outbound_thread_->joinable()) {
        outbound_thread_->join();
    }
    if (sm_data_thread_ && sm_data_thread_->joinable()) {
        sm_data_thread_->join();
    }
    if (report_worker_thread_ && report_worker_thread_->joinable()) {
        report_worker_thread_->join();
    }

    // Wake up anyone blocked in wait_for_setup so they don't hang.
    {
        std::lock_guard<std::mutex> lk(setup_complete_mu_);
        setup_complete_ = true;
    }
    setup_complete_cv_.notify_all();
    
    // Clean up SM registry — only the RAN role owns SMs. Doing this
    // unconditionally would wipe a sibling RAN-role E3Interface's registered
    // SMs in a two-roles-in-one-process scenario (integration tests, the
    // latency benchmark, multi-peer dApps colocated with a RAN). The dApp
    // role never registers an SM, so there's nothing to clear.
    if (config_.role == E3Role::RAN) {
        SmRegistry::instance().clear();
    }
    
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

void E3Interface::setup_loop_ran() {
    E3_LOG_INFO(LOG_TAG) << "Setup loop (RAN) started";
    
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
    
    E3_LOG_INFO(LOG_TAG) << "Setup loop (RAN) stopped";
}

void E3Interface::inbound_loop_ran() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    E3_LOG_INFO(LOG_TAG) << "Inbound loop (RAN) started";

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
                    if (report_queue_) {
                        // Hand off to the report worker so downstream work
                        // never blocks the inbound read path. The queue logs
                        // on overflow; surface it here as an error too.
                        if (report_queue_->push(std::move(*report)) != ErrorCode::SUCCESS) {
                            E3_LOG_ERROR(LOG_TAG) << "Report queue full — dropping dApp report";
                        }
                    }
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
    
    E3_LOG_INFO(LOG_TAG) << "Inbound loop (RAN) stopped";
}

void E3Interface::outbound_loop_ran() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    E3_LOG_INFO(LOG_TAG) << "Outbound loop (RAN) started";

    // Ignore SIGPIPE to handle closed connections gracefully
    signal(SIGPIPE, SIG_IGN);

    // Set up the outbound (PUB) socket. Try immediately; retry with backoff
    // only on failure. Putting an unconditional sleep before the first
    // attempt introduces a multi-second startup latency that breaks
    // integration tests and any colocated RAN/dApp pair.
    ErrorCode result = connector_->setup_outbound_connection();
    while (result != ErrorCode::SUCCESS && !should_stop_.load()) {
        E3_LOG_WARN(LOG_TAG) << "Outbound connection setup failed ("
                             << error_code_to_string(result) << "), retrying in 1s";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (should_stop_.load()) return;
        result = connector_->setup_outbound_connection();
    }

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
    
    E3_LOG_INFO(LOG_TAG) << "Outbound loop (RAN) stopped";
}

void E3Interface::report_worker_loop() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    E3_LOG_INFO(LOG_TAG) << "Report worker loop started";

    while (!should_stop_.load()) {
        // Blocking pop with the queue's adaptive spin-wait, mirroring
        // outbound_loop_ran's use of response_queue_.
        auto report_opt = report_queue_->pop(std::chrono::milliseconds(10));
        if (report_opt) {
            handle_dapp_report(*report_opt);
        }
    }

    // Drain anything left after shutdown was signalled.
    while (auto report_opt = report_queue_->try_pop()) {
        handle_dapp_report(*report_opt);
    }

    E3_LOG_INFO(LOG_TAG) << "Report worker loop stopped";
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
        uint32_t period = request.periodicity.value_or(0);
        auto [result, sub_id] = subscription_manager_->add_subscription(
            request.dapp_identifier,
            request.ran_function_identifier,
            request.telemetry_identifier_list,
            request.control_identifier_list,
            period
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
    
    // Create and queue response
    Pdu response_pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    response_pdu.message_id = generate_message_id();
    resp.request_id = request_message_id;
    resp.dapp_identifier = del.dapp_identifier;
    resp.response_code = response_code;
    if (response_code == ResponseCode::POSITIVE) {
        resp.subscription_id = del.subscription_id;
    }
    response_pdu.choice = resp;
    
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

// ===========================================================================
// dApp-role thread bodies
// ===========================================================================

void E3Interface::setup_loop_dapp() {
    E3_LOG_INFO(LOG_TAG) << "Setup loop (dApp) started";

    // Encode the SetupRequest from config_'s dApp identification fields.
    auto enc = encoder_->encode_setup_request(
        generate_message_id(),
        config_.e3ap_version,
        config_.dapp_name,
        config_.dapp_version,
        config_.vendor
    );
    if (!enc) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to encode SetupRequest";
        std::lock_guard<std::mutex> lk(setup_complete_mu_);
        setup_complete_ = true;
        setup_succeeded_ = false;
        setup_complete_cv_.notify_all();
        return;
    }

    // Lazy-pirate REQ: send, wait for reply with timeout, retry on no reply.
    constexpr int MAX_RETRIES = 5;
    int retries = 0;
    bool got_response = false;
    while (!should_stop_.load() && retries < MAX_RETRIES) {
        ErrorCode rc = connector_->send_setup_request_client(enc->buffer);
        if (rc != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "send_setup_request_client failed: "
                                  << error_code_to_string(rc);
            ++retries;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Try a few times to receive (connector recv has its own short timeout).
        for (int attempt = 0; attempt < 4 && !should_stop_.load(); ++attempt) {
            std::vector<uint8_t> resp_buf;
            int n = connector_->recv_setup_response_client(resp_buf);
            if (n > 0) {
                auto decoded = encoder_->decode(resp_buf.data(), static_cast<size_t>(n));
                if (!decoded) {
                    E3_LOG_ERROR(LOG_TAG) << "Failed to decode SetupResponse";
                    break;
                }
                if (decoded->type != PduType::SETUP_RESPONSE) {
                    E3_LOG_ERROR(LOG_TAG) << "Unexpected PDU on setup channel: "
                                          << pdu_type_to_string(decoded->type);
                    break;
                }
                auto* resp = std::get_if<SetupResponse>(&decoded->choice);
                if (resp) {
                    handle_setup_response(*resp);
                    got_response = true;
                }
                break;
            }
            if (n < 0) {
                E3_LOG_ERROR(LOG_TAG) << "recv_setup_response_client error";
                break;
            }
        }
        if (got_response) break;
        ++retries;
    }

    {
        std::lock_guard<std::mutex> lk(setup_complete_mu_);
        setup_complete_ = true;
        setup_succeeded_ = got_response;
    }
    setup_complete_cv_.notify_all();

    E3_LOG_INFO(LOG_TAG) << "Setup loop (dApp) finished, success="
                         << (got_response ? "yes" : "no");
}

void E3Interface::inbound_loop_dapp() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    E3_LOG_INFO(LOG_TAG) << "Inbound loop (dApp) started";

    ErrorCode result = connector_->setup_inbound_connection_client();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup inbound (dApp) connection: "
                              << error_code_to_string(result);
        return;
    }

    std::vector<uint8_t> buffer;
    while (!should_stop_.load()) {
        int ret = connector_->receive(buffer);
        if (ret <= 0) {
            if (should_stop_.load()) break;
            continue;
        }
        auto decoded = encoder_->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decoded) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to decode PDU in dApp inbound";
            continue;
        }
        Pdu& pdu = *decoded;
        switch (pdu.type) {
            case PduType::SUBSCRIPTION_RESPONSE: {
                auto* r = std::get_if<SubscriptionResponse>(&pdu.choice);
                if (r) handle_subscription_response(*r);
                break;
            }
            case PduType::INDICATION_MESSAGE: {
                auto* m = std::get_if<IndicationMessage>(&pdu.choice);
                if (m) handle_indication(*m);
                break;
            }
            case PduType::XAPP_CONTROL_ACTION: {
                auto* a = std::get_if<XAppControlAction>(&pdu.choice);
                if (a) handle_xapp_control_action(*a);
                break;
            }
            case PduType::MESSAGE_ACK: {
                auto* a = std::get_if<MessageAck>(&pdu.choice);
                if (a) handle_message_ack(*a);
                break;
            }
            default:
                E3_LOG_WARN(LOG_TAG) << "dApp received unexpected PDU type: "
                                     << pdu_type_to_string(pdu.type);
                break;
        }
    }

    E3_LOG_INFO(LOG_TAG) << "Inbound loop (dApp) stopped";
}

void E3Interface::outbound_loop_dapp() {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    E3_LOG_INFO(LOG_TAG) << "Outbound loop (dApp) started";

    signal(SIGPIPE, SIG_IGN);

    ErrorCode result = connector_->setup_outbound_connection_client();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to setup outbound (dApp) connection: "
                              << error_code_to_string(result);
        return;
    }

    while (!should_stop_.load()) {
        auto pdu_opt = response_queue_->pop(std::chrono::milliseconds(10));
        if (!pdu_opt) continue;

        auto enc = encoder_->encode(*pdu_opt);
        if (!enc) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to encode dApp outbound PDU";
            continue;
        }
        ErrorCode rc = connector_->send(enc->buffer);
        if (rc != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to send dApp outbound PDU";
        } else {
            E3_LOG_DEBUG(LOG_TAG) << "Sent dApp outbound PDU: "
                                  << pdu_type_to_string(pdu_opt->type);
        }
    }

    E3_LOG_INFO(LOG_TAG) << "Outbound loop (dApp) stopped";
}

// ===========================================================================
// dApp-role message handlers
// ===========================================================================

void E3Interface::handle_setup_response(const SetupResponse& resp) {
    E3_LOG_INFO(LOG_TAG) << "Handling SetupResponse rc="
                         << response_code_to_string(resp.response_code);
    if (dapp_state_ && resp.response_code == ResponseCode::POSITIVE) {
        uint32_t dapp_id = resp.dapp_identifier.value_or(0);
        dapp_state_->record_setup_response(dapp_id, resp.ran_function_list);
    }
    if (setup_response_handler_) {
        setup_response_handler_(resp);
    }
}

void E3Interface::handle_subscription_response(const SubscriptionResponse& resp) {
    if (dapp_state_) {
        std::lock_guard<std::mutex> lk(dapp_state_->mu);
        if (!dapp_state_->assigned_dapp_id.has_value() ||
            *dapp_state_->assigned_dapp_id != resp.dapp_identifier) {
            return;  // not for us
        }
    }
    E3_LOG_INFO(LOG_TAG) << "Handling SubscriptionResponse for request "
                         << resp.request_id << " rc="
                         << response_code_to_string(resp.response_code);
    if (dapp_state_ && resp.response_code == ResponseCode::POSITIVE
        && resp.subscription_id.has_value()) {
        // We need the RAN function id; find it via the request_id mapping.
        // Since we don't separately track requested rf_id at this layer yet,
        // the SubscriptionResponse currently doesn't echo it. The dApp-facing
        // layer (E3Agent::subscribe) is responsible for resolving rf_id and
        // calling record_subscription if needed via handlers.
        // For now we just record the sub_id without an rf_id mapping if
        // we can't determine it; the user's handler can supplement.
    }
    if (subscription_response_handler_) {
        subscription_response_handler_(resp);
    }
}

void E3Interface::handle_indication(const IndicationMessage& msg) {
    // ZMQ PUB broadcasts to every connected SUB, so a dApp instance will
    // receive indications addressed to other dApps connected to the same RAN.
    // Filter: an indication is only for us if its dApp identifier matches
    // the one the RAN assigned us during setup.
    if (dapp_state_) {
        std::lock_guard<std::mutex> lk(dapp_state_->mu);
        if (!dapp_state_->assigned_dapp_id.has_value() ||
            *dapp_state_->assigned_dapp_id != msg.dapp_identifier) {
            return;  // not for us
        }
    }
    if (indication_handler_) {
        indication_handler_(msg);
    }
}

void E3Interface::handle_xapp_control_action(const XAppControlAction& action) {
    if (dapp_state_) {
        std::lock_guard<std::mutex> lk(dapp_state_->mu);
        if (!dapp_state_->assigned_dapp_id.has_value() ||
            *dapp_state_->assigned_dapp_id != action.dapp_identifier) {
            return;
        }
    }
    if (xapp_control_handler_) {
        xapp_control_handler_(action);
    }
}

void E3Interface::handle_message_ack(const MessageAck& ack) {
    if (message_ack_handler_) {
        message_ack_handler_(ack);
    }
}

// ===========================================================================
// dApp-role accessors and outbound helpers
// ===========================================================================

std::optional<uint32_t> E3Interface::dapp_id() const noexcept {
    if (!dapp_state_) return std::nullopt;
    std::lock_guard<std::mutex> lk(dapp_state_->mu);
    return dapp_state_->assigned_dapp_id;
}

std::vector<uint32_t> E3Interface::active_subscription_ids() const {
    if (!dapp_state_) return {};
    return dapp_state_->active_subscriptions();
}

std::vector<uint32_t> E3Interface::subscribed_ran_functions() const {
    if (!dapp_state_) return {};
    return dapp_state_->subscribed_ran_functions();
}

std::vector<RanFunctionDef> E3Interface::remote_ran_functions() const {
    if (!dapp_state_) return {};
    std::lock_guard<std::mutex> lk(dapp_state_->mu);
    return dapp_state_->remote_ran_functions;
}

ErrorCode E3Interface::queue_subscription_request(
    uint32_t ran_function_id,
    std::vector<uint32_t> telemetry_ids,
    std::vector<uint32_t> control_ids,
    std::optional<uint32_t> sub_time,
    std::optional<uint32_t> periodicity
) {
    if (!dapp_state_) return ErrorCode::STATE_ERROR;
    auto id = dapp_id();
    if (!id) return ErrorCode::NOT_INITIALIZED;

    Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = *id;
    req.ran_function_identifier = ran_function_id;
    req.telemetry_identifier_list = std::move(telemetry_ids);
    req.control_identifier_list = std::move(control_ids);
    req.subscription_time = sub_time;
    req.periodicity = periodicity;
    pdu.choice = std::move(req);
    pdu.message_id = generate_message_id();
    return queue_outbound(std::move(pdu));
}

ErrorCode E3Interface::queue_subscription_delete(uint32_t ran_function_id) {
    if (!dapp_state_) return ErrorCode::STATE_ERROR;
    auto id = dapp_id();
    if (!id) return ErrorCode::NOT_INITIALIZED;
    auto sub_id = dapp_state_->subscription_id_for(ran_function_id);
    if (!sub_id) return ErrorCode::SUBSCRIPTION_NOT_FOUND;

    Pdu pdu(PduType::SUBSCRIPTION_DELETE);
    SubscriptionDelete del;
    del.dapp_identifier = *id;
    del.subscription_id = *sub_id;
    pdu.choice = del;
    pdu.message_id = generate_message_id();
    return queue_outbound(std::move(pdu));
}

ErrorCode E3Interface::queue_dapp_control_action(
    uint32_t ran_function_id,
    uint32_t control_id,
    std::vector<uint8_t> action_data
) {
    if (!dapp_state_) return ErrorCode::STATE_ERROR;
    auto id = dapp_id();
    if (!id) return ErrorCode::NOT_INITIALIZED;

    Pdu pdu(PduType::DAPP_CONTROL_ACTION);
    DAppControlAction a;
    a.dapp_identifier = *id;
    a.ran_function_identifier = ran_function_id;
    a.control_identifier = control_id;
    a.action_data = std::move(action_data);
    pdu.choice = std::move(a);
    pdu.message_id = generate_message_id();
    return queue_outbound(std::move(pdu));
}

ErrorCode E3Interface::queue_dapp_report(
    uint32_t ran_function_id,
    std::vector<uint8_t> report_data
) {
    if (!dapp_state_) return ErrorCode::STATE_ERROR;
    auto id = dapp_id();
    if (!id) return ErrorCode::NOT_INITIALIZED;

    Pdu pdu(PduType::DAPP_REPORT);
    DAppReport r;
    r.dapp_identifier = *id;
    r.ran_function_identifier = ran_function_id;
    r.report_data = std::move(report_data);
    pdu.choice = std::move(r);
    pdu.message_id = generate_message_id();
    return queue_outbound(std::move(pdu));
}

ErrorCode E3Interface::queue_release_message() {
    if (!dapp_state_) return ErrorCode::STATE_ERROR;
    auto id = dapp_id();
    if (!id) return ErrorCode::NOT_INITIALIZED;

    Pdu pdu(PduType::RELEASE_MESSAGE);
    ReleaseMessage rel;
    rel.dapp_identifier = *id;
    pdu.choice = rel;
    pdu.message_id = generate_message_id();
    return queue_outbound(std::move(pdu));
}

ErrorCode E3Interface::wait_for_setup(std::chrono::milliseconds timeout) {
    if (config_.role != E3Role::DAPP) return ErrorCode::INVALID_PARAM;
    std::unique_lock<std::mutex> lk(setup_complete_mu_);
    if (!setup_complete_cv_.wait_for(lk, timeout, [this]() { return setup_complete_; })) {
        return ErrorCode::TIMEOUT;
    }
    return setup_succeeded_ ? ErrorCode::SUCCESS : ErrorCode::CONNECTION_FAILED;
}

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
