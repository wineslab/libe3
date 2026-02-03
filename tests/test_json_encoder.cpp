/**
 * @file test_json_encoder.cpp
 * @brief Unit tests for JSON encoder
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

using namespace libe3;

// Get a JSON encoder instance
static std::unique_ptr<E3Encoder> create_encoder() {
    return create_encoder(EncodingFormat::JSON);
}

TEST(JsonEncoder_encode_setup_request) {
    auto encoder = create_encoder();
    ASSERT_TRUE(encoder != nullptr);
    
    Pdu pdu(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.id = 42;
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "TestDApp";
    req.dapp_version = "2.0.0";
    req.vendor = "TestVendor";
    pdu.choice = req;
    
    auto encoded = encoder->encode(pdu);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    ASSERT_FALSE(encoded.second.empty());
    
    // Verify JSON contains expected fields
    std::string json(encoded.second.begin(), encoded.second.end());
    ASSERT_TRUE(json.find("SETUP_REQUEST") != std::string::npos);
    ASSERT_TRUE(json.find("TestDApp") != std::string::npos);
    ASSERT_TRUE(json.find("TestVendor") != std::string::npos);
}

TEST(JsonEncoder_encode_decode_setup_request) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.id = 99;
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "MyDApp";
    req.dapp_version = "1.2.3";
    req.vendor = "MyVendor";
    original.choice = req;
    original.message_id = 12345;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    ASSERT_EQ(decoded.second.type, PduType::SETUP_REQUEST);
    
    auto& restored = std::get<SetupRequest>(decoded.second.choice);
    ASSERT_EQ(restored.id, 99u);
    ASSERT_STREQ(restored.dapp_name.c_str(), "MyDApp");
    ASSERT_STREQ(restored.e3ap_protocol_version.c_str(), "1.0.0");
    ASSERT_STREQ(restored.vendor.c_str(), "MyVendor");
}

TEST(JsonEncoder_encode_setup_response) {
    auto encoder = create_encoder();
    
    Pdu pdu(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.result = SetupResult::SUCCESS;
    resp.accepted_ran_functions.push_back(100);
    resp.rejected_ran_functions.clear();
    pdu.choice = resp;
    
    auto encoded = encoder->encode(pdu);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    std::string json(encoded.second.begin(), encoded.second.end());
    ASSERT_TRUE(json.find("SETUP_RESPONSE") != std::string::npos);
}

TEST(JsonEncoder_encode_decode_subscription_request) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_functions_to_subscribe = {100, 200, 300};
    req.ran_functions_to_unsubscribe = {400};
    original.choice = req;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<SubscriptionRequest>(decoded.second.choice);
    ASSERT_EQ(restored.dapp_identifier, 42u);
    ASSERT_EQ(restored.ran_functions_to_subscribe.size(), 3u);
    ASSERT_EQ(restored.ran_functions_to_unsubscribe.size(), 1u);
}

TEST(JsonEncoder_encode_decode_subscription_response) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.dapp_identifier = 55;
    resp.accepted_ran_functions = {100, 200};
    resp.rejected_ran_functions = {300};
    original.choice = resp;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<SubscriptionResponse>(decoded.second.choice);
    ASSERT_EQ(restored.dapp_identifier, 55u);
    ASSERT_EQ(restored.accepted_ran_functions.size(), 2u);
}

TEST(JsonEncoder_encode_decode_indication_message) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = 77;
    msg.protocol_data = {0x01, 0x02, 0x03, 0x04, 0xAB, 0xCD};
    original.choice = msg;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<IndicationMessage>(decoded.second.choice);
    ASSERT_EQ(restored.dapp_identifier, 77u);
    ASSERT_EQ(restored.protocol_data.size(), 6u);
    ASSERT_EQ(restored.protocol_data[4], 0xAB);
}

TEST(JsonEncoder_encode_decode_control_action) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::CONTROL_ACTION);
    ControlAction action;
    action.dapp_identifier = 123;
    action.ran_function_identifier = 456;
    action.action_data = {0xDE, 0xAD, 0xBE, 0xEF};
    original.choice = action;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<ControlAction>(decoded.second.choice);
    ASSERT_EQ(restored.dapp_identifier, 123u);
    ASSERT_EQ(restored.ran_function_identifier, 456u);
    ASSERT_EQ(restored.action_data.size(), 4u);
}

TEST(JsonEncoder_encode_decode_dapp_report) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::DAPP_REPORT);
    DAppReport report;
    report.dapp_identifier = 999;
    report.ran_function_identifier = 888;
    report.report_data = {0x11, 0x22, 0x33};
    original.choice = report;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<DAppReport>(decoded.second.choice);
    ASSERT_EQ(restored.dapp_identifier, 999u);
    ASSERT_EQ(restored.ran_function_identifier, 888u);
}

TEST(JsonEncoder_encode_decode_message_ack) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::MESSAGE_ACK);
    MessageAck ack;
    ack.original_message_id = 54321;
    ack.result = ErrorCode::SUCCESS;
    ack.message = "Operation completed";
    original.choice = ack;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<MessageAck>(decoded.second.choice);
    ASSERT_EQ(restored.original_message_id, 54321u);
    ASSERT_EQ(restored.result, ErrorCode::SUCCESS);
}

TEST(JsonEncoder_decode_invalid_json) {
    auto encoder = create_encoder();
    
    std::vector<uint8_t> invalid = {'n', 'o', 't', ' ', 'j', 's', 'o', 'n'};
    auto result = encoder->decode(invalid);
    ASSERT_NE(result.first, ErrorCode::SUCCESS);
}

TEST(JsonEncoder_decode_empty) {
    auto encoder = create_encoder();
    
    std::vector<uint8_t> empty;
    auto result = encoder->decode(empty);
    ASSERT_NE(result.first, ErrorCode::SUCCESS);
}

TEST(JsonEncoder_format) {
    auto encoder = create_encoder();
    ASSERT_EQ(encoder->format(), EncodingFormat::JSON);
}

TEST(JsonEncoder_roundtrip_large_data) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = 1;
    // 1KB of data
    msg.protocol_data.resize(1024);
    for (size_t i = 0; i < msg.protocol_data.size(); ++i) {
        msg.protocol_data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    original.choice = msg;
    
    auto encoded = encoder->encode(original);
    ASSERT_EQ(encoded.first, ErrorCode::SUCCESS);
    
    auto decoded = encoder->decode(encoded.second);
    ASSERT_EQ(decoded.first, ErrorCode::SUCCESS);
    
    auto& restored = std::get<IndicationMessage>(decoded.second.choice);
    ASSERT_EQ(restored.protocol_data.size(), 1024u);
    for (size_t i = 0; i < restored.protocol_data.size(); ++i) {
        ASSERT_EQ(restored.protocol_data[i], static_cast<uint8_t>(i & 0xFF));
    }
}

int main() {
    return RUN_ALL_TESTS();
}
