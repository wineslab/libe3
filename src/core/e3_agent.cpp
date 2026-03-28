/**
 * @file e3_agent.cpp
 * @brief E3Agent façade implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_agent.hpp"
#include "libe3/e3_interface.hpp"
#include "libe3/logger.hpp"
#include "libe3/version.hpp"

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "E3Agent";
}

/**
 * @brief E3Agent implementation details
 */
struct E3Agent::Impl {
    E3Config config;
    std::unique_ptr<E3Interface> interface;
    DAppReportHandler dapp_report_handler;
    DAppStatusChangedHandler dapp_status_changed_handler;
    
    explicit Impl(E3Config cfg) : config(std::move(cfg)) {}
};

E3Agent::E3Agent(E3Config config)
    : impl_(std::make_unique<Impl>(std::move(config)))
{
    E3_LOG_INFO(LOG_TAG) << "E3Agent created (version " << LIBE3_VERSION_STRING << ")";
}

E3Agent::~E3Agent() {
    stop();
    E3_LOG_INFO(LOG_TAG) << "E3Agent destroyed";
}

E3Agent::E3Agent(E3Agent&&) noexcept = default;
E3Agent& E3Agent::operator=(E3Agent&&) noexcept = default;

// =========================================================================
// Lifecycle Management
// =========================================================================

ErrorCode E3Agent::init() {
    if (impl_->interface) {
        E3_LOG_WARN(LOG_TAG) << "Agent already initialized";
        return ErrorCode::ALREADY_INITIALIZED;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Initializing E3Agent";
    
    impl_->interface = std::make_unique<E3Interface>(impl_->config);
    
    ErrorCode result = impl_->interface->init();
    if (result != ErrorCode::SUCCESS) {
        impl_->interface.reset();
        E3_LOG_ERROR(LOG_TAG) << "Failed to initialize interface: " 
                              << error_code_to_string(result);
        return result;
    }
    
    if (impl_->dapp_report_handler) {
        impl_->interface->set_dapp_report_handler(impl_->dapp_report_handler);
    }

    if (impl_->dapp_status_changed_handler) {
        impl_->interface->set_dapp_status_changed_handler(impl_->dapp_status_changed_handler);
    }
    
    E3_LOG_INFO(LOG_TAG) << "E3Agent initialized successfully";
    return ErrorCode::SUCCESS;
}

ErrorCode E3Agent::start() {
    // Initialize if not already done
    if (!impl_->interface) {
        ErrorCode init_result = init();
        if (init_result != ErrorCode::SUCCESS) {
            return init_result;
        }
    }
    
    E3_LOG_INFO(LOG_TAG) << "Starting E3Agent";
    
    ErrorCode result = impl_->interface->start();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to start interface: " 
                              << error_code_to_string(result);
        return result;
    }
    
    E3_LOG_INFO(LOG_TAG) << "E3Agent started successfully";
    return ErrorCode::SUCCESS;
}

