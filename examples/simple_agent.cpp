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
#include <chrono>
#include <thread>
#include <cstring>
#include <ctime>
#include <getopt.h>
// Simple SM wrappers
#include "sm_simple/e3sm_simple_wrapper.hpp"
// Shared Simple Service Model (also used by the full-loop latency benchmark)
#include "sm_simple/simple_service_model.hpp"

using libe3_examples::SimpleServiceModel;

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -l, --link <layer>       Link layer: zmq, posix (default: zmq)\n"
              << "  -t, --transport <layer>  Transport layer: sctp, tcp, ipc (default: ipc)\n"
              << "  -e, --encoding <format>  Encoding format: asn1, json, protobuf (default: asn1)\n"
              << "  -r, --ran_id <id>        RAN identifier advertised in setup (default: example-ran-001)\n"
              << "  -d, --socket-dir <dir>   IPC socket directory (default: /tmp/dapps).\n"
              << "                           Lets multiple RANs coexist on one host over IPC.\n"
              << "      --port-offset <K>    TCP port offset added to the default ports\n"
              << "                           (setup 9990+K, subscriber 9999+K, publisher 9991+K).\n"
              << "                           Lets multiple RANs coexist on one host over TCP.\n"
              << "      --period-us <N>      Emit one indication every N microseconds\n"
              << "                           (default: 2000000 = 2 s). e.g. 11494 ~= 87 ind/s\n"
              << "                           (one per slot); sub-ms values stress queueing.\n"
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
    if (std::strcmp(str, "protobuf") == 0) return libe3::EncodingFormat::PROTOBUF;
    std::cerr << "Invalid encoding format: " << str << ". Using default (asn1).\n";
    return libe3::EncodingFormat::ASN1;
}

static const char* encoding_to_cstr(libe3::EncodingFormat enc) {
    switch (enc) {
        case libe3::EncodingFormat::JSON:     return "json";
        case libe3::EncodingFormat::PROTOBUF: return "protobuf";
        case libe3::EncodingFormat::ASN1:     return "asn1";
    }
    return "asn1";
}

int main(int argc, char* argv[]) {
    // Default configuration
    libe3::E3LinkLayer link_layer = libe3::E3LinkLayer::ZMQ;
    libe3::E3TransportLayer transport_layer = libe3::E3TransportLayer::IPC;
    libe3::EncodingFormat encoding = libe3::EncodingFormat::ASN1;
    std::string ran_id = "example-ran-001";
    std::string socket_dir;   // empty => library default (/tmp/dapps)
    int port_offset = 0;      // TCP: added to the default ports
    uint64_t period_us = 2'000'000;  // indication emission period (default 2s)

    // Command-line options. --port-offset (1000) and --period-us (1001) are long-only.
    static struct option long_options[] = {
        {"link",        required_argument, nullptr, 'l'},
        {"transport",   required_argument, nullptr, 't'},
        {"encoding",    required_argument, nullptr, 'e'},
        {"ran_id",      required_argument, nullptr, 'r'},
        {"socket-dir",  required_argument, nullptr, 'd'},
        {"port-offset", required_argument, nullptr, 1000},
        {"period-us",   required_argument, nullptr, 1001},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr,       0,                 nullptr,  0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "l:t:e:r:d:h", long_options, nullptr)) != -1) {
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
            case 'd':
                socket_dir = optarg;
                break;
            case 1000:
                port_offset = std::atoi(optarg);
                break;
            case 1001:
                period_us = std::strtoull(optarg, nullptr, 10);  // --period-us
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

    // Optional per-RAN addressing so multiple RANs can coexist on one host:
    //   IPC -> a distinct socket directory; TCP -> a distinct port offset.
    if (!socket_dir.empty()) {
        config.setup_endpoint      = "ipc://" + socket_dir + "/setup";
        config.subscriber_endpoint = "ipc://" + socket_dir + "/dapp_socket";
        config.publisher_endpoint  = "ipc://" + socket_dir + "/e3_socket";
    }
    if (port_offset != 0) {
        config.setup_port      = static_cast<uint16_t>(config.setup_port + port_offset);
        config.subscriber_port = static_cast<uint16_t>(config.subscriber_port + port_offset);
        config.publisher_port  = static_cast<uint16_t>(config.publisher_port + port_offset);
    }

    std::cout << "Configuration:\n"
              << "  RAN ID: " << ran_id << "\n"
              << "  Link layer: " << libe3::link_layer_to_string(link_layer) << "\n"
              << "  Transport layer: " << libe3::transport_layer_to_string(transport_layer) << "\n"
              << "  Encoding: " << encoding_to_cstr(encoding) << "\n"
              << "  Socket dir: " << (socket_dir.empty() ? "/tmp/dapps (default)" : socket_dir) << "\n"
              << "  Port offset: " << port_offset << "\n"
              << "  Period: " << period_us << " us\n\n";
    
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

    // Register the simple service model (with the configured emission period)
    auto sm_result = agent.register_sm(std::make_unique<SimpleServiceModel>(period_us, encoding));
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
    agent.set_dapp_report_handler([encoding](const libe3::DAppReport& report) {
        // In the RAN report is sent to the xApp through E2, here we just decode it as en example
        libe3_examples::SimpleDAppReport decoded;
        if (libe3_examples::decode_simple_dapp_report(report.report_data, decoded, encoding)) {
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
