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
#include <cstring>
#include <getopt.h>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

class SimpleServiceModel : public libe3::ServiceModel {
public:
    static constexpr uint32_t RAN_FUNCTION_ID = 1;

    std::string name() const override { return "SIMPLE"; }
    uint32_t version() const override { return 1; }

    uint32_t ran_function_id() const override {
        return RAN_FUNCTION_ID;
    }

    std::vector<uint32_t> telemetry_ids() const override {
        return {1};
    }

    std::vector<uint32_t> control_ids() const override {
        return {1};
    }

    libe3::ErrorCode init() override {
        register_control_callback(1, [](const std::vector<uint8_t>& data) {
            std::cout << "[SIMPLE] Control action 1 (" << data.size() << " bytes)\n";
            return libe3::ErrorCode::SUCCESS;
        });
        return libe3::ErrorCode::SUCCESS;
    }

    void destroy() override {
        stop();
    }

    libe3::ErrorCode start() override {
        running_ = true;
        return libe3::ErrorCode::SUCCESS;
    }

    void stop() override {
        running_ = false;
    }

    bool is_running() const override { return running_; }

private:
    std::atomic<bool> running_{false};
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -l, --link <layer>       Link layer: zmq, posix (default: zmq)\n"
              << "  -t, --transport <layer>  Transport layer: sctp, tcp, ipc (default: ipc)\n"
              << "  -e, --encoding <format>  Encoding format: asn1, json (default: asn1)\n"
              << "  -h, --help               Show this help message\n";
}

libe3::E3LinkLayer parse_link_layer(const char* str) {
    if (std::strcmp(str, "zmq") == 0) return libe3::E3LinkLayer::ZMQ;
    if (std::strcmp(str, "posix") == 0) return libe3::E3LinkLayer::POSIX;
    std::cerr << "Invalid link layer: " << str << ". Using default (zmq).\n";
    return libe3::E3LinkLayer::POSIX;
}

libe3::E3TransportLayer parse_transport_layer(const char* str) {
    if (std::strcmp(str, "sctp") == 0) return libe3::E3TransportLayer::SCTP;
    if (std::strcmp(str, "tcp") == 0) return libe3::E3TransportLayer::TCP;
    if (std::strcmp(str, "ipc") == 0) return libe3::E3TransportLayer::IPC;
    std::cerr << "Invalid transport layer: " << str << ". Using default (ipc).\n";
    return libe3::E3TransportLayer::IPC;
}

libe3::EncodingFormat parse_encoding(const char* str) {
    if (std::strcmp(str, "asn1") == 0) return libe3::EncodingFormat::ASN1;
    if (std::strcmp(str, "json") == 0) return libe3::EncodingFormat::JSON;
    std::cerr << "Invalid encoding format: " << str << ". Using default (asn1).\n";
    return libe3::EncodingFormat::JSON;
}

int main(int argc, char* argv[]) {
    // Default configuration
    libe3::E3LinkLayer link_layer = libe3::E3LinkLayer::ZMQ;
    libe3::E3TransportLayer transport_layer = libe3::E3TransportLayer::IPC;
    libe3::EncodingFormat encoding = libe3::EncodingFormat::ASN1;
    std::string ran_id = "example-ran-001";

    // Command-line options
    static struct option long_options[] = {
        {"link",      required_argument, nullptr, 'l'},
        {"transport", required_argument, nullptr, 't'},
        {"encoding",  required_argument, nullptr, 'e'},
        {"help",      no_argument,       nullptr, 'h'},
        {nullptr,     0,                 nullptr,  0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "l:t:e:r:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'l':
                link_layer = parse_link_layer(optarg);
                break;
            case 't':
                transport_layer = parse_transport_layer(optarg);
                break;
            case 'e':
                encoding = parse_encoding(optarg);
                break;
            case 'r':
                ran_id = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    std::cout << "libe3 Simple Agent Example\n";
    std::cout << "Version: " << LIBE3_VERSION_STRING << "\n\n";
    
    // Set up signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Configure the E3 agent
    libe3::E3Config config;
    config.ran_identifier = ran_id;
    config.link_layer = link_layer;
    config.transport_layer = transport_layer;
    config.encoding = encoding;
    
    std::cout << "Configuration:\n"
              << "  RAN ID: " << ran_id << "\n"
              << "  Link layer: " << libe3::link_layer_to_string(link_layer) << "\n"
              << "  Transport layer: " << libe3::transport_layer_to_string(transport_layer) << "\n"
              << "  Encoding: " << (encoding == libe3::EncodingFormat::JSON ? "json" : "asn1") << "\n\n";
    
    // Create the agent
    libe3::E3Agent agent(std::move(config));

    // Initialize the agent
    auto init_result = agent.init();
    if (init_result != libe3::ErrorCode::SUCCESS && 
        init_result != libe3::ErrorCode::ALREADY_INITIALIZED) {
        std::cerr << "Failed to initialize agent: " 
                  << libe3::error_code_to_string(init_result) << "\n";
        return 1;
    }

    // Register the simple service model
    auto sm_result = agent.register_sm(std::make_unique<SimpleServiceModel>());
    if (sm_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to register Simple SM: "
                  << libe3::error_code_to_string(sm_result) << "\n";
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
    
    // Main loop — periodically send mock indication data to subscribers
    uint32_t seq = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Print statistics periodically
        std::cout << "  dApps: " << agent.dapp_count()
                  << ", Subscriptions: " << agent.subscription_count() << "\n";
        
        // Deliver mock indication data to every dApp subscribed to the simple SM
        auto subscribers = agent.get_ran_function_subscribers(
            SimpleServiceModel::RAN_FUNCTION_ID);
        
        if (!subscribers.empty()) {
            // Build a small mock payload: 4-byte sequence number + 12 bytes of fake metrics
            std::vector<uint8_t> mock_data(16);
            mock_data[0] = static_cast<uint8_t>((seq >> 24) & 0xFF);
            mock_data[1] = static_cast<uint8_t>((seq >> 16) & 0xFF);
            mock_data[2] = static_cast<uint8_t>((seq >>  8) & 0xFF);
            mock_data[3] = static_cast<uint8_t>((seq      ) & 0xFF);
            // Fill remaining bytes with a simple pattern
            for (size_t i = 4; i < mock_data.size(); ++i) {
                mock_data[i] = static_cast<uint8_t>(i + seq);
            }
            
            for (uint32_t dapp_id : subscribers) {
                auto rc = agent.send_indication(
                    dapp_id, SimpleServiceModel::RAN_FUNCTION_ID, mock_data);
                if (rc == libe3::ErrorCode::SUCCESS) {
                    std::cout << "  -> Sent indication #" << seq
                              << " to dApp " << dapp_id
                              << " (" << mock_data.size() << " bytes)\n";
                } else {
                    std::cerr << "  -> Failed to send indication to dApp "
                              << dapp_id << ": "
                              << libe3::error_code_to_string(rc) << "\n";
                }
            }
            ++seq;
        }
    }
    
    std::cout << "\nStopping agent...\n";
    agent.stop();
    
    std::cout << "Agent stopped. State: " 
              << libe3::agent_state_to_string(agent.state()) << "\n";
    
    return 0;
}
