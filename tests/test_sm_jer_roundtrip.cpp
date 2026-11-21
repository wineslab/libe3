/**
 * @file test_sm_jer_roundtrip.cpp
 * @brief A service model payload written as JER travels in a JSON E3AP message and decodes back.
 *
 * The Simple service model's grammar is compiled into libe3's asn1c runtime, which has JER
 * whenever libe3 has JSON. A payload is encoded with JER, carried as a nested JSON object in
 * an indication and in a setup response, decoded, JER-decoded, and compared with the value that
 * the APER encoding of the same payload decodes to.
 *
 * asn1c's JER decoder wants the members in the order of the grammar. The JSON encoder splices
 * protocolData, actionData and reportData as they were written, but parses and writes
 * ranFunctionData again with the keys sorted, so a JER payload in a setup response survives
 * only when its grammar order is also alphabetical. The two payloads below are.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/asn1_layout_check.h"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "Simple-Indication.h"
#include "Simple-RanFunctionData.h"
#include "aper_decoder.h"
#include "aper_encoder.h"
#include "jer_decoder.h"
#include "jer_encoder.h"

static_assert(LIBE3_ASN1_JER == 1, "this test needs the asn1c runtime that libe3 builds with JSON");

using namespace libe3;

namespace {

std::string jer_text(const asn_TYPE_descriptor_t& td, const void* value) {
    std::string out;
    auto append = [](const void* buffer, size_t size, void* key) -> int {
        static_cast<std::string*>(key)->append(static_cast<const char*>(buffer), size);
        return 0;
    };
    const asn_enc_rval_t encoded = jer_encode(&td, value, JER_F_MINIFIED, append, &out);
    return encoded.encoded < 0 ? std::string() : out;
}

void* from_aper(const asn_TYPE_descriptor_t& td, const void* value) {
    uint8_t buffer[256];
    const asn_enc_rval_t encoded = aper_encode_to_buffer(&td, nullptr, value, buffer, sizeof(buffer));
    if (encoded.encoded < 0) return nullptr;
    void* back = nullptr;
    const asn_dec_rval_t decoded =
        aper_decode(nullptr, &td, &back, buffer, static_cast<size_t>((encoded.encoded + 7) / 8), 0, 0);
    return decoded.code == RC_OK ? back : nullptr;
}

void* from_jer(const asn_TYPE_descriptor_t& td, const std::string& text) {
    void* back = nullptr;
    const asn_dec_rval_t decoded = jer_decode(nullptr, &td, &back, text.data(), text.size());
    return decoded.code == RC_OK ? back : nullptr;
}

void release(const asn_TYPE_descriptor_t& td, void* value) {
    td.op->free_struct(&td, value, ASFM_FREE_EVERYTHING);
}

std::vector<uint8_t> bytes_of(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string text_of(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::string wire_text(const EncodedMessage& message) {
    return std::string(message.buffer.begin(), message.buffer.end());
}

// The value's JER text, carried through `through`, must decode to the value its APER bytes give.
void expect_same_value(const asn_TYPE_descriptor_t& td, const void* original, const std::string& through) {
    void* from_wire = from_jer(td, through);
    void* from_binary = from_aper(td, original);
    ASSERT_TRUE(from_wire != nullptr);
    ASSERT_TRUE(from_binary != nullptr);
    ASSERT_EQ(td.op->compare_struct(&td, from_wire, from_binary), 0);
    ASSERT_EQ(td.op->compare_struct(&td, from_wire, original), 0);
    release(td, from_wire);
    release(td, from_binary);
}

}  // namespace

TEST(SmJerRoundtrip_indicationPayloadIsNestedAndDecodesBack) {
    Simple_Indication_t value{};
    long timestamp = 1756800000;
    value.data1 = 1234567;
    value.timestamp = &timestamp;

    const std::string jer = jer_text(asn_DEF_Simple_Indication, &value);
    ASSERT_STREQ(jer, "{\"data1\":1234567,\"timestamp\":1756800000}");

    auto encoder = create_encoder(EncodingFormat::JSON);
    ASSERT_TRUE(encoder != nullptr);

    IndicationMessage indication;
    indication.dapp_identifier = 7;
    indication.ran_function_identifier = 2;
    indication.protocol_data = bytes_of(jer);
    Pdu pdu(PduType::INDICATION_MESSAGE, indication);
    pdu.message_id = 42;

    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_TRUE(wire_text(*encoded).find("\"protocolData\":" + jer) != std::string::npos);

    auto decoded = encoder->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    const auto* back = std::get_if<IndicationMessage>(&decoded->choice);
    ASSERT_TRUE(back != nullptr);
    ASSERT_STREQ(text_of(back->protocol_data), jer);

    expect_same_value(asn_DEF_Simple_Indication, &value, text_of(back->protocol_data));
}

TEST(SmJerRoundtrip_ranFunctionDataIsNestedAndDecodesBack) {
    Simple_RanFunctionData_t value{};
    ASSERT_EQ(OCTET_STRING_fromBuf(&value.name, "simple-sm", 9), 0);

    const std::string jer = jer_text(asn_DEF_Simple_RanFunctionData, &value);
    ASSERT_TRUE(jer.size() > 2);

    auto encoder = create_encoder(EncodingFormat::JSON);
    ASSERT_TRUE(encoder != nullptr);

    SetupResponse response;
    response.request_id = 42;
    response.response_code = ResponseCode::POSITIVE;
    response.dapp_identifier = 3;
    response.ran_identifier = "ran";
    RanFunctionDef function;
    function.ran_function_identifier = 1;
    function.telemetry_identifier_list = {1};
    function.control_identifier_list = {1};
    function.ran_function_data = bytes_of(jer);
    response.ran_function_list.push_back(function);
    Pdu pdu(PduType::SETUP_RESPONSE, response);
    pdu.message_id = 42;

    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    const auto* back = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(back != nullptr);
    ASSERT_EQ(back->ran_function_list.size(), static_cast<size_t>(1));

    expect_same_value(asn_DEF_Simple_RanFunctionData, &value, text_of(back->ran_function_list[0].ran_function_data));
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_Simple_RanFunctionData, &value);
}

TEST(SmJerRoundtrip_twoMemberPayloadInSetupResponseKeepsItsOrder) {
    Simple_Indication_t value{};
    long timestamp = 99;
    value.data1 = 5;
    value.timestamp = &timestamp;
    const std::string jer = jer_text(asn_DEF_Simple_Indication, &value);

    auto encoder = create_encoder(EncodingFormat::JSON);
    ASSERT_TRUE(encoder != nullptr);

    SetupResponse response;
    response.request_id = 1;
    response.response_code = ResponseCode::POSITIVE;
    response.dapp_identifier = 1;
    response.ran_identifier = "ran";
    RanFunctionDef function;
    function.ran_function_identifier = 1;
    function.ran_function_data = bytes_of(jer);
    response.ran_function_list.push_back(function);

    auto encoded = encoder->encode(Pdu(PduType::SETUP_RESPONSE, response));
    ASSERT_TRUE(encoded.has_value());
    auto decoded = encoder->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    const auto* back = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(back != nullptr);
    ASSERT_EQ(back->ran_function_list.size(), static_cast<size_t>(1));

    expect_same_value(asn_DEF_Simple_Indication, &value, text_of(back->ran_function_list[0].ran_function_data));
}

int main() {
    return RUN_ALL_TESTS();
}
