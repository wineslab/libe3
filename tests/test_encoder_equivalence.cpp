/**
 * @file test_encoder_equivalence.cpp
 * @brief Cross-encoder equivalence: every enabled encoding must carry every
 *        PDU field identically.
 *
 * libE3 offers three interchangeable wire encodings, selected at runtime, and
 * the whole premise of that choice is that they are field-for-field
 * equivalent. Nothing enforced that. The ASN.1 encoder silently dropped
 * Pdu::timestamp for as long as the field existed, because the per-encoder
 * test suites only ever compare an encoding against itself: a field absent
 * from one grammar round-trips perfectly within that grammar.
 *
 * This suite closes that gap in two directions:
 *
 *   1. Round trip. For every enabled encoder and every PDU type, encode,
 *      decode, and compare the whole structure field by field.
 *
 *   2. Cross-encoder agreement. Decode the same PDU through every enabled
 *      encoder and require the results to agree with each other. This is the
 *      leg that catches a dropped field without the test having to know which
 *      fields exist: an encoder that loses one disagrees with the encoders
 *      that keep it.
 *
 * Which encoders are compiled in is a build option, so the set is probed at
 * run time rather than with #ifdef. The CI "All Encodings" job is where all
 * three are present at once and the second leg has teeth.
 *
 * One payload constraint, and it comes from JSON: the JSON encoder treats
 * IndicationMessage::protocol_data as a nested JSON value and rejects a
 * payload that does not parse. It preserves the bytes verbatim, so equality
 * still holds exactly; the payload just has to be valid JSON. Every other
 * octet-string payload is hex-encoded and takes arbitrary bytes.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <cstdio>
#include <string>
#include <vector>

using namespace libe3;

// ---------------------------------------------------------------------------
// Enabled-encoder probe
// ---------------------------------------------------------------------------

/// Formats this build can actually construct. create_encoder() returns nullptr
/// for an encoder that was compiled out, which makes the set discoverable
/// without mirroring the build options here.
static std::vector<EncodingFormat> available_encoders() {
    std::vector<EncodingFormat> formats;
    for (auto f : {EncodingFormat::ASN1, EncodingFormat::JSON, EncodingFormat::PROTOBUF}) {
        if (create_encoder(f) != nullptr) {
            formats.push_back(f);
        }
    }
    return formats;
}

static const char* format_name(EncodingFormat f) {
    switch (f) {
        case EncodingFormat::ASN1:     return "ASN.1";
        case EncodingFormat::JSON:     return "JSON";
        case EncodingFormat::PROTOBUF: return "protobuf";
    }
    return "unknown";
}

/// Name the encoder and the PDU type on failure. Each case below runs eleven
/// PDU types against every enabled encoder, so the assertion text alone does
/// not say which combination broke.
static void report_if(bool failed, EncodingFormat f, const char* pdu, const char* what) {
    if (failed) {
        std::fprintf(stderr, "       -> %s / %s: %s\n", format_name(f), pdu, what);
    }
}

// ---------------------------------------------------------------------------
// Structural equality
// ---------------------------------------------------------------------------
//
// Hand-written rather than derived, and deliberately exhaustive: a field left
// out here is a field this suite stops guarding. The static_assert below ties
// the comparator set to the variant, so adding a PDU type breaks the build
// here instead of quietly going untested.

static bool equals(const RanFunctionDef& a, const RanFunctionDef& b) {
    return a.ran_function_identifier == b.ran_function_identifier
        && a.telemetry_identifier_list == b.telemetry_identifier_list
        && a.control_identifier_list == b.control_identifier_list
        && a.ran_function_data == b.ran_function_data;
}

static bool equals(const SetupRequest& a, const SetupRequest& b) {
    return a.e3ap_protocol_version == b.e3ap_protocol_version
        && a.dapp_name == b.dapp_name
        && a.dapp_version == b.dapp_version
        && a.vendor == b.vendor;
}

static bool equals(const SetupResponse& a, const SetupResponse& b) {
    if (a.request_id != b.request_id) return false;
    if (a.response_code != b.response_code) return false;
    if (a.e3ap_protocol_version != b.e3ap_protocol_version) return false;
    if (a.dapp_identifier != b.dapp_identifier) return false;
    if (a.ran_identifier != b.ran_identifier) return false;
    if (a.ran_function_list.size() != b.ran_function_list.size()) return false;
    for (size_t i = 0; i < a.ran_function_list.size(); ++i) {
        if (!equals(a.ran_function_list[i], b.ran_function_list[i])) return false;
    }
    return true;
}

static bool equals(const SubscriptionRequest& a, const SubscriptionRequest& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.ran_function_identifier == b.ran_function_identifier
        && a.telemetry_identifier_list == b.telemetry_identifier_list
        && a.control_identifier_list == b.control_identifier_list
        && a.subscription_time == b.subscription_time
        && a.periodicity == b.periodicity;
}

static bool equals(const SubscriptionDelete& a, const SubscriptionDelete& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.subscription_id == b.subscription_id;
}

static bool equals(const SubscriptionResponse& a, const SubscriptionResponse& b) {
    return a.request_id == b.request_id
        && a.dapp_identifier == b.dapp_identifier
        && a.response_code == b.response_code
        && a.subscription_id == b.subscription_id;
}

static bool equals(const IndicationMessage& a, const IndicationMessage& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.ran_function_identifier == b.ran_function_identifier
        && a.protocol_data == b.protocol_data;
}

static bool equals(const DAppControlAction& a, const DAppControlAction& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.ran_function_identifier == b.ran_function_identifier
        && a.control_identifier == b.control_identifier
        && a.sequence_id == b.sequence_id
        && a.action_data == b.action_data;
}

static bool equals(const DAppReport& a, const DAppReport& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.ran_function_identifier == b.ran_function_identifier
        && a.sequence_id == b.sequence_id
        && a.report_data == b.report_data;
}

static bool equals(const XAppControlAction& a, const XAppControlAction& b) {
    return a.dapp_identifier == b.dapp_identifier
        && a.ran_function_identifier == b.ran_function_identifier
        && a.sequence_id == b.sequence_id
        && a.xapp_control_data == b.xapp_control_data;
}

static bool equals(const ReleaseMessage& a, const ReleaseMessage& b) {
    return a.dapp_identifier == b.dapp_identifier;
}

static bool equals(const MessageAck& a, const MessageAck& b) {
    return a.request_id == b.request_id
        && a.response_code == b.response_code;
}

// Adding an alternative to PduChoice without adding a comparator above (and a
// sample below) would leave that PDU type silently unguarded. Break the build
// instead.
static_assert(std::variant_size_v<PduChoice> == 11,
              "PduChoice gained or lost an alternative: add the matching "
              "equals() overload and sample PDU to this suite");

/// Full-structure equality, envelope included. The envelope is the point:
/// message_id and timestamp are exactly the fields a per-encoder test tends
/// not to look at.
static bool equals(const Pdu& a, const Pdu& b) {
    if (a.type != b.type) return false;
    if (a.message_id != b.message_id) return false;
    if (a.timestamp != b.timestamp) return false;
    if (a.choice.index() != b.choice.index()) return false;
    return std::visit(
        [&b](const auto& lhs) {
            using T = std::decay_t<decltype(lhs)>;
            return equals(lhs, std::get<T>(b.choice));
        },
        a.choice);
}

// ---------------------------------------------------------------------------
// Sample PDUs
// ---------------------------------------------------------------------------

/// A distinctive stamp, wide enough that a 32-bit truncation or a
/// milliseconds/nanoseconds mix-up shows up as a mismatch rather than as a
/// plausible number.
static constexpr uint64_t kStamp = 1787097600123456789ULL;

struct Sample {
    const char* name;
    Pdu pdu;
};

/// Every field set to a value distinguishable from its default, and every
/// value inside the ASN.1 constraints: E3-DAppID and the RAN-function and
/// control identifiers are 1..100, E3-MessageID is 1..1000, E3-Version is at
/// most 11 characters, octet strings are 1..32768 bytes.
static std::vector<Sample> sample_pdus() {
    std::vector<Sample> samples;

    auto add = [&samples](const char* name, PduType type, auto&& payload) {
        Pdu pdu(type);
        pdu.choice = std::forward<decltype(payload)>(payload);
        pdu.message_id = 917;
        pdu.timestamp = kStamp;
        samples.push_back(Sample{name, std::move(pdu)});
    };

    SetupRequest setup_req;
    setup_req.e3ap_protocol_version = "1.0.0";
    setup_req.dapp_name = "equivalence-dapp";
    setup_req.dapp_version = "2.3.4";
    setup_req.vendor = "wineslab";
    add("SetupRequest", PduType::SETUP_REQUEST, setup_req);

    RanFunctionDef ran_fn;
    ran_fn.ran_function_identifier = 5;
    ran_fn.telemetry_identifier_list = {1, 2, 3};
    ran_fn.control_identifier_list = {4, 5};
    ran_fn.ran_function_data = {0xDE, 0xAD, 0xBE, 0xEF};

    SetupResponse setup_resp;
    setup_resp.request_id = 917;
    setup_resp.response_code = ResponseCode::POSITIVE;
    setup_resp.e3ap_protocol_version = "1.0.0";
    setup_resp.dapp_identifier = 42;
    setup_resp.ran_identifier = "ran-node-1";
    setup_resp.ran_function_list = {ran_fn};
    add("SetupResponse", PduType::SETUP_RESPONSE, setup_resp);

    SubscriptionRequest sub_req;
    sub_req.dapp_identifier = 42;
    sub_req.ran_function_identifier = 5;
    sub_req.telemetry_identifier_list = {1, 2, 3};
    sub_req.control_identifier_list = {4, 5};
    sub_req.subscription_time = 600;
    sub_req.periodicity = 250;
    add("SubscriptionRequest", PduType::SUBSCRIPTION_REQUEST, sub_req);

    SubscriptionDelete sub_del;
    sub_del.dapp_identifier = 42;
    sub_del.subscription_id = 7;
    add("SubscriptionDelete", PduType::SUBSCRIPTION_DELETE, sub_del);

    SubscriptionResponse sub_resp;
    sub_resp.request_id = 917;
    sub_resp.dapp_identifier = 42;
    sub_resp.response_code = ResponseCode::POSITIVE;
    sub_resp.subscription_id = 7;
    add("SubscriptionResponse", PduType::SUBSCRIPTION_RESPONSE, sub_resp);

    // Valid JSON, because the JSON encoder nests protocolData rather than
    // hex-encoding it. The bytes are preserved verbatim by all three encoders.
    const std::string protocol_json = R"({"sfn":123,"slot":7})";
    IndicationMessage indication;
    indication.dapp_identifier = 42;
    indication.ran_function_identifier = 5;
    indication.protocol_data.assign(protocol_json.begin(), protocol_json.end());
    add("IndicationMessage", PduType::INDICATION_MESSAGE, indication);

    DAppControlAction control;
    control.dapp_identifier = 42;
    control.ran_function_identifier = 5;
    control.control_identifier = 9;
    control.sequence_id = 77;   // set: exercises the OPTIONAL-present branch
    control.action_data = {0x01, 0x02, 0x03, 0xFF, 0x00, 0x80};
    add("DAppControlAction", PduType::DAPP_CONTROL_ACTION, control);

    DAppReport report;
    report.dapp_identifier = 42;
    report.ran_function_identifier = 5;
    report.sequence_id = 77;
    report.report_data = {0xAA, 0xBB, 0xCC, 0x00, 0x11};
    add("DAppReport", PduType::DAPP_REPORT, report);

    XAppControlAction xapp;
    xapp.dapp_identifier = 42;
    xapp.ran_function_identifier = 5;
    xapp.sequence_id = 77;
    xapp.xapp_control_data = {0x7F, 0x80, 0x00, 0x01};
    add("XAppControlAction", PduType::XAPP_CONTROL_ACTION, xapp);

    ReleaseMessage release;
    release.dapp_identifier = 42;
    add("ReleaseMessage", PduType::RELEASE_MESSAGE, release);

    MessageAck ack;
    ack.request_id = 917;
    ack.response_code = ResponseCode::POSITIVE;
    add("MessageAck", PduType::MESSAGE_ACK, ack);

    return samples;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(EncoderEquivalence_at_least_one_encoder_is_enabled) {
    // Without this the whole suite would pass vacuously on a build where the
    // probe came back empty, which is exactly the failure it must not have.
    ASSERT_FALSE(available_encoders().empty());
}

TEST(EncoderEquivalence_every_encoder_covers_every_pdu_type) {
    ASSERT_EQ(sample_pdus().size(), static_cast<size_t>(std::variant_size_v<PduChoice>));
}

TEST(EncoderEquivalence_roundtrip_preserves_every_field) {
    const auto formats = available_encoders();
    ASSERT_FALSE(formats.empty());

    for (auto format : formats) {
        auto encoder = create_encoder(format);
        ASSERT_TRUE(encoder != nullptr);

        for (const auto& sample : sample_pdus()) {
            auto encoded = encoder->encode(sample.pdu);
            report_if(!encoded.has_value(), format, sample.name, "encode failed");
            ASSERT_TRUE(encoded.has_value());

            auto decoded = encoder->decode(*encoded);
            report_if(!decoded.has_value(), format, sample.name, "decode failed");
            ASSERT_TRUE(decoded.has_value());

            const bool same = equals(sample.pdu, *decoded);
            report_if(!same, format, sample.name, "round trip changed a field");
            ASSERT_TRUE(same);
        }
    }
}

TEST(EncoderEquivalence_all_encoders_decode_to_the_same_pdu) {
    const auto formats = available_encoders();
    ASSERT_FALSE(formats.empty());

    // The leg that catches a field one grammar does not carry: a per-encoder
    // round trip cannot see the difference, but two encoders disagreeing can.
    for (const auto& sample : sample_pdus()) {
        std::vector<Pdu> decoded_per_format;
        for (auto format : formats) {
            auto encoder = create_encoder(format);
            ASSERT_TRUE(encoder != nullptr);

            auto encoded = encoder->encode(sample.pdu);
            ASSERT_TRUE(encoded.has_value());
            auto decoded = encoder->decode(*encoded);
            ASSERT_TRUE(decoded.has_value());
            decoded_per_format.push_back(*decoded);
        }

        for (size_t i = 1; i < decoded_per_format.size(); ++i) {
            const bool agree = equals(decoded_per_format[0], decoded_per_format[i]);
            if (!agree) {
                // Which of the two is wrong is not knowable from here, only
                // that they disagree. Name both.
                std::fprintf(stderr, "       -> %s and %s decode %s differently\n",
                             format_name(formats[0]), format_name(formats[i]),
                             sample.name);
            }
            ASSERT_TRUE(agree);
        }
    }
}

TEST(EncoderEquivalence_timestamp_survives_the_wire) {
    const auto formats = available_encoders();
    ASSERT_FALSE(formats.empty());

    // Called out separately from the structural comparison above because this
    // is the field that was being dropped, and because the ASN.1 failure mode
    // was not a zero but the receiver's own clock: a value that looks right.
    for (auto format : formats) {
        auto encoder = create_encoder(format);
        ASSERT_TRUE(encoder != nullptr);

        for (const auto& sample : sample_pdus()) {
            auto encoded = encoder->encode(sample.pdu);
            ASSERT_TRUE(encoded.has_value());
            auto decoded = encoder->decode(*encoded);
            ASSERT_TRUE(decoded.has_value());
            report_if(decoded->timestamp != kStamp, format, sample.name,
                      "timestamp did not survive the wire");
            ASSERT_EQ(decoded->timestamp, kStamp);
        }
    }
}

TEST(EncoderEquivalence_absent_timestamp_decodes_to_zero) {
    const auto formats = available_encoders();
    ASSERT_FALSE(formats.empty());

    // Zero is the "no producer reference" sentinel, and it has to survive as
    // zero on every path. A PDU without a timestamp is a normal message, so
    // no encoder may reject it either.
    for (auto format : formats) {
        auto encoder = create_encoder(format);
        ASSERT_TRUE(encoder != nullptr);

        for (const auto& sample : sample_pdus()) {
            Pdu unstamped = sample.pdu;
            unstamped.timestamp = 0;

            auto encoded = encoder->encode(unstamped);
            ASSERT_TRUE(encoded.has_value());
            auto decoded = encoder->decode(*encoded);
            ASSERT_TRUE(decoded.has_value());
            report_if(decoded->timestamp != 0u, format, sample.name,
                      "absent timestamp did not decode to zero");
            ASSERT_EQ(decoded->timestamp, 0u);
            ASSERT_TRUE(equals(unstamped, *decoded));
        }
    }
}

TEST(EncoderEquivalence_pdu_is_stamped_on_construction) {
    const uint64_t before = now_realtime_ns();
    Pdu pdu(PduType::INDICATION_MESSAGE);
    const uint64_t after = now_realtime_ns();

    ASSERT_GE(pdu.timestamp, before);
    ASSERT_LE(pdu.timestamp, after);

    // Nanoseconds, not milliseconds: a millisecond epoch stamp is ~1.8e12 and
    // would fail this by six orders of magnitude.
    ASSERT_GT(pdu.timestamp, 1000000000000000ULL);
}

int main() {
    return RUN_ALL_TESTS();
}
