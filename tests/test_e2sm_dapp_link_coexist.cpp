/**
 * @file test_e2sm_dapp_link_coexist.cpp
 * @brief libe3 and libe3::e2sm_dapp in one executable.
 *
 * Both carry their own copy of the asn1c runtime (asn1_e3ap inside libe3,
 * asn1_e2sm_dapp inside the codec), compiled from different grammars. This
 * test is the proof that they link together, and that an E3AP APER encode and
 * an E2SM-DAPP pack, interleaved, still give the right bytes. It is built twice:
 * static libe3 + static codec, and shared libe3 + static codec (see
 * cmake/libe3Tests.cmake).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include "libe3/e2sm_dapp.hpp"
#include "libe3/e2sm_dapp_c.h"
#include "libe3/e3_encoder.hpp"
#include "libe3/libe3.hpp"
#include "libe3/types.hpp"

#include "e2sm_dapp_test_util.hpp"

using namespace e2sm_dapp_test;
namespace fx = e2sm_dapp_fixtures;

namespace {

std::vector<uint8_t> make_payload(size_t n) {
    std::vector<uint8_t> v(n);
    uint32_t             s = 0x9E3779B9u;
    for (size_t i = 0; i < n; ++i) {
        s    = s * 1103515245u + 12345u;
        v[i] = static_cast<uint8_t>(s >> 16);
    }
    return v;
}

/// Encode a DAppReport with the E3AP APER encoder and decode it back.
void e3ap_roundtrip(size_t payload_size) {
    auto enc = libe3::create_encoder(libe3::EncodingFormat::ASN1);
    ASSERT_TRUE(enc != nullptr);

    const auto payload = make_payload(payload_size);
    libe3::Pdu pdu(libe3::PduType::DAPP_REPORT);
    pdu.message_id = 42;
    libe3::DAppReport rep;
    rep.dapp_identifier         = 1;
    rep.ran_function_identifier = 1;
    rep.sequence_id             = 1;
    rep.report_data             = payload;
    pdu.choice                  = rep;

    auto encoded = enc->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_GT(encoded->buffer.size(), payload_size);

    auto decoded = enc->decode(encoded->buffer.data(), encoded->buffer.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->message_id, static_cast<uint32_t>(42));
    const auto* back = std::get_if<libe3::DAppReport>(&decoded->choice);
    ASSERT_TRUE(back != nullptr);
    ASSERT_TRUE(back->report_data == payload);
}

} // namespace

TEST(E2smDappLinkCoexist_e3ap_encode_works) {
    e3ap_roundtrip(70);
    e3ap_roundtrip(4096);
}

TEST(E2smDappLinkCoexist_e2sm_dapp_pack_works) {
    for (const auto& c : all_cases()) {
        std::vector<uint8_t> out;
        ASSERT_EQ(c.pack_fixture(out), ASN_SUCCESS);
        ASSERT_TRUE(matches_golden(golden(c.name), out));
    }
}

TEST(E2smDappLinkCoexist_interleaved) {
    // Alternate between the two codecs, so that any state or symbol they might wrongly share shows up.
    for (int i = 0; i < 50; ++i) {
        e3ap_roundtrip(static_cast<size_t>(10 + i * 37));

        std::vector<uint8_t> out;
        auto                 ih = fx::ih1_full();
        ASSERT_EQ(ih.pack(out), ASN_SUCCESS);
        libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s back;
        ASSERT_EQ(back.unpack(out.data(), out.size()), ASN_SUCCESS);
        ASSERT_TRUE(back == ih);

        e3ap_roundtrip(static_cast<size_t>(5 + i));

        libe3_e2sm_dapp_bytes_t b{nullptr, 0};
        libe3_e2sm_dapp_event_trigger_t et;
        et.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1;
        ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(&et, &b), E3_SUCCESS);
        ASSERT_EQ(b.len, static_cast<size_t>(1));
        libe3_e2sm_dapp_bytes_free(&b);
    }
}

int main() {
    return RUN_ALL_TESTS();
}
