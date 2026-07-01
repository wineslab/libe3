/**
 * @file simple_dapp.cpp
 * @brief Minimal example dApp using libe3 in DAPP role.
 *
 * Mirrors examples/simple_agent.cpp. Connects to one OR MORE libe3-based RAN
 * agents (one E3Agent instance per RAN — the dApp can drive several RANs at
 * once), registers handlers for incoming indications and xApp control actions,
 * subscribes to the Simple service model (RAN function id 1), and:
 *   - decodes every indication via the sm_simple wrapper
 *   - optionally sends a Simple-Control action every 5th indication (--control)
 *
 * Each RAN peer is selected per the chosen transport: repeated --socket-dir for
 * IPC, repeated --port-offset for TCP. With no peer flags it connects to the
 * single default RAN (today's behaviour). Every indication line is tagged with
 * the originating peer's ran identifier and subscription id so the source of a
 * message is unambiguous when several RANs are connected at once.
 *
 * Pairs with examples/simple_agent.cpp and is wire-compatible with the
 * Python spear-dApp/src/simple/simple_dapp.py.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include "sm_simple/e3sm_simple_wrapper.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <getopt.h>

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running = false;
}

static void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -l, --link <layer>       Link layer: zmq, posix (default: zmq)\n"
              << "  -t, --transport <layer>  Transport: sctp, tcp, ipc (default: ipc)\n"
              << "  -e, --encoding <format>  Encoding: asn1, json, protobuf (default: asn1)\n"
              << "  -c, --control            Send a Simple-Control every 5th indication\n"
              << "  -T, --timed <secs>       Stop after this many seconds (0 = unlimited)\n"
              << "  -d, --socket-dir <dir>   IPC socket dir of a RAN to connect to.\n"
              << "                           Repeatable: one per RAN (default: /tmp/dapps).\n"
              << "      --port-offset <K>    TCP port offset of a RAN to connect to.\n"
              << "                           Repeatable: one per RAN (default: 0).\n"
              << "  -q, --quiet              Suppress per-indication lines (keep the summary).\n"
              << "                           Use at high (sub-ms) rates so stdout I/O does not\n"
              << "                           dominate the queueing-time measurement.\n"
              << "  -h, --help               Show this help message\n";
}

// One connected RAN peer: its own E3Agent instance plus the identity the RAN
// hands back (ran_identifier on setup, subscription_id on subscribe) so each
// indication can be attributed to the RAN it came from.
struct Peer {
    std::string label;                  // human-readable tag (socket-dir basename / off<K>)
    std::string socket_dir;             // IPC addressing (empty => default)
    int port_offset{0};                 // TCP addressing
    std::unique_ptr<libe3::E3Agent> agent;
    std::string ran_id;                 // filled from SetupResponse
    std::optional<uint32_t> sub_id;     // filled from SubscriptionResponse
    std::atomic<uint32_t> count{0};     // indications received from this RAN
    // Queueing-time accounting (test-only; written solely by this peer's
    // inbound thread, read after stop()). age = recv_ms - send_ms.
    uint64_t age_sum_ms{0};
    uint32_t age_max_ms{0};
    uint32_t age_max_seq{0};   // which indication carried the max age
    // coarse age histogram (ms): <=1, 2..5, 6..10, >10
    uint64_t age_le1{0}, age_2_5{0}, age_6_10{0}, age_gt10{0};
    // Data-drop accounting from the monotonic sequence number (data1). With
    // conflate removed we expect ~0 gaps; expected = max-min+1.
    bool seq_seen{false};
    uint32_t min_seq{0};
    uint32_t max_seq{0};
};

static std::string peer_label(const std::string& dir, int offset) {
    if (!dir.empty()) {
        auto slash = dir.find_last_of('/');
        return (slash == std::string::npos) ? dir : dir.substr(slash + 1);
    }
    if (offset != 0) return "off" + std::to_string(offset);
    return "default";
}

static libe3::E3LinkLayer parse_link_layer(const char* str) {
    if (std::strcmp(str, "zmq") == 0)   return libe3::E3LinkLayer::ZMQ;
    if (std::strcmp(str, "posix") == 0) return libe3::E3LinkLayer::POSIX;
    std::cerr << "Invalid link layer: " << str << " (using zmq)\n";
    return libe3::E3LinkLayer::ZMQ;
}

static libe3::E3TransportLayer parse_transport_layer(const char* str) {
    if (std::strcmp(str, "sctp") == 0) return libe3::E3TransportLayer::SCTP;
    if (std::strcmp(str, "tcp") == 0)  return libe3::E3TransportLayer::TCP;
    if (std::strcmp(str, "ipc") == 0)  return libe3::E3TransportLayer::IPC;
    std::cerr << "Invalid transport: " << str << " (using ipc)\n";
    return libe3::E3TransportLayer::IPC;
}

static libe3::EncodingFormat parse_encoding(const char* str) {
    if (std::strcmp(str, "asn1") == 0) return libe3::EncodingFormat::ASN1;
    if (std::strcmp(str, "json") == 0) return libe3::EncodingFormat::JSON;
    if (std::strcmp(str, "protobuf") == 0) return libe3::EncodingFormat::PROTOBUF;
    std::cerr << "Invalid encoding: " << str << " (using asn1)\n";
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
    libe3::E3LinkLayer link_layer = libe3::E3LinkLayer::ZMQ;
    libe3::E3TransportLayer transport_layer = libe3::E3TransportLayer::IPC;
    libe3::EncodingFormat encoding = libe3::EncodingFormat::ASN1;
    bool control_enabled = false;
    bool quiet = false;
    int timed_seconds = 0;
    std::vector<std::string> socket_dirs;   // repeated --socket-dir (IPC peers)
    std::vector<int> port_offsets;          // repeated --port-offset (TCP peers)

    // --port-offset is long-only (val 1000).
    static struct option long_options[] = {
        {"link",        required_argument, nullptr, 'l'},
        {"transport",   required_argument, nullptr, 't'},
        {"encoding",    required_argument, nullptr, 'e'},
        {"control",     no_argument,       nullptr, 'c'},
        {"timed",       required_argument, nullptr, 'T'},
        {"socket-dir",  required_argument, nullptr, 'd'},
        {"port-offset", required_argument, nullptr, 1000},
        {"quiet",       no_argument,       nullptr, 'q'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr,       0,                 nullptr,  0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "l:t:e:cT:d:qh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'l': link_layer = parse_link_layer(optarg); break;
            case 't': transport_layer = parse_transport_layer(optarg); break;
            case 'e': encoding = parse_encoding(optarg); break;
            case 'c': control_enabled = true; break;
            case 'T': timed_seconds = std::atoi(optarg); break;
            case 'd': socket_dirs.emplace_back(optarg); break;
            case 1000: port_offsets.push_back(std::atoi(optarg)); break;
            case 'q': quiet = true; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    std::cout << "libe3 Simple dApp Example\n";
    std::cout << "Version: " << LIBE3_VERSION_STRING << "\n\n";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Build the list of RAN peers to connect to. Each --socket-dir (IPC) or
    // --port-offset (TCP) adds one peer; with none we connect to the single
    // default RAN — preserving the original 1-RAN-1-dApp behaviour.
    std::vector<std::unique_ptr<Peer>> peers;
    if (!socket_dirs.empty()) {
        for (const auto& dir : socket_dirs) {
            auto p = std::make_unique<Peer>();
            p->socket_dir = dir;
            p->label = peer_label(dir, 0);
            peers.push_back(std::move(p));
        }
    } else if (!port_offsets.empty()) {
        for (int off : port_offsets) {
            auto p = std::make_unique<Peer>();
            p->port_offset = off;
            p->label = peer_label("", off);
            peers.push_back(std::move(p));
        }
    } else {
        auto p = std::make_unique<Peer>();
        p->label = peer_label("", 0);
        peers.push_back(std::move(p));
    }

    std::cout << "Configuration:\n"
              << "  Role: dapp\n"
              << "  Link: " << libe3::link_layer_to_string(link_layer) << "\n"
              << "  Transport: " << libe3::transport_layer_to_string(transport_layer) << "\n"
              << "  Encoding: " << encoding_to_cstr(encoding) << "\n"
              << "  Control: " << (control_enabled ? "on" : "off") << "\n"
              << "  Timed (s): " << timed_seconds << "\n"
              << "  RAN peers: " << peers.size() << "\n\n";

    // Construct and wire one E3Agent per RAN peer. Handlers capture the raw
    // Peer* (stable — owned by the vector) so each message is attributed to the
    // RAN it came from.
    for (auto& pp : peers) {
        Peer* p = pp.get();

        libe3::E3Config config;
        config.role = libe3::E3Role::DAPP;
        config.dapp_name = "SimpleDApp-" + p->label;
        config.dapp_version = "1.0.0";
        config.vendor = "WinesLab";
        config.link_layer = link_layer;
        config.transport_layer = transport_layer;
        config.encoding = encoding;
        config.log_level = 4;
        if (!p->socket_dir.empty()) {
            config.setup_endpoint      = "ipc://" + p->socket_dir + "/setup";
            config.subscriber_endpoint = "ipc://" + p->socket_dir + "/dapp_socket";
            config.publisher_endpoint  = "ipc://" + p->socket_dir + "/e3_socket";
        }
        if (p->port_offset != 0) {
            config.setup_port      = static_cast<uint16_t>(config.setup_port + p->port_offset);
            config.subscriber_port = static_cast<uint16_t>(config.subscriber_port + p->port_offset);
            config.publisher_port  = static_cast<uint16_t>(config.publisher_port + p->port_offset);
        }

        p->agent = std::make_unique<libe3::E3Agent>(std::move(config));

        p->agent->set_indication_handler([p, control_enabled, quiet, encoding](const libe3::IndicationMessage& msg) {
            libe3_examples::SimpleIndication si;
            if (!libe3_examples::decode_simple_indication(msg.protocol_data, si, encoding)) {
                std::cerr << "[SIMPLE] peer=" << p->label << " failed to decode indication ("
                          << msg.protocol_data.size() << " bytes)\n";
                return;
            }
            ++p->count;
            const uint32_t seq = si.data1;

            // Track the sequence range so the summary can report dropped
            // indications (gaps) — the data-loss half of the conflate study.
            if (!p->seq_seen) {
                p->seq_seen = true;
                p->min_seq = p->max_seq = seq;
            } else {
                if (seq < p->min_seq) p->min_seq = seq;
                if (seq > p->max_seq) p->max_seq = seq;
            }

            // Indication age = recv time - agent send time (ms), masked into
            // the 31-bit range the agent stamped. Captures transport + inbound
            // queueing delay; with ZMQ_CONFLATE removed this is what could grow
            // if a dApp backlogs. Measured only here in the example/test layer.
            std::string age_str = "n/a";
            if (si.timestamp.has_value()) {
                const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const uint32_t now = static_cast<uint32_t>(now_ms & 0x7FFFFFFF);
                const uint32_t age = (now - *si.timestamp) & 0x7FFFFFFF;  // modular wrap
                p->age_sum_ms += age;
                if (age > p->age_max_ms) { p->age_max_ms = age; p->age_max_seq = seq; }
                if (age <= 1) ++p->age_le1;
                else if (age <= 5) ++p->age_2_5;
                else if (age <= 10) ++p->age_6_10;
                else ++p->age_gt10;
                age_str = std::to_string(age) + "ms";
            }

            // Provenance-tagged: ran= and sub= identify the source RAN.
            // Suppressed under --quiet so stdout I/O can't bottleneck high rates.
            if (!quiet) {
                std::cout << "[SIMPLE] peer=" << p->label
                          << " ran=" << (p->ran_id.empty() ? "?" : p->ran_id)
                          << " sub=" << (p->sub_id ? std::to_string(*p->sub_id) : "?")
                          << " dapp=" << msg.dapp_identifier
                          << " Indication #" << seq
                          << " age=" << age_str
                          << " (RAN function " << msg.ran_function_identifier << ")\n";
            }

            // Every 5th indication, optionally echo back a Simple-Control.
            if (control_enabled && seq % 5 == 0) {
                const int sampling = static_cast<int>(seq % 101);
                std::vector<uint8_t> encoded;
                if (libe3_examples::encode_simple_control(sampling, encoded, encoding)) {
                    auto rc = p->agent->send_control(/*ran_function_id=*/1, /*control_id=*/1, encoded);
                    if (rc == libe3::ErrorCode::SUCCESS) {
                        if (!quiet) {
                            std::cout << "  -> [" << p->label << "] Sent Simple-Control samplingThreshold="
                                      << sampling << "\n";
                        }
                    } else {
                        std::cerr << "  -> [" << p->label << "] Failed to send Simple-Control: "
                                  << libe3::error_code_to_string(rc) << "\n";
                    }
                }
            }
        });

        p->agent->set_xapp_control_handler([p](const libe3::XAppControlAction& a) {
            std::cout << "[SIMPLE] peer=" << p->label
                      << " xApp control for RAN function " << a.ran_function_identifier
                      << " (" << a.xapp_control_data.size() << " bytes)\n";
        });

        p->agent->set_setup_response_handler([p](const libe3::SetupResponse& resp) {
            p->ran_id = resp.ran_identifier;
            std::cout << "[SETUP] peer=" << p->label
                      << " rc=" << libe3::response_code_to_string(resp.response_code)
                      << " ran=" << resp.ran_identifier;
            if (resp.dapp_identifier) std::cout << " assigned_dapp_id=" << *resp.dapp_identifier;
            std::cout << " ran_functions=" << resp.ran_function_list.size() << "\n";
        });

        p->agent->set_subscription_response_handler([p](const libe3::SubscriptionResponse& r) {
            if (r.subscription_id) p->sub_id = *r.subscription_id;
            std::cout << "[SUB] peer=" << p->label
                      << " rc=" << libe3::response_code_to_string(r.response_code)
                      << " request_id=" << r.request_id;
            if (r.subscription_id) std::cout << " subscription_id=" << *r.subscription_id;
            std::cout << "\n";
        });

        p->agent->set_message_ack_handler([p](const libe3::MessageAck& ack) {
            std::cout << "[ACK] peer=" << p->label << " request_id=" << ack.request_id
                      << " rc=" << libe3::response_code_to_string(ack.response_code) << "\n";
        });
    }

    // Start, complete setup, and subscribe each peer.
    for (auto& pp : peers) {
        Peer* p = pp.get();

        auto rc = p->agent->start();
        if (rc != libe3::ErrorCode::SUCCESS) {
            std::cerr << "[" << p->label << "] Failed to start agent: "
                      << libe3::error_code_to_string(rc) << "\n";
            return 1;
        }

        // Wait for the setup handshake (up to 6 s).
        rc = p->agent->wait_for_setup(std::chrono::milliseconds(6000));
        if (rc != libe3::ErrorCode::SUCCESS) {
            std::cerr << "[" << p->label << "] Setup failed: "
                      << libe3::error_code_to_string(rc) << "\n";
            p->agent->stop();
            return 1;
        }
        auto dapp_id = p->agent->dapp_id();
        std::cout << "[" << p->label << "] Setup OK. dApp ID = "
                  << (dapp_id ? std::to_string(*dapp_id) : "<none>") << "\n";

        // Subscribe to the Simple SM (RAN function id 1).
        rc = p->agent->subscribe(/*ran_function_id=*/1, /*telemetry=*/{1}, /*control=*/{1});
        if (rc != libe3::ErrorCode::SUCCESS) {
            std::cerr << "[" << p->label << "] subscribe() failed: "
                      << libe3::error_code_to_string(rc) << "\n";
            p->agent->stop();
            return 1;
        }
        std::cout << "[" << p->label << "] Subscribed to RAN function 1\n";
    }

    const auto start = std::chrono::steady_clock::now();
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (timed_seconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timed_seconds) {
                std::cout << "Timer elapsed after " << elapsed << " s\n";
                break;
            }
        }
    }

    std::cout << "Releasing and stopping dApp...\n";
    for (auto& pp : peers) pp->agent->release();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for (auto& pp : peers) pp->agent->stop();

    // Per-peer breakdown so each RAN's contribution and the indication
    // queueing time are visible. Reported only (not a hard gate); a note is
    // emitted when the max age exceeds 1 ms.
    uint32_t total = 0;
    for (auto& pp : peers) {
        const uint32_t n = pp->count.load();
        total += n;
        const double avg = n ? static_cast<double>(pp->age_sum_ms) / n : 0.0;
        // Drop accounting from the sequence range: expected = max-min+1.
        const uint64_t expected = pp->seq_seen ? (static_cast<uint64_t>(pp->max_seq) - pp->min_seq + 1) : 0;
        const uint64_t dropped = (expected > n) ? (expected - n) : 0;
        const double drop_pct = expected ? (100.0 * dropped / expected) : 0.0;
        std::cout << "  peer=" << pp->label
                  << " ran=" << (pp->ran_id.empty() ? "?" : pp->ran_id)
                  << " sub=" << (pp->sub_id ? std::to_string(*pp->sub_id) : "?")
                  << " indications=" << n
                  << " seq=[" << (pp->seq_seen ? pp->min_seq : 0) << ".." << (pp->seq_seen ? pp->max_seq : 0) << "]"
                  << " dropped=" << dropped << " (" << drop_pct << "%)"
                  << " age_ms(avg=" << avg << " max=" << pp->age_max_ms
                  << " @seq=" << pp->age_max_seq << ")"
                  << " hist[<=1:" << pp->age_le1 << " 2-5:" << pp->age_2_5
                  << " 6-10:" << pp->age_6_10 << " >10:" << pp->age_gt10 << "]";
        if (pp->age_max_ms > 1) {
            std::cout << "  [note: indication queueing >1ms]";
        }
        std::cout << "\n";
    }
    std::cout << "Done. Total indications: " << total << "\n";
    return 0;
}
