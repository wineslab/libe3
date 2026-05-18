/**
 * @file e3_interface.cpp
 * @brief E3Interface implementation
 *
 * Supports 1 or 2 I/O channels (see `Channel` in e3_interface.hpp). With one
 * channel the behavior is identical to the historic single-encoding path.
 * With two channels each one binds its own port triplet, runs its own setup/
 * subscriber/publisher threads, and encodes outbound PDUs with its own
 * encoder. SubscriptionManager records the channel each dApp registered
 * through; outbound PDUs are routed back to the dApp's channel.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_interface.hpp"
#include "libe3/logger.hpp"
#include <chrono>
#include <optional>
#include <random>
#include <signal.h>
#include <type_traits>
#include <variant>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "E3Iface";

/**
 * @brief Apply CPU-affinity and niceness to the calling thread.
 *
 * Invoked at the very start of each I/O thread so that the settings take
 * effect before any real work begins.
 *
 * @param affinity  Logical CPU core to pin to, or -1 to skip pinning.
 * @param niceness  Nice value in [-20, 19], or 0 to skip.
 */
void apply_thread_config(int affinity, int niceness) noexcept {
#ifdef __linux__
    if (affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<size_t>(affinity), &cpuset);
        // Non-fatal if this fails (e.g. core index out of range).
        (void)pthread_setaffinity_np(pthread_self(),
                                     sizeof(cpu_set_t), &cpuset);
    }
    if (niceness != 0) {
        // Use the kernel TID so only this thread's scheduling priority changes.
        // syscall(SYS_gettid) returns the kernel thread ID as a long; cast it
        // to pid_t first, then to the id_t expected by setpriority(PRIO_PROCESS).
        auto tid = static_cast<id_t>(static_cast<pid_t>(syscall(SYS_gettid)));
        // Non-fatal: negative values require CAP_SYS_NICE.
        (void)setpriority(PRIO_PROCESS, tid, niceness);
    }
#else
    (void)affinity;
    (void)niceness;
#endif
}

/**
 * @brief Thread-local channel hint for outbound PDUs that carry no dApp tag.
 *
 * Set by subscriber_loop at the start of each inbound dispatch so that a
 * MessageAck emitted synchronously from the SM is routed back through the
 * same channel the request came in on. SIZE_MAX means "no hint" — those
 * PDUs fall back to channel 0.
 */
thread_local size_t t_inbound_channel = SIZE_MAX;

/**
 * @brief Extract a target dApp id from a PDU's variant, if it has one.
 *
 * SetupResponse is intentionally not handled here: setup responses are
 * sent inline via Channel::connector->send_response() (not queued).
 */
std::optional<uint32_t> pdu_target_dapp(const Pdu& pdu) noexcept {
    return std::visit([](const auto& v) -> std::optional<uint32_t> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, SubscriptionResponse>
                   || std::is_same_v<T, IndicationMessage>
                   || std::is_same_v<T, XAppControlAction>
                   || std::is_same_v<T, DAppControlAction>
                   || std::is_same_v<T, SubscriptionRequest>
                   || std::is_same_v<T, SubscriptionDelete>
                   || std::is_same_v<T, DAppReport>
                   || std::is_same_v<T, ReleaseMessage>) {
            return v.dapp_identifier;
        } else if constexpr (std::is_same_v<T, SetupResponse>) {
            return v.dapp_identifier;  // std::optional<uint32_t>
        } else {
            // SetupRequest, MessageAck — no dApp tag.
            return std::nullopt;
        }
    }, pdu.choice);
}

} // anonymous namespace

uint32_t E3Interface::generate_message_id() {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(1, 1000);
    return dist(rng);
}

E3Interface::E3Interface(const E3Config& config)
    : config_(config)
{
    Logger::instance().set_log_file("/tmp/e3_agent.log");
    Logger::instance().set_level(config.log_level);
    E3_LOG_INFO(LOG_TAG) << "E3Interface created";
}

