/**
 * @file test_e2sm_dapp_c_api.cpp
 * @brief Tests of the E2SM-DAPP C API (libe3/e2sm_dapp_c.h).
 *
 * Encode/decode/free for every message type; encoded bytes equal the golden
 * vectors; NULL arguments, double free, error codes, and ownership (every
 * pointer in a decoded struct is released by `free`, which is what flexric's C
 * code does).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include "libe3/e2sm_dapp_c.h"

#include "e2sm_dapp_test_util.hpp"

#include <cstdlib>
#include <cstring>

using namespace e2sm_dapp_test;
namespace fx = e2sm_dapp_fixtures;

namespace {

// ---- helpers --------------------------------------------------------------

std::vector<uint8_t> to_vec(const libe3_e2sm_dapp_bytes_t& b) {
    return b.data == nullptr ? std::vector<uint8_t>() : std::vector<uint8_t>(b.data, b.data + b.len);
}

libe3_e2sm_dapp_bytes_t heap_bytes(const uint8_t* p, size_t n) {
    libe3_e2sm_dapp_bytes_t b{nullptr, 0};
    if (n != 0) {
        b.data = static_cast<uint8_t*>(std::malloc(n));
        std::memcpy(b.data, p, n);
        b.len = n;
    }
    return b;
}

libe3_e2sm_dapp_bytes_t heap_string(const std::string& s) {
    return heap_bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

/// The golden bytes of a case (hashed ones are produced by the C++ codec and verified against the hash).
std::vector<uint8_t> golden_bytes(const std::string& name, const Case& c) {
    const Golden g = golden(name);
    if (g.inlined) return g.bytes;
    std::vector<uint8_t> out;
    if (c.pack_fixture(out) != ASN_SUCCESS || !matches_golden(g, out)) {
        throw std::runtime_error(name + ": cannot reproduce the golden bytes");
    }
    return out;
}

// ---- C struct builders (test side), from the C++ fixtures ------------------

libe3_e2sm_dapp_sub_list_t c_subs(const libe3::e2sm_dapp::dapp_e3_subscription_list_l& l) {
    libe3_e2sm_dapp_sub_list_t out{0, nullptr};
    out.n_items = l.size();
    if (!l.empty()) {
        out.items = static_cast<libe3_e2sm_dapp_sub_item_t*>(std::calloc(l.size(), sizeof(libe3_e2sm_dapp_sub_item_t)));
        for (size_t i = 0; i < l.size(); ++i) {
            out.items[i].dapp_id         = static_cast<uint32_t>(l[i].dapp_id);
            out.items[i].n_ran_functions = l[i].subscribed_e3_ran_functions.size();
            out.items[i].ran_functions =
                static_cast<uint32_t*>(std::calloc(l[i].subscribed_e3_ran_functions.size() + 1, sizeof(uint32_t)));
            for (size_t j = 0; j < l[i].subscribed_e3_ran_functions.size(); ++j) {
                out.items[i].ran_functions[j] = static_cast<uint32_t>(l[i].subscribed_e3_ran_functions[j]);
            }
        }
    }
    return out;
}

void free_subs(libe3_e2sm_dapp_sub_list_t& l) {
    for (size_t i = 0; i < l.n_items; ++i) std::free(l.items[i].ran_functions);
    std::free(l.items);
    l = {0, nullptr};
}

/// One golden case seen through the C API: encode from a C struct built from the fixture, and decode back
/// then re-encode.
struct CCase {
    std::string name;
    std::function<e3_error_t(libe3_e2sm_dapp_bytes_t*)>                                encode_fixture;
    std::function<e3_error_t(const std::vector<uint8_t>&, libe3_e2sm_dapp_bytes_t*)>   decode_reencode;
};

template <class CT, class Build, class FreeBuilt, class Enc, class Dec, class Free>
CCase make_ccase(const std::string& name, Build build, FreeBuilt free_built, Enc enc, Dec dec, Free free_fn) {
    CCase c;
    c.name           = name;
    c.encode_fixture = [=](libe3_e2sm_dapp_bytes_t* out) {
        CT v{};
        build(v);
        e3_error_t rc = enc(&v, out);
        free_built(v);
        return rc;
    };
    c.decode_reencode = [=](const std::vector<uint8_t>& bytes, libe3_e2sm_dapp_bytes_t* out) {
        CT         v{};
        e3_error_t rc = dec(bytes.data(), bytes.size(), &v);
        if (rc != E3_SUCCESS) return rc;
        rc = enc(&v, out);
        free_fn(&v);
        return rc;
    };
    return c;
}

// builders ---------------------------------------------------------------

template <class Cpp>
void noop_free(Cpp&) {}

void hdr_from(libe3_e2sm_dapp_ind_hdr_t& v, const libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s& s) {
    using types = libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types;
    if (s.ric_ind_hdr_formats.type().value == types::ind_hdr_format1) {
        const auto& f = s.ric_ind_hdr_formats.ind_hdr_format1();
        v.format      = LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1;
        auto& o       = v.frmt_1;
        o.ran_function_id = static_cast<uint32_t>(f.ran_function_id);
        o.dapp_id         = static_cast<uint32_t>(f.dapp_id);
        o.node_type       = f.node_type;
        std::memcpy(o.node_plmn_id, f.node_plmn_id.data(), 3);
        o.node_nb_id        = static_cast<uint32_t>(f.node_nb_id);
        o.has_node_cu_du_id = f.node_cu_du_id_present;
        o.node_cu_du_id     = static_cast<uint32_t>(f.node_cu_du_id);
        o.has_timestamp     = f.timestamp_present;
        o.timestamp         = f.timestamp;
        o.has_sequence_id   = f.sequence_id_present;
        o.sequence_id       = f.sequence_id;
    } else {
        const auto& f = s.ric_ind_hdr_formats.ind_hdr_format2();
        v.format      = LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2;
        auto& o       = v.frmt_2;
        o.node_type   = f.node_type;
        std::memcpy(o.node_plmn_id, f.node_plmn_id.data(), 3);
        o.node_nb_id        = static_cast<uint32_t>(f.node_nb_id);
        o.has_node_cu_du_id = f.node_cu_du_id_present;
        o.node_cu_du_id     = static_cast<uint32_t>(f.node_cu_du_id);
        o.has_timestamp     = f.timestamp_present;
        o.timestamp         = f.timestamp;
        o.has_sequence_id   = f.sequence_id_present;
        o.sequence_id       = f.sequence_id;
    }
}

void msg_from(libe3_e2sm_dapp_ind_msg_t& v, const libe3::e2sm_dapp::e2sm_dapp_ind_msg_s& s) {
    using types = libe3::e2sm_dapp::e2sm_dapp_ind_msg_s::ric_ind_msg_formats_c_::types;
    if (s.ric_ind_msg_formats.type().value == types::ind_msg_format1) {
        const auto& d = s.ric_ind_msg_formats.ind_msg_format1().data;
        v.format      = LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1;
        v.frmt_1.data = heap_bytes(d.data(), d.size());
    } else {
        v.format = LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2;
        v.frmt_2.dapp_e3_subscriptions = c_subs(s.ric_ind_msg_formats.ind_msg_format2().dapp_e3_subscriptions);
    }
}

void free_msg(libe3_e2sm_dapp_ind_msg_t& v) {
    std::free(v.frmt_1.data.data);
    free_subs(v.frmt_2.dapp_e3_subscriptions);
}

void ctrl_hdr_from(libe3_e2sm_dapp_ctrl_hdr_t& v, const libe3::e2sm_dapp::e2sm_dapp_ctrl_hdr_s& s) {
    const auto& f     = s.ric_ctrl_hdr_formats.ctrl_hdr_format1();
    v.format          = LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1;
    v.ran_function_id = static_cast<uint32_t>(f.ran_function_id);
    v.dapp_id         = static_cast<uint32_t>(f.dapp_id);
    v.has_timestamp   = f.timestamp_present;
    v.timestamp       = f.timestamp;
    v.has_sequence_id = f.sequence_id_present;
    v.sequence_id     = f.sequence_id;
}

void ctrl_msg_from(libe3_e2sm_dapp_ctrl_msg_t& v, const libe3::e2sm_dapp::e2sm_dapp_ctrl_msg_s& s) {
    const auto& d = s.ric_ctrl_msg_formats.ctrl_msg_format1().data;
    v.format      = LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1;
    v.data        = heap_bytes(d.data(), d.size());
}

void outcome_from(libe3_e2sm_dapp_ctrl_outcome_t& v, const libe3::e2sm_dapp::e2sm_dapp_ctrl_outcome_s& s) {
    const auto& f            = s.ric_ctrl_outcome_formats.ctrl_outcome_format1();
    v.format                 = LIBE3_E2SM_DAPP_CTRL_OUTCOME_FORMAT_1;
    v.has_timestamp          = f.timestamp_present;
    v.timestamp              = f.timestamp;
    v.has_sequence_id        = f.sequence_id_present;
    v.sequence_id            = f.sequence_id;
    v.has_e3_control_outcome = f.e3_ctrl_outcome_present;
    if (f.e3_ctrl_outcome_present) v.e3_control_outcome = heap_bytes(f.e3_ctrl_outcome.data(), f.e3_ctrl_outcome.size());
}

void free_outcome(libe3_e2sm_dapp_ctrl_outcome_t& v) {
    std::free(v.e3_control_outcome.data);
}

void def_from(libe3_e2sm_dapp_ran_function_definition_t& v, const libe3::e2sm_dapp::e2sm_dapp_ran_function_definition_s& s) {
    v.name.short_name  = heap_string(s.ran_function_name.ran_function_short_name.to_string());
    v.name.e2sm_oid    = heap_string(s.ran_function_name.ran_function_e2sm_o_id.to_string());
    v.name.description = heap_string(s.ran_function_name.ran_function_description.to_string());
    v.name.has_instance = s.ran_function_name.ran_function_instance_present;
    v.name.instance     = s.ran_function_name.ran_function_instance;
    v.has_event_trigger = s.ran_function_definition_event_trigger_present;
    v.has_report        = s.ran_function_definition_report_present;
    if (v.has_report) {
        const auto& l   = s.ran_function_definition_report.ric_report_style_list;
        v.n_report_styles = l.size();
        v.report_styles   = static_cast<libe3_e2sm_dapp_report_style_t*>(std::calloc(l.size(), sizeof(libe3_e2sm_dapp_report_style_t)));
        for (size_t i = 0; i < l.size(); ++i) {
            v.report_styles[i].report_style_type   = l[i].ric_report_style_type;
            v.report_styles[i].name                = heap_string(l[i].ric_report_style_name.to_string());
            v.report_styles[i].ind_hdr_format_type = l[i].ric_ind_hdr_format_type;
            v.report_styles[i].ind_msg_format_type = l[i].ric_ind_msg_format_type;
            v.report_styles[i].has_dapp_e3_subscriptions = l[i].dapp_e3_subscriptions_present;
            v.report_styles[i].dapp_e3_subscriptions     = c_subs(l[i].dapp_e3_subscriptions_present ? l[i].dapp_e3_subscriptions
                                                                                                      : libe3::e2sm_dapp::dapp_e3_subscription_list_l{});
        }
    }
    v.has_ctrl = s.ran_function_definition_ctrl_present;
    if (v.has_ctrl) {
        const auto& l   = s.ran_function_definition_ctrl.ric_ctrl_style_list;
        v.n_ctrl_styles = l.size();
        v.ctrl_styles   = static_cast<libe3_e2sm_dapp_ctrl_style_t*>(std::calloc(l.size(), sizeof(libe3_e2sm_dapp_ctrl_style_t)));
        for (size_t i = 0; i < l.size(); ++i) {
            v.ctrl_styles[i].ctrl_style_type          = l[i].ric_ctrl_style_type;
            v.ctrl_styles[i].name                     = heap_string(l[i].ric_ctrl_style_name.to_string());
            v.ctrl_styles[i].ctrl_hdr_format_type     = l[i].ric_ctrl_hdr_format_type;
            v.ctrl_styles[i].ctrl_msg_format_type     = l[i].ric_ctrl_msg_format_type;
            v.ctrl_styles[i].ctrl_outcome_format_type = l[i].ric_ctrl_outcome_format_type;
            v.ctrl_styles[i].has_dapp_e3_subscriptions = l[i].dapp_e3_subscriptions_present;
            v.ctrl_styles[i].dapp_e3_subscriptions     = c_subs(l[i].dapp_e3_subscriptions_present ? l[i].dapp_e3_subscriptions
                                                                                                    : libe3::e2sm_dapp::dapp_e3_subscription_list_l{});
        }
    }
}

void free_def(libe3_e2sm_dapp_ran_function_definition_t& v) {
    std::free(v.name.short_name.data);
    std::free(v.name.e2sm_oid.data);
    std::free(v.name.description.data);
    for (size_t i = 0; i < v.n_report_styles; ++i) {
        std::free(v.report_styles[i].name.data);
        free_subs(v.report_styles[i].dapp_e3_subscriptions);
    }
    std::free(v.report_styles);
    for (size_t i = 0; i < v.n_ctrl_styles; ++i) {
        std::free(v.ctrl_styles[i].name.data);
        free_subs(v.ctrl_styles[i].dapp_e3_subscriptions);
    }
    std::free(v.ctrl_styles);
}

const std::vector<CCase>& c_cases() {
    namespace f = e2sm_dapp_fixtures;
    static const std::vector<CCase> cases = [] {
        std::vector<CCase> v;
        auto add_et = [&](const char* n) {
            v.push_back(make_ccase<libe3_e2sm_dapp_event_trigger_t>(
                n, [](libe3_e2sm_dapp_event_trigger_t& c) { c.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1; },
                noop_free<libe3_e2sm_dapp_event_trigger_t>, libe3_e2sm_dapp_encode_event_trigger,
                libe3_e2sm_dapp_decode_event_trigger, libe3_e2sm_dapp_free_event_trigger));
        };
        add_et("et_f1");
        auto add_ad = [&](const char* n, int64_t style) {
            v.push_back(make_ccase<libe3_e2sm_dapp_action_definition_t>(
                n,
                [style](libe3_e2sm_dapp_action_definition_t& c) {
                    c.format         = LIBE3_E2SM_DAPP_ACTION_DEFINITION_FORMAT_1;
                    c.ric_style_type = style;
                },
                noop_free<libe3_e2sm_dapp_action_definition_t>, libe3_e2sm_dapp_encode_action_definition,
                libe3_e2sm_dapp_decode_action_definition, libe3_e2sm_dapp_free_action_definition));
        };
        add_ad("ad_style1", 1);
        add_ad("ad_style2", 2);
        add_ad("ad_style_max", 4294967295LL);

        using IH = libe3::e2sm_dapp::e2sm_dapp_ind_hdr_s (*)();
        auto add_ih = [&](const char* n, IH fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ind_hdr_t>(
                n, [fix](libe3_e2sm_dapp_ind_hdr_t& c) { hdr_from(c, fix()); }, noop_free<libe3_e2sm_dapp_ind_hdr_t>,
                libe3_e2sm_dapp_encode_ind_hdr, libe3_e2sm_dapp_decode_ind_hdr, libe3_e2sm_dapp_free_ind_hdr));
        };
        add_ih("ih1_full", &f::ih1_full);
        add_ih("ih1_min", &f::ih1_min);
        add_ih("ih1_bounds", &f::ih1_bounds);
        add_ih("ih2_full", &f::ih2_full);
        add_ih("ih2_min", &f::ih2_min);

        using IM = libe3::e2sm_dapp::e2sm_dapp_ind_msg_s (*)();
        auto add_im = [&](const char* n, IM fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ind_msg_t>(
                n, [fix](libe3_e2sm_dapp_ind_msg_t& c) { msg_from(c, fix()); }, free_msg,
                libe3_e2sm_dapp_encode_ind_msg, libe3_e2sm_dapp_decode_ind_msg, libe3_e2sm_dapp_free_ind_msg));
        };
        add_im("im1_small", &f::im1_small);
        add_im("im1_one", &f::im1_one);
        add_im("im1_max", &f::im1_max);
        add_im("im2_empty", &f::im2_empty);
        add_im("im2_one", &f::im2_one);
        add_im("im2_multi", &f::im2_multi);
        add_im("im2_255", &f::im2_255);
        add_im("im2_256", &f::im2_256);

        using CH = libe3::e2sm_dapp::e2sm_dapp_ctrl_hdr_s (*)();
        auto add_ch = [&](const char* n, CH fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ctrl_hdr_t>(
                n, [fix](libe3_e2sm_dapp_ctrl_hdr_t& c) { ctrl_hdr_from(c, fix()); },
                noop_free<libe3_e2sm_dapp_ctrl_hdr_t>, libe3_e2sm_dapp_encode_ctrl_hdr, libe3_e2sm_dapp_decode_ctrl_hdr,
                libe3_e2sm_dapp_free_ctrl_hdr));
        };
        add_ch("ch1_full", &f::ch1_full);
        add_ch("ch1_min", &f::ch1_min);
        add_ch("ch1_bounds", &f::ch1_bounds);

        using CM = libe3::e2sm_dapp::e2sm_dapp_ctrl_msg_s (*)();
        auto add_cm = [&](const char* n, CM fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ctrl_msg_t>(
                n, [fix](libe3_e2sm_dapp_ctrl_msg_t& c) { ctrl_msg_from(c, fix()); },
                [](libe3_e2sm_dapp_ctrl_msg_t& c) { std::free(c.data.data); }, libe3_e2sm_dapp_encode_ctrl_msg,
                libe3_e2sm_dapp_decode_ctrl_msg, libe3_e2sm_dapp_free_ctrl_msg));
        };
        add_cm("cm1_small", &f::cm1_small);
        add_cm("cm1_one", &f::cm1_one);
        add_cm("cm1_large", &f::cm1_large);

        using CO = libe3::e2sm_dapp::e2sm_dapp_ctrl_outcome_s (*)();
        auto add_co = [&](const char* n, CO fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ctrl_outcome_t>(
                n, [fix](libe3_e2sm_dapp_ctrl_outcome_t& c) { outcome_from(c, fix()); }, free_outcome,
                libe3_e2sm_dapp_encode_ctrl_outcome, libe3_e2sm_dapp_decode_ctrl_outcome,
                libe3_e2sm_dapp_free_ctrl_outcome));
        };
        add_co("co1_full", &f::co1_full);
        add_co("co1_min", &f::co1_min);
        add_co("co1_tsseq", &f::co1_tsseq);
        add_co("co1_payload", &f::co1_payload);

        using FD = libe3::e2sm_dapp::e2sm_dapp_ran_function_definition_s (*)();
        auto add_fd = [&](const char* n, FD fix) {
            v.push_back(make_ccase<libe3_e2sm_dapp_ran_function_definition_t>(
                n, [fix](libe3_e2sm_dapp_ran_function_definition_t& c) { def_from(c, fix()); }, free_def,
                libe3_e2sm_dapp_encode_ran_function_definition, libe3_e2sm_dapp_decode_ran_function_definition,
                libe3_e2sm_dapp_free_ran_function_definition));
        };
        add_fd("fd_min", &f::fd_min);
        add_fd("fd_report", &f::fd_report);
        add_fd("fd_full", &f::fd_full);
        add_fd("fd_ctrl", &f::fd_ctrl);
        return v;
    }();
    return cases;
}

const Case& cpp_case(const std::string& name) {
    for (const auto& c : all_cases()) {
        if (c.name == name) return c;
    }
    throw std::runtime_error("no case " + name);
}

} // namespace

// ---------------------------------------------------------------------------

TEST(E2smDappCApi_every_golden_case_encodes_to_the_golden_bytes) {
    ASSERT_EQ(c_cases().size(), static_cast<size_t>(31));
    for (const auto& c : c_cases()) {
        libe3_e2sm_dapp_bytes_t out{nullptr, 0};
        e3_error_t              rc = c.encode_fixture(&out);
        if (rc != E3_SUCCESS) {
            throw std::runtime_error(c.name + ": encode failed, rc=" + std::to_string(rc));
        }
        const bool ok = matches_golden(golden(c.name), to_vec(out));
        libe3_e2sm_dapp_bytes_free(&out);
        if (!ok) throw std::runtime_error(c.name + ": bytes differ from the golden vector");
        ASSERT_TRUE(out.data == nullptr);
        ASSERT_EQ(out.len, static_cast<size_t>(0));
    }
}

TEST(E2smDappCApi_every_golden_case_decodes_and_reencodes_to_the_same_bytes) {
    for (const auto& c : c_cases()) {
        const auto              bytes = golden_bytes(c.name, cpp_case(c.name));
        libe3_e2sm_dapp_bytes_t out{nullptr, 0};
        e3_error_t              rc = c.decode_reencode(bytes, &out);
        if (rc != E3_SUCCESS) {
            throw std::runtime_error(c.name + ": decode/encode failed, rc=" + std::to_string(rc));
        }
        const bool same = (to_vec(out) == bytes);
        libe3_e2sm_dapp_bytes_free(&out);
        if (!same) throw std::runtime_error(c.name + ": re-encoded bytes differ");
    }
}

TEST(E2smDappCApi_decoded_fields_are_what_the_vector_says) {
    {
        const auto            b = golden("ih1_full").bytes;
        libe3_e2sm_dapp_ind_hdr_t h;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(b.data(), b.size(), &h), E3_SUCCESS);
        ASSERT_EQ(static_cast<int>(h.format), static_cast<int>(LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1));
        ASSERT_EQ(h.frmt_1.ran_function_id, 255u);
        ASSERT_EQ(h.frmt_1.dapp_id, 7u);
        ASSERT_EQ(static_cast<int>(h.frmt_1.node_type), 2);
        ASSERT_EQ(static_cast<int>(h.frmt_1.node_plmn_id[1]), 0xf1);
        ASSERT_EQ(h.frmt_1.node_nb_id, 1234u);
        ASSERT_TRUE(h.frmt_1.has_node_cu_du_id);
        ASSERT_EQ(h.frmt_1.node_cu_du_id, 5u);
        ASSERT_TRUE(h.frmt_1.has_timestamp);
        ASSERT_EQ(h.frmt_1.timestamp, static_cast<int64_t>(1700000000123456789LL));
        ASSERT_TRUE(h.frmt_1.has_sequence_id);
        ASSERT_EQ(h.frmt_1.sequence_id, static_cast<int64_t>(42));
        libe3_e2sm_dapp_free_ind_hdr(&h);
    }
    {
        const auto            b = golden("ih1_min").bytes;
        libe3_e2sm_dapp_ind_hdr_t h;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(b.data(), b.size(), &h), E3_SUCCESS);
        ASSERT_FALSE(h.frmt_1.has_node_cu_du_id || h.frmt_1.has_timestamp || h.frmt_1.has_sequence_id);
        ASSERT_EQ(h.frmt_1.timestamp, static_cast<int64_t>(0));
        libe3_e2sm_dapp_free_ind_hdr(&h);
    }
    {
        const auto            b = golden("ih2_min").bytes;
        libe3_e2sm_dapp_ind_hdr_t h;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(b.data(), b.size(), &h), E3_SUCCESS);
        ASSERT_EQ(static_cast<int>(h.format), static_cast<int>(LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2));
        ASSERT_EQ(h.frmt_2.node_nb_id, 99u);
        libe3_e2sm_dapp_free_ind_hdr(&h);
    }
    {
        const auto            b = golden("im1_small").bytes;
        libe3_e2sm_dapp_ind_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(b.data(), b.size(), &m), E3_SUCCESS);
        ASSERT_EQ(static_cast<int>(m.format), static_cast<int>(LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1));
        ASSERT_EQ(to_hex(to_vec(m.frmt_1.data)), std::string("deadbeef"));
        libe3_e2sm_dapp_free_ind_msg(&m);
    }
    {
        const auto            b = golden("im2_multi").bytes;
        libe3_e2sm_dapp_ind_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(b.data(), b.size(), &m), E3_SUCCESS);
        ASSERT_EQ(static_cast<int>(m.format), static_cast<int>(LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2));
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.n_items, static_cast<size_t>(2));
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.items[0].dapp_id, 7u);
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.items[0].n_ran_functions, static_cast<size_t>(3));
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.items[1].ran_functions[1], 4294967295u);
        libe3_e2sm_dapp_free_ind_msg(&m);
    }
    {
        const auto            b = golden("im2_empty").bytes;
        libe3_e2sm_dapp_ind_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(b.data(), b.size(), &m), E3_SUCCESS);
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.n_items, static_cast<size_t>(0));
        ASSERT_TRUE(m.frmt_2.dapp_e3_subscriptions.items == nullptr);
        libe3_e2sm_dapp_free_ind_msg(&m);
    }
    {
        const auto             b = golden("ch1_bounds").bytes;
        libe3_e2sm_dapp_ctrl_hdr_t h;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_hdr(b.data(), b.size(), &h), E3_SUCCESS);
        ASSERT_EQ(h.ran_function_id, 4294967295u);
        ASSERT_EQ(h.dapp_id, 0u);
        ASSERT_TRUE(h.has_timestamp && h.has_sequence_id);
        ASSERT_EQ(h.timestamp, static_cast<int64_t>(INT64_MAX));
        libe3_e2sm_dapp_free_ctrl_hdr(&h);
    }
    {
        const auto             b = golden("cm1_small").bytes;
        libe3_e2sm_dapp_ctrl_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_msg(b.data(), b.size(), &m), E3_SUCCESS);
        ASSERT_EQ(to_hex(to_vec(m.data)), std::string("010203"));
        libe3_e2sm_dapp_free_ctrl_msg(&m);
    }
    {
        const auto                 b = golden("co1_full").bytes;
        libe3_e2sm_dapp_ctrl_outcome_t o;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_outcome(b.data(), b.size(), &o), E3_SUCCESS);
        ASSERT_TRUE(o.has_timestamp && o.has_sequence_id && o.has_e3_control_outcome);
        ASSERT_EQ(to_hex(to_vec(o.e3_control_outcome)), std::string("102030405060708090a0"));
        libe3_e2sm_dapp_free_ctrl_outcome(&o);
        const auto b2 = golden("co1_min").bytes;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_outcome(b2.data(), b2.size(), &o), E3_SUCCESS);
        ASSERT_FALSE(o.has_timestamp || o.has_sequence_id || o.has_e3_control_outcome);
        ASSERT_TRUE(o.e3_control_outcome.data == nullptr);
        libe3_e2sm_dapp_free_ctrl_outcome(&o);
    }
    {
        const auto                                b = golden("fd_full").bytes;
        libe3_e2sm_dapp_ran_function_definition_t d;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(b.data(), b.size(), &d), E3_SUCCESS);
        ASSERT_EQ(std::string(reinterpret_cast<const char*>(d.name.short_name.data), d.name.short_name.len),
                  std::string("E2SM-DAPP"));
        ASSERT_EQ(std::string(reinterpret_cast<const char*>(d.name.e2sm_oid.data), d.name.e2sm_oid.len),
                  std::string("1.3.6.1.4.1.53148.1.1.255.3"));
        ASSERT_FALSE(d.name.has_instance);
        ASSERT_TRUE(d.has_event_trigger && d.has_report && d.has_ctrl);
        ASSERT_EQ(d.n_report_styles, static_cast<size_t>(2));
        ASSERT_EQ(d.n_ctrl_styles, static_cast<size_t>(1));
        ASSERT_FALSE(d.report_styles[0].has_dapp_e3_subscriptions);
        ASSERT_TRUE(d.report_styles[1].has_dapp_e3_subscriptions);
        ASSERT_EQ(d.report_styles[1].dapp_e3_subscriptions.n_items, static_cast<size_t>(2));
        ASSERT_EQ(std::string(reinterpret_cast<const char*>(d.ctrl_styles[0].name.data), d.ctrl_styles[0].name.len),
                  std::string("dApp Control"));
        ASSERT_TRUE(d.ctrl_styles[0].has_dapp_e3_subscriptions);
        libe3_e2sm_dapp_free_ran_function_definition(&d);
    }
}

TEST(E2smDappCApi_decoded_memory_can_be_released_with_plain_free) {
    // flexric's C code frees what it is given with free(): every pointer in a decoded struct must be a
    // malloc'ed block of its own. (Run under AddressSanitizer this catches a mismatch.)
    const auto                                b = golden("fd_full").bytes;
    libe3_e2sm_dapp_ran_function_definition_t d;
    ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(b.data(), b.size(), &d), E3_SUCCESS);
    free_def(d);

    const auto                b2 = golden("im2_multi").bytes;
    libe3_e2sm_dapp_ind_msg_t m;
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(b2.data(), b2.size(), &m), E3_SUCCESS);
    free_msg(m);

    libe3_e2sm_dapp_bytes_t out{nullptr, 0};
    libe3_e2sm_dapp_event_trigger_t et;
    et.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1;
    ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(&et, &out), E3_SUCCESS);
    ASSERT_EQ(out.len, static_cast<size_t>(1));
    std::free(out.data);
}

TEST(E2smDappCApi_free_zeroes_and_freeing_twice_is_harmless) {
    {
        const auto                                b = golden("fd_full").bytes;
        libe3_e2sm_dapp_ran_function_definition_t d;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(b.data(), b.size(), &d), E3_SUCCESS);
        libe3_e2sm_dapp_free_ran_function_definition(&d);
        libe3_e2sm_dapp_ran_function_definition_t zero;
        std::memset(&zero, 0, sizeof(zero));
        ASSERT_TRUE(std::memcmp(&d, &zero, sizeof(d)) == 0);
        libe3_e2sm_dapp_free_ran_function_definition(&d);
        ASSERT_TRUE(std::memcmp(&d, &zero, sizeof(d)) == 0);
    }
    {
        const auto                b = golden("im2_multi").bytes;
        libe3_e2sm_dapp_ind_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(b.data(), b.size(), &m), E3_SUCCESS);
        libe3_e2sm_dapp_free_ind_msg(&m);
        libe3_e2sm_dapp_free_ind_msg(&m);
        ASSERT_TRUE(m.frmt_2.dapp_e3_subscriptions.items == nullptr);
        ASSERT_EQ(m.frmt_2.dapp_e3_subscriptions.n_items, static_cast<size_t>(0));
    }
    {
        const auto                 b = golden("co1_full").bytes;
        libe3_e2sm_dapp_ctrl_outcome_t o;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_outcome(b.data(), b.size(), &o), E3_SUCCESS);
        libe3_e2sm_dapp_free_ctrl_outcome(&o);
        libe3_e2sm_dapp_free_ctrl_outcome(&o);
        ASSERT_TRUE(o.e3_control_outcome.data == nullptr && !o.has_e3_control_outcome);
    }
    {
        const auto             b = golden("cm1_small").bytes;
        libe3_e2sm_dapp_ctrl_msg_t m;
        ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_msg(b.data(), b.size(), &m), E3_SUCCESS);
        libe3_e2sm_dapp_free_ctrl_msg(&m);
        libe3_e2sm_dapp_free_ctrl_msg(&m);
        ASSERT_TRUE(m.data.data == nullptr);
    }
    {
        libe3_e2sm_dapp_bytes_t out{nullptr, 0};
        libe3_e2sm_dapp_event_trigger_t et;
        et.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1;
        ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(&et, &out), E3_SUCCESS);
        libe3_e2sm_dapp_bytes_free(&out);
        libe3_e2sm_dapp_bytes_free(&out);
        ASSERT_TRUE(out.data == nullptr);
        ASSERT_EQ(out.len, static_cast<size_t>(0));
    }
    // A free of NULL is a no-op, and of a zeroed struct too.
    libe3_e2sm_dapp_bytes_free(nullptr);
    libe3_e2sm_dapp_free_event_trigger(nullptr);
    libe3_e2sm_dapp_free_action_definition(nullptr);
    libe3_e2sm_dapp_free_ind_hdr(nullptr);
    libe3_e2sm_dapp_free_ind_msg(nullptr);
    libe3_e2sm_dapp_free_ctrl_hdr(nullptr);
    libe3_e2sm_dapp_free_ctrl_msg(nullptr);
    libe3_e2sm_dapp_free_ctrl_outcome(nullptr);
    libe3_e2sm_dapp_free_ran_function_definition(nullptr);
    libe3_e2sm_dapp_ran_function_definition_t zero;
    std::memset(&zero, 0, sizeof(zero));
    libe3_e2sm_dapp_free_ran_function_definition(&zero);
}

TEST(E2smDappCApi_null_arguments_are_invalid_params) {
    libe3_e2sm_dapp_bytes_t out{nullptr, 0};
    const uint8_t           buf[1] = {0};

    libe3_e2sm_dapp_event_trigger_t et;
    et.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1;
    ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(&et, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_event_trigger(nullptr, 1, &et), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_event_trigger(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_action_definition_t ad;
    ASSERT_EQ(libe3_e2sm_dapp_encode_action_definition(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_action_definition(&ad, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_action_definition(nullptr, 1, &ad), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_action_definition(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ind_hdr_t ih;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ind_hdr(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ind_hdr(&ih, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(nullptr, 1, &ih), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ind_msg_t im;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&im, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(nullptr, 1, &im), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ctrl_hdr_t ch;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_hdr(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_hdr(&ch, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_hdr(nullptr, 1, &ch), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_hdr(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ctrl_msg_t cm;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&cm, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_msg(nullptr, 1, &cm), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_msg(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ctrl_outcome_t co;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_outcome(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_outcome(&co, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_outcome(nullptr, 1, &co), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_outcome(buf, 1, nullptr), E3_INVALID_PARAM);

    libe3_e2sm_dapp_ran_function_definition_t fd;
    ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(nullptr, &out), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&fd, nullptr), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(nullptr, 1, &fd), E3_INVALID_PARAM);
    ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(buf, 1, nullptr), E3_INVALID_PARAM);

    ASSERT_TRUE(out.data == nullptr);
}

TEST(E2smDappCApi_decode_failure_zeroes_the_output) {
    const uint8_t junk[] = {0xff, 0xff, 0xff, 0xff};
    {
        libe3_e2sm_dapp_ind_hdr_t h;
        std::memset(&h, 0x5a, sizeof(h));
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(junk, sizeof(junk), &h), E3_DECODE_FAILED);
        libe3_e2sm_dapp_ind_hdr_t zero;
        std::memset(&zero, 0, sizeof(zero));
        ASSERT_TRUE(std::memcmp(&h, &zero, sizeof(h)) == 0);
    }
    {
        libe3_e2sm_dapp_ran_function_definition_t d;
        std::memset(&d, 0x5a, sizeof(d));
        ASSERT_EQ(libe3_e2sm_dapp_decode_ran_function_definition(junk, sizeof(junk), &d), E3_DECODE_FAILED);
        libe3_e2sm_dapp_ran_function_definition_t zero;
        std::memset(&zero, 0, sizeof(zero));
        ASSERT_TRUE(std::memcmp(&d, &zero, sizeof(d)) == 0);
    }
    {
        libe3_e2sm_dapp_ind_msg_t m;
        std::memset(&m, 0x5a, sizeof(m));
        ASSERT_EQ(libe3_e2sm_dapp_decode_ind_msg(junk, 2, &m), E3_DECODE_FAILED);
        ASSERT_TRUE(m.frmt_1.data.data == nullptr && m.frmt_2.dapp_e3_subscriptions.items == nullptr);
    }
    // Truncated golden vectors.
    for (const auto& c : c_cases()) {
        const auto bytes = golden_bytes(c.name, cpp_case(c.name));
        if (bytes.size() > 300) continue;
        for (size_t n = 1; n < bytes.size(); ++n) {
            std::vector<uint8_t>    prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
            libe3_e2sm_dapp_bytes_t out{nullptr, 0};
            e3_error_t              rc = c.decode_reencode(prefix, &out);
            if (rc != E3_DECODE_FAILED) {
                throw std::runtime_error(c.name + ": a truncated vector did not give E3_DECODE_FAILED");
            }
            ASSERT_TRUE(out.data == nullptr);
        }
    }
}

TEST(E2smDappCApi_empty_buffer_is_a_decode_failure) {
    uint8_t                   one = 0;
    libe3_e2sm_dapp_ind_hdr_t h;
    ASSERT_EQ(libe3_e2sm_dapp_decode_ind_hdr(&one, 0, &h), E3_DECODE_FAILED);
    libe3_e2sm_dapp_event_trigger_t et;
    ASSERT_EQ(libe3_e2sm_dapp_decode_event_trigger(&one, 0, &et), E3_DECODE_FAILED);
}

TEST(E2smDappCApi_trailing_bytes_are_accepted) {
    auto b = golden("ch1_min").bytes;
    b.push_back(0x99);
    b.push_back(0x98);
    libe3_e2sm_dapp_ctrl_hdr_t h;
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_hdr(b.data(), b.size(), &h), E3_SUCCESS);
    ASSERT_EQ(h.ran_function_id, 1u);
    ASSERT_EQ(h.dapp_id, 2u);
}

TEST(E2smDappCApi_encode_failures_return_encode_failed_and_leave_out_empty) {
    libe3_e2sm_dapp_bytes_t out{reinterpret_cast<uint8_t*>(0x1), 77}; // garbage the call must not trust

    // Payload empty / too large / NULL with a length.
    {
        libe3_e2sm_dapp_ctrl_msg_t m;
        std::memset(&m, 0, sizeof(m));
        m.format = LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1;
        out      = {nullptr, 0};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&m, &out), E3_ENCODE_FAILED); // empty payload
        ASSERT_TRUE(out.data == nullptr);
        std::vector<uint8_t> big(32769, 1);
        m.data = {big.data(), big.size()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&m, &out), E3_ENCODE_FAILED);
        ASSERT_TRUE(out.data == nullptr && out.len == 0);
        // An absurd length is refused before it is read.
        m.data = {big.data(), static_cast<size_t>(1) << 40};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&m, &out), E3_ENCODE_FAILED);
        m.data = {nullptr, 5};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&m, &out), E3_INVALID_PARAM);
        big.resize(32768);
        m.data = {big.data(), big.size()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_msg(&m, &out), E3_SUCCESS);
        ASSERT_EQ(out.len, static_cast<size_t>(32773));
        libe3_e2sm_dapp_bytes_free(&out);
    }
    // Subscription list: 257 items, 0 functions, 65 functions.
    {
        libe3_e2sm_dapp_ind_msg_t m;
        std::memset(&m, 0, sizeof(m));
        m.format = LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2;
        std::vector<libe3_e2sm_dapp_sub_item_t> items(257);
        std::vector<uint32_t>                   rfs(65, 1);
        for (auto& it : items) {
            it.dapp_id         = 1;
            it.n_ran_functions = 1;
            it.ran_functions   = rfs.data();
        }
        m.frmt_2.dapp_e3_subscriptions = {items.size(), items.data()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_ENCODE_FAILED);
        ASSERT_TRUE(out.data == nullptr);
        m.frmt_2.dapp_e3_subscriptions = {256, items.data()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_SUCCESS);
        libe3_e2sm_dapp_bytes_free(&out);

        items[3].n_ran_functions = 0;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_ENCODE_FAILED);
        items[3].n_ran_functions = 65;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_ENCODE_FAILED);
        items[3].n_ran_functions = 2;
        items[3].ran_functions   = nullptr;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_INVALID_PARAM);
        m.frmt_2.dapp_e3_subscriptions = {2, nullptr};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_INVALID_PARAM);
        ASSERT_TRUE(out.data == nullptr);
    }
    // Strings and style counts.
    {
        libe3_e2sm_dapp_ran_function_definition_t d;
        std::memset(&d, 0, sizeof(d));
        const std::string name = "E2SM-DAPP", oid = "1.3.6.1.4.1.53148.1.1.255.3", desc = "d";
        d.name.short_name  = {reinterpret_cast<uint8_t*>(const_cast<char*>(name.data())), name.size()};
        d.name.e2sm_oid    = {reinterpret_cast<uint8_t*>(const_cast<char*>(oid.data())), oid.size()};
        d.name.description = {reinterpret_cast<uint8_t*>(const_cast<char*>(desc.data())), desc.size()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_SUCCESS);
        libe3_e2sm_dapp_bytes_free(&out);

        std::string bad = "bad_name"; // '_' is not a PrintableString character
        d.name.short_name = {reinterpret_cast<uint8_t*>(bad.data()), bad.size()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_ENCODE_FAILED);
        std::string longname(151, 'a');
        d.name.short_name = {reinterpret_cast<uint8_t*>(longname.data()), longname.size()};
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_ENCODE_FAILED);
        d.name.short_name = {reinterpret_cast<uint8_t*>(const_cast<char*>(name.data())), name.size()};

        d.has_report      = true;
        d.n_report_styles = 0;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_ENCODE_FAILED);
        d.n_report_styles = 3;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_ENCODE_FAILED);
        d.has_report = false;
        d.has_ctrl   = true;
        d.n_ctrl_styles = 2;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_ENCODE_FAILED);
        d.n_ctrl_styles = 1;
        d.ctrl_styles   = nullptr;
        ASSERT_EQ(libe3_e2sm_dapp_encode_ran_function_definition(&d, &out), E3_INVALID_PARAM);
        ASSERT_TRUE(out.data == nullptr);
    }
}

TEST(E2smDappCApi_unknown_format_is_an_invalid_param) {
    libe3_e2sm_dapp_bytes_t out{nullptr, 0};
    {
        libe3_e2sm_dapp_ind_hdr_t h;
        std::memset(&h, 0, sizeof(h)); // format 0 is not a format
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_hdr(&h, &out), E3_INVALID_PARAM);
        h.format = static_cast<libe3_e2sm_dapp_ind_hdr_format_e>(3);
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_hdr(&h, &out), E3_INVALID_PARAM);
    }
    {
        libe3_e2sm_dapp_ind_msg_t m;
        std::memset(&m, 0, sizeof(m));
        ASSERT_EQ(libe3_e2sm_dapp_encode_ind_msg(&m, &out), E3_INVALID_PARAM);
    }
    {
        libe3_e2sm_dapp_event_trigger_t et;
        std::memset(&et, 0, sizeof(et));
        ASSERT_EQ(libe3_e2sm_dapp_encode_event_trigger(&et, &out), E3_INVALID_PARAM);
        libe3_e2sm_dapp_ctrl_hdr_t ch;
        std::memset(&ch, 0, sizeof(ch));
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_hdr(&ch, &out), E3_INVALID_PARAM);
        libe3_e2sm_dapp_ctrl_outcome_t co;
        std::memset(&co, 0, sizeof(co));
        ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_outcome(&co, &out), E3_INVALID_PARAM);
        libe3_e2sm_dapp_action_definition_t ad;
        std::memset(&ad, 0, sizeof(ad));
        ASSERT_EQ(libe3_e2sm_dapp_encode_action_definition(&ad, &out), E3_INVALID_PARAM);
    }
    ASSERT_TRUE(out.data == nullptr);
}

TEST(E2smDappCApi_zero_is_a_value_when_the_has_flag_is_set) {
    libe3_e2sm_dapp_ctrl_hdr_t h;
    std::memset(&h, 0, sizeof(h));
    h.format          = LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1;
    h.ran_function_id = 1;
    h.dapp_id         = 2;
    libe3_e2sm_dapp_bytes_t absent{nullptr, 0}, zero{nullptr, 0};
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_hdr(&h, &absent), E3_SUCCESS);
    h.has_timestamp   = true; // timestamp == 0
    h.has_sequence_id = true; // sequence_id == 0
    ASSERT_EQ(libe3_e2sm_dapp_encode_ctrl_hdr(&h, &zero), E3_SUCCESS);
    ASSERT_GT(zero.len, absent.len);
    libe3_e2sm_dapp_ctrl_hdr_t back;
    ASSERT_EQ(libe3_e2sm_dapp_decode_ctrl_hdr(zero.data, zero.len, &back), E3_SUCCESS);
    ASSERT_TRUE(back.has_timestamp && back.has_sequence_id);
    ASSERT_EQ(back.timestamp, static_cast<int64_t>(0));
    ASSERT_EQ(back.sequence_id, static_cast<int64_t>(0));
    libe3_e2sm_dapp_bytes_free(&absent);
    libe3_e2sm_dapp_bytes_free(&zero);
}

TEST(E2smDappCApi_c_and_cpp_apis_agree_on_the_bytes) {
    // The same message through both APIs gives the same bytes, for a message built by hand in C.
    libe3_e2sm_dapp_ind_hdr_t h;
    std::memset(&h, 0, sizeof(h));
    h.format                   = LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1;
    h.frmt_1.ran_function_id   = 255;
    h.frmt_1.dapp_id           = 7;
    h.frmt_1.node_type         = 2;
    h.frmt_1.node_plmn_id[0]   = 0x00;
    h.frmt_1.node_plmn_id[1]   = 0xf1;
    h.frmt_1.node_plmn_id[2]   = 0x10;
    h.frmt_1.node_nb_id        = 1234;
    h.frmt_1.has_node_cu_du_id = true;
    h.frmt_1.node_cu_du_id     = 5;
    h.frmt_1.has_timestamp     = true;
    h.frmt_1.timestamp         = 1700000000123456789LL;
    h.frmt_1.has_sequence_id   = true;
    h.frmt_1.sequence_id       = 42;
    libe3_e2sm_dapp_bytes_t out{nullptr, 0};
    ASSERT_EQ(libe3_e2sm_dapp_encode_ind_hdr(&h, &out), E3_SUCCESS);
    ASSERT_TRUE(matches_golden(golden("ih1_full"), to_vec(out)));
    libe3_e2sm_dapp_bytes_free(&out);
}

int main() {
    return RUN_ALL_TESTS();
}
