/**
 * @file test_e2sm_dapp_encode.cpp
 * @brief Encoding half of the E2SM-DAPP codec tests.
 *
 * Every fixture must pack to exactly the bytes flexric's own encoder produced
 * (tests/e2sm_dapp_golden.hpp), optional fields must change the encoding as
 * the grammar says, boundary values must encode, and every value outside the
 * grammar's constraints must be rejected with ASN_ERROR_ENCODE_FAIL, leaving
 * the output empty. Nothing here decodes.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include "e2sm_dapp_test_util.hpp"

#include <iostream>

using namespace e2sm_dapp_test;
namespace fx = e2sm_dapp_fixtures;
using namespace libe3::e2sm_dapp;

namespace {

/// A buffer that already holds garbage, so that "left empty" is a real check.
std::vector<uint8_t> dirty() {
    return std::vector<uint8_t>(7, 0xEE);
}

template <class T>
size_t pack_size(const T& v) {
    std::vector<uint8_t> out;
    asn_code             rc = v.pack(out);
    if (rc != ASN_SUCCESS) {
        throw std::runtime_error("pack failed where it should have succeeded");
    }
    return out.size();
}

template <class T>
void expect_rejected(const T& v, const char* what) {
    std::vector<uint8_t> out = dirty();
    asn_code             rc  = v.pack(out);
    if (rc != ASN_ERROR_ENCODE_FAIL) {
        throw std::runtime_error(std::string("not rejected: ") + what + " (rc=" + std::to_string(rc) + ")");
    }
    if (!out.empty()) {
        throw std::runtime_error(std::string("output not left empty: ") + what);
    }
}

template <class T>
void expect_ok(const T& v, const char* what) {
    std::vector<uint8_t> out = dirty();
    asn_code             rc  = v.pack(out);
    if (rc != ASN_SUCCESS || out.empty()) {
        throw std::runtime_error(std::string("rejected: ") + what + " (rc=" + std::to_string(rc) + ")");
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Golden vectors
// ---------------------------------------------------------------------------

TEST(E2smDappEncode_every_fixture_packs_to_its_golden_vector) {
    ASSERT_EQ(all_cases().size(), static_cast<size_t>(31));
    for (const auto& c : all_cases()) {
        const Golden         g = golden(c.name);
        std::vector<uint8_t> out = dirty();
        asn_code             rc  = c.pack_fixture(out);
        if (rc != ASN_SUCCESS) {
            throw std::runtime_error(c.name + ": pack failed, rc=" + std::to_string(rc));
        }
        if (out.size() != g.length) {
            throw std::runtime_error(c.name + ": length " + std::to_string(out.size()) + ", golden " +
                                     std::to_string(g.length));
        }
        if (g.inlined) {
            if (out != g.bytes) {
                throw std::runtime_error(c.name + ": got " + to_hex(out) + ", golden " + to_hex(g.bytes));
            }
        } else if (fnv1a64(out) != g.hash) {
            throw std::runtime_error(c.name + ": FNV-1a 64 differs from the golden hash");
        }
    }
}

TEST(E2smDappEncode_report_encoded_sizes) {
    // Not an assertion on its own: the sizes are the numbers the specification quotes.
    for (const auto& c : all_cases()) {
        std::vector<uint8_t> out;
        ASSERT_EQ(c.pack_fixture(out), ASN_SUCCESS);
        std::cout << "       encoded size " << c.name << " = " << out.size() << " bytes\n";
    }
}

TEST(E2smDappEncode_repeated_packs_are_identical) {
    for (const auto& c : all_cases()) {
        std::vector<uint8_t> a, b;
        ASSERT_EQ(c.pack_fixture(a), ASN_SUCCESS);
        ASSERT_EQ(c.pack_fixture(b), ASN_SUCCESS);
        ASSERT_TRUE(a == b);
    }
}

TEST(E2smDappEncode_pack_replaces_previous_content_of_out) {
    std::vector<uint8_t> out = dirty();
    ASSERT_EQ(fx::et_f1().pack(out), ASN_SUCCESS);
    ASSERT_EQ(to_hex(out), std::string("00"));
}

TEST(E2smDappEncode_ext_flags_are_ignored_on_encode) {
    // `ext` never sets the extension bit: no extension additions are defined.
    auto ih = fx::ih1_full();
    ih.ext  = true;
    ih.ric_ind_hdr_formats.ind_hdr_format1().ext = true;
    std::vector<uint8_t> out;
    ASSERT_EQ(ih.pack(out), ASN_SUCCESS);
    ASSERT_TRUE(matches_golden(golden("ih1_full"), out));

    auto fd = fx::fd_full();
    fd.ext  = true;
    fd.ran_function_name.ext = true;
    fd.ran_function_definition_report.ext = true;
    std::vector<uint8_t> out2;
    ASSERT_EQ(fd.pack(out2), ASN_SUCCESS);
    ASSERT_TRUE(matches_golden(golden("fd_full"), out2));
}

// ---------------------------------------------------------------------------
// Optional fields
// ---------------------------------------------------------------------------

TEST(E2smDappEncode_ind_hdr_format1_optionals_each_add_bytes) {
    auto         base = fx::ih1_min();
    const size_t s0   = pack_size(base);

    auto with_cu = base;
    with_cu.ric_ind_hdr_formats.ind_hdr_format1().node_cu_du_id_present = true;
    ASSERT_GT(pack_size(with_cu), s0);

    auto with_ts = base;
    with_ts.ric_ind_hdr_formats.ind_hdr_format1().timestamp_present = true;
    ASSERT_GT(pack_size(with_ts), s0);

    auto with_seq = base;
    with_seq.ric_ind_hdr_formats.ind_hdr_format1().sequence_id_present = true;
    ASSERT_GT(pack_size(with_seq), s0);

    // A *_present flag decides, not the value: absent with a non-zero value packs like absent with zero.
    auto stale = base;
    auto& sf   = stale.ric_ind_hdr_formats.ind_hdr_format1();
    sf.node_cu_du_id = 77;
    sf.timestamp     = 123456;
    sf.sequence_id   = 9;
    std::vector<uint8_t> a, b;
    ASSERT_EQ(base.pack(a), ASN_SUCCESS);
    ASSERT_EQ(stale.pack(b), ASN_SUCCESS);
    ASSERT_TRUE(a == b);
}

TEST(E2smDappEncode_zero_is_a_legal_value_when_the_flag_is_set) {
    // flexric's old C struct used 0 as "absent"; here 0 is a value, and it is sent.
    auto absent = fx::ch1_min();
    auto zero   = fx::ch1_min();
    auto& z     = zero.ric_ctrl_hdr_formats.ctrl_hdr_format1();
    z.timestamp_present   = true;
    z.timestamp           = 0;
    z.sequence_id_present = true;
    z.sequence_id         = 0;
    ASSERT_GT(pack_size(zero), pack_size(absent));

    auto oc_absent = fx::co1_min();
    auto oc_zero   = fx::co1_min();
    auto& o        = oc_zero.ric_ctrl_outcome_formats.ctrl_outcome_format1();
    o.timestamp_present   = true;
    o.sequence_id_present = true;
    ASSERT_GT(pack_size(oc_zero), pack_size(oc_absent));
}

TEST(E2smDappEncode_ctrl_outcome_present_but_empty_payload_is_sent) {
    auto absent = fx::co1_min();
    auto empty  = fx::co1_min();
    empty.ric_ctrl_outcome_formats.ctrl_outcome_format1().e3_ctrl_outcome_present = true;
    ASSERT_GT(pack_size(empty), pack_size(absent));
}

TEST(E2smDappEncode_ran_function_instance_optional) {
    auto absent = fx::fd_min();
    auto set    = fx::fd_min();
    set.ran_function_name.ran_function_instance_present = true;
    set.ran_function_name.ran_function_instance         = 3;
    ASSERT_GT(pack_size(set), pack_size(absent));
    auto zero = set;
    zero.ran_function_name.ran_function_instance = 0;
    ASSERT_GT(pack_size(zero), pack_size(absent));
}

TEST(E2smDappEncode_fd_optional_blocks) {
    const size_t min    = pack_size(fx::fd_min());
    const size_t report = pack_size(fx::fd_report());
    const size_t ctrl   = pack_size(fx::fd_ctrl());
    const size_t full   = pack_size(fx::fd_full());
    ASSERT_GT(report, min);
    ASSERT_GT(ctrl, min);
    ASSERT_GT(full, report);
    ASSERT_GT(full, ctrl);

    // The event trigger item on its own.
    auto et = fx::fd_min();
    et.ran_function_definition_event_trigger_present = true;
    ASSERT_GT(pack_size(et), min);

    // A report item with and without its subscription list.
    auto no_subs = fx::fd_report();
    no_subs.ran_function_definition_report.ric_report_style_list[1].dapp_e3_subscriptions_present = false;
    ASSERT_GT(report, pack_size(no_subs));

    // A present but empty subscription list is legal and differs from absent.
    auto empty_subs = no_subs;
    empty_subs.ran_function_definition_report.ric_report_style_list[1].dapp_e3_subscriptions_present = true;
    empty_subs.ran_function_definition_report.ric_report_style_list[1].dapp_e3_subscriptions.clear();
    ASSERT_GT(pack_size(empty_subs), pack_size(no_subs));
}

// ---------------------------------------------------------------------------
// Boundary values that must encode
// ---------------------------------------------------------------------------

TEST(E2smDappEncode_boundary_integers) {
    {
        auto v = fx::ad_with_style(INT64_MIN);
        expect_ok(v, "RIC-Style-Type = INT64_MIN");
        v = fx::ad_with_style(-1);
        expect_ok(v, "RIC-Style-Type = -1");
        v = fx::ad_with_style(INT64_MAX);
        expect_ok(v, "RIC-Style-Type = INT64_MAX");
        v = fx::ad_with_style(0);
        expect_ok(v, "RIC-Style-Type = 0");
    }
    {
        auto v = fx::ih1_bounds();
        auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        f.timestamp   = INT64_MIN;
        f.sequence_id = INT64_MIN;
        expect_ok(v, "timestamp and sequence-id = INT64_MIN");
        f.timestamp   = -1;
        f.sequence_id = -1;
        expect_ok(v, "timestamp and sequence-id = -1");
    }
    {
        auto v = fx::fd_min();
        v.ran_function_name.ran_function_instance_present = true;
        v.ran_function_name.ran_function_instance         = INT64_MAX;
        expect_ok(v, "RANfunction-Instance = INT64_MAX");
        v.ran_function_name.ran_function_instance = INT64_MIN;
        expect_ok(v, "RANfunction-Instance = INT64_MIN");
    }
    {
        // node-type is a uint8_t in the C++ type, so 0 and 255 are the whole range.
        auto v = fx::ih1_min();
        v.ric_ind_hdr_formats.ind_hdr_format1().node_type = 255;
        expect_ok(v, "node-type = 255");
    }
}

TEST(E2smDappEncode_payload_boundaries) {
    for (size_t n : {size_t(1), size_t(2), size_t(255), size_t(256), size_t(16383), size_t(16384), size_t(16385),
                     size_t(32767), size_t(32768)}) {
        auto im = fx::im1_with(fx::pattern(n, 5, 1));
        expect_ok(im, "indication message payload at a size boundary");
        auto cm = fx::cm1_with(fx::pattern(n, 5, 1));
        expect_ok(cm, "control message payload at a size boundary");
    }
    // data-size is a separate INTEGER (0..32768) and is not tied to the payload length.
    auto im = fx::im1_small();
    im.ric_ind_msg_formats.ind_msg_format1().data_size = 0;
    expect_ok(im, "data-size = 0");
    im.ric_ind_msg_formats.ind_msg_format1().data_size = 32768;
    expect_ok(im, "data-size = 32768");
}

TEST(E2smDappEncode_unbounded_ctrl_outcome_payload_sizes) {
    // e3-control-outcome has no size constraint: it must survive APER fragmentation (16384 and up).
    for (size_t n : {size_t(1), size_t(127), size_t(128), size_t(16383), size_t(16384), size_t(40000), size_t(70000)}) {
        auto p = fx::pattern(n, 3, 7);
        auto v = fx::co1_with(false, p);
        expect_ok(v, "e3-control-outcome of an unconstrained size");
    }
}

TEST(E2smDappEncode_string_boundaries) {
    auto v = fx::fd_min();
    v.ran_function_name.ran_function_short_name.from_string(std::string(1, 'A'));
    v.ran_function_name.ran_function_description.from_string(std::string(150, 'd'));
    v.ran_function_name.ran_function_e2sm_o_id.from_string(std::string(1000, '1'));
    expect_ok(v, "names at their size bounds");

    // Every character of the PrintableString alphabet.
    std::string alphabet;
    for (char c = 'A'; c <= 'Z'; ++c) alphabet.push_back(c);
    for (char c = 'a'; c <= 'z'; ++c) alphabet.push_back(c);
    for (char c = '0'; c <= '9'; ++c) alphabet.push_back(c);
    alphabet += " '()+,-./:=?";
    ASSERT_EQ(alphabet.size(), static_cast<size_t>(74));
    v.ran_function_name.ran_function_short_name.from_string(alphabet);
    expect_ok(v, "the whole PrintableString alphabet");
}

TEST(E2smDappEncode_list_boundaries) {
    // 256 items of 64 functions: the grammar's maximum (im2_256 is the golden vector of it).
    expect_ok(fx::im2_256(), "256 items, one with 64 functions");
    expect_ok(fx::im2_empty(), "empty subscription list");

    std::vector<uint64_t>              ids;
    std::vector<std::vector<uint64_t>> rfs;
    for (size_t i = 0; i < 256; ++i) {
        ids.push_back(i);
        rfs.push_back(std::vector<uint64_t>(64, 4294967295ULL));
    }
    expect_ok(fx::im2_with(fx::make_subs(ids, rfs)), "256 items of 64 functions each");
}

// ---------------------------------------------------------------------------
// Rejections: every one returns ASN_ERROR_ENCODE_FAIL and leaves `out` empty.
// ---------------------------------------------------------------------------

TEST(E2smDappEncode_rejects_empty_payload) {
    auto im = fx::im1_small();
    im.ric_ind_msg_formats.ind_msg_format1().data.clear();
    im.ric_ind_msg_formats.ind_msg_format1().data_size = 0;
    expect_rejected(im, "empty indication message data");

    auto cm = fx::cm1_small();
    cm.ric_ctrl_msg_formats.ctrl_msg_format1().data.clear();
    cm.ric_ctrl_msg_formats.ctrl_msg_format1().data_size = 0;
    expect_rejected(cm, "empty control message data");
}

TEST(E2smDappEncode_rejects_oversized_payload) {
    for (size_t n : {size_t(32769), size_t(40000), size_t(1000000)}) {
        auto im = fx::im1_with(std::vector<uint8_t>(n, 0xab));
        im.ric_ind_msg_formats.ind_msg_format1().data_size = 32768;
        expect_rejected(im, "indication message data > 32768");
        auto cm = fx::cm1_with(std::vector<uint8_t>(n, 0xab));
        cm.ric_ctrl_msg_formats.ctrl_msg_format1().data_size = 32768;
        expect_rejected(cm, "control message data > 32768");
    }
}

TEST(E2smDappEncode_rejects_data_size_above_32768) {
    for (uint16_t ds : {uint16_t(32769), uint16_t(40000), uint16_t(65535)}) {
        auto im = fx::im1_small();
        im.ric_ind_msg_formats.ind_msg_format1().data_size = ds;
        expect_rejected(im, "indication message data-size > 32768");
        auto cm = fx::cm1_small();
        cm.ric_ctrl_msg_formats.ctrl_msg_format1().data_size = ds;
        expect_rejected(cm, "control message data-size > 32768");
    }
}

TEST(E2smDappEncode_rejects_257_subscription_items) {
    expect_rejected(fx::im2_many(257, 1), "257 subscription items in an indication message");

    // And inside the RAN function definition.
    auto fd = fx::fd_report();
    std::vector<uint64_t>              ids;
    std::vector<std::vector<uint64_t>> rfs;
    for (size_t i = 0; i < 257; ++i) {
        ids.push_back(i);
        rfs.push_back({1});
    }
    fd.ran_function_definition_report.ric_report_style_list[1].dapp_e3_subscriptions = fx::make_subs(ids, rfs);
    expect_rejected(fd, "257 subscription items in a report style");
}

TEST(E2smDappEncode_rejects_0_or_65_ran_functions_in_an_item) {
    auto zero = fx::im2_with(fx::make_subs({1}, {std::vector<uint64_t>{}}));
    expect_rejected(zero, "subscription item with 0 RAN functions");

    expect_rejected(fx::im2_many(1, 65), "subscription item with 65 RAN functions");
    expect_rejected(fx::im2_many(1, 0), "first subscription item with 0 RAN functions");
    expect_ok(fx::im2_many(1, 64), "subscription item with 64 RAN functions");

    // An item in the middle of the list.
    auto mid = fx::im2_with(fx::make_subs({1, 2, 3}, {{1}, {}, {3}}));
    expect_rejected(mid, "second of three items with 0 RAN functions");

    auto fd = fx::fd_ctrl();
    fd.ran_function_definition_ctrl.ric_ctrl_style_list[0].dapp_e3_subscriptions_present = true;
    fd.ran_function_definition_ctrl.ric_ctrl_style_list[0].dapp_e3_subscriptions         = fx::make_subs({1}, {std::vector<uint64_t>{}});
    expect_rejected(fd, "control style subscription item with 0 RAN functions");
}

TEST(E2smDappEncode_rejects_ids_above_uint32_max) {
    const uint64_t big = 4294967296ULL;
    {
        auto v = fx::ih1_min();
        v.ric_ind_hdr_formats.ind_hdr_format1().ran_function_id = big;
        expect_rejected(v, "ind hdr 1 ran-function-id");
    }
    {
        auto v = fx::ih1_min();
        v.ric_ind_hdr_formats.ind_hdr_format1().dapp_id = big;
        expect_rejected(v, "ind hdr 1 dapp-id");
    }
    {
        auto v = fx::ih1_min();
        v.ric_ind_hdr_formats.ind_hdr_format1().node_nb_id = big;
        expect_rejected(v, "ind hdr 1 node-nb-id");
    }
    {
        auto v = fx::ih1_min();
        auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        f.node_cu_du_id_present = true;
        f.node_cu_du_id         = big;
        expect_rejected(v, "ind hdr 1 node-cu-du-id");
    }
    {
        auto v = fx::ih2_min();
        v.ric_ind_hdr_formats.ind_hdr_format2().node_nb_id = big;
        expect_rejected(v, "ind hdr 2 node-nb-id");
        v = fx::ih2_min();
        auto& f = v.ric_ind_hdr_formats.ind_hdr_format2();
        f.node_cu_du_id_present = true;
        f.node_cu_du_id         = UINT64_MAX;
        expect_rejected(v, "ind hdr 2 node-cu-du-id");
    }
    {
        auto v = fx::ch1_min();
        v.ric_ctrl_hdr_formats.ctrl_hdr_format1().ran_function_id = big;
        expect_rejected(v, "ctrl hdr ran-function-id");
        v = fx::ch1_min();
        v.ric_ctrl_hdr_formats.ctrl_hdr_format1().dapp_id = UINT64_MAX;
        expect_rejected(v, "ctrl hdr dapp-id");
    }
    {
        expect_rejected(fx::im2_with(fx::make_subs({big}, {{1}})), "subscription item dapp-id");
        expect_rejected(fx::im2_with(fx::make_subs({1}, {{big}})), "subscribed function id");
        expect_rejected(fx::im2_with(fx::make_subs({1}, {{1, 2, big}})), "last subscribed function id");
        expect_ok(fx::im2_with(fx::make_subs({4294967295ULL}, {{4294967295ULL}})), "ids at uint32 max");
    }
}

TEST(E2smDappEncode_rejects_wrong_number_of_styles) {
    {
        auto v = fx::fd_report();
        v.ran_function_definition_report.ric_report_style_list.push_back(
            v.ran_function_definition_report.ric_report_style_list[0]);
        ASSERT_EQ(v.ran_function_definition_report.ric_report_style_list.size(), static_cast<size_t>(3));
        expect_rejected(v, "3 report styles");
    }
    {
        auto v = fx::fd_report();
        v.ran_function_definition_report.ric_report_style_list.clear();
        expect_rejected(v, "0 report styles");
    }
    {
        auto v = fx::fd_report();
        v.ran_function_definition_report.ric_report_style_list.clear();
        v.ran_function_definition_report.ric_report_style_list.resize(1);
        // One style, but with a default (empty) name: rejected for the name, then fixed.
        expect_rejected(v, "report style with an empty name");
        v.ran_function_definition_report.ric_report_style_list[0].ric_report_style_name.from_string("x");
        expect_ok(v, "one report style");
    }
    {
        auto v = fx::fd_ctrl();
        v.ran_function_definition_ctrl.ric_ctrl_style_list.push_back(
            v.ran_function_definition_ctrl.ric_ctrl_style_list[0]);
        ASSERT_EQ(v.ran_function_definition_ctrl.ric_ctrl_style_list.size(), static_cast<size_t>(2));
        expect_rejected(v, "2 control styles");
    }
    {
        auto v = fx::fd_ctrl();
        v.ran_function_definition_ctrl.ric_ctrl_style_list.clear();
        expect_rejected(v, "0 control styles");
    }
}

TEST(E2smDappEncode_rejects_bad_printable_strings) {
    const std::string long150(150, 'a');
    const std::string long151(151, 'a');
    const std::string bad_chars[] = {"a_b", "a&b", "a@b", "a#b", "a\"b", "a*b", "a;b", "a<b", "a|b", "a~b", "a\tb",
                                     "a\nb", std::string("a\0b", 3), "caf\xc3\xa9", "\x7f", "a!b", "a%b", "a[b", "a_"};

    // RANfunction-Name: short name, OID, description.
    {
        auto v = fx::fd_min();
        v.ran_function_name.ran_function_short_name.from_string(long151);
        expect_rejected(v, "short name of 151 characters");
        v.ran_function_name.ran_function_short_name.from_string(long150);
        expect_ok(v, "short name of 150 characters");
        v.ran_function_name.ran_function_short_name.from_string("");
        expect_rejected(v, "empty short name");
        for (const auto& b : bad_chars) {
            v.ran_function_name.ran_function_short_name.from_string(b);
            expect_rejected(v, "short name with a character outside the alphabet");
        }
    }
    {
        auto v = fx::fd_min();
        v.ran_function_name.ran_function_e2sm_o_id.from_string(std::string(1001, '1'));
        expect_rejected(v, "OID of 1001 characters");
        v.ran_function_name.ran_function_e2sm_o_id.from_string("");
        expect_rejected(v, "empty OID");
        v.ran_function_name.ran_function_e2sm_o_id.from_string("1.3.6_1");
        expect_rejected(v, "OID with a character outside the alphabet");
    }
    {
        auto v = fx::fd_min();
        v.ran_function_name.ran_function_description.from_string(long151);
        expect_rejected(v, "description of 151 characters");
        v.ran_function_name.ran_function_description.from_string("");
        expect_rejected(v, "empty description");
        v.ran_function_name.ran_function_description.from_string("DAPP & Co");
        expect_rejected(v, "description with a character outside the alphabet");
    }
    // Report style name and control style name.
    {
        auto v = fx::fd_report();
        auto& n = v.ran_function_definition_report.ric_report_style_list[0].ric_report_style_name;
        n.from_string(long151);
        expect_rejected(v, "report style name of 151 characters");
        n.from_string("");
        expect_rejected(v, "empty report style name");
        n.from_string("E3_Report");
        expect_rejected(v, "report style name with a character outside the alphabet");
        n.from_string(long150);
        expect_ok(v, "report style name of 150 characters");
    }
    {
        auto v = fx::fd_ctrl();
        auto& n = v.ran_function_definition_ctrl.ric_ctrl_style_list[0].ric_ctrl_style_name;
        n.from_string(long151);
        expect_rejected(v, "control style name of 151 characters");
        n.from_string("");
        expect_rejected(v, "empty control style name");
        n.from_string("dApp#1");
        expect_rejected(v, "control style name with a character outside the alphabet");
        n.from_string(long150);
        expect_ok(v, "control style name of 150 characters");
    }
}

TEST(E2smDappEncode_rejects_unset_choice) {
    {
        e2sm_dapp_ind_hdr_s v;
        // Default constructed: the choice is nulltype.
        ASSERT_EQ(static_cast<int>(v.ric_ind_hdr_formats.type().value),
                  static_cast<int>(e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types::nulltype));
        expect_rejected(v, "indication header with no format set");
        v.ric_ind_hdr_formats.set(e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types::nulltype);
        expect_rejected(v, "indication header with the choice explicitly reset");
    }
    {
        e2sm_dapp_ind_msg_s v;
        ASSERT_EQ(static_cast<int>(v.ric_ind_msg_formats.type().value),
                  static_cast<int>(e2sm_dapp_ind_msg_s::ric_ind_msg_formats_c_::types::nulltype));
        expect_rejected(v, "indication message with no format set");
        v.ric_ind_msg_formats.set_ind_msg_format1();
        expect_rejected(v, "indication message format 1 left empty");
        v.ric_ind_msg_formats.set(e2sm_dapp_ind_msg_s::ric_ind_msg_formats_c_::types::nulltype);
        expect_rejected(v, "indication message with the choice reset");
    }
}

TEST(E2smDappEncode_default_constructed_messages) {
    // The single-format messages cannot have an unset choice, so a default value is a valid message
    // for the ones without mandatory content, and rejected for those that need it.
    expect_ok(e2sm_dapp_event_trigger_s{}, "default event trigger");
    expect_ok(e2sm_dapp_action_definition_s{}, "default action definition");
    expect_ok(e2sm_dapp_ctrl_hdr_s{}, "default control header");
    expect_ok(e2sm_dapp_ctrl_outcome_s{}, "default control outcome");
    expect_rejected(e2sm_dapp_ctrl_msg_s{}, "default control message (no payload)");
    expect_rejected(e2sm_dapp_ran_function_definition_s{}, "default RAN function definition (empty names)");
}

TEST(E2smDappEncode_rejection_leaves_no_residue_for_the_next_pack) {
    std::vector<uint8_t> out;
    auto                 bad = fx::im1_with(std::vector<uint8_t>(40000, 1));
    ASSERT_EQ(bad.pack(out), ASN_ERROR_ENCODE_FAIL);
    ASSERT_TRUE(out.empty());
    ASSERT_EQ(fx::im1_small().pack(out), ASN_SUCCESS);
    ASSERT_TRUE(matches_golden(golden("im1_small"), out));
}

int main() {
    return RUN_ALL_TESTS();
}
