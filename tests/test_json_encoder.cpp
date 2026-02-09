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
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "TestDApp";
    req.dapp_version = "2.0.0";
    req.vendor = "TestVendor";
    pdu.choice = req;
    
    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_FALSE(encoded->buffer.empty());
    
    // Verify JSON contains expected fields
    std::string json(encoded->buffer.begin(), encoded->buffer.end());
    ASSERT_TRUE(json.find("SetupRequest") != std::string::npos);
    ASSERT_TRUE(json.find("TestDApp") != std::string::npos);
    ASSERT_TRUE(json.find("TestVendor") != std::string::npos);
}

TEST(JsonEncoder_encode_decode_setup_request) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "MyDApp";
    req.dapp_version = "1.2.3";
    req.vendor = "MyVendor";
    original.choice = req;
    original.message_id = 12345;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->type == PduType::SETUP_REQUEST);
    
    auto& restored = std::get<SetupRequest>(decoded->choice);
    ASSERT_STREQ(restored.dapp_name.c_str(), "MyDApp");
    ASSERT_STREQ(restored.e3ap_protocol_version.c_str(), "1.0.0");
    ASSERT_STREQ(restored.vendor.c_str(), "MyVendor");
}

TEST(JsonEncoder_encode_setup_response) {
    auto encoder = create_encoder();
    
    Pdu pdu(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.request_id = 100;
    resp.response_code = ResponseCode::POSITIVE;
    resp.dapp_identifier = 42;
    pdu.choice = resp;
    
    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    
    std::string json(encoded->buffer.begin(), encoded->buffer.end());
    ASSERT_TRUE(json.find("SetupResponse") != std::string::npos);
}

TEST(JsonEncoder_encode_decode_subscription_request) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_function_identifier = 100;
    req.telemetry_identifier_list = {1, 2, 3};
    req.control_identifier_list = {10, 20};
    original.choice = req;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<SubscriptionRequest>(decoded->choice);
    ASSERT_EQ(restored.dapp_identifier, 42u);
    ASSERT_EQ(restored.ran_function_identifier, 100u);
}

TEST(JsonEncoder_encode_decode_subscription_delete) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SUBSCRIPTION_DELETE);
    SubscriptionDelete del;
    del.dapp_identifier = 42;
    del.subscription_id = 77;
    original.choice = del;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->type == PduType::SUBSCRIPTION_DELETE);
    
    auto& restored = std::get<SubscriptionDelete>(decoded->choice);
    ASSERT_EQ(restored.dapp_identifier, 42u);
    ASSERT_EQ(restored.subscription_id, 77u);
}

TEST(JsonEncoder_encode_decode_subscription_response) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.request_id = 55;
    resp.response_code = ResponseCode::POSITIVE;
    original.choice = resp;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<SubscriptionResponse>(decoded->choice);
    ASSERT_EQ(restored.request_id, 55u);
}

TEST(JsonEncoder_encode_decode_indication_message) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = 77;
    msg.ran_function_identifier = 55;
    msg.protocol_data = {0x01, 0x02, 0x03, 0x04, 0xAB, 0xCD};
    original.choice = msg;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode((*encoded));
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<IndicationMessage>((*decoded).choice);
    ASSERT_EQ(restored.dapp_identifier, 77u);
    ASSERT_EQ(restored.ran_function_identifier, 55u);
    ASSERT_EQ(restored.protocol_data.size(), 6u);
    ASSERT_EQ(restored.protocol_data[4], 0xAB);
}

TEST(JsonEncoder_encode_decode_control_action) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::DAPP_CONTROL_ACTION);
    DAppControlAction action;
    action.dapp_identifier = 123;
    action.ran_function_identifier = 456;
    action.control_identifier = 789;
    action.action_data = {0xDE, 0xAD, 0xBE, 0xEF};
    original.choice = action;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<DAppControlAction>(decoded->choice);
    ASSERT_EQ(restored.dapp_identifier, 123u);
    ASSERT_EQ(restored.ran_function_identifier, 456u);
    ASSERT_EQ(restored.control_identifier, 789u);
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
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<DAppReport>(decoded->choice);
    ASSERT_EQ(restored.dapp_identifier, 999u);
    ASSERT_EQ(restored.ran_function_identifier, 888u);
}

TEST(JsonEncoder_encode_decode_message_ack) {
    auto encoder = create_encoder();
    
    Pdu original(PduType::MESSAGE_ACK);
    MessageAck ack;
    ack.request_id = 54321;
    ack.response_code = ResponseCode::POSITIVE;
    original.choice = ack;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<MessageAck>(decoded->choice);
    ASSERT_EQ(restored.request_id, 54321u);
    ASSERT_TRUE(restored.response_code == ResponseCode::POSITIVE);
}

TEST(JsonEncoder_decode_invalid_json) {
    auto encoder = create_encoder();
    
    std::vector<uint8_t> invalid = {'n', 'o', 't', ' ', 'j', 's', 'o', 'n'};
    EncodedMessage encoded_msg(invalid, EncodingFormat::JSON);
    auto result = encoder->decode(encoded_msg);
    ASSERT_FALSE(result.has_value());
}

TEST(JsonEncoder_decode_empty) {
    auto encoder = create_encoder();
    
    std::vector<uint8_t> empty;
    EncodedMessage encoded_msg(empty, EncodingFormat::JSON);
    auto result = encoder->decode(encoded_msg);
    ASSERT_FALSE(result.has_value());
}

TEST(JsonEncoder_format) {
    auto encoder = create_encoder();
    ASSERT_TRUE(encoder->format() == EncodingFormat::JSON);
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
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode((*encoded));
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<IndicationMessage>((*decoded).choice);
    ASSERT_EQ(restored.protocol_data.size(), 1024u);
    for (size_t i = 0; i < restored.protocol_data.size(); ++i) {
        ASSERT_EQ(restored.protocol_data[i], static_cast<uint8_t>(i & 0xFF));
    }
}

int main() {
    return RUN_ALL_TESTS();
}
