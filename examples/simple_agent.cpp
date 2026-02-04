/**
 * @file simple_agent.cpp
 * @brief Simple E3 Agent example
 *
 * Demonstrates basic usage of libe3 to create an E3 agent
 * that can communicate with dApps.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::cout << "libe3 Simple Agent Example\n";
    std::cout << "Version: " << LIBE3_VERSION_STRING << "\n\n";
    
    // Set up signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Configure the E3 agent
    libe3::E3Config config;
    config.ran_identifier = "example-ran-001";
    config.link_layer = libe3::E3LinkLayer::POSIX;
    config.transport_layer = libe3::E3TransportLayer::IPC;
    config.encoding = libe3::EncodingFormat::JSON;
    
    // Create the agent
    libe3::E3Agent agent(std::move(config));
    
    // Set up control action callback
    agent.set_control_callback([](uint32_t dapp_id, 
                                  uint32_t ran_function_id,
                                  const std::vector<uint8_t>& data) {
        std::cout << "Received control action from dApp " << dapp_id
                  << " for RAN function " << ran_function_id
                  << " (" << data.size() << " bytes)\n";
        return libe3::ErrorCode::SUCCESS;
    });
    
    // Set up indication callback
    agent.set_indication_callback([](uint32_t dapp_id,
                                     uint32_t ran_function_id,
                                     const std::vector<uint8_t>& data) {
        std::cout << "Sending indication to dApp " << dapp_id
                  << " for RAN function " << ran_function_id
                  << " (" << data.size() << " bytes)\n";
    });
    
    // Initialize the agent
    auto init_result = agent.init();
    if (init_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize agent: " 
                  << libe3::error_code_to_string(init_result) << "\n";
        return 1;
    }
    
    // Start the agent
    auto start_result = agent.start();
    if (start_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to start agent: "
                  << libe3::error_code_to_string(start_result) << "\n";
        return 1;
    }
    
    std::cout << "Agent started successfully\n";
    std::cout << "State: " << libe3::agent_state_to_string(agent.state()) << "\n";
    std::cout << "Press Ctrl+C to stop...\n\n";
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print statistics periodically
        std::cout << "  dApps: " << agent.dapp_count()
                  << ", Subscriptions: " << agent.subscription_count() << "\n";
    }
    
    std::cout << "\nStopping agent...\n";
    agent.stop();
    
    std::cout << "Agent stopped. State: " 
              << libe3::agent_state_to_string(agent.state()) << "\n";
    
    return 0;
}