E3Interface::~E3Interface() {
    stop();
    E3_LOG_INFO(LOG_TAG) << "E3Interface destroyed";
}

ErrorCode E3Interface::init() {
    if (state_.load() != AgentState::UNINITIALIZED) {
        E3_LOG_WARN(LOG_TAG) << "Interface already initialized";
        return ErrorCode::ALREADY_INITIALIZED;
    }

    E3_LOG_INFO(LOG_TAG) << "Initializing E3Interface";

    subscription_manager_ = std::make_unique<SubscriptionManager>();
    subscription_manager_->set_sm_lifecycle_callback(
        [this](uint32_t ran_function_id, bool should_start) {
            on_sm_lifecycle_change(ran_function_id, should_start);
        }
    );

    // Build channel descriptors. Always at least the primary; add the
    // secondary when dual-encoding is requested.
    struct ChannelSpec {
        EncodingFormat encoding;
        uint16_t setup_port;
        uint16_t subscriber_port;
        uint16_t publisher_port;
    };
    std::vector<ChannelSpec> specs;
    specs.push_back({
        config_.encoding,
        config_.setup_port,
        config_.subscriber_port,
        config_.publisher_port,
    });

    if (config_.enable_dual_encoding) {
        EncodingFormat sec_enc = config_.secondary_encoding;
        if (sec_enc == config_.encoding) {
            // Auto-flip if the operator left secondary_encoding at the
            // type's default and it collides with primary.
            sec_enc = (config_.encoding == EncodingFormat::ASN1)
                        ? EncodingFormat::JSON
                        : EncodingFormat::ASN1;
            E3_LOG_WARN(LOG_TAG)
                << "secondary_encoding == encoding; auto-flipping secondary to "
                << (sec_enc == EncodingFormat::ASN1 ? "ASN1" : "JSON");
        }
        if (config_.secondary_setup_port == 0
         || config_.secondary_subscriber_port == 0
         || config_.secondary_publisher_port == 0) {
            E3_LOG_ERROR(LOG_TAG)
                << "enable_dual_encoding=true but secondary_*_port is 0; "
                   "set secondary_setup_port/secondary_subscriber_port/"
                   "secondary_publisher_port in E3Config.";
            return ErrorCode::INVALID_PARAM;
        }
        if (config_.secondary_setup_port == config_.setup_port
         || config_.secondary_subscriber_port == config_.subscriber_port
         || config_.secondary_publisher_port == config_.publisher_port) {
            E3_LOG_ERROR(LOG_TAG)
                << "secondary_*_port collides with primary; pick distinct ports.";
            return ErrorCode::INVALID_PARAM;
        }
        specs.push_back({
            sec_enc,
            config_.secondary_setup_port,
            config_.secondary_subscriber_port,
            config_.secondary_publisher_port,
        });
    }

    channels_.reserve(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        const auto& s = specs[i];
        auto ch = std::make_unique<Channel>();
        ch->encoding = s.encoding;
        ch->setup_port = s.setup_port;
        ch->subscriber_port = s.subscriber_port;
        ch->publisher_port = s.publisher_port;

        ch->encoder = create_encoder(s.encoding);
        if (!ch->encoder) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to create encoder for channel " << i
                                  << " (encoding=" << static_cast<int>(s.encoding) << ")";
            return ErrorCode::INTERNAL_ERROR;
        }

        ch->response_queue = std::make_unique<ResponseQueue>();

        ch->connector = create_connector(
            config_.link_layer,
            config_.transport_layer,
            config_.setup_endpoint,
            config_.subscriber_endpoint,
            config_.publisher_endpoint,
            s.setup_port,
            s.subscriber_port,
            s.publisher_port,
            config_.io_threads,
            s.encoding
        );
        if (!ch->connector) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to create connector for channel " << i;
            return ErrorCode::INTERNAL_ERROR;
        }

        E3_LOG_INFO(LOG_TAG)
            << "Channel " << i
            << ": encoding=" << (s.encoding == EncodingFormat::ASN1 ? "ASN1" : "JSON")
            << " ports[setup=" << s.setup_port
            << " sub=" << s.subscriber_port
            << " pub=" << s.publisher_port << "]";

        channels_.push_back(std::move(ch));
    }

    state_.store(AgentState::INITIALIZED);
    E3_LOG_INFO(LOG_TAG) << "E3Interface initialized successfully with "
                         << channels_.size() << " channel(s)";

    return ErrorCode::SUCCESS;
}

