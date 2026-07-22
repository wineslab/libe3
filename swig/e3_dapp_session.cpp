/**
 * @file e3_dapp_session.cpp
 * @brief Implementation of the batched dApp Python seam. See e3_dapp_session.hpp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "e3_dapp_session.hpp"

#include <libe3/e3_agent.hpp>
#include <libe3/lockfree_queue.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

namespace libe3 {
namespace py {

namespace {
// Map an optional negative sentinel to std::optional<uint32_t>.
inline std::optional<uint32_t> opt_u32(int v) {
    if (v < 0) return std::nullopt;
    return static_cast<uint32_t>(v);
}
}  // namespace

struct DAppSession::Impl {
    explicit Impl(libe3::E3Config config, std::size_t queue_capacity)
        : agent(std::make_unique<libe3::E3Agent>(std::move(config))),
          queue(queue_capacity) {}

    std::unique_ptr<libe3::E3Agent> agent;
    // Stable object (never moved) so a poll_events() reference stays valid; a
    // prior stop() shut it down, and start() re-arms it in place (LockFreeQueue's
    // shutdown is one-way, E3Agent itself is restartable).
    libe3::LockFreeQueue<E3Event> queue;
    std::atomic<unsigned long long> dropped{0};

    // SetupResponse snapshot for the setup accessors (written once on setup).
    mutable std::mutex setup_mu;
    bool have_setup{false};
    libe3::SetupResponse setup;

    void enqueue(E3Event ev) {
        if (queue.push(std::move(ev)) != libe3::ErrorCode::SUCCESS) {
            dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }
};

DAppSession::DAppSession(libe3::E3Config config, std::size_t queue_capacity) {
    config.role = libe3::E3Role::DAPP;  // this seam is dApp-only
    impl_ = std::make_unique<Impl>(std::move(config), queue_capacity);
    Impl* impl = impl_.get();
    auto& agent = *impl->agent;

    agent.set_indication_handler([impl](const libe3::IndicationMessage& m) {
        E3Event ev;
        ev.kind = E3_EVENT_INDICATION;
        ev.dapp_id = m.dapp_identifier;
        ev.ran_function_id = m.ran_function_identifier;
        ev.payload = m.protocol_data;  // one copy from libe3's const&
        impl->enqueue(std::move(ev));
    });

    agent.set_xapp_control_handler([impl](const libe3::XAppControlAction& a) {
        E3Event ev;
        ev.kind = E3_EVENT_XAPP_CONTROL;
        ev.dapp_id = a.dapp_identifier;
        ev.ran_function_id = a.ran_function_identifier;
        ev.payload = a.xapp_control_data;
        impl->enqueue(std::move(ev));
    });

    agent.set_subscription_response_handler([impl](const libe3::SubscriptionResponse& r) {
        E3Event ev;
        ev.kind = E3_EVENT_SUBSCRIPTION_RESPONSE;
        ev.dapp_id = r.dapp_identifier;
        ev.request_id = r.request_id;
        ev.subscription_id = r.subscription_id.value_or(0);
        ev.response_code = static_cast<int>(r.response_code);
        impl->enqueue(std::move(ev));
    });

    agent.set_setup_response_handler([impl](const libe3::SetupResponse& r) {
        {
            std::lock_guard<std::mutex> lk(impl->setup_mu);
            impl->setup = r;
            impl->have_setup = true;
        }
        E3Event ev;
        ev.kind = E3_EVENT_SETUP_RESPONSE;
        ev.dapp_id = r.dapp_identifier.value_or(0);
        ev.request_id = r.request_id;
        ev.response_code = static_cast<int>(r.response_code);
        impl->enqueue(std::move(ev));
    });

    agent.set_message_ack_handler([impl](const libe3::MessageAck& ack) {
        E3Event ev;
        ev.kind = E3_EVENT_MESSAGE_ACK;
        ev.request_id = ack.request_id;
        ev.response_code = static_cast<int>(ack.response_code);
        impl->enqueue(std::move(ev));
    });
}

DAppSession::~DAppSession() {
    if (impl_) {
        impl_->queue.shutdown();
        if (impl_->agent) impl_->agent->stop();
    }
}

int DAppSession::start() {
    // Re-arm the inbound ring in place (a prior stop() shut it down; E3Agent is
    // restartable). Re-arming the same object — rather than replacing it — keeps
    // any reference held by a concurrent poll_events() valid.
    impl_->queue.rearm();
    return static_cast<int>(impl_->agent->start());
}

int DAppSession::wait_for_setup(int timeout_ms) {
    return static_cast<int>(
        impl_->agent->wait_for_setup(std::chrono::milliseconds(timeout_ms)));
}

int DAppSession::release() {
    return static_cast<int>(impl_->agent->release());
}

void DAppSession::stop() {
    impl_->queue.shutdown();
    impl_->agent->stop();
}

long DAppSession::dapp_id() const {
    auto id = impl_->agent->dapp_id();
    return id ? static_cast<long>(*id) : -1;
}

std::string DAppSession::ran_identifier() const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    return impl_->have_setup ? impl_->setup.ran_identifier : std::string{};
}

int DAppSession::setup_response_code() const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    if (!impl_->have_setup) return -1;
    return static_cast<int>(impl_->setup.response_code);
}

std::size_t DAppSession::setup_ran_function_count() const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    return impl_->have_setup ? impl_->setup.ran_function_list.size() : 0;
}

uint32_t DAppSession::setup_ran_function_id(std::size_t i) const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    if (!impl_->have_setup || i >= impl_->setup.ran_function_list.size()) return 0;
    return impl_->setup.ran_function_list[i].ran_function_identifier;
}

std::vector<uint32_t> DAppSession::setup_ran_function_telemetry(std::size_t i) const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    if (!impl_->have_setup || i >= impl_->setup.ran_function_list.size()) return {};
    return impl_->setup.ran_function_list[i].telemetry_identifier_list;
}

std::vector<uint32_t> DAppSession::setup_ran_function_control(std::size_t i) const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    if (!impl_->have_setup || i >= impl_->setup.ran_function_list.size()) return {};
    return impl_->setup.ran_function_list[i].control_identifier_list;
}

std::vector<uint8_t> DAppSession::setup_ran_function_data(std::size_t i) const {
    std::lock_guard<std::mutex> lk(impl_->setup_mu);
    if (!impl_->have_setup || i >= impl_->setup.ran_function_list.size()) return {};
    return impl_->setup.ran_function_list[i].ran_function_data;
}

int DAppSession::subscribe(uint32_t ran_function_id,
                           std::vector<uint32_t> telemetry_ids,
                           std::vector<uint32_t> control_ids,
                           int sub_time_ms,
                           int periodicity) {
    return static_cast<int>(impl_->agent->subscribe(
        ran_function_id, std::move(telemetry_ids), std::move(control_ids),
        opt_u32(sub_time_ms), opt_u32(periodicity)));
}

int DAppSession::unsubscribe(uint32_t ran_function_id) {
    return static_cast<int>(impl_->agent->unsubscribe(ran_function_id));
}

int DAppSession::send_control(uint32_t ran_function_id, uint32_t control_id,
                              std::vector<uint8_t> action_data) {
    return static_cast<int>(
        impl_->agent->send_control(ran_function_id, control_id, std::move(action_data)));
}

int DAppSession::send_report(uint32_t ran_function_id, std::vector<uint8_t> report_data) {
    return static_cast<int>(
        impl_->agent->send_report(ran_function_id, std::move(report_data)));
}

int DAppSession::send_message_ack(uint32_t request_id, int response_code) {
    return static_cast<int>(impl_->agent->send_message_ack(
        request_id, static_cast<libe3::ResponseCode>(response_code)));
}

std::vector<E3Event> DAppSession::poll_events(std::size_t max_batch, int timeout_ms) {
    std::vector<E3Event> out;
    if (max_batch == 0) return out;
    auto& queue = impl_->queue;

    // timeout_ms <= 0 is a non-blocking poll (try_pop); a positive value blocks
    // up to that long for the first event. (pop() spins/yields before checking
    // the deadline, so 0 there would busy-wait.)
    std::optional<E3Event> first =
        (timeout_ms <= 0) ? queue.try_pop()
                          : queue.pop(std::chrono::milliseconds(timeout_ms));
    if (!first) return out;  // quiet tick, or shutdown after stop()

    out.reserve(std::min(max_batch, queue.capacity()));
    out.push_back(std::move(*first));
    while (out.size() < max_batch) {
        auto next = queue.try_pop();
        if (!next) break;
        out.push_back(std::move(*next));
    }
    return out;
}

unsigned long long DAppSession::dropped_events() const {
    return impl_->dropped.load(std::memory_order_relaxed);
}

} // namespace py
} // namespace libe3
