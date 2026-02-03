/**
 * @file test_response_queue.cpp
 * @brief Unit tests for ResponseQueue
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/response_queue.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace libe3;

TEST(ResponseQueue_initial_state) {
    ResponseQueue queue(100);
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0u);
}

TEST(ResponseQueue_push_pop) {
    ResponseQueue queue(100);
    
    Pdu pdu(PduType::INDICATION_MESSAGE);
    pdu.message_id = 42;
    
    auto pushed = queue.push(pdu);
    ASSERT_TRUE(pushed == ErrorCode::SUCCESS);
    ASSERT_FALSE(queue.empty());
    ASSERT_EQ(queue.size(), 1u);
    
    auto popped = queue.pop();
    ASSERT_EQ(popped.message_id, 42u);
    ASSERT_TRUE(queue.empty());
}

TEST(ResponseQueue_try_pop_empty) {
    ResponseQueue queue(100);
    
    auto result = queue.try_pop();
    ASSERT_FALSE(result.has_value());
}

TEST(ResponseQueue_pop_with_timeout) {
    ResponseQueue queue(100);
    
    auto start = std::chrono::steady_clock::now();
    auto result = queue.pop(std::chrono::milliseconds(50));
    auto end = std::chrono::steady_clock::now();
    
    ASSERT_FALSE(result.has_value());
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    ASSERT_GE(elapsed.count(), 40); // Allow some slack
}

TEST(ResponseQueue_fifo_order) {
    ResponseQueue queue(100);
    
    for (uint32_t i = 0; i < 5; ++i) {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        pdu.message_id = i;
        auto result = queue.push(pdu);
        (void)result;  // ignore nodiscard warning
    }
    
    ASSERT_EQ(queue.size(), 5u);
    
    for (uint32_t i = 0; i < 5; ++i) {
        auto pdu = queue.pop();
        ASSERT_EQ(pdu.message_id, i);
    }
    
    ASSERT_TRUE(queue.empty());
}

TEST(ResponseQueue_capacity_limit) {
    ResponseQueue queue(3);
    
    for (int i = 0; i < 3; ++i) {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        auto result = queue.push(pdu);
        ASSERT_TRUE(result == ErrorCode::SUCCESS);
    }
    
    ASSERT_EQ(queue.size(), 3u);
    
    // This should fail (queue full)
    Pdu extra(PduType::INDICATION_MESSAGE);
    auto pushed = queue.push(extra);
    ASSERT_TRUE(pushed == ErrorCode::BUFFER_TOO_SMALL);
}

TEST(ResponseQueue_producer_consumer) {
    ResponseQueue queue(100);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < 50; ++i) {
            Pdu pdu(PduType::INDICATION_MESSAGE);
            pdu.message_id = static_cast<uint32_t>(i);
            (void)queue.push(pdu);
            ++produced;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!done || !queue.empty()) {
            auto pdu = queue.pop(std::chrono::milliseconds(10));
            if (pdu.has_value()) {
                ++consumed;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    ASSERT_EQ(produced.load(), 50);
    ASSERT_EQ(consumed.load(), 50);
}

TEST(ResponseQueue_multiple_producers) {
    ResponseQueue queue(1000);
    const int items_per_producer = 100;
    const int num_producers = 4;
    std::atomic<int> total_produced{0};
    
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < items_per_producer; ++i) {
                Pdu pdu(PduType::INDICATION_MESSAGE);
                pdu.message_id = static_cast<uint32_t>(p * 1000 + i);
                (void)queue.push(pdu);
                ++total_produced;
            }
        });
    }
    
    for (auto& t : producers) {
        t.join();
    }
    
    ASSERT_EQ(total_produced.load(), num_producers * items_per_producer);
    ASSERT_EQ(queue.size(), static_cast<size_t>(num_producers * items_per_producer));
}

TEST(ResponseQueue_clear) {
    ResponseQueue queue(100);
    
    for (int i = 0; i < 10; ++i) {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        (void)queue.push(pdu);
    }
    
    ASSERT_EQ(queue.size(), 10u);
    
    queue.clear();
    
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0u);
}

TEST(ResponseQueue_capacity) {
    ResponseQueue queue(42);
    ASSERT_EQ(queue.capacity(), 42u);
}

TEST(ResponseQueue_blocking_pop) {
    ResponseQueue queue(100);
    std::atomic<bool> got_item{false};
    
    std::thread consumer([&]() {
        auto pdu = queue.pop(); // Will block
        got_item = true;
        (void)pdu;  // Use the result
    });
    
    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(got_item.load());
    
    // Push an item
    Pdu pdu(PduType::INDICATION_MESSAGE);
    (void)queue.push(pdu);
    
    consumer.join();
    ASSERT_TRUE(got_item.load());
}

int main() {
    return RUN_ALL_TESTS();
}