ErrorCode E3Interface::start() {
    AgentState expected = AgentState::INITIALIZED;
    if (!state_.compare_exchange_strong(expected, AgentState::CONNECTING)) {
        E3_LOG_ERROR(LOG_TAG) << "Cannot start from state: "
                              << agent_state_to_string(expected);
        return ErrorCode::STATE_ERROR;
    }

    E3_LOG_INFO(LOG_TAG) << "Starting E3Interface";
    should_stop_.store(false);

    // Bring up each channel's setup (REQ/REP) socket before spawning threads.
    for (size_t i = 0; i < channels_.size(); ++i) {
        ErrorCode conn_result = channels_[i]->connector->setup_initial_connection();
        if (conn_result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << i << ": setup_initial_connection failed";
            state_.store(AgentState::ERROR);
            return conn_result;
        }
    }

    state_.store(AgentState::CONNECTED);

    for (size_t i = 0; i < channels_.size(); ++i) {
        channels_[i]->setup_thread = std::make_unique<std::thread>(
            &E3Interface::setup_loop, this, i);
        channels_[i]->subscriber_thread = std::make_unique<std::thread>(
            &E3Interface::subscriber_loop, this, i);
        channels_[i]->publisher_thread = std::make_unique<std::thread>(
            &E3Interface::publisher_loop, this, i);
    }

    state_.store(AgentState::RUNNING);
    E3_LOG_INFO(LOG_TAG) << "E3Interface started ("
                         << channels_.size() << " channel(s))";

    return ErrorCode::SUCCESS;
}

void E3Interface::stop() {
    if (state_.load() == AgentState::UNINITIALIZED ||
        state_.load() == AgentState::STOPPING) {
        return;
    }

    E3_LOG_INFO(LOG_TAG) << "Stopping E3Interface";
    state_.store(AgentState::STOPPING);
    should_stop_.store(true);

    // Wake every channel's outbound queue and interrupt every connector.
    for (auto& ch : channels_) {
        if (ch->response_queue) ch->response_queue->shutdown();
        if (ch->connector) ch->connector->shutdown();
    }

    // Join all threads.
    for (auto& ch : channels_) {
        if (ch->setup_thread && ch->setup_thread->joinable())
            ch->setup_thread->join();
        if (ch->subscriber_thread && ch->subscriber_thread->joinable())
            ch->subscriber_thread->join();
        if (ch->publisher_thread && ch->publisher_thread->joinable())
            ch->publisher_thread->join();
    }

    SmRegistry::instance().clear();

    for (auto& ch : channels_) {
        if (ch->connector) ch->connector->dispose();
    }

    state_.store(AgentState::INITIALIZED);
    E3_LOG_INFO(LOG_TAG) << "E3Interface stopped";
}

ErrorCode E3Interface::queue_outbound(Pdu pdu) {
    if (channels_.empty()) {
        return ErrorCode::NOT_INITIALIZED;
    }
    size_t ch = resolve_outbound_channel(pdu);
    if (ch >= channels_.size()) ch = 0;
    return channels_[ch]->response_queue->push(std::move(pdu));
}

size_t E3Interface::resolve_outbound_channel(const Pdu& pdu) const noexcept {
    if (auto dapp = pdu_target_dapp(pdu); dapp && *dapp != 0) {
        if (auto bound = subscription_manager_->get_dapp_channel(*dapp)) {
            return *bound;
        }
    }
    // MessageAck (no dApp tag) and any unresolved PDU fall back to the
    // thread-local hint set by subscriber_loop, else channel 0.
    if (t_inbound_channel != SIZE_MAX && t_inbound_channel < channels_.size()) {
        return t_inbound_channel;
    }
    return 0;
}

