#include "libe3/c_api.h"

#include "libe3/e3_agent.hpp"
#include "libe3/sm_interface.hpp"
#include "libe3/types.hpp"

#include <string>
#include <vector>
#include <memory>
#include <cstring>

using namespace libe3;

struct e3_agent_handle_s {
    std::unique_ptr<E3Agent> agent;
};

struct e3_service_model_handle_s : public ServiceModel {
    // store copied values
    std::string name_s;
    uint32_t version_s;
    uint32_t ran_function_id_s;
    std::vector<uint32_t> telemetry_ids_s;
    std::vector<uint32_t> control_ids_s;

    // callbacks
    e3_sm_init_cb init_cb = nullptr;
    e3_sm_destroy_cb destroy_cb = nullptr;
    e3_sm_start_cb start_cb = nullptr;
    e3_sm_stop_cb stop_cb = nullptr;
    e3_sm_is_running_cb is_running_cb = nullptr;
    e3_sm_process_control_cb process_control_cb = nullptr;
    void* user_data = nullptr;

    // mark if ownership was transferred to an agent
    bool transferred_to_agent = false;

    explicit e3_service_model_handle_s(const e3_c_service_model_desc_t& desc) {
        if (desc.name) name_s = desc.name;
        version_s = desc.version;
        ran_function_id_s = desc.ran_function_id;
        if (desc.telemetry_ids && desc.telemetry_ids_len) {
            telemetry_ids_s.assign(desc.telemetry_ids, desc.telemetry_ids + desc.telemetry_ids_len);
        }
        if (desc.control_ids && desc.control_ids_len) {
            control_ids_s.assign(desc.control_ids, desc.control_ids + desc.control_ids_len);
        }
        init_cb = desc.init_cb;
        destroy_cb = desc.destroy_cb;
        start_cb = desc.start_cb;
        stop_cb = desc.stop_cb;
        is_running_cb = desc.is_running_cb;
        process_control_cb = desc.process_control_cb;
        user_data = desc.user_data;
    }

    // ServiceModel virtuals
    std::string name() const override { return name_s; }
    uint32_t version() const override { return version_s; }
    uint32_t ran_function_id() const override { return ran_function_id_s; }
    std::vector<uint32_t> telemetry_ids() const override { return telemetry_ids_s; }
    std::vector<uint32_t> control_ids() const override { return control_ids_s; }
    ErrorCode init() override {
        if (init_cb) return static_cast<ErrorCode>(init_cb(user_data));
        return ErrorCode::SUCCESS;
    }
    void destroy() override {
        if (destroy_cb) destroy_cb(user_data);
    }
    ErrorCode start() override {
        if (start_cb) return static_cast<ErrorCode>(start_cb(user_data));
        return ErrorCode::SUCCESS;
    }
    void stop() override {
        if (stop_cb) stop_cb(user_data);
    }
    bool is_running() const override {
        if (is_running_cb) return is_running_cb(user_data) != 0;
        return false;
    }
};

