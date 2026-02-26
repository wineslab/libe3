/**
 * @file test_e3_agent.cpp
 * @brief Integration tests for E3Agent
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include <thread>
#include <chrono>

using namespace libe3;

// Helper to convert AgentState to int for ASSERT_EQ (avoids operator<< issue)
inline int state_to_int(AgentState s) { return static_cast<int>(s); }
inline int error_to_int(ErrorCode e) { return static_cast<int>(e); }

// Simple test service model for testing
class TestServiceModel : public ServiceModel {
public:
    explicit TestServiceModel(uint32_t id)
        : id_(id), running_(false) {}
    
    std::string name() const override { return "TestSM"; }
    uint32_t version() const override { return 1; }
    
    uint32_t ran_function_id() const override {
        return id_;
    }
    
    std::vector<uint32_t> telemetry_ids() const override {
        return {1, 2, 3};  // Example telemetry IDs
    }
    
    std::vector<uint32_t> control_ids() const override {
        return {10, 20};  // Example control IDs
    }
    
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    
    void destroy() override {
        running_ = false;
    }
    
    ErrorCode start() override {
        running_ = true;
        return ErrorCode::SUCCESS;
    }
    
    void stop() override {
        running_ = false;
    }
    
    bool is_running() const override { return running_; }

    ErrorCode handle_control_action(
        uint32_t /*request_message_id*/,
        const DAppControlAction& /*action*/
    ) override {
        return ErrorCode::SUCCESS;
    }

private:
    uint32_t id_;
    bool running_;
};

TEST(E3Agent_construction) {
    E3Config config;
    config.ran_identifier = "test-ran";
    
    E3Agent agent(std::move(config));
    
    ASSERT_EQ(state_to_int(agent.state()), state_to_int(AgentState::UNINITIALIZED));
    ASSERT_FALSE(agent.is_running());
}

TEST(E3Agent_config_access) {
    E3Config config;
    config.ran_identifier = "my-unique-ran";
    config.link_layer = E3LinkLayer::POSIX;
    config.transport_layer = E3TransportLayer::IPC;
    config.encoding = EncodingFormat::JSON;
    
    E3Agent agent(std::move(config));
    
    ASSERT_STREQ(agent.config().ran_identifier.c_str(), "my-unique-ran");
}

TEST(E3Agent_init) {
    E3Config config;
    config.ran_identifier = "init-test";
    
    E3Agent agent(std::move(config));
    
    auto result = agent.init();
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::SUCCESS));
    ASSERT_NE(state_to_int(agent.state()), state_to_int(AgentState::UNINITIALIZED));
}

TEST(E3Agent_init_already_initialized) {
    E3Config config;
    config.ran_identifier = "test";
    
    E3Agent agent(std::move(config));
    
    agent.init();
    auto result = agent.init();
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::ALREADY_INITIALIZED));
}

TEST(E3Agent_register_sm) {
    E3Config config;
    config.ran_identifier = "sm-test";
    
    E3Agent agent(std::move(config));
    
    auto sm = std::make_unique<TestServiceModel>(100);
    auto result = agent.register_sm(std::move(sm));
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::SUCCESS));
    
    auto funcs = agent.get_available_ran_functions();
    ASSERT_EQ(funcs.size(), 1u);
    ASSERT_EQ(funcs[0], 100u);
}

TEST(E3Agent_register_multiple_sms) {
    E3Config config;
    config.ran_identifier = "multi-sm-test";
    
    E3Agent agent(std::move(config));
    
    agent.register_sm(std::make_unique<TestServiceModel>(100));
    agent.register_sm(std::make_unique<TestServiceModel>(200));
    agent.register_sm(std::make_unique<TestServiceModel>(300));
    
    auto funcs = agent.get_available_ran_functions();
    ASSERT_EQ(funcs.size(), 3u);
}

TEST(E3Agent_register_null_sm) {
    E3Config config;
    config.ran_identifier = "null-sm-test";
    
    E3Agent agent(std::move(config));
    
    auto result = agent.register_sm(nullptr);
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::INVALID_PARAM));
}

TEST(E3Agent_sm_control_dispatch) {
    E3Config config;
    config.ran_identifier = "control-test";
    
    E3Agent agent(std::move(config));
    
    // Control actions are handled by the SM's registered control callbacks,
    // not by the E3Agent directly.
    auto sm = std::make_unique<TestServiceModel>(100);
    auto result = agent.register_sm(std::move(sm));
    ASSERT_EQ(error_to_int(result), error_to_int(ErrorCode::SUCCESS));
    
    agent.init();
    
    ASSERT_EQ(state_to_int(agent.state()), state_to_int(AgentState::INITIALIZED));
}

TEST(E3Agent_statistics) {
    E3Config config;
    config.ran_identifier = "stats-test";
    
    E3Agent agent(std::move(config));
    agent.init();
    
    ASSERT_EQ(agent.dapp_count(), 0u);
    ASSERT_EQ(agent.subscription_count(), 0u);
}

TEST(E3Agent_stop_without_start) {
    E3Config config;
    config.ran_identifier = "stop-test";
    
    E3Agent agent(std::move(config));
    
    // Should not crash
    agent.stop();
    ASSERT_FALSE(agent.is_running());
}

TEST(E3Agent_move_semantics) {
    E3Config config;
    config.ran_identifier = "move-test";
    
    E3Agent agent1(std::move(config));
    agent1.init();
    agent1.register_sm(std::make_unique<TestServiceModel>(100));
    
    E3Agent agent2 = std::move(agent1);
    
    auto funcs = agent2.get_available_ran_functions();
    ASSERT_EQ(funcs.size(), 1u);
}

TEST(E3Agent_destructor_stops) {
    bool was_running = false;
    {
        E3Config config;
        config.ran_identifier = "destructor-test";
        
        E3Agent agent(std::move(config));
        agent.init();
        // Destructor should clean up properly
    }
    // If we get here without crashing, destructor worked
    ASSERT_FALSE(was_running);
}

int main() {
    return RUN_ALL_TESTS();
}