std::vector<uint32_t> E3Interface::get_available_ran_functions() const {
    return SmRegistry::instance().get_available_ran_functions();
}

ErrorCode E3Interface::register_sm(std::unique_ptr<ServiceModel> sm) {
    if (!sm) {
        return ErrorCode::INVALID_PARAM;
    }
    const uint32_t ran_function_id = sm->ran_function_id();
    sm->set_subscribers_provider([this, ran_function_id]() {
        if (!subscription_manager_) {
            return std::vector<uint32_t>{};
        }
        return subscription_manager_->get_subscribed_dapps(ran_function_id);
    });
    sm->set_outbound_emitter([this](Pdu&& pdu) {
        if (pdu.message_id == 0) {
            pdu.message_id = generate_message_id();
        }
        return queue_outbound(std::move(pdu));
    });
    return SmRegistry::instance().register_sm(std::move(sm));
}

void E3Interface::notify_dapp_status_changed() {
    if (dapp_status_changed_handler_) {
        dapp_status_changed_handler_();
    }
}

// =========================================================================
// Thread Entry Points
// =========================================================================

void E3Interface::setup_loop(size_t channel_idx) {
    Channel& ch = *channels_[channel_idx];
    E3_LOG_INFO(LOG_TAG) << "Setup loop started on channel " << channel_idx;

    while (!should_stop_.load()) {
        std::vector<uint8_t> buffer;
        int ret = ch.connector->recv_setup_request(buffer);

        if (ret <= 0) {
            if (should_stop_.load()) break;
            continue;
        }

        E3_LOG_INFO(LOG_TAG) << "Channel " << channel_idx
                             << ": setup request received (" << ret << " bytes)";

        auto decode_result = ch.encoder->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": failed to decode setup request (ret=" << ret << ")";
            continue;
        }

        Pdu& pdu = *decode_result;
        if (pdu.type != PduType::SETUP_REQUEST) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": unexpected PDU type in setup: "
                                  << pdu_type_to_string(pdu.type);
            continue;
        }

        auto* request = std::get_if<SetupRequest>(&pdu.choice);
        if (!request) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": failed to get SetupRequest from PDU";
            continue;
        }

        handle_setup_request(*request, pdu.message_id, channel_idx);
    }

    E3_LOG_INFO(LOG_TAG) << "Setup loop stopped on channel " << channel_idx;
}

void E3Interface::subscriber_loop(size_t channel_idx) {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    Channel& ch = *channels_[channel_idx];
    E3_LOG_INFO(LOG_TAG) << "Subscriber loop started on channel " << channel_idx;

    ErrorCode result = ch.connector->setup_inbound_connection();
    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                              << ": failed to setup inbound connection";
        return;
    }

    // Set thread-local channel hint so MessageAcks emitted synchronously
    // by SMs from within this thread route back through the same channel.
    t_inbound_channel = channel_idx;

    std::vector<uint8_t> buffer;

    while (!should_stop_.load()) {
        int ret = ch.connector->receive(buffer);

        if (ret <= 0) {
            if (should_stop_.load()) break;
            continue;
        }

        auto decode_result = ch.encoder->decode(buffer.data(), static_cast<size_t>(ret));
        if (!decode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": failed to decode PDU in subscriber";
            continue;
        }

        Pdu& pdu = *decode_result;

        switch (pdu.type) {
            case PduType::SUBSCRIPTION_REQUEST: {
                auto* request = std::get_if<SubscriptionRequest>(&pdu.choice);
                if (request) {
                    handle_subscription_request(*request, pdu.message_id);
                }
                break;
            }

            case PduType::SUBSCRIPTION_DELETE: {
                auto* del = std::get_if<SubscriptionDelete>(&pdu.choice);
                if (del) {
                    handle_subscription_delete(*del, pdu.message_id);
                }
                break;
            }

            case PduType::DAPP_CONTROL_ACTION: {
                auto* action = std::get_if<DAppControlAction>(&pdu.choice);
                if (action) {
                    handle_control_action(*action, pdu.message_id);
                }
                break;
            }

            case PduType::DAPP_REPORT: {
                auto* report = std::get_if<DAppReport>(&pdu.choice);
                if (report) {
                    handle_dapp_report(*report);
                }
                break;
            }

            case PduType::RELEASE_MESSAGE: {
                auto* release = std::get_if<ReleaseMessage>(&pdu.choice);
                if (release) {
                    handle_release_message(*release);
                }
                break;
            }

            default:
                E3_LOG_WARN(LOG_TAG) << "Channel " << channel_idx
                                     << ": unexpected PDU type: "
                                     << pdu_type_to_string(pdu.type);
                break;
        }
    }

    t_inbound_channel = SIZE_MAX;
    E3_LOG_INFO(LOG_TAG) << "Subscriber loop stopped on channel " << channel_idx;
}

