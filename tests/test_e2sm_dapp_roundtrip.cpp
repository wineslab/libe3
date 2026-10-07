/**
 * @file test_e2sm_dapp_roundtrip.cpp
 * @brief pack -> unpack round trips of the E2SM-DAPP codec.
 *
 * pack(x) then unpack must give a struct equal to x, and packing that again
 * must give identical bytes. Besides the golden fixtures this covers the
 * largest messages the grammar allows (no golden vector exists for those) and
 * a few hundred deterministic pseudo-random messages of every type.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include "e2sm_dapp_test_util.hpp"

using namespace e2sm_dapp_test;
namespace fx = e2sm_dapp_fixtures;
using namespace libe3::e2sm_dapp;

namespace {

/// pack, unpack into a fresh value, compare, pack again, compare bytes. Returns the encoded size.
template <class T>
size_t roundtrip(const T& v, const char* what) {
    std::vector<uint8_t> a;
    if (v.pack(a) != ASN_SUCCESS) {
        throw std::runtime_error(std::string(what) + ": pack failed");
    }
    T back;
    if (back.unpack(a.data(), a.size()) != ASN_SUCCESS) {
        throw std::runtime_error(std::string(what) + ": unpack failed");
    }
    if (!(back == v)) {
        throw std::runtime_error(std::string(what) + ": unpacked value differs");
    }
    std::vector<uint8_t> b;
    if (back.pack(b) != ASN_SUCCESS || a != b) {
        throw std::runtime_error(std::string(what) + ": re-pack differs");
    }
    return a.size();
}

} // namespace

TEST(E2smDappRoundtrip_every_golden_fixture) {
    for (const auto& c : all_cases()) {
        std::vector<uint8_t> a;
        ASSERT_EQ(c.pack_fixture(a), ASN_SUCCESS);
        bool                 equal = false;
        std::vector<uint8_t> b;
        ASSERT_EQ(c.unpack(a.data(), a.size(), &equal, &b), ASN_SUCCESS);
        if (!equal) {
            throw std::runtime_error(c.name + ": unpacked value differs from the fixture");
        }
        if (a != b) {
            throw std::runtime_error(c.name + ": re-pack differs");
        }
    }
}

TEST(E2smDappRoundtrip_full_size_control_message) {
    // The grammar's maximum, 32768 bytes. flexric's 16 KiB buffer could not carry this; no golden vector exists.
    auto v = fx::cm1_with(fx::pattern(32768, 11, 9));
    ASSERT_EQ(v.ric_ctrl_msg_formats.ctrl_msg_format1().data_size, static_cast<uint16_t>(32768));
    const size_t n = roundtrip(v, "control message of 32768 bytes");
    ASSERT_GT(n, static_cast<size_t>(32768));
    ASSERT_LT(n, static_cast<size_t>(32768 + 16));
    std::cout << "       control message with 32768 bytes of data encodes to " << n << " bytes\n";

    // And the sizes around the fragmentation and length-determinant boundaries.
    for (size_t len : {size_t(1), size_t(127), size_t(128), size_t(255), size_t(256), size_t(16383), size_t(16384),
                       size_t(16385), size_t(32767)}) {
        roundtrip(fx::cm1_with(fx::pattern(len, 13, 5)), "control message at a size boundary");
        roundtrip(fx::im1_with(fx::pattern(len, 17, 3)), "indication message at a size boundary");
    }
}

TEST(E2smDappRoundtrip_full_size_indication_message_and_lists) {
    roundtrip(fx::im1_max(), "indication message of 32768 bytes");
    roundtrip(fx::im2_256(), "256 subscription items");

    // 256 items with 64 functions each, ids at both ends of the range.
    std::vector<uint64_t>              ids;
    std::vector<std::vector<uint64_t>> rfs;
    for (size_t i = 0; i < 256; ++i) {
        ids.push_back(i % 2 == 0 ? i : 4294967295ULL - i);
        std::vector<uint64_t> f;
        for (size_t j = 0; j < 64; ++j) {
            f.push_back((i * 64 + j) % 3 == 0 ? 4294967295ULL - j : i * 100 + j);
        }
        rfs.push_back(f);
    }
    const size_t n = roundtrip(fx::im2_with(fx::make_subs(ids, rfs)), "256 x 64 subscription list");
    std::cout << "       256 items x 64 functions encode to " << n << " bytes\n";
}

TEST(E2smDappRoundtrip_unbounded_control_outcome) {
    for (size_t len : {size_t(0), size_t(1), size_t(200), size_t(16383), size_t(16384), size_t(32768), size_t(65535),
                       size_t(100000)}) {
        e2sm_dapp_ctrl_outcome_s v;
        auto&                    f = v.ric_ctrl_outcome_formats.ctrl_outcome_format1();
        f.e3_ctrl_outcome_present  = true;
        const auto p               = fx::pattern(len, 7, 1);
        f.e3_ctrl_outcome.from_bytes(p.data(), p.size());
        roundtrip(v, "control outcome with a payload of unconstrained size");
    }
}

TEST(E2smDappRoundtrip_extreme_values) {
    {
        auto v = fx::ih1_bounds();
        auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
        f.timestamp   = INT64_MIN;
        f.sequence_id = -1;
        roundtrip(v, "negative timestamp and sequence-id");
        f.timestamp   = 0;
        f.sequence_id = 0;
        roundtrip(v, "zero timestamp and sequence-id, flags set");
        ASSERT_TRUE(f.timestamp_present && f.sequence_id_present);
    }
    for (int64_t style : {INT64_MIN, int64_t(-129), int64_t(-128), int64_t(-1), int64_t(0), int64_t(127),
                          int64_t(128), int64_t(255), int64_t(256), int64_t(32767), int64_t(32768),
                          int64_t(2147483647), int64_t(2147483648LL), int64_t(4294967295LL), int64_t(4294967296LL),
                          INT64_MAX}) {
        roundtrip(fx::ad_with_style(style), "RIC-Style-Type");
    }
    {
        auto v = fx::fd_full();
        v.ran_function_name.ran_function_instance_present = true;
        v.ran_function_name.ran_function_instance         = -7;
        auto& rs = v.ran_function_definition_report.ric_report_style_list;
        rs[0].ric_report_style_type   = INT64_MIN;
        rs[0].ric_ind_hdr_format_type = INT64_MAX;
        rs[1].ric_ind_msg_format_type = -1;
        auto& cs = v.ran_function_definition_ctrl.ric_ctrl_style_list;
        cs[0].ric_ctrl_style_type          = -2;
        cs[0].ric_ctrl_outcome_format_type = 4294967296LL;
        roundtrip(v, "RAN function definition with extreme format types");
    }
    {
        auto v = fx::fd_min();
        v.ran_function_name.ran_function_short_name.from_string(std::string(150, 'S'));
        v.ran_function_name.ran_function_description.from_string(std::string(150, 'D'));
        v.ran_function_name.ran_function_e2sm_o_id.from_string(std::string(1000, '9'));
        roundtrip(v, "names at their maximum length");
        v.ran_function_name.ran_function_short_name.from_string("a");
        v.ran_function_name.ran_function_description.from_string("b");
        v.ran_function_name.ran_function_e2sm_o_id.from_string("1");
        roundtrip(v, "names at their minimum length");
    }
}

TEST(E2smDappRoundtrip_absent_optionals_compare_equal_whatever_the_stale_value) {
    // An absent OPTIONAL has no value: two structs differing only in the unused field are equal, and a
    // round trip, which drops the stale value, must still compare equal to the original.
    auto v = fx::ih1_min();
    auto& f = v.ric_ind_hdr_formats.ind_hdr_format1();
    f.node_cu_du_id = 99;
    f.timestamp     = 5;
    f.sequence_id   = 6;
    roundtrip(v, "absent optionals carrying stale values");
    ASSERT_TRUE(v == fx::ih1_min());

    auto fd = fx::fd_report();
    fd.ran_function_name.ran_function_instance = 12;
    fd.ran_function_definition_report.ric_report_style_list[0].dapp_e3_subscriptions =
        fx::make_subs({1}, {{2}});
    roundtrip(fd, "absent instance and subscription list carrying stale values");
    ASSERT_TRUE(fd == fx::fd_report());

    // But the flags themselves are part of the value.
    auto g = fx::ch1_min();
    g.ric_ctrl_hdr_formats.ctrl_hdr_format1().timestamp_present = true;
    ASSERT_FALSE(g == fx::ch1_min());
    ASSERT_TRUE(g != fx::ch1_min());
}

TEST(E2smDappRoundtrip_pseudo_random_messages) {
    Rng rng(0xE2D4115ULL);
    auto u32 = [&]() { return rng.next() >> 32; };
    auto i64 = [&]() { return static_cast<int64_t>(rng.next()); };
    auto chance = [&](unsigned pct) { return rng.below(100) < pct; };
    const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 '()+,-./:=?";
    auto text = [&](size_t lo, size_t hi) {
        std::string s;
        const size_t n = lo + static_cast<size_t>(rng.below(hi - lo + 1));
        for (size_t i = 0; i < n; ++i) s.push_back(alphabet[static_cast<size_t>(rng.below(alphabet.size()))]);
        return s;
    };
    auto bytes = [&](size_t lo, size_t hi) {
        std::vector<uint8_t> b(lo + static_cast<size_t>(rng.below(hi - lo + 1)));
        for (auto& x : b) x = static_cast<uint8_t>(rng.next() >> 24);
        return b;
    };
    auto subs = [&]() {
        dapp_e3_subscription_list_l l;
        l.resize(static_cast<size_t>(rng.below(12)));
        for (auto& it : l) {
            it.dapp_id = u32();
            it.subscribed_e3_ran_functions.resize(1 + static_cast<size_t>(rng.below(64)));
            for (auto& r : it.subscribed_e3_ran_functions) r = u32();
        }
        return l;
    };

    for (int it = 0; it < 300; ++it) {
        {   // indication header, either format
            e2sm_dapp_ind_hdr_s v;
            if (chance(50)) {
                auto& f = v.ric_ind_hdr_formats.set_ind_hdr_format1();
                f.ran_function_id = u32();
                f.dapp_id         = u32();
                f.node_type       = static_cast<uint8_t>(rng.next() >> 40);
                for (auto& b : f.node_plmn_id) b = static_cast<uint8_t>(rng.next() >> 33);
                f.node_nb_id            = u32();
                f.node_cu_du_id_present = chance(50);
                f.node_cu_du_id         = u32();
                f.timestamp_present     = chance(50);
                f.timestamp             = i64();
                f.sequence_id_present   = chance(50);
                f.sequence_id           = i64();
            } else {
                auto& f = v.ric_ind_hdr_formats.set_ind_hdr_format2();
                f.node_type = static_cast<uint8_t>(rng.next() >> 40);
                for (auto& b : f.node_plmn_id) b = static_cast<uint8_t>(rng.next() >> 33);
                f.node_nb_id            = u32();
                f.node_cu_du_id_present = chance(50);
                f.node_cu_du_id         = u32();
                f.timestamp_present     = chance(50);
                f.timestamp             = i64();
                f.sequence_id_present   = chance(50);
                f.sequence_id           = i64();
            }
            roundtrip(v, "random indication header");
        }
        {   // indication message
            e2sm_dapp_ind_msg_s v;
            if (chance(50)) {
                auto& f   = v.ric_ind_msg_formats.set_ind_msg_format1();
                auto  b   = bytes(1, 600);
                f.data_size = static_cast<uint16_t>(rng.below(32769));
                f.data.assign(b.begin(), b.end());
            } else {
                v.ric_ind_msg_formats.set_ind_msg_format2().dapp_e3_subscriptions = subs();
            }
            roundtrip(v, "random indication message");
        }
        {   // control header, message, outcome
            e2sm_dapp_ctrl_hdr_s h;
            auto& hf = h.ric_ctrl_hdr_formats.ctrl_hdr_format1();
            hf.ran_function_id     = u32();
            hf.dapp_id             = u32();
            hf.timestamp_present   = chance(50);
            hf.timestamp           = i64();
            hf.sequence_id_present = chance(50);
            hf.sequence_id         = i64();
            roundtrip(h, "random control header");

            e2sm_dapp_ctrl_msg_s m;
            auto& mf = m.ric_ctrl_msg_formats.ctrl_msg_format1();
            auto  b  = bytes(1, 900);
            mf.data_size = static_cast<uint16_t>(rng.below(32769));
            mf.data.assign(b.begin(), b.end());
            roundtrip(m, "random control message");

            e2sm_dapp_ctrl_outcome_s o;
            auto& of = o.ric_ctrl_outcome_formats.ctrl_outcome_format1();
            of.timestamp_present       = chance(50);
            of.timestamp               = i64();
            of.sequence_id_present     = chance(50);
            of.sequence_id             = i64();
            of.e3_ctrl_outcome_present = chance(50);
            const auto p               = bytes(0, 300);
            of.e3_ctrl_outcome.from_bytes(p.data(), p.size());
            roundtrip(o, "random control outcome");
        }
        {   // RAN function definition
            e2sm_dapp_ran_function_definition_s v;
            v.ran_function_name.ran_function_short_name.from_string(text(1, 150));
            v.ran_function_name.ran_function_e2sm_o_id.from_string(text(1, 1000));
            v.ran_function_name.ran_function_description.from_string(text(1, 150));
            v.ran_function_name.ran_function_instance_present = chance(50);
            v.ran_function_name.ran_function_instance         = i64();
            v.ran_function_definition_event_trigger_present   = chance(50);
            if ((v.ran_function_definition_report_present = chance(60))) {
                v.ran_function_definition_report.ric_report_style_list.resize(1 + static_cast<size_t>(rng.below(2)));
                for (auto& s : v.ran_function_definition_report.ric_report_style_list) {
                    s.ric_report_style_type = i64();
                    s.ric_report_style_name.from_string(text(1, 150));
                    s.ric_ind_hdr_format_type       = i64();
                    s.ric_ind_msg_format_type       = i64();
                    s.dapp_e3_subscriptions_present = chance(50);
                    if (s.dapp_e3_subscriptions_present) s.dapp_e3_subscriptions = subs();
                }
            }
            if ((v.ran_function_definition_ctrl_present = chance(60))) {
                v.ran_function_definition_ctrl.ric_ctrl_style_list.resize(1);
                auto& s = v.ran_function_definition_ctrl.ric_ctrl_style_list[0];
                s.ric_ctrl_style_type = i64();
                s.ric_ctrl_style_name.from_string(text(1, 150));
                s.ric_ctrl_hdr_format_type      = i64();
                s.ric_ctrl_msg_format_type      = i64();
                s.ric_ctrl_outcome_format_type  = i64();
                s.dapp_e3_subscriptions_present = chance(50);
                if (s.dapp_e3_subscriptions_present) s.dapp_e3_subscriptions = subs();
            }
            roundtrip(v, "random RAN function definition");
        }
        roundtrip(fx::ad_with_style(i64()), "random action definition");
    }
}

int main() {
    return RUN_ALL_TESTS();
}
