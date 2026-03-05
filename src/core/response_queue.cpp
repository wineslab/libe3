/**
 * @file response_queue.cpp
 * @brief Thread-safe queue implementation
 *
 * Ported from the original C implementation e3_response_queue.c
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/response_queue.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "Queue";
}

ResponseQueue::ResponseQueue(size_t max_size)
    : max_size_(max_size) {
    E3_LOG_DEBUG(LOG_TAG) << "Response queue created with capacity " << max_size;
}

ResponseQueue::~ResponseQueue() {
    shutdown();
    E3_LOG_DEBUG(LOG_TAG) << "Response queue destroyed";
}

ErrorCode ResponseQueue::push(Pdu pdu) {
    std::unique_lock lock(mutex_);
    
    if (shutdown_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    if (queue_.size() >= max_size_) {
        E3_LOG_WARN(LOG_TAG) << "Queue full, dropping message";
        return ErrorCode::BUFFER_TOO_SMALL;
    }
    
    queue_.push(std::move(pdu));
    E3_LOG_TRACE(LOG_TAG) << "Pushed PDU, queue size: " << queue_.size();
    
    lock.unlock();
    not_empty_.notify_one();
    
    return ErrorCode::SUCCESS;
}

Pdu ResponseQueue::pop() {
    std::unique_lock lock(mutex_);
    
    not_empty_.wait(lock, [this] { 
        return !queue_.empty() || shutdown_; 
    });
    
    if (shutdown_ && queue_.empty()) {
        // Return empty PDU on shutdown
        return Pdu{};
    }
    
    Pdu pdu = std::move(queue_.front());
    queue_.pop();
    
    E3_LOG_TRACE(LOG_TAG) << "Popped PDU, queue size: " << queue_.size();
    return pdu;
}

std::optional<Pdu> ResponseQueue::pop(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    
    if (!not_empty_.wait_for(lock, timeout, [this] { 
        return !queue_.empty() || shutdown_; 
    })) {
        return std::nullopt; // Timeout
    }
    
    if (shutdown_ && queue_.empty()) {
        return std::nullopt;
    }
    
    Pdu pdu = std::move(queue_.front());
    queue_.pop();
    
    E3_LOG_TRACE(LOG_TAG) << "Popped PDU (with timeout), queue size: " << queue_.size();
    return pdu;
}

std::optional<Pdu> ResponseQueue::try_pop() {
    std::unique_lock lock(mutex_);
    
    if (queue_.empty()) {
        return std::nullopt;
    }
    
    Pdu pdu = std::move(queue_.front());
    queue_.pop();
    
    return pdu;
}

bool ResponseQueue::empty() const {
    std::unique_lock lock(mutex_);
    return queue_.empty();
}

size_t ResponseQueue::size() const {
    std::unique_lock lock(mutex_);
    return queue_.size();
}

void ResponseQueue::clear() {
    std::unique_lock lock(mutex_);
    std::queue<Pdu> empty;
    std::swap(queue_, empty);
    E3_LOG_DEBUG(LOG_TAG) << "Queue cleared";
}

void ResponseQueue::shutdown() {
    {
        std::unique_lock lock(mutex_);
        shutdown_ = true;
    }
    not_empty_.notify_all();
    E3_LOG_DEBUG(LOG_TAG) << "Queue shutdown signaled";
}

bool ResponseQueue::is_shutdown() const {
    std::unique_lock lock(mutex_);
    return shutdown_;
}

} // namespace libe3
