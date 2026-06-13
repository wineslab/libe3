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
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

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
    pdu.message_id = 234;   // E3-MessageID is constrained to INTEGER (1..1000)
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

/**
 * A setupResponse that advertises an SM with EMPTY ran_function_data must
 * still encode. The schema makes ranFunctionData a mandatory
 * OCTET STRING (SIZE (1..32768)), so the encoder substitutes a 1-byte 0x00
 * placeholder; without that substitution the APER size constraint fails and
 * the whole setupResponse encode aborts. The result must also decode, with
 * the placeholder visible to the peer and every other field intact.
 */
TEST(Asn1_setupResponse_emptyRanFunctionData_stillEncodes) {
    auto enc = make_encoder();

    RanFunctionDef with_data;
    with_data.ran_function_identifier = 1;
    with_data.telemetry_identifier_list = {1, 2};
    with_data.control_identifier_list = {10};
    with_data.ran_function_data = {0xAB, 0xCD};

    RanFunctionDef without_data;
    without_data.ran_function_identifier = 2;
    without_data.telemetry_identifier_list = {3};
    without_data.control_identifier_list = {20};
    // ran_function_data deliberately left empty (ServiceModel default)

    auto encoded = enc->encode_setup_response(
        7,                              // message_id
        42,                             // request_id
        ResponseCode::POSITIVE,
        std::string("1.0.0"),           // e3ap_protocol_version
        3u,                             // dapp_identifier
        "asn1-test-ran",                // ran_identifier
        {with_data, without_data});
    ASSERT_TRUE(encoded.has_value());

    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(static_cast<int>(decoded->type),
              static_cast<int>(PduType::SETUP_RESPONSE));

    auto* resp = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(resp != nullptr);
    ASSERT_EQ(static_cast<int>(resp->response_code),
              static_cast<int>(ResponseCode::POSITIVE));
    ASSERT_EQ(resp->request_id, 42u);
    ASSERT_EQ(resp->ran_function_list.size(), 2u);

    // SM with real data: payload round-trips byte-for-byte.
    ASSERT_EQ(resp->ran_function_list[0].ran_function_identifier, 1u);
    ASSERT_EQ(resp->ran_function_list[0].ran_function_data.size(), 2u);
    ASSERT_EQ(static_cast<int>(resp->ran_function_list[0].ran_function_data[0]), 0xAB);
    ASSERT_EQ(static_cast<int>(resp->ran_function_list[0].ran_function_data[1]), 0xCD);

    // SM without data: the wire carries the 1-byte 0x00 placeholder.
    ASSERT_EQ(resp->ran_function_list[1].ran_function_identifier, 2u);
    ASSERT_EQ(resp->ran_function_list[1].ran_function_data.size(), 1u);
    ASSERT_EQ(static_cast<int>(resp->ran_function_list[1].ran_function_data[0]), 0x00);
}

// ---------------------------------------------------------------------------

int main() {
    return RUN_ALL_TESTS();
}
