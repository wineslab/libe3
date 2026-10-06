/**
 * @file test_asn1_golden.cpp
 * @brief Pins the APER bytes libe3 puts on the wire, one PDU per message type.
 *
 * Nothing else in the suite decodes bytes it did not just produce, so an
 * encoder or grammar change could alter what a peer receives without a test
 * failing. These vectors are the same shape as the ones OCUDU keeps (message
 * id 42, timestamp 1756800000123456789, one PDU per procedure), so OCUDU can
 * re-pin against them.
 *
 * A failure here means the wire format changed. If that was intended, update
 * the hex below and add a row to the change ledger in docs/interop.md in the
 * same PR; if not, the change is a bug.
 *
 * The second half feeds libe3 the 11 PDUs OCUDU recorded from libe3 0.1.3
 * (grammar before 3be38cd, release 0.2.0) and checks that the current decoder
 * refuses them rather than inventing a message.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace libe3;

namespace {

constexpr uint32_t kMessageId = 42;
constexpr uint64_t kTimestamp = 1756800000123456789ull;

std::string to_hex(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (uint8_t b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0xF];
    }
    return out;
}

std::vector<uint8_t> from_hex(const std::string& hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

struct Golden {
    const char* name;
    Pdu pdu;
    const char* hex;
};

Pdu make(PduType type, PduChoice choice) {
    Pdu pdu(type);
    pdu.message_id = kMessageId;
    pdu.timestamp = kTimestamp;
    pdu.choice = std::move(choice);
    return pdu;
}

std::vector<Golden> goldens() {
    std::vector<Golden> out;

    SetupRequest setup_request;
    setup_request.e3ap_protocol_version = "1.0.0";
    setup_request.dapp_name = "vector-dapp";
    setup_request.dapp_version = "0.1.3";
    setup_request.vendor = "wineslab";
    out.push_back({"SetupRequest", make(PduType::SETUP_REQUEST, setup_request), "4029081861684accc3cd150080312e302e3028766563746f722d6461707020302e312e331c77696e65736c6162"});

    SetupResponse setup_response;
    setup_response.request_id = kMessageId;
    setup_response.response_code = ResponseCode::POSITIVE;
    setup_response.e3ap_protocol_version = "1.0.0";
    setup_response.dapp_identifier = 7;
    setup_response.ran_identifier = "vector-ran";
    RanFunctionDef function;
    function.ran_function_identifier = 1;
    function.telemetry_identifier_list = {1, 2};
    function.control_identifier_list = {1};
    function.ran_function_data = {0xde, 0xad};
    setup_response.ran_function_list = {function};
    out.push_back({"SetupResponse", make(PduType::SETUP_RESPONSE, setup_response), "4029081861684accc3cd150b802912766563746f722d72616e20312e302e3000060200000002000000010001000002dead"});

    SubscriptionRequest subscription_request;
    subscription_request.dapp_identifier = 7;
    subscription_request.ran_function_identifier = 1;
    subscription_request.telemetry_identifier_list = {1, 2};
    subscription_request.subscription_time = 60;
    subscription_request.periodicity = 100;
    out.push_back({"SubscriptionRequest",
                   make(PduType::SUBSCRIPTION_REQUEST, subscription_request), "4029081861684accc3cd1513000600000002000000010000003c0064"});

    SubscriptionDelete subscription_delete;
    subscription_delete.dapp_identifier = 7;
    subscription_delete.subscription_id = 1;
    out.push_back({"SubscriptionDelete",
                   make(PduType::SUBSCRIPTION_DELETE, subscription_delete), "4029081861684accc3cd151800060000"});

    SubscriptionResponse subscription_response;
    subscription_response.request_id = kMessageId;
    subscription_response.dapp_identifier = 7;
    subscription_response.response_code = ResponseCode::POSITIVE;
    subscription_response.subscription_id = 1;
    out.push_back({"SubscriptionResponse",
                   make(PduType::SUBSCRIPTION_RESPONSE, subscription_response), "4029081861684accc3cd152200290006000000"});

    IndicationMessage indication;
    indication.dapp_identifier = 7;
    indication.ran_function_identifier = 1;
    indication.protocol_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    out.push_back({"IndicationMessage", make(PduType::INDICATION_MESSAGE, indication), "4029081861684accc3cd152800060000000f0102030405060708090a0b0c0d0e0f10"});

    DAppControlAction control;
    control.dapp_identifier = 7;
    control.ran_function_identifier = 1;
    control.control_identifier = 1;
    control.action_data = {0xaa, 0xbb};
    out.push_back({"DAppControlAction", make(PduType::DAPP_CONTROL_ACTION, control), "4029081861684accc3cd15300006000000000001aabb"});

    DAppReport report;
    report.dapp_identifier = 7;
    report.ran_function_identifier = 1;
    report.report_data = {0xcc};
    report.sequence_id = 9;
    out.push_back({"DAppReport", make(PduType::DAPP_REPORT, report), "4029081861684accc3cd15380006000000080000cc"});

    XAppControlAction xapp;
    xapp.dapp_identifier = 7;
    xapp.ran_function_identifier = 1;
    xapp.xapp_control_data = {0xee, 0xff};
    xapp.sequence_id = 9;
    out.push_back({"XAppControlAction", make(PduType::XAPP_CONTROL_ACTION, xapp), "4029081861684accc3cd15400006000000080001eeff"});

    ReleaseMessage release;
    release.dapp_identifier = 7;
    out.push_back({"ReleaseMessage", make(PduType::RELEASE_MESSAGE, release), "4029081861684accc3cd15480006"});

    MessageAck ack;
    ack.request_id = kMessageId;
    ack.response_code = ResponseCode::NEGATIVE;
    out.push_back({"MessageAck", make(PduType::MESSAGE_ACK, ack), "4029081861684accc3cd15502980"});

    return out;
}

// The 11 PDUs OCUDU recorded from libe3 0.1.3 (python/tests/fixtures/libe3_vectors.json
// in ocudu-dapp-platform, 38fc00bb526c). libe3's own output, from before ids were
// widened and sequenceId was added, so the current grammar must not decode them.
struct Legacy {
    const char* name;
    const char* hex;
};

const Legacy kLegacy[] = {
    {"SetupRequest", "400029081861684accc3cd150100312e302e3028766563746f722d6461707040302e312e331c77696e65736c6162"},
    {"SetupResponse", "400029081861684accc3cd150b80002912766563746f722d72616e40312e302e300c010004000810000001dead"},
    {"SubscriptionRequest", "400029081861684accc3cd15130c0010002000003c0064"},
    {"SubscriptionDelete", "400029081861684accc3cd15183000"},
    {"SubscriptionResponse", "400029081861684accc3cd15220000290c00"},
    {"IndicationMessage", "400029081861684accc3cd15283000000f0102030405060708090a0b0c0d0e0f10"},
    {"DAppControlAction", "400029081861684accc3cd15303000000001aabb"},
    {"DAppReport", "400029081861684accc3cd153830000000cc"},
    {"XAppControlAction", "400029081861684accc3cd154030000001eeff"},
    {"ReleaseMessage", "400029081861684accc3cd154830"},
    {"MessageAck", "400029081861684accc3cd1550002980"},
};

}  // namespace

TEST(Asn1Golden_encodeProducesThePinnedBytes) {
    auto encoder = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    std::string drift;
    for (const auto& g : goldens()) {
        auto encoded = encoder->encode(g.pdu);
        ASSERT_TRUE(encoded.has_value());
        const std::string got = to_hex(encoded->buffer);
        if (got != g.hex) drift += std::string("\n  ") + g.name + ": " + got;
    }
    ASSERT_STREQ(drift, "");
}

TEST(Asn1Golden_pinnedBytesDecodeAndReencodeIdentically) {
    auto encoder = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    for (const auto& g : goldens()) {
        const std::vector<uint8_t> wire = from_hex(g.hex);
        auto decoded = encoder->decode(wire.data(), wire.size());
        ASSERT_TRUE(decoded.has_value());
        ASSERT_TRUE(decoded->type == g.pdu.type);
        ASSERT_EQ(decoded->message_id, kMessageId);
        ASSERT_EQ(decoded->timestamp, kTimestamp);

        auto again = encoder->encode(*decoded);
        ASSERT_TRUE(again.has_value());
        ASSERT_STREQ(to_hex(again->buffer), g.hex);
    }
}

TEST(Asn1Golden_libe3_0_1_3_pdusAreRefused) {
    auto encoder = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    std::string accepted;
    for (const auto& legacy : kLegacy) {
        const std::vector<uint8_t> wire = from_hex(legacy.hex);
        auto decoded = encoder->decode(wire.data(), wire.size());
        if (decoded.has_value()) {
            accepted += std::string("\n  ") + legacy.name;
        } else {
            // A decode failure is reported as one, not as an encode failure.
            ASSERT_EQ(static_cast<int>(decoded.error()),
                      static_cast<int>(ErrorCode::DECODE_FAILED));
        }
    }
    ASSERT_STREQ(accepted, "");
}

int main() {
    return RUN_ALL_TESTS();
}