void E3Agent::stop() {
    if (!impl_ || !impl_->interface) {
        return;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Stopping E3Agent";
    impl_->interface->stop();
    E3_LOG_INFO(LOG_TAG) << "E3Agent stopped";
}

AgentState E3Agent::state() const noexcept {
    if (!impl_ || !impl_->interface) {
        return AgentState::UNINITIALIZED;
    }
    return impl_->interface->state();
}

bool E3Agent::is_running() const noexcept {
    return impl_ && impl_->interface && impl_->interface->is_running();
}

// =========================================================================
// Service Model Registration
// =========================================================================

ErrorCode E3Agent::register_sm(std::unique_ptr<ServiceModel> sm) {
    if (!sm) {
        return ErrorCode::INVALID_PARAM;
    }
    
    // Initialize interface if needed
    if (!impl_->interface) {
        ErrorCode init_result = init();
        if (init_result != ErrorCode::SUCCESS) {
            return init_result;
        }
    }
    
    return impl_->interface->register_sm(std::move(sm));
}

std::vector<uint32_t> E3Agent::get_available_ran_functions() const {
    if (!impl_->interface) {
        return {};
    }
    return impl_->interface->get_available_ran_functions();
}

void E3Agent::set_dapp_report_handler(DAppReportHandler handler) {
    impl_->dapp_report_handler = std::move(handler);
    if (impl_->interface && impl_->dapp_report_handler) {
        impl_->interface->set_dapp_report_handler(impl_->dapp_report_handler);
    }
}

void E3Agent::set_dapp_status_changed_handler(DAppStatusChangedHandler handler) {
    impl_->dapp_status_changed_handler = std::move(handler);
    if (impl_->interface && impl_->dapp_status_changed_handler) {
        impl_->interface->set_dapp_status_changed_handler(impl_->dapp_status_changed_handler);
    }
}

// =========================================================================
// Manual Operations
// =========================================================================

ErrorCode E3Agent::send_indication(
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const std::vector<uint8_t>& data
) {
    if (!impl_->interface || !impl_->interface->is_running()) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    Pdu pdu(PduType::INDICATION_MESSAGE);
    uint32_t mid = impl_->interface->generate_message_id();
    pdu.message_id = mid;
    IndicationMessage msg;
    msg.dapp_identifier = dapp_id;
    msg.ran_function_identifier = ran_function_id;
    msg.protocol_data = data;
    pdu.choice = msg;
    
    return impl_->interface->queue_outbound(std::move(pdu));
}

ErrorCode E3Agent::send_xapp_control(
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const std::vector<uint8_t>& control_data
) {
    if (!impl_->interface || !impl_->interface->is_running()) {
        return ErrorCode::NOT_INITIALIZED;
    }
    Pdu pdu(PduType::XAPP_CONTROL_ACTION);
    pdu.message_id = impl_->interface->generate_message_id();
    XAppControlAction action;
    action.dapp_identifier = dapp_id;
    action.ran_function_identifier = ran_function_id;
    action.xapp_control_data = control_data;
    pdu.choice = action;
    return impl_->interface->queue_outbound(std::move(pdu));
}

ErrorCode E3Agent::send_message_ack(uint32_t request_id, ResponseCode response_code) {
    if (!impl_->interface || !impl_->interface->is_running()) {
        return ErrorCode::NOT_INITIALIZED;
    }
    Pdu pdu(PduType::MESSAGE_ACK);
    pdu.message_id = impl_->interface->generate_message_id();
    MessageAck ack;
    ack.request_id = request_id;
    ack.response_code = response_code;
    pdu.choice = ack;
    return impl_->interface->queue_outbound(std::move(pdu));
}

std::vector<uint32_t> E3Agent::get_registered_dapps() const {
    if (!impl_->interface) {
        return {};
    }
    return impl_->interface->subscription_manager().get_registered_dapps();
}

std::vector<uint32_t> E3Agent::get_dapp_subscriptions(uint32_t dapp_id) const {
    if (!impl_->interface) {
        return {};
    }
    return impl_->interface->subscription_manager().get_dapp_subscriptions(dapp_id);
}

std::vector<uint32_t> E3Agent::get_ran_function_subscribers(uint32_t ran_function_id) const {
    if (!impl_->interface) {
        return {};
    }
    return impl_->interface->subscription_manager().get_subscribed_dapps(ran_function_id);
}

// =========================================================================
// Configuration
// =========================================================================

const E3Config& E3Agent::config() const noexcept {
    return impl_->config;
}

// =========================================================================
// Statistics
// =========================================================================

size_t E3Agent::dapp_count() const {
    if (!impl_->interface) {
        return 0;
    }
    return impl_->interface->subscription_manager().dapp_count();
}

size_t E3Agent::subscription_count() const {
    if (!impl_->interface) {
        return 0;
    }
    return impl_->interface->subscription_manager().subscription_count();
}

} // namespace libe3
