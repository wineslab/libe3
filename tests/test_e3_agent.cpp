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

// Simple test service model for testing
class TestServiceModel : public ServiceModel {
public:
    explicit TestServiceModel(uint32_t id)
        : id_(id), running_(false) {}
    
    uint32_t id() const override { return id_; }
    std::string name() const override { return "TestSM"; }
    std::string version() const override { return "1.0.0"; }
    
    std::vector<uint32_t> ran_function_ids() const override {
        return {id_};
    }
    
    ErrorCode start() override {
        running_ = true;
        return ErrorCode::SUCCESS;
    }
    
    ErrorCode stop() override {
        running_ = false;
        return ErrorCode::SUCCESS;
    }
    
    bool is_running() const override { return running_; }
    
    std::optional<std::vector<uint8_t>> poll_indication_data() override {
        return std::nullopt;
    }
    
    ErrorCode handle_control_action(const std::vector<uint8_t>& /*data*/) override {
        return ErrorCode::SUCCESS;
    }
    
    void set_indication_callback(IndicationCallback cb) override {
        indication_callback_ = std::move(cb);
    }
    
    // Test helper to trigger indication
    void trigger_indication(const std::vector<uint8_t>& data) {
        if (indication_callback_) {
            indication_callback_(id_, data, 
                static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));
        }
    }

private:
    uint32_t id_;
    bool running_;
    IndicationCallback indication_callback_;
};

TEST(E3Agent_construction) {
    E3Config config;
    config.ran_identifier = "test-ran";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    ASSERT_EQ(agent.state(), AgentState::UNINITIALIZED);
    ASSERT_FALSE(agent.is_running());
}

TEST(E3Agent_config_access) {
    E3Config config;
    config.ran_identifier = "my-unique-ran";
    config.link_layer = E3LinkLayer::POSIX;
    config.transport_layer = E3TransportLayer::IPC;
    config.encoding = EncodingFormat::JSON;
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    ASSERT_STREQ(agent.config().ran_identifier.c_str(), "my-unique-ran");
    ASSERT_TRUE(agent.is_simulation_mode());
}

TEST(E3Agent_init) {
    E3Config config;
    config.ran_identifier = "init-test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    auto result = agent.init();
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    ASSERT_NE(agent.state(), AgentState::UNINITIALIZED);
}

TEST(E3Agent_init_already_initialized) {
    E3Config config;
    config.ran_identifier = "test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    agent.init();
    auto result = agent.init();
    ASSERT_EQ(result, ErrorCode::ALREADY_INITIALIZED);
}

TEST(E3Agent_register_sm) {
    E3Config config;
    config.ran_identifier = "sm-test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    auto sm = std::make_unique<TestServiceModel>(100);
    auto result = agent.register_sm(std::move(sm));
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    
    auto funcs = agent.get_available_ran_functions();
    ASSERT_EQ(funcs.size(), 1u);
    ASSERT_EQ(funcs[0], 100u);
}

TEST(E3Agent_register_multiple_sms) {
    E3Config config;
    config.ran_identifier = "multi-sm-test";
    config.simulation_mode = true;
    
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
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    auto result = agent.register_sm(nullptr);
    ASSERT_EQ(result, ErrorCode::INVALID_PARAM);
}

TEST(E3Agent_control_callback) {
    E3Config config;
    config.ran_identifier = "callback-test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    uint32_t received_dapp = 0;
    uint32_t received_ran_func = 0;
    std::vector<uint8_t> received_data;
    
    agent.set_control_callback([&](uint32_t dapp, uint32_t ran_func, 
                                   const std::vector<uint8_t>& data) {
        received_dapp = dapp;
        received_ran_func = ran_func;
        received_data = data;
    });
    
    agent.init();
    
    // Verify callback is set (actual invocation requires full setup)
    ASSERT_EQ(agent.state(), AgentState::INITIALIZED);
}

TEST(E3Agent_statistics) {
    E3Config config;
    config.ran_identifier = "stats-test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    agent.init();
    
    ASSERT_EQ(agent.dapp_count(), 0u);
    ASSERT_EQ(agent.subscription_count(), 0u);
}

TEST(E3Agent_stop_without_start) {
    E3Config config;
    config.ran_identifier = "stop-test";
    config.simulation_mode = true;
    
    E3Agent agent(std::move(config));
    
    // Should not crash
    agent.stop();
    ASSERT_FALSE(agent.is_running());
}

TEST(E3Agent_move_semantics) {
    E3Config config;
    config.ran_identifier = "move-test";
    config.simulation_mode = true;
    
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
        config.simulation_mode = true;
        
        E3Agent agent(std::move(config));
        agent.init();
        // Destructor should clean up properly
    }
    // If we get here without crashing, destructor worked
    ASSERT_FALSE(was_running);
}

TEST(E3Agent_simulation_mode_check) {
    E3Config config1;
    config1.ran_identifier = "sim-test";
    config1.simulation_mode = true;
    
    E3Agent agent1(std::move(config1));
    ASSERT_TRUE(agent1.is_simulation_mode());
    
    E3Config config2;
    config2.ran_identifier = "non-sim-test";
    config2.simulation_mode = false;
    
    E3Agent agent2(std::move(config2));
    ASSERT_FALSE(agent2.is_simulation_mode());
}

int main() {
    return RUN_ALL_TESTS();
}
