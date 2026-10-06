/**
 * @file test_asn1_size.cpp
 * @brief Tests for the ASN.1 APER encoder.
 *
 * Verifies three properties of the encoder:
 *
 *   (1) The encoded buffer size is bounded as a small linear function of
 *       the payload size — i.e., encoded ≤ 2·payload + a fixed envelope
 *       allowance — across a range of payload sizes from tens of bytes
 *       to several kilobytes.
 *
 *   (2) Encode → decode round-trips preserve the original payload
 *       byte-for-byte, with no truncation, reordering, or trailing
 *       contamination.
 *
 *   (3) The encoded size grows linearly with payload size, so the
 *       difference between encodings of different-sized payloads tracks
 *       the payload delta plus the fixed envelope.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <string>
#include <tuple>
#include <vector>
#include <cstdint>

using namespace libe3;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Maximum byte overhead the E3-PDU + per-type CHOICE wrapper may add.
 *
 * The ASN.1 APER envelope (message_id, dApp/RAN-function identifiers,
 * length prefixes, CHOICE tag) typically occupies a few tens of bytes
 * for the E3-PDU types in this library.  256 B of headroom keeps the
 * bound robust to schema changes that legitimately grow the envelope.
 */
static constexpr size_t ENVELOPE_OVERHEAD_MAX = 256;

/**
 * @brief Upper bound on the encoded buffer size for a payload of N bytes.
 *
 * A well-formed APER encoder produces output of roughly N bytes plus a
 * small fixed envelope.  We use 2·N + ENVELOPE_OVERHEAD_MAX as the
 * assertion threshold — comfortable headroom for any reasonable
 * encoder/schema variation.
 */
static size_t expected_max_encoded(size_t payload_bytes) {
    return 2 * payload_bytes + ENVELOPE_OVERHEAD_MAX;
}

/**
 * @brief Build a deterministic payload of a given size.
 *
 * Uses a simple LCG so every byte position is distinguishable; round-trip
 * checks can therefore detect any byte reordering, truncation, or
 * modification of the payload.
 */
static std::vector<uint8_t> make_payload(size_t n) {
    std::vector<uint8_t> v(n);
    uint32_t s = 0x9E3779B9u;  // golden-ratio LCG seed
    for (size_t i = 0; i < n; ++i) {
        s = s * 1103515245u + 12345u;
        v[i] = static_cast<uint8_t>(s >> 16);
    }
    return v;
}

