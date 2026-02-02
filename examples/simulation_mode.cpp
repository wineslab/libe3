/**
 * @file simulation_mode.cpp
 * @brief Example of using libe3 in simulation mode
 *
 * Demonstrates how to use libe3's simulation mode for testing
 * without requiring actual RAN connectivity or dApp infrastructure.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief Mock Service Model for simulation
 */
class MockServiceModel : public libe3::ServiceModel {
public:
    MockServiceModel(uint32_t id, std::string name)
        : id_(id), name_(std::move(name)) {}
    
    uint32_t id() const override { return id_; }
    std::string name() const override { return name_; }
    std::string version() const override { return "1.0.0"; }
    
    std::vector<uint32_t> ran_function_ids() const override {
        return {id_};
    }
    
    libe3::ErrorCode start() override {
        running_ = true;
        std::cout << "  [" << name_ << "] Started\n";
        return libe3::ErrorCode::SUCCESS;
    }
    
    libe3::ErrorCode stop() override {
        running_ = false;
        std::cout << "  [" << name_ << "] Stopped\n";
        return libe3::ErrorCode::SUCCESS;
    }
    
    bool is_running() const override { return running_; }
    
    std::optional<std::vector<uint8_t>> poll_indication_data() override {
        return std::nullopt;
    }
    
    libe3::ErrorCode handle_control_action(const std::vector<uint8_t>& data) override {
        std::cout << "  [" << name_ << "] Control action received: " 
                  << data.size() << " bytes\n";
        control_count_++;
        return libe3::ErrorCode::SUCCESS;
    }
    
    void set_indication_callback(IndicationCallback cb) override {
        callback_ = std::move(cb);
    }
    
    // Simulate sending indication data
    void simulate_indication(const std::vector<uint8_t>& data) {
        if (callback_) {
            auto now = std::chrono::system_clock::now();
            auto ts = static_cast<uint64_t>(now.time_since_epoch().count());
            callback_(id_, data, ts);
        }
    }
    
    uint32_t get_control_count() const { return control_count_; }

private:
    uint32_t id_;
    std::string name_;
    std::atomic<bool> running_{false};
    IndicationCallback callback_;
    std::atomic<uint32_t> control_count_{0};
};

int main() {
    std::cout << "========================================\n";
    std::cout << "  libe3 Simulation Mode Example\n";
    std::cout << "  Version: " << LIBE3_VERSION_STRING << "\n";
    std::cout << "========================================\n\n";
    
    // --------------------------------------------------------
    // 1. Configure for Simulation Mode
    // --------------------------------------------------------
    std::cout << "1. Configuring E3Agent in simulation mode...\n";
    
    libe3::E3Config config;
    config.ran_identifier = "simulated-ran";
    config.simulation_mode = true;   // <-- Key setting for simulation
    config.link_layer = libe3::E3LinkLayer::POSIX;
    config.transport_layer = libe3::E3TransportLayer::IPC;
    config.encoding = libe3::EncodingFormat::JSON;
    
    libe3::E3Agent agent(std::move(config));
    std::cout << "   Simulation mode: " << (agent.is_simulation_mode() ? "enabled" : "disabled") << "\n\n";
    
    // --------------------------------------------------------
    // 2. Register Mock Service Models
    // --------------------------------------------------------
    std::cout << "2. Registering mock service models...\n";
    
    auto kpm_sm = std::make_unique<MockServiceModel>(100, "KPM");
    auto rc_sm = std::make_unique<MockServiceModel>(200, "RC");
    auto ni_sm = std::make_unique<MockServiceModel>(300, "NI");
    
    agent.register_sm(std::move(kpm_sm));
    agent.register_sm(std::move(rc_sm));
    agent.register_sm(std::move(ni_sm));
    
    std::cout << "   Registered " << agent.get_available_ran_functions().size() 
              << " RAN functions\n\n";
    
    // --------------------------------------------------------
    // 3. Set Up Event Handlers
    // --------------------------------------------------------
    std::cout << "3. Setting up event handlers...\n";
    
    int control_events = 0;
    int indication_events = 0;
    
    agent.set_control_callback([&](uint32_t dapp, uint32_t ran_func,
                                   const std::vector<uint8_t>& data) {
        control_events++;
        std::cout << "   [CONTROL] dApp=" << dapp 
                  << " RAN_func=" << ran_func
                  << " data_size=" << data.size() << "\n";
    });
    
    agent.set_indication_callback([&](uint32_t dapp, uint32_t ran_func,
                                      const std::vector<uint8_t>& data) {
        indication_events++;
        std::cout << "   [INDICATION] dApp=" << dapp
                  << " RAN_func=" << ran_func
                  << " data_size=" << data.size() << "\n";
    });
    
    std::cout << "   Handlers configured\n\n";
    
    // --------------------------------------------------------
    // 4. Initialize and Start Agent
    // --------------------------------------------------------
    std::cout << "4. Starting agent...\n";
    
    auto result = agent.start();
    if (result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "   ERROR: " << libe3::error_code_to_string(result) << "\n";
        return 1;
    }
    
    std::cout << "   Agent state: " << libe3::agent_state_to_string(agent.state()) << "\n\n";
    
    // --------------------------------------------------------
    // 5. Simulate dApp Registration and Subscription
    // --------------------------------------------------------
    std::cout << "5. Simulating dApp interactions...\n";
    std::cout << "   (In simulation mode, we can manually trigger events)\n\n";
    
    // Simulate some time passing
    std::cout << "   Simulating 3 seconds of operation...\n";
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "   [" << (i+1) << "s] dApps=" << agent.dapp_count()
                  << " subscriptions=" << agent.subscription_count() << "\n";
    }
    
    std::cout << "\n";
    
    // --------------------------------------------------------
    // 6. Print Final Statistics
    // --------------------------------------------------------
    std::cout << "6. Final statistics:\n";
    std::cout << "   - Control events:    " << control_events << "\n";
    std::cout << "   - Indication events: " << indication_events << "\n";
    std::cout << "   - Registered dApps:  " << agent.dapp_count() << "\n";
    std::cout << "   - Active subscriptions: " << agent.subscription_count() << "\n\n";
    
    // --------------------------------------------------------
    // 7. Clean Shutdown
    // --------------------------------------------------------
    std::cout << "7. Stopping agent...\n";
    agent.stop();
    std::cout << "   Final state: " << libe3::agent_state_to_string(agent.state()) << "\n\n";
    
    std::cout << "========================================\n";
    std::cout << "  Simulation complete!\n";
    std::cout << "========================================\n";
    
    return 0;
}