void E3Interface::publisher_loop(size_t channel_idx) {
    apply_thread_config(config_.io_thread_affinity, config_.io_thread_niceness);
    Channel& ch = *channels_[channel_idx];
    E3_LOG_INFO(LOG_TAG) << "Publisher loop started on channel " << channel_idx;

    // Ignore SIGPIPE to handle closed connections gracefully (process-wide).
    signal(SIGPIPE, SIG_IGN);

    // Retry outbound connection setup.
    ErrorCode result;
    do {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (should_stop_.load()) return;

        E3_LOG_INFO(LOG_TAG) << "Channel " << channel_idx
                             << ": trying to set up outbound connection";
        result = ch.connector->setup_outbound_connection();
    } while (result != ErrorCode::SUCCESS && !should_stop_.load());

    if (result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                              << ": failed to set up outbound connection";
        return;
    }
    E3_LOG_INFO(LOG_TAG) << "Channel " << channel_idx
                         << ": outbound connection established";

    while (!should_stop_.load()) {
        auto pdu_opt = ch.response_queue->pop(std::chrono::milliseconds(10));
        if (!pdu_opt) continue;

        auto encode_result = ch.encoder->encode(*pdu_opt);
        if (!encode_result) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": failed to encode PDU for sending";
            continue;
        }

        ErrorCode send_result = ch.connector->send(encode_result->buffer);
        if (send_result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                                  << ": failed to send PDU";
        } else {
            E3_LOG_DEBUG(LOG_TAG) << "Channel " << channel_idx
                                  << ": sent PDU: " << pdu_type_to_string(pdu_opt->type);
        }
    }

    E3_LOG_INFO(LOG_TAG) << "Publisher loop stopped on channel " << channel_idx;
}

// =========================================================================
// Message Handlers
// =========================================================================

