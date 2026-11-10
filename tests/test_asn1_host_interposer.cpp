/**
 * @file test_asn1_host_interposer.cpp
 * @brief A host that exports asn1c functions of its own does not get libe3's calls.
 *
 * OAI links -rdynamic and carries its own asn1c runtime. When libe3's shared library
 * exports its runtime, libe3's calls into it resolve to the host's copy, whose type
 * descriptors may have another layout: the first encode then crashes. This executable
 * is such a host. It defines the three runtime functions libe3's ASN.1 encoder calls,
 * counts the calls and forwards them, and links libe3's shared library. Encoding and
 * decoding must give the pinned bytes and must never reach the host's functions.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <dlfcn.h>

#include <string>
#include <vector>

extern "C" {
#include "OCTET_STRING.h"
#include "aper_decoder.h"
#include "aper_encoder.h"
}

namespace {

int g_host_calls = 0;

template <typename Fn>
Fn next_symbol(const char* name) {
    return reinterpret_cast<Fn>(dlsym(RTLD_NEXT, name));
}

std::string to_hex(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (uint8_t b : bytes) {
        out += digits[b >> 4];
        out += digits[b & 0xF];
    }
    return out;
}

}  // namespace

extern "C" {

asn_enc_rval_t aper_encode_to_buffer(const asn_TYPE_descriptor_t* td, const asn_per_constraints_t* constraints,
                                     const void* struct_ptr, void* buffer, size_t buffer_size) {
    ++g_host_calls;
    static auto next = next_symbol<decltype(&aper_encode_to_buffer)>("aper_encode_to_buffer");
    return next(td, constraints, struct_ptr, buffer, buffer_size);
}

asn_dec_rval_t aper_decode(const asn_codec_ctx_t* opt_codec_ctx, const asn_TYPE_descriptor_t* td, void** struct_ptr,
                           const void* buffer, size_t size, int skip_bits, int unused_bits) {
    ++g_host_calls;
    static auto next = next_symbol<decltype(&aper_decode)>("aper_decode");
    return next(opt_codec_ctx, td, struct_ptr, buffer, size, skip_bits, unused_bits);
}

int OCTET_STRING_fromBuf(OCTET_STRING_t* s, const char* str, int len) {
    ++g_host_calls;
    static auto next = next_symbol<decltype(&OCTET_STRING_fromBuf)>("OCTET_STRING_fromBuf");
    return next(s, str, len);
}

}  // extern "C"

namespace {

libe3::Pdu make(libe3::PduType type, libe3::PduChoice choice) {
    libe3::Pdu pdu(type);
    pdu.message_id = 42;
    pdu.timestamp = 1756800000123456789ull;
    pdu.choice = std::move(choice);
    return pdu;
}

void round_trip(const libe3::Pdu& pdu, const std::string& hex) {
    auto encoder = libe3::create_encoder(libe3::EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_STREQ(to_hex(encoded->buffer), hex);

    auto decoded = encoder->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(decoded->type == pdu.type);
    ASSERT_EQ(decoded->message_id, pdu.message_id);
}

}  // namespace

TEST(Asn1HostInterposer_setupRequestNeverReachesTheHost) {
    libe3::SetupRequest request;
    request.e3ap_protocol_version = "1.0.0";
    request.dapp_name = "vector-dapp";
    request.dapp_version = "0.1.3";
    request.vendor = "wineslab";
    round_trip(make(libe3::PduType::SETUP_REQUEST, request),
               "4029081861684accc3cd150080312e302e3028766563746f722d6461707020302e312e331c77696e65736c6162");
    ASSERT_EQ(g_host_calls, 0);
}

TEST(Asn1HostInterposer_indicationNeverReachesTheHost) {
    libe3::IndicationMessage indication;
    indication.dapp_identifier = 7;
    indication.ran_function_identifier = 1;
    indication.protocol_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    round_trip(make(libe3::PduType::INDICATION_MESSAGE, indication),
               "4029081861684accc3cd152800060000000f0102030405060708090a0b0c0d0e0f10");
    ASSERT_EQ(g_host_calls, 0);
}

int main() {
    return RUN_ALL_TESTS();
}