static std::unique_ptr<E3Encoder> make_encoder() {
    auto enc = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(enc != nullptr);
    return enc;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * Encoded size of a small DAppReport (~70 B payload) stays within
 * 2·payload + envelope.
 */
TEST(Asn1Size_DAppReport_smallPayload) {
    auto enc = make_encoder();
    auto payload = make_payload(70);

    Pdu pdu(PduType::DAPP_REPORT);
    pdu.message_id = 42;
    DAppReport rep;
    rep.dapp_identifier = 1;
    rep.ran_function_identifier = 1;
    rep.sequence_id = 1;
    rep.report_data = payload;
    pdu.choice = rep;

    auto result = enc->encode(pdu);
    ASSERT_TRUE(result.has_value());

    const size_t encoded_size = result->buffer.size();
    ASSERT_GT(encoded_size, 0u);
    ASSERT_LE(encoded_size, expected_max_encoded(payload.size()));
}

/**
 * Encoded size of a 225-byte XAppControlAction stays within
 * 2·payload + envelope.
 */
TEST(Asn1Size_XAppControlAction_mediumPayload) {
    auto enc = make_encoder();
    auto payload = make_payload(225);

    Pdu pdu(PduType::XAPP_CONTROL_ACTION);
    pdu.message_id = 7;
    XAppControlAction action;
    action.dapp_identifier = 1;
    action.ran_function_identifier = 1;
    action.sequence_id = 1;
    action.xapp_control_data = payload;
    pdu.choice = action;

    auto result = enc->encode(pdu);
    ASSERT_TRUE(result.has_value());

    const size_t encoded_size = result->buffer.size();
    ASSERT_GT(encoded_size, payload.size());     // sanity: at least N bytes
    ASSERT_LE(encoded_size, expected_max_encoded(payload.size()));
}

/**
 * Encoded size of an IndicationMessage with an 8 KB payload stays within
 * 2·payload + envelope.
 */
TEST(Asn1Size_IndicationMessage_largePayload) {
    auto enc = make_encoder();
    auto payload = make_payload(8192);

    Pdu pdu(PduType::INDICATION_MESSAGE);
    pdu.message_id = 234;   // E3-MessageID is INTEGER (1..4294967295)
    IndicationMessage msg;
    msg.dapp_identifier = 1;
    msg.ran_function_identifier = 1;
    msg.protocol_data = payload;
    pdu.choice = msg;

    auto result = enc->encode(pdu);
    ASSERT_TRUE(result.has_value());

    const size_t encoded_size = result->buffer.size();
    ASSERT_GT(encoded_size, payload.size());
    ASSERT_LE(encoded_size, expected_max_encoded(payload.size()));
}

/**
 * Encode → decode round-trip: the decoded payload must equal the
 * original byte-for-byte.
 */
TEST(Asn1Size_DAppReport_roundTrip_preservesPayload) {
    auto enc = make_encoder();
    auto payload = make_payload(225);

    Pdu pdu(PduType::DAPP_REPORT);
    pdu.message_id = 99;
    DAppReport rep;
    rep.dapp_identifier = 3;
    rep.ran_function_identifier = 1;
    rep.sequence_id = 1;
    rep.report_data = payload;
    pdu.choice = rep;

    auto encoded = enc->encode(pdu);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(static_cast<int>(decoded->type),
              static_cast<int>(PduType::DAPP_REPORT));

    auto* out = std::get_if<DAppReport>(&decoded->choice);
    ASSERT_TRUE(out != nullptr);
    ASSERT_EQ(out->report_data.size(), payload.size());
    for (size_t i = 0; i < payload.size(); ++i) {
        ASSERT_EQ(static_cast<int>(out->report_data[i]),
                  static_cast<int>(payload[i]));
    }
}

/**
 * Encode → decode round-trip of a SubscriptionRequest: the OPTIONAL
 * subscriptionTime and periodicity members must survive.
 */
TEST(Asn1Size_SubscriptionRequest_roundTrip_preservesOptionals) {
    auto enc = make_encoder();

    Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
    pdu.message_id = 7;
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_function_identifier = 2;
    req.telemetry_identifier_list = {1, 2, 3};
    req.control_identifier_list = {1};
    req.subscription_time = 60;
    req.periodicity = 500;
    pdu.choice = req;

    auto encoded = enc->encode(pdu);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(static_cast<int>(decoded->type),
              static_cast<int>(PduType::SUBSCRIPTION_REQUEST));

    auto* out = std::get_if<SubscriptionRequest>(&decoded->choice);
    ASSERT_TRUE(out != nullptr);
    ASSERT_EQ(out->dapp_identifier, 42u);
    ASSERT_EQ(out->ran_function_identifier, 2u);
    ASSERT_TRUE(out->subscription_time.has_value());
    ASSERT_EQ(out->subscription_time.value(), 60u);
    ASSERT_TRUE(out->periodicity.has_value());
    ASSERT_EQ(out->periodicity.value(), 500u);
}

/**
 * periodicity is microseconds, 0..60000000. Aerial's default (100000) and the
 * ceiling must round-trip on the request and on the granted value in the
 * response; one past the ceiling must not encode.
 */
TEST(Asn1Size_SubscriptionRequest_roundTrip_periodicityRange) {
    auto enc = make_encoder();

    for (uint32_t value : {0u, 100000u, 60000000u}) {
        Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
        pdu.message_id = 7;
        SubscriptionRequest req;
        req.dapp_identifier = 42;
        req.ran_function_identifier = 2;
        req.periodicity = value;
        pdu.choice = req;

        auto encoded = enc->encode(pdu);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
        ASSERT_TRUE(decoded.has_value());
        auto* out = std::get_if<SubscriptionRequest>(&decoded->choice);
        ASSERT_TRUE(out != nullptr);
        ASSERT_TRUE(out->periodicity.has_value());
        ASSERT_EQ(out->periodicity.value(), value);
    }
}

TEST(Asn1Size_SubscriptionResponse_roundTrip_periodicityRange) {
    auto enc = make_encoder();

    for (uint32_t value : {0u, 100000u, 60000000u}) {
        Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
        pdu.message_id = 8;
        SubscriptionResponse resp;
        resp.request_id = 7;
        resp.dapp_identifier = 42;
        resp.response_code = ResponseCode::POSITIVE;
        resp.subscription_id = 1;
        resp.periodicity = value;
        pdu.choice = resp;

        auto encoded = enc->encode(pdu);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
        ASSERT_TRUE(decoded.has_value());
        auto* out = std::get_if<SubscriptionResponse>(&decoded->choice);
        ASSERT_TRUE(out != nullptr);
        ASSERT_TRUE(out->periodicity.has_value());
        ASSERT_EQ(out->periodicity.value(), value);
    }
}

TEST(Asn1Size_SubscriptionRequest_encode_rejectsPeriodicityAboveRange) {
    auto enc = make_encoder();

    Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
    pdu.message_id = 7;
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_function_identifier = 2;
    req.periodicity = 60000001u;
    pdu.choice = req;

    auto encoded = enc->encode(pdu);
    ASSERT_FALSE(encoded.has_value());
    ASSERT_EQ(static_cast<int>(encoded.error()),
              static_cast<int>(ErrorCode::ENCODE_FAILED));
}

/**
 * Encoded size grows linearly with payload size: the delta between
 * small-payload and large-payload encodings tracks the payload delta
 * plus the fixed envelope.
 */
TEST(Asn1Size_growsLinearlyWithPayload) {
    auto enc = make_encoder();

    auto encode_size = [&](size_t n) -> size_t {
        Pdu pdu(PduType::DAPP_REPORT);
        pdu.message_id = 1;
        DAppReport rep;
        rep.dapp_identifier = 1;
        rep.ran_function_identifier = 1;
        rep.sequence_id = 1;
        rep.report_data = make_payload(n);
        pdu.choice = rep;
        auto r = enc->encode(pdu);
        ASSERT_TRUE(r.has_value());
        return r->buffer.size();
    };

    const size_t s_small  = encode_size(64);
    const size_t s_medium = encode_size(512);
    const size_t s_large  = encode_size(4096);

    // Each payload size individually satisfies the linear bound.
    ASSERT_LE(s_small,  expected_max_encoded(64));
    ASSERT_LE(s_medium, expected_max_encoded(512));
    ASSERT_LE(s_large,  expected_max_encoded(4096));

    // Growth rate from small to large tracks payload growth, not a
    // larger multiple.
    const size_t delta = s_large - s_small;
    const size_t naive_payload_delta = 4096 - 64;          // 4032
    ASSERT_LE(delta, 2 * naive_payload_delta + ENVELOPE_OVERHEAD_MAX);
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Setup strings. dAppName, vendor and ranIdentifier are OCTET STRING (SIZE
// (1..64)) and the version fields OCTET STRING (SIZE (1..32)); sizes count
// bytes, not characters.
// ---------------------------------------------------------------------------

static std::string repeat(const std::string& unit, size_t times) {
    std::string out;
    for (size_t i = 0; i < times; ++i) out += unit;
    return out;
}

static EncodeResult<EncodedMessage> encode_setup_request_strings(
        const std::string& version, const std::string& name,
        const std::string& dapp_version, const std::string& vendor) {
    auto enc = make_encoder();
    return enc->encode_setup_request(42, version, name, dapp_version, vendor);
}

static EncodeResult<EncodedMessage> encode_setup_response_strings(
        const std::string& version, const std::string& ran_identifier) {
    auto enc = make_encoder();
    return enc->encode_setup_response(
        42, 7, ResponseCode::POSITIVE, version, 3u, ran_identifier);
}

TEST(Asn1Size_SetupRequest_strings_roundTripAtBounds) {
    auto enc = make_encoder();
    const std::string name64 = repeat("n", 64);
    const std::string vendor64 = repeat("v", 64);
    const std::string version32 = repeat("1", 32);

    for (const auto& [name, vendor, version] : {
             std::tuple<std::string, std::string, std::string>{"a", "b", "c"},
             {name64, vendor64, version32}}) {
        auto encoded = encode_setup_request_strings(version, name, version, vendor);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
        ASSERT_TRUE(decoded.has_value());
        auto* out = std::get_if<SetupRequest>(&decoded->choice);
        ASSERT_TRUE(out != nullptr);
        ASSERT_TRUE(out->dapp_name == name);
        ASSERT_TRUE(out->vendor == vendor);
        ASSERT_TRUE(out->e3ap_protocol_version == version);
        ASSERT_TRUE(out->dapp_version == version);
    }
}

TEST(Asn1Size_SetupRequest_strings_outOfRangeFailToEncode) {
    const std::string ok = "x";
    const int failed = static_cast<int>(ErrorCode::ENCODE_FAILED);

    auto r = encode_setup_request_strings(ok, "", ok, ok);
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(static_cast<int>(r.error()), failed);
    r = encode_setup_request_strings(ok, repeat("n", 65), ok, ok);
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(static_cast<int>(r.error()), failed);
    r = encode_setup_request_strings(ok, ok, ok, "");
    ASSERT_FALSE(r.has_value());
    r = encode_setup_request_strings(ok, ok, ok, repeat("v", 65));
    ASSERT_FALSE(r.has_value());
    r = encode_setup_request_strings("", ok, ok, ok);
    ASSERT_FALSE(r.has_value());
    r = encode_setup_request_strings(repeat("1", 33), ok, ok, ok);
    ASSERT_FALSE(r.has_value());
    r = encode_setup_request_strings(ok, ok, repeat("1", 33), ok);
    ASSERT_FALSE(r.has_value());
}

TEST(Asn1Size_SetupRequest_strings_countBytesNotCharacters) {
    auto enc = make_encoder();
    // U+00E9 is two bytes in UTF-8: 32 of them are 64 bytes, 33 are 66.
    const std::string e_acute = "\xC3\xA9";

    auto encoded = encode_setup_request_strings("1.0.0", repeat(e_acute, 32), "1.0.0", "v");
    ASSERT_TRUE(encoded.has_value());
    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    auto* out = std::get_if<SetupRequest>(&decoded->choice);
    ASSERT_TRUE(out != nullptr);
    ASSERT_TRUE(out->dapp_name == repeat(e_acute, 32));

    ASSERT_FALSE(encode_setup_request_strings("1.0.0", repeat(e_acute, 33), "1.0.0", "v").has_value());
}

TEST(Asn1Size_SetupResponse_strings_roundTripAndRejectOutOfRange) {
    auto enc = make_encoder();
    const std::string ran64 = repeat("r", 64);
    const std::string version32 = repeat("2", 32);

    auto encoded = encode_setup_response_strings(version32, ran64);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    auto* out = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(out != nullptr);
    ASSERT_TRUE(out->ran_identifier == ran64);
    ASSERT_TRUE(out->e3ap_protocol_version.has_value());
    ASSERT_TRUE(*out->e3ap_protocol_version == version32);

    ASSERT_FALSE(encode_setup_response_strings("1.0.0", "").has_value());
    ASSERT_FALSE(encode_setup_response_strings("1.0.0", repeat("r", 65)).has_value());
    ASSERT_FALSE(encode_setup_response_strings(repeat("2", 33), "ran").has_value());
}

static std::string to_hex(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (uint8_t b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0xF];
    }
    return out;
}

/// Pins the APER bytes of one SetupRequest. A string occupies a 6-bit length
/// (SIZE (1..64) holds 64 values) before its octets, a version a 5-bit one
/// (SIZE (1..32)); a different grammar or compiler shows up here first.
TEST(Asn1Size_SetupRequest_strings_wireBytesArePinned) {
    auto enc = make_encoder();
    Pdu pdu(PduType::SETUP_REQUEST);
    pdu.message_id = 42;
    pdu.timestamp = 1756800000123456789ull;
    SetupRequest req;
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "vector-dapp";
    req.dapp_version = "0.1.3";
    req.vendor = "wineslab";
    pdu.choice = req;

    auto encoded = enc->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_STREQ(to_hex(encoded->buffer).c_str(), "4029081861684accc3cd150080312e302e3028766563746f722d6461707020302e312e331c77696e65736c6162");
}

int main() {
    return RUN_ALL_TESTS();
}