void E3Interface::handle_setup_request(const SetupRequest& request,
                                       uint32_t request_message_id,
                                       size_t channel_idx) {
    Channel& ch = *channels_[channel_idx];

    E3_LOG_INFO(LOG_TAG) << "Channel " << channel_idx
                         << ": setup request from dApp '" << request.dapp_name
                         << "' (version=" << request.dapp_version
                         << ", vendor=" << request.vendor
                         << ", e3ap_version=" << request.e3ap_protocol_version << ")";

    ResponseCode response_code = ResponseCode::NEGATIVE;
    uint32_t assigned_dapp_id = 0;

    auto [result, dapp_id] = subscription_manager_->register_dapp(channel_idx);
    assigned_dapp_id = dapp_id;

    if (result == ErrorCode::SUCCESS) {
        response_code = ResponseCode::POSITIVE;
        E3_LOG_INFO(LOG_TAG) << "dApp '" << request.dapp_name
                             << "' registered with assigned ID " << assigned_dapp_id
                             << " (channel " << channel_idx << ")";
        notify_dapp_status_changed();
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to register dApp '" << request.dapp_name
                              << "': " << error_code_to_string(result);
    }

    auto available_ran_function_ids = SmRegistry::instance().get_available_ran_functions();
    E3_LOG_DEBUG(LOG_TAG) << "Available RAN function ids count: " << available_ran_function_ids.size();
    std::vector<RanFunctionDef> ran_function_list;
    for (auto id : available_ran_function_ids) {
        RanFunctionDef func;
        func.ran_function_identifier = id;
        ServiceModel* sm = SmRegistry::instance().get_by_ran_function(id);
        if (sm) {
            func.telemetry_identifier_list = sm->telemetry_ids();
            func.control_identifier_list = sm->control_ids();
            func.ran_function_data = sm->ran_function_data();
        }
        ran_function_list.push_back(func);
    }
    E3_LOG_DEBUG(LOG_TAG) << "Constructed ran_function_list size: " << ran_function_list.size();
    for (const auto &rf : ran_function_list) {
        E3_LOG_DEBUG(LOG_TAG) << "  RAN function id=" << rf.ran_function_identifier
                               << ", telemetry_count=" << rf.telemetry_identifier_list.size()
                               << ", control_count=" << rf.control_identifier_list.size()
                               << ", ran_function_data_len=" << rf.ran_function_data.size();
    }

    auto encode_result = ch.encoder->encode_setup_response(
        generate_message_id(),
        request_message_id,
        response_code,
        config_.e3ap_version,
        assigned_dapp_id,
        config_.ran_identifier,
        ran_function_list
    );

    if (!encode_result) {
        E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                              << ": failed to encode setup response for request id "
                              << request_message_id;
        return;
    }

    ErrorCode send_result = ch.connector->send_response(encode_result->buffer);
    if (send_result != ErrorCode::SUCCESS) {
        E3_LOG_ERROR(LOG_TAG) << "Channel " << channel_idx
                              << ": failed to send setup response for request id "
                              << request_message_id
                              << "; error=" << error_code_to_string(send_result);
    } else {
        E3_LOG_INFO(LOG_TAG) << "Channel " << channel_idx
                             << ": sent setup response for request id "
                             << request_message_id;
    }
}

void E3Interface::handle_subscription_request(const SubscriptionRequest& request,
                                              uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling subscription request from dApp " << request.dapp_identifier
                         << " for RAN function " << request.ran_function_identifier;

    ResponseCode response_code = ResponseCode::NEGATIVE;
    uint32_t subscription_id = 1;

    if (!subscription_manager_->is_dapp_registered(request.dapp_identifier)) {
        E3_LOG_ERROR(LOG_TAG) << "dApp " << request.dapp_identifier << " not registered";
    } else {
        uint32_t period = request.periodicity.value_or(0);
        auto [result, sub_id] = subscription_manager_->add_subscription(
            request.dapp_identifier,
            request.ran_function_identifier,
            request.telemetry_identifier_list,
            period
        );
        subscription_id = sub_id;

        if (result == ErrorCode::SUCCESS || result == ErrorCode::SUBSCRIPTION_EXISTS) {
            response_code = ResponseCode::POSITIVE;
            E3_LOG_INFO(LOG_TAG) << "Subscription added: dApp " << request.dapp_identifier
                                 << " -> RAN function " << request.ran_function_identifier
                                 << " (subscription_id=" << subscription_id << ")";
            if (result == ErrorCode::SUCCESS) {
                notify_dapp_status_changed();
            }
        } else {
            E3_LOG_ERROR(LOG_TAG) << "Failed to add subscription: "
                                  << error_code_to_string(result);
        }
    }

    Pdu response_pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    response_pdu.message_id = generate_message_id();
    resp.request_id = request_message_id;
    resp.dapp_identifier = request.dapp_identifier;
    resp.response_code = response_code;
    if (response_code == ResponseCode::POSITIVE) {
        resp.subscription_id = subscription_id;
    }
    response_pdu.choice = resp;

    queue_outbound(std::move(response_pdu));
}

