/**
 * @file test_protobuf_encoder.cpp
 * @brief Unit tests for the Protocol Buffers encoder
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <vector>

using namespace libe3;

// Get a protobuf encoder instance.
static std::unique_ptr<E3Encoder> make_encoder() {
    return create_encoder(EncodingFormat::PROTOBUF);
}

// A byte pattern covering every value 0..255 (exercises binary integrity,
// including embedded NUL bytes that a hex/JSON path would have to escape).
static std::vector<uint8_t> all_bytes() {
    std::vector<uint8_t> v(256);
    for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>(i);
    return v;
}

TEST(ProtobufEncoder_available) {
    auto encoder = make_encoder();
    ASSERT_TRUE(encoder != nullptr);
    ASSERT_TRUE(encoder->format() == EncodingFormat::PROTOBUF);
}

TEST(ProtobufEncoder_setup_request_roundtrip) {
    auto encoder = make_encoder();
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
    ASSERT_FALSE(encoded->buffer.empty());
    ASSERT_TRUE(encoded->format == EncodingFormat::PROTOBUF);

    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->type == PduType::SETUP_REQUEST);
    ASSERT_EQ(decoded->message_id, 12345u);

    auto& r = std::get<SetupRequest>(decoded->choice);
    ASSERT_STREQ(r.e3ap_protocol_version.c_str(), "1.0.0");
    ASSERT_STREQ(r.dapp_name.c_str(), "MyDApp");
    ASSERT_STREQ(r.dapp_version.c_str(), "1.2.3");
    ASSERT_STREQ(r.vendor.c_str(), "MyVendor");
}

TEST(ProtobufEncoder_setup_response_roundtrip_full) {
    auto encoder = make_encoder();
    Pdu original(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.request_id = 7;
    resp.response_code = ResponseCode::POSITIVE;
    resp.e3ap_protocol_version = "1.0.0";
    resp.dapp_identifier = 42;
    resp.ran_identifier = "ran-001";
    RanFunctionDef rf;
    rf.ran_function_identifier = 1;
    rf.telemetry_identifier_list = {1, 2, 3};
    rf.control_identifier_list = {10, 20};
    rf.ran_function_data = all_bytes();
    resp.ran_function_list.push_back(rf);
    original.choice = resp;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->type == PduType::SETUP_RESPONSE);

    auto& r = std::get<SetupResponse>(decoded->choice);
    ASSERT_EQ(r.request_id, 7u);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(r.e3ap_protocol_version.has_value());
    ASSERT_STREQ(r.e3ap_protocol_version->c_str(), "1.0.0");
    ASSERT_TRUE(r.dapp_identifier.has_value());
    ASSERT_EQ(*r.dapp_identifier, 42u);
    ASSERT_STREQ(r.ran_identifier.c_str(), "ran-001");
    ASSERT_EQ(r.ran_function_list.size(), 1u);
    ASSERT_EQ(r.ran_function_list[0].ran_function_identifier, 1u);
    ASSERT_EQ(r.ran_function_list[0].telemetry_identifier_list.size(), 3u);
    ASSERT_EQ(r.ran_function_list[0].control_identifier_list.size(), 2u);
    ASSERT_TRUE(r.ran_function_list[0].ran_function_data == all_bytes());
}

TEST(ProtobufEncoder_setup_response_roundtrip_optionals_absent) {
    auto encoder = make_encoder();
    Pdu original(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.request_id = 9;
    resp.response_code = ResponseCode::NEGATIVE;
    resp.ran_identifier = "ran-x";
    // e3ap_protocol_version, dapp_identifier, ran_function_list all empty/absent
    original.choice = resp;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<SetupResponse>(decoded->choice);
    ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
    ASSERT_FALSE(r.e3ap_protocol_version.has_value());
    ASSERT_FALSE(r.dapp_identifier.has_value());
    ASSERT_EQ(r.ran_function_list.size(), 0u);
}

TEST(ProtobufEncoder_subscription_request_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_function_identifier = 100;
    req.telemetry_identifier_list = {1, 2, 3};
    req.control_identifier_list = {10, 20};
    req.subscription_time = 3600;
    req.periodicity = 500;
    original.choice = req;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<SubscriptionRequest>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 42u);
    ASSERT_EQ(r.ran_function_identifier, 100u);
    ASSERT_TRUE(r.telemetry_identifier_list == std::vector<uint32_t>({1, 2, 3}));
    ASSERT_TRUE(r.control_identifier_list == std::vector<uint32_t>({10, 20}));
    ASSERT_TRUE(r.subscription_time.has_value());
    ASSERT_EQ(*r.subscription_time, 3600u);
    ASSERT_TRUE(r.periodicity.has_value());
    ASSERT_EQ(*r.periodicity, 500u);
}

TEST(ProtobufEncoder_subscription_request_optionals_absent) {
    auto encoder = make_encoder();
    Pdu original(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = 1;
    req.ran_function_identifier = 1;
    original.choice = req;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<SubscriptionRequest>(decoded->choice);
    ASSERT_FALSE(r.subscription_time.has_value());
    ASSERT_FALSE(r.periodicity.has_value());
}

TEST(ProtobufEncoder_subscription_delete_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::SUBSCRIPTION_DELETE);
    SubscriptionDelete del;
    del.dapp_identifier = 42;
    del.subscription_id = 77;
    original.choice = del;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<SubscriptionDelete>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 42u);
    ASSERT_EQ(r.subscription_id, 77u);
}

TEST(ProtobufEncoder_subscription_response_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.request_id = 3;
    resp.dapp_identifier = 42;
    resp.response_code = ResponseCode::POSITIVE;
    resp.subscription_id = 5;
    original.choice = resp;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<SubscriptionResponse>(decoded->choice);
    ASSERT_EQ(r.request_id, 3u);
    ASSERT_EQ(r.dapp_identifier, 42u);
    ASSERT_TRUE(r.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(r.subscription_id.has_value());
    ASSERT_EQ(*r.subscription_id, 5u);
}

TEST(ProtobufEncoder_indication_roundtrip_binary) {
    auto encoder = make_encoder();
    Pdu original(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = 1;
    msg.ran_function_identifier = 2;
    msg.protocol_data = all_bytes();
    original.choice = msg;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<IndicationMessage>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 1u);
    ASSERT_EQ(r.ran_function_identifier, 2u);
    ASSERT_TRUE(r.protocol_data == all_bytes());
}

TEST(ProtobufEncoder_dapp_control_action_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::DAPP_CONTROL_ACTION);
    DAppControlAction act;
    act.dapp_identifier = 1;
    act.ran_function_identifier = 2;
    act.control_identifier = 3;
    act.action_data = {0xDE, 0xAD, 0x00, 0xBE, 0xEF};
    original.choice = act;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<DAppControlAction>(decoded->choice);
    ASSERT_EQ(r.control_identifier, 3u);
    ASSERT_TRUE(r.action_data == std::vector<uint8_t>({0xDE, 0xAD, 0x00, 0xBE, 0xEF}));
}

TEST(ProtobufEncoder_dapp_report_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::DAPP_REPORT);
    DAppReport rep;
    rep.dapp_identifier = 4;
    rep.ran_function_identifier = 5;
    rep.report_data = {1, 2, 3, 0, 4};
    original.choice = rep;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<DAppReport>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 4u);
    ASSERT_EQ(r.ran_function_identifier, 5u);
    ASSERT_TRUE(r.report_data == std::vector<uint8_t>({1, 2, 3, 0, 4}));
}

TEST(ProtobufEncoder_xapp_control_action_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::XAPP_CONTROL_ACTION);
    XAppControlAction act;
    act.dapp_identifier = 6;
    act.ran_function_identifier = 7;
    act.xapp_control_data = {9, 8, 7};
    original.choice = act;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<XAppControlAction>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 6u);
    ASSERT_EQ(r.ran_function_identifier, 7u);
    ASSERT_TRUE(r.xapp_control_data == std::vector<uint8_t>({9, 8, 7}));
}

TEST(ProtobufEncoder_release_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::RELEASE_MESSAGE);
    ReleaseMessage rel;
    rel.dapp_identifier = 55;
    original.choice = rel;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<ReleaseMessage>(decoded->choice);
    ASSERT_EQ(r.dapp_identifier, 55u);
}

TEST(ProtobufEncoder_message_ack_roundtrip) {
    auto encoder = make_encoder();
    Pdu original(PduType::MESSAGE_ACK);
    MessageAck ack;
    ack.request_id = 123;
    ack.response_code = ResponseCode::NEGATIVE;
    original.choice = ack;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());

    auto& r = std::get<MessageAck>(decoded->choice);
    ASSERT_EQ(r.request_id, 123u);
    ASSERT_TRUE(r.response_code == ResponseCode::NEGATIVE);
}

TEST(ProtobufEncoder_envelope_preserved) {
    auto encoder = make_encoder();
    Pdu original(PduType::RELEASE_MESSAGE);
    ReleaseMessage rel;
    rel.dapp_identifier = 1;
    original.choice = rel;
    original.message_id = 999;
    original.timestamp = 1700000000123ULL;

    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(*encoded);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->message_id, 999u);
    ASSERT_EQ(decoded->timestamp, 1700000000123ULL);
}

TEST(ProtobufEncoder_reject_garbage) {
    auto encoder = make_encoder();
    // Bytes that do not decode to a valid E3Pdu with a set oneof.
    std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    auto result = encoder->decode(garbage.data(), garbage.size());
    ASSERT_FALSE(result.has_value());
}

int main() {
    return RUN_ALL_TESTS();
}
