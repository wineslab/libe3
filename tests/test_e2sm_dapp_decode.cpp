/**
 * @file test_e2sm_dapp_decode.cpp
 * @brief Decoding half of the E2SM-DAPP codec tests.
 *
 * The golden bytes (what flexric's encoder produces) must unpack to a struct
 * equal to the fixture; truncated, empty, null and corrupt input must be
 * rejected (or, for the deterministic fuzz, at least never crash); trailing
 * bytes after a valid PDU are accepted; a PDU with the extension bit set
 * decodes. Nothing here relies on the encoder except for the four vectors
 * that are too large to inline, whose bytes are produced by pack() after the
 * length and FNV-1a 64 hash of the result are checked against the golden ones.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include "e2sm_dapp_test_util.hpp"

#include <algorithm>
#include <iostream>

using namespace e2sm_dapp_test;
namespace fx = e2sm_dapp_fixtures;
using namespace libe3::e2sm_dapp;

namespace {

/// The bytes of a golden vector: decoded from the hex, or (for the hashed ones) produced by
/// pack() and only used after they match the golden length and hash.
std::vector<uint8_t> golden_bytes(const Case& c) {
    const Golden g = golden(c.name);
    if (g.inlined) {
        return g.bytes;
    }
    std::vector<uint8_t> out;
    if (c.pack_fixture(out) != ASN_SUCCESS || !matches_golden(g, out)) {
        throw std::runtime_error(c.name + ": cannot reproduce the golden bytes of a hashed vector");
    }
    return out;
}

std::string type_prefix(const std::string& name) {
    return name.substr(0, name.find_first_of("_0123456789"));
}

} // namespace

// ---------------------------------------------------------------------------
// Golden vectors
// ---------------------------------------------------------------------------

TEST(E2smDappDecode_golden_bytes_unpack_to_the_fixture) {
    for (const auto& c : all_cases()) {
        const auto bytes = golden_bytes(c);
        bool       equal = false;
        asn_code   rc    = c.unpack(bytes.data(), bytes.size(), &equal, nullptr);
        if (rc != ASN_SUCCESS) {
            throw std::runtime_error(c.name + ": unpack failed, rc=" + std::to_string(rc));
        }
        if (!equal) {
            throw std::runtime_error(c.name + ": unpacked struct differs from the fixture");
        }
    }
}

TEST(E2smDappDecode_spot_check_fields) {
    {
        e2sm_dapp_ind_hdr_s v;
        const auto          b = golden("ih1_full").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_TRUE(v.ric_ind_hdr_formats.type().value == e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types::ind_hdr_format1);
        const auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        ASSERT_EQ(f.ran_function_id, static_cast<uint64_t>(255));
        ASSERT_EQ(f.dapp_id, static_cast<uint64_t>(7));
        ASSERT_EQ(static_cast<int>(f.node_type), 2);
        ASSERT_EQ(static_cast<int>(f.node_plmn_id[0]), 0x00);
        ASSERT_EQ(static_cast<int>(f.node_plmn_id[1]), 0xf1);
        ASSERT_EQ(static_cast<int>(f.node_plmn_id[2]), 0x10);
        ASSERT_EQ(f.node_nb_id, static_cast<uint64_t>(1234));
        ASSERT_TRUE(f.node_cu_du_id_present);
        ASSERT_EQ(f.node_cu_du_id, static_cast<uint64_t>(5));
        ASSERT_TRUE(f.timestamp_present);
        ASSERT_EQ(f.timestamp, static_cast<int64_t>(1700000000123456789LL));
        ASSERT_TRUE(f.sequence_id_present);
        ASSERT_EQ(f.sequence_id, static_cast<int64_t>(42));
        ASSERT_FALSE(v.ext);
    }
    {
        e2sm_dapp_ind_hdr_s v;
        const auto          b = golden("ih1_min").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        ASSERT_FALSE(f.node_cu_du_id_present);
        ASSERT_FALSE(f.timestamp_present);
        ASSERT_FALSE(f.sequence_id_present);
        ASSERT_EQ(f.timestamp, static_cast<int64_t>(0));
    }
    {
        e2sm_dapp_ind_hdr_s v;
        const auto          b = golden("ih1_bounds").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        ASSERT_EQ(f.dapp_id, static_cast<uint64_t>(4294967295ULL));
        ASSERT_EQ(static_cast<int>(f.node_type), 255);
        ASSERT_EQ(f.node_nb_id, static_cast<uint64_t>(4294967295ULL));
        ASSERT_EQ(f.node_cu_du_id, static_cast<uint64_t>(4294967295ULL));
        ASSERT_EQ(f.timestamp, static_cast<int64_t>(INT64_MAX));
        ASSERT_EQ(f.sequence_id, static_cast<int64_t>(INT64_MAX));
    }
    {
        e2sm_dapp_ind_hdr_s v;
        const auto          b = golden("ih2_full").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_TRUE(v.ric_ind_hdr_formats.type().value == e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types::ind_hdr_format2);
        ASSERT_EQ(v.ric_ind_hdr_formats.ind_hdr_format2().node_nb_id, static_cast<uint64_t>(1234));
    }
    {
        e2sm_dapp_action_definition_s v;
        const auto                   b = golden("ad_style_max").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_EQ(v.ric_style_type, static_cast<int64_t>(4294967295LL));
    }
    {
        e2sm_dapp_ind_msg_s v;
        const auto          b = golden("im1_small").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& f = v.ric_ind_msg_formats.ind_msg_format1();
        ASSERT_EQ(f.data_size, static_cast<uint16_t>(4));
        ASSERT_EQ(f.data.size(), static_cast<size_t>(4));
        ASSERT_EQ(to_hex(f.data), std::string("deadbeef"));
    }
    {
        e2sm_dapp_ind_msg_s v;
        const auto          b = golden("im2_multi").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& l = v.ric_ind_msg_formats.ind_msg_format2().dapp_e3_subscriptions;
        ASSERT_EQ(l.size(), static_cast<size_t>(2));
        ASSERT_EQ(l[0].dapp_id, static_cast<uint64_t>(7));
        ASSERT_EQ(l[0].subscribed_e3_ran_functions.size(), static_cast<size_t>(3));
        ASSERT_EQ(l[1].dapp_id, static_cast<uint64_t>(4294967295ULL));
        ASSERT_EQ(l[1].subscribed_e3_ran_functions[1], static_cast<uint64_t>(4294967295ULL));
    }
    {
        e2sm_dapp_ind_msg_s v;
        const auto          b = golden("im2_empty").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_EQ(v.ric_ind_msg_formats.ind_msg_format2().dapp_e3_subscriptions.size(), static_cast<size_t>(0));
    }
    {
        e2sm_dapp_ctrl_hdr_s v;
        const auto           b = golden("ch1_bounds").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& f = v.ric_ctrl_hdr_formats.ctrl_hdr_format1();
        ASSERT_EQ(f.ran_function_id, static_cast<uint64_t>(4294967295ULL));
        ASSERT_EQ(f.dapp_id, static_cast<uint64_t>(0));
        ASSERT_TRUE(f.timestamp_present);
        ASSERT_TRUE(f.sequence_id_present);
    }
    {
        e2sm_dapp_ctrl_msg_s v;
        const auto           b = golden("cm1_small").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_EQ(to_hex(v.ric_ctrl_msg_formats.ctrl_msg_format1().data), std::string("010203"));
    }
    {
        e2sm_dapp_ctrl_outcome_s v;
        const auto               b = golden("co1_full").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        const auto& f = v.ric_ctrl_outcome_formats.ctrl_outcome_format1();
        ASSERT_TRUE(f.timestamp_present && f.sequence_id_present && f.e3_ctrl_outcome_present);
        ASSERT_EQ(to_hex(f.e3_ctrl_outcome), std::string("102030405060708090a0"));
        e2sm_dapp_ctrl_outcome_s m;
        const auto               bm = golden("co1_min").bytes;
        ASSERT_EQ(m.unpack(bm.data(), bm.size()), ASN_SUCCESS);
        const auto& g = m.ric_ctrl_outcome_formats.ctrl_outcome_format1();
        ASSERT_FALSE(g.timestamp_present || g.sequence_id_present || g.e3_ctrl_outcome_present);
    }
    {
        e2sm_dapp_ran_function_definition_s v;
        const auto                          b = golden("fd_full").bytes;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_STREQ(v.ran_function_name.ran_function_short_name.to_string(), "E2SM-DAPP");
        ASSERT_STREQ(v.ran_function_name.ran_function_e2sm_o_id.to_string(), "1.3.6.1.4.1.53148.1.1.255.3");
        ASSERT_STREQ(v.ran_function_name.ran_function_description.to_string(), "DAPP Service Model for E2/E3 bridge");
        ASSERT_FALSE(v.ran_function_name.ran_function_instance_present);
        ASSERT_TRUE(v.ran_function_definition_event_trigger_present);
        ASSERT_TRUE(v.ran_function_definition_report_present);
        ASSERT_TRUE(v.ran_function_definition_ctrl_present);
        const auto& rs = v.ran_function_definition_report.ric_report_style_list;
        ASSERT_EQ(rs.size(), static_cast<size_t>(2));
        ASSERT_STREQ(rs[0].ric_report_style_name.to_string(), "E3 Data Report");
        ASSERT_FALSE(rs[0].dapp_e3_subscriptions_present);
        ASSERT_TRUE(rs[1].dapp_e3_subscriptions_present);
        ASSERT_EQ(rs[1].dapp_e3_subscriptions.size(), static_cast<size_t>(2));
        const auto& cs = v.ran_function_definition_ctrl.ric_ctrl_style_list;
        ASSERT_EQ(cs.size(), static_cast<size_t>(1));
        ASSERT_STREQ(cs[0].ric_ctrl_style_name.to_string(), "dApp Control");
        ASSERT_TRUE(cs[0].dapp_e3_subscriptions_present);
    }
}

TEST(E2smDappDecode_ext_is_false_after_decode) {
    // Extension additions are not parsed, so `ext` is always false on a decoded struct.
    e2sm_dapp_ind_hdr_s v;
    v.ext = true;
    v.ric_ind_hdr_formats.set_ind_hdr_format1().ext = true;
    const auto b = golden("ih1_full").bytes;
    ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
    ASSERT_FALSE(v.ext);
    ASSERT_FALSE(v.ric_ind_hdr_formats.ind_hdr_format1().ext);
}

TEST(E2smDappDecode_unpack_replaces_the_whole_value) {
    // Decoding over a struct that holds the other alternative leaves nothing of the old one.
    e2sm_dapp_ind_hdr_s v = fx::ih2_full();
    const auto          b = golden("ih1_min").bytes;
    ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
    ASSERT_TRUE(v == fx::ih1_min());

    e2sm_dapp_ran_function_definition_s fd = fx::fd_full();
    const auto                          bm = golden("fd_min").bytes;
    ASSERT_EQ(fd.unpack(bm.data(), bm.size()), ASN_SUCCESS);
    ASSERT_TRUE(fd == fx::fd_min());
    ASSERT_FALSE(fd.ran_function_definition_report_present);
}

TEST(E2smDappDecode_failed_unpack_leaves_the_value_untouched) {
    e2sm_dapp_ind_hdr_s v = fx::ih1_full();
    const uint8_t       junk[] = {0xff, 0xff, 0xff};
    ASSERT_EQ(v.unpack(junk, sizeof(junk)), ASN_ERROR_DECODE_FAIL);
    ASSERT_TRUE(v == fx::ih1_full());
    ASSERT_EQ(v.unpack(nullptr, 0), ASN_ERROR_DECODE_FAIL);
    ASSERT_TRUE(v == fx::ih1_full());
}

// ---------------------------------------------------------------------------
// Truncation
// ---------------------------------------------------------------------------

TEST(E2smDappDecode_every_proper_prefix_of_every_golden_vector_is_rejected) {
    // Investigated result, pinned per vector: for all 31 golden vectors there is NO proper prefix
    // (length 0 .. size-1) that decodes. Each one is ASN_ERROR_DECODE_FAIL. In particular no
    // prefix of a golden vector is itself a valid shorter PDU.
    size_t checked = 0;
    for (const auto& c : all_cases()) {
        const auto bytes = golden_bytes(c);
        std::vector<size_t> accepted;
        for (size_t n = 0; n < bytes.size(); ++n) {
            // Hand each prefix its own exact-size heap copy, so that a read past the end is caught
            // by AddressSanitizer rather than served from the rest of the full vector.
            std::vector<uint8_t> prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
            asn_code             rc = c.unpack(prefix.empty() ? nullptr : prefix.data(), n, nullptr, nullptr);
            if (rc == ASN_SUCCESS) {
                accepted.push_back(n);
            } else if (rc != ASN_ERROR_DECODE_FAIL) {
                throw std::runtime_error(c.name + ": unexpected rc for a truncated input");
            }
            ++checked;
        }
        if (!accepted.empty()) {
            throw std::runtime_error(c.name + ": a proper prefix of length " + std::to_string(accepted.front()) +
                                     " decodes");
        }
    }
    std::cout << "       " << checked << " truncated inputs, all rejected\n";
}

TEST(E2smDappDecode_empty_and_null_buffers) {
    uint8_t one = 0;
    for (const auto& c : all_cases()) {
        ASSERT_EQ(c.unpack(nullptr, 0, nullptr, nullptr), ASN_ERROR_DECODE_FAIL);
        ASSERT_EQ(c.unpack(nullptr, 1, nullptr, nullptr), ASN_ERROR_DECODE_FAIL);
        ASSERT_EQ(c.unpack(nullptr, 100000, nullptr, nullptr), ASN_ERROR_DECODE_FAIL);
        ASSERT_EQ(c.unpack(&one, 0, nullptr, nullptr), ASN_ERROR_DECODE_FAIL);
    }
}

// ---------------------------------------------------------------------------
// Trailing bytes
// ---------------------------------------------------------------------------

TEST(E2smDappDecode_trailing_bytes_after_a_valid_pdu_are_accepted) {
    for (const auto& c : all_cases()) {
        for (size_t extra : {size_t(1), size_t(2), size_t(7), size_t(64)}) {
            auto bytes = golden_bytes(c);
            for (size_t i = 0; i < extra; ++i) {
                bytes.push_back(static_cast<uint8_t>(0xA5 ^ i));
            }
            bool     equal = false;
            asn_code rc    = c.unpack(bytes.data(), bytes.size(), &equal, nullptr);
            if (rc != ASN_SUCCESS || !equal) {
                throw std::runtime_error(c.name + ": trailing bytes not accepted");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Extension bit
// ---------------------------------------------------------------------------

TEST(E2smDappDecode_pdu_with_the_extension_bit_set_decodes) {
    // E2SM-DAPP-EventTrigger with the SEQUENCE extension bit set and one (unknown) extension addition:
    //   1 | choice-ext 0 | format1-ext 0 | small-length flag 0, n-1 = 000000 | bitmap 1 | pad
    //   = 80 20, then the addition as an open type: length 01, content 00.
    {
        const auto b = from_hex("80200100");
        e2sm_dapp_event_trigger_s v;
        v.ext = true;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_FALSE(v.ext);
        ASSERT_TRUE(v == fx::et_f1());
    }
    // A longer addition is skipped as a whole.
    {
        const auto b = from_hex("802003aabbcc");
        e2sm_dapp_event_trigger_s v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_TRUE(v == fx::et_f1());
    }
    // Pinned asn1c behavior: extension additions are skipped without being checked. An addition that is
    // shorter than its declared length, or missing altogether, still decodes; only a PDU that stops
    // inside the extension bitmap itself ("80": the ext bit and nothing after) is a truncated PDU.
    {
        e2sm_dapp_event_trigger_s v;
        for (const char* hex : {"802003aabb", "802003", "8020"}) {
            const auto b = from_hex(hex);
            ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
            ASSERT_TRUE(v == fx::et_f1());
        }
        const auto b2 = from_hex("80");
        ASSERT_EQ(v.unpack(b2.data(), b2.size()), ASN_ERROR_DECODE_FAIL);
    }
    // E2SM-DAPP-ActionDefinition (style 1): ext bit set, style 01 01, choice/format1 ext 0 0, addition bitmap.
    {
        const auto b = from_hex("800101400100");
        e2sm_dapp_action_definition_s v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_EQ(v.ric_style_type, static_cast<int64_t>(1));
        ASSERT_TRUE(v == fx::ad_style1());
    }
}

TEST(E2smDappDecode_unknown_choice_alternative_is_rejected) {
    // The CHOICE extension bit set: an alternative this version does not know. Not a valid format.
    {
        const auto b = from_hex("40000100");
        e2sm_dapp_event_trigger_s v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
    }
    {
        const auto b = from_hex("000101" "800100");
        e2sm_dapp_action_definition_s v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
    }
}

TEST(E2smDappDecode_values_outside_the_root_constraints_are_rejected) {
    // ih2_min = 20 01 13 00 14 00 63 (format 2, node-type 1, PLMN 13 00 14, node-nb-id 99).
    // The same PDU with node-type's own extension bit set (first byte 21) carries node-type as an
    // unconstrained INTEGER: 02 01 2c is 300, which the uint8_t of the C++ struct cannot hold.
    {
        const auto            b = from_hex("2102012c13001400" "63");
        e2sm_dapp_ind_hdr_s   v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
    }
    // The same extension encoding of an in-range value (5) is accepted: decoding does not insist on
    // the canonical form.
    {
        const auto            b = from_hex("21010513001400" "63");
        e2sm_dapp_ind_hdr_s   v;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_EQ(static_cast<int>(v.ric_ind_hdr_formats.ind_hdr_format2().node_type), 5);
    }
    // PrintableString: a character outside the alphabet is rejected, one inside is kept.
    {
        auto  b   = golden("fd_min").bytes;
        const std::string name = "E2SM-DAPP";
        auto  it  = std::search(b.begin(), b.end(), name.begin(), name.end());
        ASSERT_TRUE(it != b.end());
        const auto dash = it + 4;
        ASSERT_EQ(static_cast<int>(*dash), static_cast<int>('-'));
        e2sm_dapp_ran_function_definition_s v;

        *dash = '_';
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
        *dash = 0x01;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
        *dash = 0xe9;
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_ERROR_DECODE_FAIL);
        *dash = '+';
        ASSERT_EQ(v.unpack(b.data(), b.size()), ASN_SUCCESS);
        ASSERT_STREQ(v.ran_function_name.ran_function_short_name.to_string(), "E2SM+DAPP");
    }
}

TEST(E2smDappDecode_single_bit_flips_never_decode_to_something_unpackable) {
    // Every single-bit flip of every small golden vector either fails to decode, or decodes to a value
    // that packs again (decode never returns a value the encoder would refuse).
    size_t flips = 0, decoded = 0;
    for (const auto& c : all_cases()) {
        auto bytes = golden_bytes(c);
        if (bytes.size() > 200) continue;
        for (size_t i = 0; i < bytes.size(); ++i) {
            for (int bit = 0; bit < 8; ++bit) {
                auto m = bytes;
                m[i]   = static_cast<uint8_t>(m[i] ^ (1u << bit));
                std::vector<uint8_t> repacked;
                asn_code             rc = c.unpack(m.data(), m.size(), nullptr, &repacked);
                ++flips;
                if (rc == ASN_SUCCESS) {
                    ++decoded;
                    if (repacked.empty()) {
                        throw std::runtime_error(c.name + ": decoded a value that cannot be packed again");
                    }
                }
            }
        }
    }
    std::cout << "       " << flips << " bit flips, " << decoded << " still decode\n";
}

// ---------------------------------------------------------------------------
// Deterministic fuzz: nothing may crash, and whatever decodes must pack again.
// ---------------------------------------------------------------------------

TEST(E2smDappDecode_fuzz_random_bytes_per_type) {
    const char* const prefixes[] = {"et", "ad", "ih", "im", "ch", "cm", "co", "fd"};
    size_t            decoded = 0, total = 0;
    for (const char* prefix : prefixes) {
        // Any case of the type will do: unpack() ignores the fixture except for the comparison.
        const Case* rep = nullptr;
        for (const auto& c : all_cases()) {
            if (type_prefix(c.name) == prefix) { rep = &c; break; }
        }
        ASSERT_TRUE(rep != nullptr);
        Rng rng(0x1234567ULL + static_cast<uint64_t>(prefix[0]) * 131 + static_cast<uint64_t>(prefix[1]));
        for (int it = 0; it < 4000; ++it) {
            const size_t         len = 1 + static_cast<size_t>(rng.below(it % 10 == 0 ? 600 : 48));
            std::vector<uint8_t> buf(len);
            for (auto& b : buf) b = static_cast<uint8_t>(rng.next() >> 24);
            if (it % 4 == 1) {
                // Bias towards the first byte being a plausible preamble.
                buf[0] = static_cast<uint8_t>(buf[0] & 0x3f);
            }
            std::vector<uint8_t> repacked;
            asn_code             rc = rep->unpack(buf.data(), buf.size(), nullptr, &repacked);
            ++total;
            if (rc == ASN_SUCCESS) {
                ++decoded;
                if (repacked.empty()) {
                    throw std::runtime_error(std::string(prefix) + ": random bytes decoded to a value that does not pack");
                }
            } else if (rc != ASN_ERROR_DECODE_FAIL) {
                throw std::runtime_error("unexpected return code");
            }
        }
    }
    std::cout << "       " << total << " random inputs, " << decoded << " decoded\n";
}

TEST(E2smDappDecode_fuzz_corrupted_golden_vectors) {
    size_t decoded = 0, total = 0;
    Rng    rng(0xC0FFEEULL);
    for (const auto& c : all_cases()) {
        const auto base = golden_bytes(c);
        const int  iterations = base.size() > 4096 ? 150 : 1500;
        for (int it = 0; it < iterations; ++it) {
            auto m = base;
            switch (rng.below(6)) {
                case 0: // flip one bit
                    m[rng.below(m.size())] ^= static_cast<uint8_t>(1u << rng.below(8));
                    break;
                case 1: // overwrite a byte
                    m[rng.below(m.size())] = static_cast<uint8_t>(rng.next() >> 16);
                    break;
                case 2: // flip a few bits in the first 16 bytes (the length determinants and preambles)
                    for (int k = 0; k < 3; ++k) {
                        m[rng.below(std::min<size_t>(m.size(), 16))] ^= static_cast<uint8_t>(1u << rng.below(8));
                    }
                    break;
                case 3: // delete a byte
                    m.erase(m.begin() + static_cast<std::ptrdiff_t>(rng.below(m.size())));
                    break;
                case 4: // insert a byte
                    m.insert(m.begin() + static_cast<std::ptrdiff_t>(rng.below(m.size() + 1)),
                             static_cast<uint8_t>(rng.next() >> 20));
                    break;
                default: // truncate, then append noise
                    m.resize(rng.below(m.size()) + 1);
                    for (size_t k = rng.below(8); k > 0; --k) m.push_back(static_cast<uint8_t>(rng.next() >> 24));
                    break;
            }
            if (m.empty()) continue;
            std::vector<uint8_t> repacked;
            asn_code             rc = c.unpack(m.data(), m.size(), nullptr, &repacked);
            ++total;
            if (rc == ASN_SUCCESS) {
                ++decoded;
                if (repacked.empty()) {
                    throw std::runtime_error(c.name + ": corrupted input decoded to a value that does not pack");
                }
            } else if (rc != ASN_ERROR_DECODE_FAIL) {
                throw std::runtime_error("unexpected return code");
            }
        }
    }
    std::cout << "       " << total << " corrupted inputs, " << decoded << " decoded\n";
}

TEST(E2smDappDecode_all_zero_and_all_ones_inputs) {
    for (const auto& c : all_cases()) {
        for (size_t n : {size_t(1), size_t(2), size_t(5), size_t(40), size_t(300)}) {
            std::vector<uint8_t> zeros(n, 0x00), ones(n, 0xff);
            std::vector<uint8_t> repacked;
            for (const auto* buf : {&zeros, &ones}) {
                repacked.clear();
                asn_code rc = c.unpack(buf->data(), buf->size(), nullptr, &repacked);
                if (rc != ASN_SUCCESS && rc != ASN_ERROR_DECODE_FAIL) {
                    throw std::runtime_error("unexpected return code");
                }
                if (rc == ASN_SUCCESS && repacked.empty()) {
                    throw std::runtime_error(c.name + ": uniform bytes decoded to a value that does not pack");
                }
            }
        }
    }
}

int main() {
    return RUN_ALL_TESTS();
}