void E3Interface::handle_subscription_delete(const SubscriptionDelete& del,
                                             uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling subscription delete from dApp " << del.dapp_identifier
                         << " for subscription " << del.subscription_id;

    ResponseCode response_code = ResponseCode::NEGATIVE;

    ErrorCode result = subscription_manager_->remove_subscription_by_id(
        del.dapp_identifier,
        del.subscription_id
    );

    if (result == ErrorCode::SUCCESS) {
        response_code = ResponseCode::POSITIVE;
        E3_LOG_INFO(LOG_TAG) << "Subscription " << del.subscription_id
                             << " removed for dApp " << del.dapp_identifier;
        notify_dapp_status_changed();
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to remove subscription: "
                              << error_code_to_string(result);
    }

    Pdu response_pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    response_pdu.message_id = generate_message_id();
    resp.request_id = request_message_id;
    resp.dapp_identifier = del.dapp_identifier;
    resp.response_code = response_code;
    if (response_code == ResponseCode::POSITIVE) {
        resp.subscription_id = del.subscription_id;
    }
    response_pdu.choice = resp;

    queue_outbound(std::move(response_pdu));
}

void E3Interface::handle_control_action(const DAppControlAction& action,
                                        uint32_t request_message_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling control action from dApp " << action.dapp_identifier
                         << " for RAN function " << action.ran_function_identifier
                         << " control " << action.control_identifier
                         << " (" << action.action_data.size() << " bytes)";

    ServiceModel* sm = SmRegistry::instance().get_by_ran_function(action.ran_function_identifier);

    if (sm && sm->is_running()) {
        ErrorCode result = sm->handle_control_action(request_message_id, action);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "SM failed to process control action: "
                                  << error_code_to_string(result);
            return;
        }
        E3_LOG_INFO(LOG_TAG) << "Control action processed by SM";
    } else {
        E3_LOG_ERROR(LOG_TAG) << "No running SM found for RAN function "
                              << action.ran_function_identifier;
        return;
    }
}

void E3Interface::handle_dapp_report(const DAppReport& report) {
    E3_LOG_INFO(LOG_TAG) << "Handling dApp report from dApp " << report.dapp_identifier
                         << " for RAN function " << report.ran_function_identifier;

    if (dapp_report_handler_) {
        dapp_report_handler_(report);
    }
}

void E3Interface::handle_release_message(const ReleaseMessage& release) {
    E3_LOG_INFO(LOG_TAG) << "Handling release message from dApp " << release.dapp_identifier;
    handle_dapp_disconnection(release.dapp_identifier);
}

void E3Interface::handle_dapp_disconnection(uint32_t dapp_id) {
    E3_LOG_INFO(LOG_TAG) << "Handling dApp disconnection: " << dapp_id;

    auto subscriptions = subscription_manager_->get_dapp_subscriptions(dapp_id);
    ErrorCode result = subscription_manager_->unregister_dapp(dapp_id);

    if (result == ErrorCode::SUCCESS) {
        E3_LOG_INFO(LOG_TAG) << "dApp " << dapp_id << " unregistered, "
                             << subscriptions.size() << " subscriptions removed";
        notify_dapp_status_changed();
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Failed to unregister dApp " << dapp_id << ": "
                              << error_code_to_string(result);
    }
}

// =========================================================================
// SM Lifecycle Management
// =========================================================================

void E3Interface::on_sm_lifecycle_change(uint32_t ran_function_id, bool should_start) {
    if (should_start) {
        ErrorCode result = SmRegistry::instance().start_sm(ran_function_id);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_ERROR(LOG_TAG) << "Failed to start SM for RAN function "
                                  << ran_function_id << ": " << error_code_to_string(result);
        }
    } else {
        ErrorCode result = SmRegistry::instance().stop_sm(ran_function_id);
        if (result != ErrorCode::SUCCESS) {
            E3_LOG_WARN(LOG_TAG) << "Failed to stop SM for RAN function "
                                 << ran_function_id << ": " << error_code_to_string(result);
        }
    }
}

} // namespace libe3
