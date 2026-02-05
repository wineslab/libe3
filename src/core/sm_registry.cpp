/**
 * @file sm_registry.cpp
 * @brief Service Model Registry implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/sm_interface.hpp"
#include "libe3/logger.hpp"
#include <algorithm>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "SmReg";
}

SmRegistry& SmRegistry::instance() {
    static SmRegistry registry;
    return registry;
}

ErrorCode SmRegistry::register_sm(std::unique_ptr<ServiceModel> sm) {
    if (!sm) {
        return ErrorCode::INVALID_PARAM;
    }

    std::lock_guard lock(mutex_);

    uint32_t ran_func = sm->ran_function_id();
    
    // Check for conflict
    if (sms_.count(ran_func) > 0) {
        E3_LOG_ERROR(LOG_TAG) << "SM already registered for RAN function " << ran_func;
        return ErrorCode::SM_ALREADY_REGISTERED;
    }

    // Initialize the SM
    ErrorCode init_result = sm->init();
    if (init_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to initialize SM: " << sm->name();
        return init_result;
    }

    E3_LOG_INFO(LOG_TAG) << "Registered SM '" << sm->name() 
                         << "' for RAN function " << ran_func;

    sms_[ran_func] = std::move(sm);

    return ErrorCode::SUCCESS;
}

ErrorCode SmRegistry::register_sm_factory(uint32_t ran_function_id, SmFactory factory) {
    if (!factory) {
        return ErrorCode::INVALID_PARAM;
    }

    std::lock_guard lock(mutex_);

    if (sms_.count(ran_function_id) > 0 || factories_.count(ran_function_id) > 0) {
        E3_LOG_ERROR(LOG_TAG) << "SM or factory already registered for RAN function " 
                              << ran_function_id;
        return ErrorCode::SM_ALREADY_REGISTERED;
    }

    factories_[ran_function_id] = std::move(factory);
    E3_LOG_INFO(LOG_TAG) << "Registered SM factory for RAN function " << ran_function_id;

    return ErrorCode::SUCCESS;
}

ErrorCode SmRegistry::unregister_sm(uint32_t ran_function_id) {
    std::lock_guard lock(mutex_);

    auto it = sms_.find(ran_function_id);
    if (it != sms_.end()) {
        if (it->second->is_running()) {
            it->second->stop();
        }
        it->second->destroy();
        sms_.erase(it);
        E3_LOG_INFO(LOG_TAG) << "Unregistered SM for RAN function " << ran_function_id;
        return ErrorCode::SUCCESS;
    }

    auto factory_it = factories_.find(ran_function_id);
    if (factory_it != factories_.end()) {
        factories_.erase(factory_it);
        return ErrorCode::SUCCESS;
    }

    return ErrorCode::SM_NOT_FOUND;
}

ServiceModel* SmRegistry::get_by_ran_function(uint32_t ran_function_id) {
    std::lock_guard lock(mutex_);

    // Direct lookup
    auto it = sms_.find(ran_function_id);
    if (it != sms_.end()) {
        return it->second.get();
    }

    // Check if there's a factory for this RAN function
    auto factory_it = factories_.find(ran_function_id);
    if (factory_it != factories_.end()) {
        // Create SM from factory
        auto sm = factory_it->second();
        if (sm) {
            ErrorCode init_result = sm->init();
            if (init_result == ErrorCode::SUCCESS) {
                ServiceModel* ptr = sm.get();
                sms_[ran_function_id] = std::move(sm);
                factories_.erase(factory_it);
                return ptr;
            }
        }
    }

    return nullptr;
}

std::vector<uint32_t> SmRegistry::get_available_ran_functions() const {
    std::lock_guard lock(mutex_);

    std::vector<uint32_t> result;
    result.reserve(sms_.size() + factories_.size());
    
    for (const auto& [id, sm] : sms_) {
        result.push_back(id);
    }

    for (const auto& [id, factory] : factories_) {
        result.push_back(id);
    }

    std::sort(result.begin(), result.end());
    return result;
}

ErrorCode SmRegistry::start_sm(uint32_t ran_function_id) {
    std::lock_guard lock(mutex_);

    auto it = sms_.find(ran_function_id);
    if (it == sms_.end()) {
        E3_LOG_ERROR(LOG_TAG) << "SM not found for RAN function " << ran_function_id;
        E3_LOG_ERROR(LOG_TAG) << "SM not found for RAN function " << ran_function_id;
        return ErrorCode::SM_NOT_FOUND;
    }

    auto& sm = it->second;

    if (sm->is_running()) {
        E3_LOG_DEBUG(LOG_TAG) << "SM already running for RAN function " << ran_function_id;
        return ErrorCode::SUCCESS;
    }

    ErrorCode result = sm->start();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to start SM for RAN function " << ran_function_id;
        return result;
    }

    E3_LOG_INFO(LOG_TAG) << "Started SM for RAN function " << ran_function_id;
    return ErrorCode::SUCCESS;
}

ErrorCode SmRegistry::stop_sm(uint32_t ran_function_id) {
    std::lock_guard lock(mutex_);

    auto it = sms_.find(ran_function_id);
    if (it == sms_.end()) {
        return ErrorCode::SM_NOT_FOUND;
    }

    auto& sm = it->second;

    if (!sm->is_running()) {
        return ErrorCode::SUCCESS;
    }

    sm->stop();
    E3_LOG_INFO(LOG_TAG) << "Stopped SM for RAN function " << ran_function_id;
    return ErrorCode::SUCCESS;
}

bool SmRegistry::is_sm_running(uint32_t ran_function_id) const {
    std::lock_guard lock(mutex_);

    auto it = sms_.find(ran_function_id);
    if (it != sms_.end()) {
        return it->second->is_running();
    }

    return false;
}

void SmRegistry::clear() {
    std::lock_guard lock(mutex_);

    for (auto& [id, sm] : sms_) {
        if (sm->is_running()) {
            sm->stop();
        }
        sm->destroy();
    }

    sms_.clear();
    factories_.clear();
    E3_LOG_INFO(LOG_TAG) << "SM registry cleared";
}

} // namespace libe3
