/**
 * @file test_sm_registration_contract.cpp
 * @brief Registration-time enforcement of the E3AP ranFunctionData contract.
 *
 * E3-RanFunctionDefinition.ranFunctionData is a mandatory
 * OCTET STRING (SIZE (1..32768)). An SM that advertises nothing (or too much)
 * cannot be encoded into a SetupResponse, and the failure would take every
 * other registered RAN function down with it, so registration rejects it.
 *
 * Regression test for issue #30.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/c_api.h"

#include <cstring>

using namespace libe3;

namespace {

inline int error_to_int(ErrorCode e) { return static_cast<int>(e); }

/// SM whose advertised ranFunctionData length is a constructor argument.
class SizedDataSM : public ServiceModel {
public:
    SizedDataSM(uint32_t id, size_t data_len) : id_(id), data_len_(data_len) {}

    std::string name() const override { return "SizedDataSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return id_; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    std::vector<uint8_t> ran_function_data() const override {
        return std::vector<uint8_t>(data_len_, 0xAB);
    }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }

private:
    uint32_t id_;
    size_t data_len_;
    bool running_ = false;
};

/// SM that leaves ran_function_data() at the base-class default.
class DefaultDataSM : public SizedDataSM {
public:
    explicit DefaultDataSM(uint32_t id) : SizedDataSM(id, 0) {}
    std::vector<uint8_t> ran_function_data() const override {
        return ServiceModel::ran_function_data();
    }
};

E3Config ran_config(const char* id) {
    E3Config cfg;
    cfg.ran_identifier = id;
    cfg.log_level = 0;
    return cfg;
}

e3_error_t register_c_sm(const uint8_t* data, size_t data_len) {
    e3_c_service_model_desc_t desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.name = "CApiSM";
    desc.version = 1;
    desc.ran_function_id = 77;
    desc.ran_function_data = data;
    desc.ran_function_data_len = data_len;

    e3_agent_handle_t* agent = e3_agent_create_default();
    ASSERT_TRUE(agent != nullptr);
    e3_service_model_handle_t* sm = e3_service_model_create_from_c(&desc);
    ASSERT_TRUE(sm != nullptr);

    e3_error_t rc = e3_agent_register_sm(agent, sm);
    if (rc != 0) {
        // Rejected before the ownership transfer, so the caller still owns the
        // handle and destroying it must be safe (and must actually free it).
        e3_service_model_destroy(sm);
    }
    e3_agent_destroy(agent);
    return rc;
}

} // namespace

TEST(SmRegistration_rejects_empty_ran_function_data) {
    E3Agent agent(ran_config("rfd-empty"));
    auto rc = agent.register_sm(std::make_unique<SizedDataSM>(10, 0));
    ASSERT_EQ(error_to_int(rc), error_to_int(ErrorCode::INVALID_PARAM));
    ASSERT_TRUE(agent.get_available_ran_functions().empty());
}

TEST(SmRegistration_rejects_base_class_default) {
    // The default implementation returns {}: it is a diagnostic, not a usable
    // default, so an SM that forgets to override it does not register.
    E3Agent agent(ran_config("rfd-default"));
    auto rc = agent.register_sm(std::make_unique<DefaultDataSM>(11));
    ASSERT_EQ(error_to_int(rc), error_to_int(ErrorCode::INVALID_PARAM));
}

TEST(SmRegistration_accepts_one_byte) {
    E3Agent agent(ran_config("rfd-min"));
    auto rc = agent.register_sm(std::make_unique<SizedDataSM>(12, 1));
    ASSERT_EQ(error_to_int(rc), error_to_int(ErrorCode::SUCCESS));
}

TEST(SmRegistration_accepts_upper_bound) {
    E3Agent agent(ran_config("rfd-max"));
    auto rc = agent.register_sm(
        std::make_unique<SizedDataSM>(13, MAX_PROTOCOL_DATA_SIZE));
    ASSERT_EQ(error_to_int(rc), error_to_int(ErrorCode::SUCCESS));
}

TEST(SmRegistration_rejects_above_upper_bound) {
    E3Agent agent(ran_config("rfd-over"));
    auto rc = agent.register_sm(
        std::make_unique<SizedDataSM>(14, MAX_PROTOCOL_DATA_SIZE + 1));
    ASSERT_EQ(error_to_int(rc), error_to_int(ErrorCode::INVALID_PARAM));
}

TEST(SmRegistration_c_api_rejects_null_data) {
    ASSERT_EQ(register_c_sm(nullptr, 0),
              static_cast<e3_error_t>(ErrorCode::INVALID_PARAM));
}

TEST(SmRegistration_c_api_rejects_zero_length) {
    const uint8_t data[] = {0x01};
    ASSERT_EQ(register_c_sm(data, 0),
              static_cast<e3_error_t>(ErrorCode::INVALID_PARAM));
}

TEST(SmRegistration_c_api_accepts_non_empty_data) {
    const uint8_t data[] = {'C', 'S', 'M'};
    ASSERT_EQ(register_c_sm(data, sizeof(data)), 0);
}

int main() {
    return RUN_ALL_TESTS();
}