extern "C" {

e3_service_model_handle_t* e3_service_model_create_from_c(const e3_c_service_model_desc_t* desc) {
    if (!desc) return nullptr;
    try {
        return new e3_service_model_handle_s(*desc);
    } catch (...) {
        return nullptr;
    }
}

void e3_service_model_destroy(e3_service_model_handle_t* sm) {
    if (!sm) return;
    // If ownership was transferred to an agent, we must not delete here.
    if (sm->transferred_to_agent) return;
    delete sm;
}

e3_agent_handle_t* e3_agent_create_default() {
    return e3_agent_create_with_config(nullptr);
}

e3_agent_handle_t* e3_agent_create_with_config(const e3_config_t* config) {
    try {
        E3Config cfg;
        if (config) {
            if (config->ran_identifier) cfg.ran_identifier = config->ran_identifier;
            if (config->link_layer >= 0 && config->link_layer <= 1)
                cfg.link_layer = static_cast<E3LinkLayer>(config->link_layer);
            if (config->transport_layer >= 0 && config->transport_layer <= 2)
                cfg.transport_layer = static_cast<E3TransportLayer>(config->transport_layer);
            if (config->setup_port != 0) cfg.setup_port = config->setup_port;
            if (config->subscriber_port != 0) cfg.subscriber_port = config->subscriber_port;
            if (config->publisher_port != 0) cfg.publisher_port = config->publisher_port;
            if (config->setup_endpoint) cfg.setup_endpoint = config->setup_endpoint;
            if (config->subscriber_endpoint) cfg.subscriber_endpoint = config->subscriber_endpoint;
            if (config->publisher_endpoint) cfg.publisher_endpoint = config->publisher_endpoint;
            if (config->encoding >= 0 && config->encoding <= 1)
                cfg.encoding = static_cast<EncodingFormat>(config->encoding);
            if (config->io_threads != 0) cfg.io_threads = config->io_threads;
            if (config->log_level >= 0) cfg.log_level = config->log_level;
        }
        e3_agent_handle_t* h = new e3_agent_handle_s();
        h->agent = std::make_unique<E3Agent>(std::move(cfg));
        return h;
    } catch (...) {
        return nullptr;
    }
}

void e3_agent_destroy(e3_agent_handle_t* agent) {
    if (!agent) return;
    delete agent;
}

e3_error_t e3_agent_init(e3_agent_handle_t* agent) {
    if (!agent || !agent->agent) return static_cast<int>(ErrorCode::STATE_ERROR);
    return static_cast<int>(agent->agent->init());
}

e3_error_t e3_agent_start(e3_agent_handle_t* agent) {
    if (!agent || !agent->agent) return static_cast<int>(ErrorCode::STATE_ERROR);
    return static_cast<int>(agent->agent->start());
}

void e3_agent_stop(e3_agent_handle_t* agent) {
    if (!agent || !agent->agent) return;
    agent->agent->stop();
}

e3_error_t e3_agent_register_sm(e3_agent_handle_t* agent, e3_service_model_handle_t* sm) {
    if (!agent || !agent->agent || !sm) return static_cast<int>(ErrorCode::INVALID_PARAM);

    // Transfer ownership into the agent
    try {
        sm->transferred_to_agent = true;
        std::unique_ptr<ServiceModel> uptr(sm);
        return static_cast<int>(agent->agent->register_sm(std::move(uptr)));
    } catch (...) {
        return static_cast<int>(ErrorCode::INTERNAL_ERROR);
    }
}

    int e3_agent_get_state(e3_agent_handle_t* agent) {
        if (!agent || !agent->agent) return static_cast<int>(AgentState::ERROR);
        return static_cast<int>(agent->agent->state());
    }

    int e3_agent_is_running(e3_agent_handle_t* agent) {
        if (!agent || !agent->agent) return 0;
        return agent->agent->is_running() ? 1 : 0;
    }

    static uint32_t* copy_vector_u32_to_c(const std::vector<uint32_t>& v, size_t* out_len) {
        if (out_len) *out_len = v.size();
        if (v.empty()) return nullptr;
        uint32_t* arr = (uint32_t*)malloc(sizeof(uint32_t) * v.size());
        if (!arr) return nullptr;
        memcpy(arr, v.data(), sizeof(uint32_t) * v.size());
        return arr;
    }

    uint32_t* e3_agent_get_available_ran_functions(e3_agent_handle_t* agent, size_t* out_len) {
        if (!agent || !agent->agent) { if (out_len) *out_len = 0; return nullptr; }
        auto v = agent->agent->get_available_ran_functions();
        return copy_vector_u32_to_c(v, out_len);
    }

    uint32_t* e3_agent_get_registered_dapps(e3_agent_handle_t* agent, size_t* out_len) {
        if (!agent || !agent->agent) { if (out_len) *out_len = 0; return nullptr; }
        auto v = agent->agent->get_registered_dapps();
        return copy_vector_u32_to_c(v, out_len);
    }

    uint32_t* e3_agent_get_dapp_subscriptions(e3_agent_handle_t* agent, uint32_t dapp_id, size_t* out_len) {
        if (!agent || !agent->agent) { if (out_len) *out_len = 0; return nullptr; }
        auto v = agent->agent->get_dapp_subscriptions(dapp_id);
        return copy_vector_u32_to_c(v, out_len);
    }

    uint32_t* e3_agent_get_ran_function_subscribers(e3_agent_handle_t* agent, uint32_t ran_function_id, size_t* out_len) {
        if (!agent || !agent->agent) { if (out_len) *out_len = 0; return nullptr; }
        auto v = agent->agent->get_ran_function_subscribers(ran_function_id);
        return copy_vector_u32_to_c(v, out_len);
    }

    void e3_agent_free_uint32_array(uint32_t* arr) {
        free(arr);
    }

    e3_error_t e3_agent_send_indication(
        e3_agent_handle_t* agent,
        uint32_t dapp_id,
        uint32_t ran_function_id,
        const uint8_t* data,
        size_t data_len
    ) {
        if (!agent || !agent->agent) return static_cast<int>(ErrorCode::INVALID_PARAM);
        std::vector<uint8_t> buf;
        if (data && data_len) buf.assign(data, data + data_len);
        return static_cast<int>(agent->agent->send_indication(dapp_id, ran_function_id, buf));
    }

    size_t e3_agent_dapp_count(e3_agent_handle_t* agent) {
        if (!agent || !agent->agent) return 0;
        return agent->agent->dapp_count();
    }

    size_t e3_agent_subscription_count(e3_agent_handle_t* agent) {
        if (!agent || !agent->agent) return 0;
        return agent->agent->subscription_count();
    }

} // extern "C"
