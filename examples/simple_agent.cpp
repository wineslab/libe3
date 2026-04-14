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
#include <ctime>
#include <getopt.h>
// Simple SM wrappers
#include "sm_simple/e3sm_simple_wrapper.hpp"

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

    libe3::ErrorCode init() override { return libe3::ErrorCode::SUCCESS; }

    void destroy() override {
        stop();
    }

    libe3::ErrorCode start() override {
        if (running_) {
            return libe3::ErrorCode::SUCCESS;
        }
        running_ = true;
        worker_ = std::thread(&SimpleServiceModel::worker_loop, this);
        return libe3::ErrorCode::SUCCESS;
    }

    std::vector<uint8_t> ran_function_data() const override {
        const std::string name = "SIMPLE";
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data(name, out)) {
            return out;
        }
        return {};
    }

    void stop() override {
        if (!running_) {
            return;
        }
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool is_running() const override { return running_; }

    libe3::ErrorCode handle_control_action(
        uint32_t request_message_id,
        const libe3::DAppControlAction& action
    ) override {
        int sampling = 0;
        bool decode_ok = libe3_examples::decode_simple_control(action.action_data, sampling);
        if (decode_ok) {
            std::cout << "[SIMPLE] Control action " << action.control_identifier
                      << ": samplingThreshold=" << sampling << "\n";
        } else {
            std::cout << "[SIMPLE] Control action " << action.control_identifier
                      << ": failed to decode (" << action.action_data.size() << " bytes)\n";
        }

        libe3::Pdu ack_pdu = make_message_ack_pdu(
            request_message_id,
            decode_ok ? libe3::ResponseCode::POSITIVE : libe3::ResponseCode::NEGATIVE
        );
        return emit_outbound(std::move(ack_pdu));
    }

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
    uint32_t seq_{0};

    void worker_loop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!running_) {
                break;
            }

            auto subscribers = get_subscribers();
            if (subscribers.empty()) {
                continue;
            }

            libe3_examples::SimpleIndication si;
            si.data1 = seq_;
            si.timestamp = static_cast<uint32_t>(std::time(nullptr));

            std::vector<uint8_t> encoded;
            if (!libe3_examples::encode_simple_indication(si, encoded)) {
                std::cerr << "Failed to encode Simple-Indication\n";
                continue;
            }

            for (uint32_t dapp_id : subscribers) {
                libe3::Pdu pdu = make_indication_pdu(dapp_id, RAN_FUNCTION_ID, encoded);
                auto rc = emit_outbound(std::move(pdu));
                if (rc == libe3::ErrorCode::SUCCESS) {
                    std::cout << "  -> Sent indication #" << seq_
                              << " to dApp " << dapp_id
                              << " (" << encoded.size() << " bytes)\n";
                } else {
                    std::cerr << "  -> Failed to send indication to dApp "
                              << dapp_id << ": "
                              << libe3::error_code_to_string(rc) << "\n";
                }
            }
            ++seq_;
        }
    }
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
    // Enable debug logging for diagnostics
    config.log_level = 4;
    
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

    // Handle dApp status changes (connect, subscribe, unsubscribe, disconnect)
    agent.set_dapp_status_changed_handler([&agent]() {
        std::cout << "[STATUS] dApp status changed — "
                  << "dApps: " << agent.dapp_count()
                  << ", subscriptions: " << agent.subscription_count() << "\n";
    });

    // Handle dApp reports for the RAN
    agent.set_dapp_report_handler([](const libe3::DAppReport& report) {
        // In the RAN report is sent to the xApp through E2, here we just decode it as en example
        libe3_examples::SimpleDAppReport decoded;
        if (libe3_examples::decode_simple_dapp_report(report.report_data, decoded)) {
            std::cout << "[SIMPLE] dApp report from dApp " << report.dapp_identifier
                      << " (RAN function " << report.ran_function_identifier
                      << "): bin1=" << decoded.bin1 << "\n";
        } else {
            std::cout << "[SIMPLE] dApp report from dApp " << report.dapp_identifier
                      << " (" << report.report_data.size() << " bytes, decode failed)\n";
        }
    });
    
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
    
    // Main loop — keep process alive and print statistics
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
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
