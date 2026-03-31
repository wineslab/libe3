/**
 * @file response_queue.cpp
 * @brief Lock-free bounded queue for E3AP outbound PDUs
 *
 * Replaces the original mutex + condition-variable implementation with the
 * MPMC lock-free ring buffer from mpmc_queue.hpp.  Blocking pop() variants
 * use a three-phase adaptive spin-wait to minimise latency while avoiding
 * a busy-loop when the queue is idle for longer periods.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/response_queue.hpp"
#include "libe3/logger.hpp"
#include <thread>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "Queue";
} // anonymous namespace

ResponseQueue::ResponseQueue(size_t capacity)
    : ring_(capacity)
{
    E3_LOG_DEBUG(LOG_TAG) << "Lock-free response queue created, capacity="
                          << ring_.capacity();
}

ResponseQueue::~ResponseQueue() {
    shutdown();
    E3_LOG_DEBUG(LOG_TAG) << "Response queue destroyed";
}

ErrorCode ResponseQueue::push(Pdu pdu) {
    if (shutdown_.load(std::memory_order_relaxed)) {
        return ErrorCode::NOT_INITIALIZED;
    }

    if (!ring_.try_push(std::move(pdu))) {
        E3_LOG_WARN(LOG_TAG) << "Queue full, dropping message";
        return ErrorCode::BUFFER_TOO_SMALL;
    }

    E3_LOG_TRACE(LOG_TAG) << "Pushed PDU";
    return ErrorCode::SUCCESS;
}

Pdu ResponseQueue::pop() {
    Pdu pdu;

    // Phase 1: CPU-pause spin (nanosecond latency when producer is fast)
    for (size_t i = 0; i < SPIN_COUNT; ++i) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return Pdu{};
        cpu_relax();
    }

    // Phase 2: Cooperative yield (microsecond range)
    for (size_t i = 0; i < YIELD_COUNT; ++i) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return Pdu{};
        std::this_thread::yield();
    }

    // Phase 3: Short sleep until data arrives or shutdown is signalled
    while (true) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return Pdu{};
        std::this_thread::sleep_for(SLEEP_DURATION);
    }
}

std::optional<Pdu> ResponseQueue::pop(std::chrono::milliseconds timeout) {
    Pdu pdu;
    auto deadline = std::chrono::steady_clock::now() + timeout;

    // Phase 1: CPU-pause spin
    for (size_t i = 0; i < SPIN_COUNT; ++i) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
        cpu_relax();
    }

    // Phase 2: Cooperative yield
    for (size_t i = 0; i < YIELD_COUNT; ++i) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
        std::this_thread::yield();
    }

    // Phase 3: Timed sleep
    while (std::chrono::steady_clock::now() < deadline) {
        if (ring_.try_pop(pdu)) return pdu;
        if (shutdown_.load(std::memory_order_relaxed)) return std::nullopt;
        std::this_thread::sleep_for(SLEEP_DURATION);
    }

    // One last attempt after deadline
    if (ring_.try_pop(pdu)) return pdu;
    return std::nullopt;
}

std::optional<Pdu> ResponseQueue::try_pop() {
    Pdu pdu;
    if (ring_.try_pop(pdu)) return pdu;
    return std::nullopt;
}

bool ResponseQueue::empty() const {
    return ring_.empty_approx();
}

size_t ResponseQueue::size() const {
    return ring_.size_approx();
}

void ResponseQueue::clear() {
    Pdu pdu;
    while (ring_.try_pop(pdu)) {}
    E3_LOG_DEBUG(LOG_TAG) << "Queue cleared";
}

void ResponseQueue::shutdown() {
    shutdown_.store(true, std::memory_order_relaxed);
    E3_LOG_DEBUG(LOG_TAG) << "Queue shutdown signalled";
}

bool ResponseQueue::is_shutdown() const {
    return shutdown_.load(std::memory_order_relaxed);
}

} // namespace libe3
