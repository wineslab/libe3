/**
 * @file test_message_timestamps.cpp
 * @brief Round-trip tests for the mandatory E3AP message timestamps
 *
 * Every encoding must carry the envelope timestamp and the per-message
 * timestamp of the five data-plane PDUs. The tests run against whichever
 * encoders this build enabled; create_encoder returns nullptr for the others.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <cstring>
#include <memory>
#include <vector>

using namespace libe3;

namespace {

// A value that exercises the full 64-bit ns range rather than a small
// literal: a millisecond-wide field would truncate it.
constexpr uint64_t kStamp = 1783012345678901234ULL;

std::vector<EncodingFormat> available_encoders() {
    std::vector<EncodingFormat> formats;
    for (auto f : {EncodingFormat::ASN1, EncodingFormat::JSON, EncodingFormat::PROTOBUF}) {
        if (create_encoder(f) != nullptr) formats.push_back(f);
    }
    return formats;
}

// protocolData is a nested JSON object on the JSON path, so it has to stay
// parseable there; the other encodings treat it as opaque bytes.
std::vector<uint8_t> protocol_data() {
    const char* json = "{\"sfn\":1,\"slot\":2}";
    return std::vector<uint8_t>(json, json + strlen(json));
}

} // namespace

TEST(MessageTimestamps_envelope_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::DAPP_REPORT);
        original.message_id = 7;
        original.timestamp = kStamp;
        DAppReport rep;
        rep.dapp_identifier = 1;
        rep.ran_function_identifier = 1;
        rep.report_data = {1, 2, 3};
        original.choice = rep;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->timestamp, kStamp);
    }
}

TEST(MessageTimestamps_indication_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::INDICATION_MESSAGE);
        original.message_id = 1;
        IndicationMessage msg;
        msg.dapp_identifier = 1;
        msg.ran_function_identifier = 2;
        msg.protocol_data = protocol_data();
        msg.message_timestamp = kStamp;
        original.choice = msg;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(std::get<IndicationMessage>(decoded->choice).message_timestamp, kStamp);
    }
}

TEST(MessageTimestamps_dapp_control_action_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::DAPP_CONTROL_ACTION);
        original.message_id = 2;
        DAppControlAction act;
        act.dapp_identifier = 1;
        act.ran_function_identifier = 2;
        act.control_identifier = 3;
        act.action_data = {0xDE, 0xAD};
        act.message_timestamp = kStamp;
        original.choice = act;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(std::get<DAppControlAction>(decoded->choice).message_timestamp, kStamp);
    }
}

TEST(MessageTimestamps_dapp_report_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::DAPP_REPORT);
        original.message_id = 3;
        DAppReport rep;
        rep.dapp_identifier = 4;
        rep.ran_function_identifier = 5;
        rep.report_data = {1, 2, 3};
        rep.message_timestamp = kStamp;
        original.choice = rep;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(std::get<DAppReport>(decoded->choice).message_timestamp, kStamp);
    }
}

TEST(MessageTimestamps_xapp_control_action_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::XAPP_CONTROL_ACTION);
        original.message_id = 4;
        XAppControlAction act;
        act.dapp_identifier = 6;
        act.ran_function_identifier = 7;
        act.xapp_control_data = {9, 8, 7};
        act.message_timestamp = kStamp;
        original.choice = act;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(std::get<XAppControlAction>(decoded->choice).message_timestamp, kStamp);
    }
}

TEST(MessageTimestamps_message_ack_roundtrip) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu original(PduType::MESSAGE_ACK);
        original.message_id = 5;
        MessageAck ack;
        ack.request_id = 11;
        ack.response_code = ResponseCode::POSITIVE;
        ack.message_timestamp = kStamp;
        original.choice = ack;

        auto encoded = encoder->encode(original);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = encoder->decode(*encoded);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(std::get<MessageAck>(decoded->choice).message_timestamp, kStamp);
    }
}

// The field is stamped by the default member initializer, so a caller cannot
// send an unstamped message by forgetting to set it.
TEST(MessageTimestamps_stamped_on_construction) {
    const uint64_t before = now_realtime_ns();

    IndicationMessage ind;
    DAppControlAction act;
    DAppReport rep;
    XAppControlAction xact;
    MessageAck ack;

    const uint64_t after = now_realtime_ns();

    for (uint64_t stamp : {ind.message_timestamp, act.message_timestamp, rep.message_timestamp,
                           xact.message_timestamp, ack.message_timestamp}) {
        ASSERT_TRUE(stamp >= before);
        ASSERT_TRUE(stamp <= after);
    }
}

// A realtime ns count sits above 1e18 today; a milliseconds-wide reading
// would be ~1e12 and a seconds-wide one ~1e9.
TEST(MessageTimestamps_unit_is_nanoseconds) {
    constexpr uint64_t kYear2001Ns = 1000000000000000000ULL;
    ASSERT_TRUE(now_realtime_ns() > kYear2001Ns);
    ASSERT_TRUE(Pdu(PduType::INDICATION_MESSAGE).timestamp > kYear2001Ns);
}

// proto3 has no `required`, so the schema cannot enforce what the ASN.1 grammar
// does structurally. Every encoding rejects an unset timestamp instead, so the
// three behave alike.
TEST(MessageTimestamps_unset_is_rejected) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);

        Pdu no_message_stamp(PduType::INDICATION_MESSAGE);
        no_message_stamp.message_id = 1;
        IndicationMessage msg;
        msg.dapp_identifier = 1;
        msg.ran_function_identifier = 2;
        msg.protocol_data = protocol_data();
        msg.message_timestamp = 0;
        no_message_stamp.choice = msg;
        ASSERT_FALSE(encoder->encode(no_message_stamp).has_value());

        Pdu no_envelope_stamp(PduType::INDICATION_MESSAGE);
        no_envelope_stamp.message_id = 1;
        no_envelope_stamp.timestamp = 0;
        IndicationMessage stamped;
        stamped.dapp_identifier = 1;
        stamped.ran_function_identifier = 2;
        stamped.protocol_data = protocol_data();
        no_envelope_stamp.choice = stamped;
        ASSERT_FALSE(encoder->encode(no_envelope_stamp).has_value());
    }
}

// A PDU type that carries no message timestamp still encodes.
TEST(MessageTimestamps_untimestamped_types_still_encode) {
    for (auto format : available_encoders()) {
        auto encoder = create_encoder(format);
        Pdu pdu(PduType::RELEASE_MESSAGE);
        pdu.message_id = 9;
        ReleaseMessage rel;
        rel.dapp_identifier = 1;
        pdu.choice = rel;
        ASSERT_TRUE(encoder->encode(pdu).has_value());
    }
}

int main() {
    return RUN_ALL_TESTS();
}
