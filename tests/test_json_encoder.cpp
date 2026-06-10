/**
 * @file test_json_encoder.cpp
 * @brief Unit tests for JSON encoder
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"
#include <nlohmann/json.hpp>

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
    ASSERT_TRUE(json.find("setupRequest") != std::string::npos);
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
    ASSERT_TRUE(json.find("setupResponse") != std::string::npos);
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
    std::string payload = R"({"rnti":42069,"mcs":9,"snr":12.5})";
    msg.protocol_data.assign(payload.begin(), payload.end());
    original.choice = msg;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode((*encoded));
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<IndicationMessage>((*decoded).choice);
    ASSERT_EQ(restored.dapp_identifier, 77u);
    ASSERT_EQ(restored.ran_function_identifier, 55u);
    auto restored_json = nlohmann::json::parse(restored.protocol_data);
    ASSERT_EQ(restored_json["rnti"].get<int>(), 42069);
    ASSERT_EQ(restored_json["mcs"].get<int>(), 9);
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
    nlohmann::json large;
    for (int i = 0; i < 256; ++i) {
        large["k" + std::to_string(i)] = i;
    }
    std::string payload = large.dump();
    msg.protocol_data.assign(payload.begin(), payload.end());
    original.choice = msg;
    
    auto encoded = encoder->encode(original);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = encoder->decode((*encoded));
    ASSERT_TRUE(decoded.has_value());
    
    auto& restored = std::get<IndicationMessage>((*decoded).choice);
    auto restored_json = nlohmann::json::parse(restored.protocol_data);
    ASSERT_EQ(restored_json.size(), 256u);
    ASSERT_EQ(restored_json["k0"].get<int>(), 0);
    ASSERT_EQ(restored_json["k255"].get<int>(), 255);
}

TEST(JsonEncoder_reject_nested_data_wrapper) {
    auto encoder = create_encoder();

    std::string nested_json = R"({
        "type": "setupRequest",
        "id": 99,
        "timestamp": 0,
        "data": {
            "e3apProtocolVersion": "2.0.0",
            "dAppName": "NestedDApp",
            "dAppVersion": "3.0.0",
            "vendor": "NestedVendor"
        }
    })";

    std::vector<uint8_t> buf(nested_json.begin(), nested_json.end());
    auto result = encoder->decode(buf.data(), buf.size());
    ASSERT_FALSE(result.has_value());
}

TEST(JsonEncoder_reject_pascal_case_pdu_type) {
    auto encoder = create_encoder();

    std::string pascal_json = R"({
        "type": "SetupRequest",
        "id": 1,
        "timestamp": 0,
        "dAppName": "Test",
        "dAppVersion": "1.0",
        "vendor": "V"
    })";

    std::vector<uint8_t> buf(pascal_json.begin(), pascal_json.end());
    auto result = encoder->decode(buf.data(), buf.size());
    ASSERT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// actionData payload-shape acceptance matrix
//
// libe3 must tolerate every binary-payload shape a peer might emit (Postel's
// law on the receive side). These tests pin the four accepted shapes:
//   1. inline JSON object   (cuBB / libe3-self / spear-dApp)
//   2. {"__hex__": "..."}   (libe3's opaque-bytes sentinel)
//   3. plain hex string     (NVIDIA aerial, spear-aerial-sample-apps@19f9bd3)
//   4. plain UTF-8 string   (final-fallback so any text payload survives)
// ---------------------------------------------------------------------------

namespace {
std::vector<uint8_t> decode_action_data(const std::string& wire) {
    auto encoder = create_encoder();
    std::vector<uint8_t> buf(wire.begin(), wire.end());
    auto decoded = encoder->decode(buf.data(), buf.size());
    if (!decoded.has_value()) return {};
    return std::get<DAppControlAction>(decoded->choice).action_data;
}
}  // namespace

TEST(JsonEncoder_actionData_inline_object) {
    // Shape 1 — inline JSON object. The SM-facing bytes are the compact
    // re-dump of that object.
    std::string wire = R"({
        "type": "dAppControlAction",
        "id": 1, "timestamp": 0,
        "dAppIdentifier": 7,
        "ranFunctionIdentifier": 4,
        "controlIdentifier": 1,
        "actionData": {"enabled": false, "ul_bw": 40}
    })";
    auto bytes = decode_action_data(wire);
    ASSERT_FALSE(bytes.empty());
    auto reparsed = nlohmann::json::parse(bytes);
    ASSERT_TRUE(reparsed.is_object());
    ASSERT_EQ(reparsed["ul_bw"].get<int>(), 40);
    ASSERT_EQ(reparsed["enabled"].get<bool>(), false);
}

TEST(JsonEncoder_actionData_hex_sentinel) {
    // Shape 2 — explicit {"__hex__": "<hex>"} wrapper. Bytes are the literal
    // hex-decoded blob.
    std::string wire = R"({
        "type": "dAppControlAction",
        "id": 2, "timestamp": 0,
        "dAppIdentifier": 7,
        "ranFunctionIdentifier": 4,
        "controlIdentifier": 1,
        "actionData": {"__hex__": "deadbeef"}
    })";
    auto bytes = decode_action_data(wire);
    ASSERT_EQ(bytes.size(), 4u);
    ASSERT_EQ(bytes[0], 0xDEu);
    ASSERT_EQ(bytes[1], 0xADu);
    ASSERT_EQ(bytes[2], 0xBEu);
    ASSERT_EQ(bytes[3], 0xEFu);
}

TEST(JsonEncoder_actionData_plain_hex_string) {
    // Shape 3 — NVIDIA aerial convention (spear-aerial-sample-apps@19f9bd3,
    // e3_manager.cpp:1022+). actionData is a bare JSON string whose chars
    // are pairwise hex digits of the inner JSON payload. Decoder must
    // hex-decode rather than treat the string as opaque UTF-8.
    // "7b22656e61626c6564223a747275657d" == `{"enabled":true}`.
    std::string wire = R"({
        "type": "dAppControlAction",
        "id": 3, "timestamp": 0,
        "dAppIdentifier": 7,
        "ranFunctionIdentifier": 4,
        "controlIdentifier": 1,
        "actionData": "7b22656e61626c6564223a747275657d"
    })";
    auto bytes = decode_action_data(wire);
    ASSERT_FALSE(bytes.empty());
    auto reparsed = nlohmann::json::parse(bytes);
    ASSERT_TRUE(reparsed.is_object());
    ASSERT_EQ(reparsed["enabled"].get<bool>(), true);
}

TEST(JsonEncoder_actionData_plain_utf8_string) {
    // Shape 4 — bare string that is NOT pure hex. Decoder must NOT attempt
    // hex-decode; the SM receives the literal UTF-8 bytes of the string.
    std::string wire = R"({
        "type": "dAppControlAction",
        "id": 4, "timestamp": 0,
        "dAppIdentifier": 7,
        "ranFunctionIdentifier": 4,
        "controlIdentifier": 1,
        "actionData": "hello world"
    })";
    auto bytes = decode_action_data(wire);
    std::string recovered(bytes.begin(), bytes.end());
    ASSERT_EQ(recovered, std::string("hello world"));
}

// Edge: hex-looking string of odd length must NOT be hex-decoded (length
// invariant violated) — fall through to plain UTF-8 shape.
TEST(JsonEncoder_actionData_odd_length_hex_is_utf8) {
    std::string wire = R"({
        "type": "dAppControlAction",
        "id": 5, "timestamp": 0,
        "dAppIdentifier": 7,
        "ranFunctionIdentifier": 4,
        "controlIdentifier": 1,
        "actionData": "abc"
    })";
    auto bytes = decode_action_data(wire);
    std::string recovered(bytes.begin(), bytes.end());
    ASSERT_EQ(recovered, std::string("abc"));
}

int main() {
    return RUN_ALL_TESTS();
}
