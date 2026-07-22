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

    // dApp-side handlers stored before init() so they can be forwarded
    // to E3Interface after construction.
    SetupResponseHandler setup_response_handler;
    SubscriptionResponseHandler subscription_response_handler;
    IndicationHandler indication_handler;
    XAppControlHandler xapp_control_handler;
    MessageAckHandler message_ack_handler;

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

    if (impl_->setup_response_handler) {
        impl_->interface->set_setup_response_handler(impl_->setup_response_handler);
    }
    if (impl_->subscription_response_handler) {
        impl_->interface->set_subscription_response_handler(impl_->subscription_response_handler);
    }
    if (impl_->indication_handler) {
        impl_->interface->set_indication_handler(impl_->indication_handler);
    }
    if (impl_->xapp_control_handler) {
        impl_->interface->set_xapp_control_handler(impl_->xapp_control_handler);
    }
    if (impl_->message_ack_handler) {
        impl_->interface->set_message_ack_handler(impl_->message_ack_handler);
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
    if (impl_->config.role != E3Role::RAN) {
        E3_LOG_ERROR(LOG_TAG) << "register_sm() is only valid on RAN role";
        return ErrorCode::STATE_ERROR;
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
    if (impl_->config.role != E3Role::RAN) {
        return ErrorCode::STATE_ERROR;
    }
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
    if (impl_->config.role != E3Role::RAN) {
        return ErrorCode::STATE_ERROR;
    }
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
    if (!impl_->interface || impl_->config.role != E3Role::RAN) {
        return {};
    }
    return impl_->interface->subscription_manager().get_registered_dapps();
}

std::vector<uint32_t> E3Agent::get_dapp_subscriptions(uint32_t dapp_id) const {
    if (!impl_->interface || impl_->config.role != E3Role::RAN) {
        return {};
    }
    return impl_->interface->subscription_manager().get_dapp_subscriptions(dapp_id);
}

std::vector<uint32_t> E3Agent::get_ran_function_subscribers(uint32_t ran_function_id) const {
    if (!impl_->interface || impl_->config.role != E3Role::RAN) {
        return {};
    }
    return impl_->interface->subscription_manager().get_subscribed_dapps(ran_function_id);
}

std::vector<uint32_t> E3Agent::get_active_ran_functions() const {
    if (!impl_->interface || impl_->config.role != E3Role::RAN) {
        return {};
    }
    return impl_->interface->subscription_manager().get_active_ran_functions();
}

uint32_t E3Agent::get_subscription_periodicity(uint32_t dapp_id, uint32_t ran_function_id) const {
    if (!impl_->interface) {
        return 0;
    }
    const auto* details = impl_->interface->subscription_manager()
                              .get_subscription_details(dapp_id, ran_function_id);
    return details ? details->periodicity_us : 0;
}

std::vector<uint32_t> E3Agent::get_subscription_telemetry_ids(uint32_t dapp_id, uint32_t ran_function_id) const {
    if (!impl_->interface) {
        return {};
    }
    const auto* details = impl_->interface->subscription_manager()
                              .get_subscription_details(dapp_id, ran_function_id);
    return details ? details->telemetry_ids : std::vector<uint32_t>{};
}

std::vector<uint32_t> E3Agent::get_subscription_control_ids(uint32_t dapp_id, uint32_t ran_function_id) const {
    if (!impl_->interface) {
        return {};
    }
    const auto* details = impl_->interface->subscription_manager()
                              .get_subscription_details(dapp_id, ran_function_id);
    return details ? details->control_ids : std::vector<uint32_t>{};
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
    if (impl_->config.role == E3Role::RAN) {
        return impl_->interface->subscription_manager().dapp_count();
    }
    // On the dApp role, "dapp_count" is 0/1 depending on whether we have a
    // dApp identifier assigned.
    return impl_->interface->dapp_id().has_value() ? 1u : 0u;
}

size_t E3Agent::subscription_count() const {
    if (!impl_->interface) {
        return 0;
    }
    if (impl_->config.role == E3Role::RAN) {
        return impl_->interface->subscription_manager().subscription_count();
    }
    return impl_->interface->active_subscription_ids().size();
}

// =========================================================================
// dApp-role API
// =========================================================================

void E3Agent::set_setup_response_handler(SetupResponseHandler handler) {
    impl_->setup_response_handler = std::move(handler);
    if (impl_->interface && impl_->setup_response_handler) {
        impl_->interface->set_setup_response_handler(impl_->setup_response_handler);
    }
}

void E3Agent::set_subscription_response_handler(SubscriptionResponseHandler handler) {
    impl_->subscription_response_handler = std::move(handler);
    if (impl_->interface && impl_->subscription_response_handler) {
        impl_->interface->set_subscription_response_handler(impl_->subscription_response_handler);
    }
}

void E3Agent::set_indication_handler(IndicationHandler handler) {
    impl_->indication_handler = std::move(handler);
    if (impl_->interface && impl_->indication_handler) {
        impl_->interface->set_indication_handler(impl_->indication_handler);
    }
}

void E3Agent::set_xapp_control_handler(XAppControlHandler handler) {
    impl_->xapp_control_handler = std::move(handler);
    if (impl_->interface && impl_->xapp_control_handler) {
        impl_->interface->set_xapp_control_handler(impl_->xapp_control_handler);
    }
}

void E3Agent::set_message_ack_handler(MessageAckHandler handler) {
    impl_->message_ack_handler = std::move(handler);
    if (impl_->interface && impl_->message_ack_handler) {
        impl_->interface->set_message_ack_handler(impl_->message_ack_handler);
    }
}

std::optional<uint32_t> E3Agent::dapp_id() const noexcept {
    if (!impl_->interface || impl_->config.role != E3Role::DAPP) return std::nullopt;
    return impl_->interface->dapp_id();
}

std::vector<uint32_t> E3Agent::subscribed_ran_functions() const {
    if (!impl_->interface || impl_->config.role != E3Role::DAPP) return {};
    return impl_->interface->subscribed_ran_functions();
}

std::vector<uint32_t> E3Agent::active_subscription_ids() const {
    if (!impl_->interface || impl_->config.role != E3Role::DAPP) return {};
    return impl_->interface->active_subscription_ids();
}

ErrorCode E3Agent::wait_for_setup(std::chrono::milliseconds timeout) {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::INVALID_PARAM;
    if (!impl_->interface) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->wait_for_setup(timeout);
}

ErrorCode E3Agent::subscribe(uint32_t ran_function_id,
                             std::vector<uint32_t> telemetry_ids,
                             std::vector<uint32_t> control_ids,
                             std::optional<uint32_t> sub_time,
                             std::optional<uint32_t> periodicity) {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::STATE_ERROR;
    if (!impl_->interface || !impl_->interface->is_running()) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->queue_subscription_request(
        ran_function_id, std::move(telemetry_ids), std::move(control_ids), sub_time, periodicity);
}

ErrorCode E3Agent::unsubscribe(uint32_t ran_function_id) {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::STATE_ERROR;
    if (!impl_->interface || !impl_->interface->is_running()) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->queue_subscription_delete(ran_function_id);
}

ErrorCode E3Agent::send_control(uint32_t ran_function_id,
                                uint32_t control_id,
                                std::vector<uint8_t> action_data) {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::STATE_ERROR;
    if (!impl_->interface || !impl_->interface->is_running()) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->queue_dapp_control_action(
        ran_function_id, control_id, std::move(action_data));
}

ErrorCode E3Agent::send_report(uint32_t ran_function_id, std::vector<uint8_t> report_data) {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::STATE_ERROR;
    if (!impl_->interface || !impl_->interface->is_running()) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->queue_dapp_report(ran_function_id, std::move(report_data));
}

ErrorCode E3Agent::release() {
    if (impl_->config.role != E3Role::DAPP) return ErrorCode::STATE_ERROR;
    if (!impl_->interface || !impl_->interface->is_running()) return ErrorCode::NOT_INITIALIZED;
    return impl_->interface->queue_release_message();
}

} // namespace libe3
