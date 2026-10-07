// SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
// SPDX-License-Identifier: Apache-2.0
//
// Builders for every input of the E2SM-DAPP golden vectors
// (tools/e2sm_dapp_golden/gen_golden.c), written against the type and field
// names that libe3 (namespace libe3::e2sm_dapp) and OCUDU (namespace
// asn1::e2sm_dapp) share. This body is byte-identical in both repositories, so
// that building a message reads the same in either stack and the golden vectors
// then prove that both encode it to the same bytes.
//
// The including file defines, before including this one:
//   E2SM_DAPP_NS                         the namespace holding the types
//   E2SM_DAPP_SET_STRING(dst, "text")    assign a PrintableString
//   E2SM_DAPP_SET_OCTETS(dst, ptr, n)    assign an unbounded OCTET STRING
//
// Each builder name is the golden vector's name. Messages are returned by value.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace e2sm_dapp_fixtures {

using namespace E2SM_DAPP_NS;

// Deterministic byte pattern, the same one gen_golden.c uses.
inline std::vector<uint8_t> pattern(size_t n, unsigned mul, unsigned add)
{
  std::vector<uint8_t> p(n);
  for (size_t i = 0; i < n; ++i) {
    p[i] = static_cast<uint8_t>(i * mul + add);
  }
  return p;
}

template <typename Plmn>
inline void set_plmn(Plmn& dst, uint8_t a, uint8_t b, uint8_t c)
{
  dst[0] = a;
  dst[1] = b;
  dst[2] = c;
}

/// Subscription list of n items, item i carrying dapp_id ids[i] and the functions rfs[i].
inline dapp_e3_subscription_list_l make_subs(const std::vector<uint64_t>&              ids,
                                             const std::vector<std::vector<uint64_t>>& rfs)
{
  dapp_e3_subscription_list_l l;
  l.resize(static_cast<uint32_t>(ids.size()));
  for (size_t i = 0; i < ids.size(); ++i) {
    l[i].dapp_id = ids[i];
    l[i].subscribed_e3_ran_functions.resize(static_cast<uint32_t>(rfs[i].size()));
    for (size_t j = 0; j < rfs[i].size(); ++j) {
      l[i].subscribed_e3_ran_functions[j] = rfs[i][j];
    }
  }
  return l;
}

// ---- event trigger -------------------------------------------------------

inline e2sm_dapp_event_trigger_s et_f1()
{
  e2sm_dapp_event_trigger_s v;
  return v;
}

// ---- action definition ---------------------------------------------------

inline e2sm_dapp_action_definition_s ad_with_style(int64_t style)
{
  e2sm_dapp_action_definition_s v;
  v.ric_style_type = style;
  return v;
}
inline e2sm_dapp_action_definition_s ad_style1()
{
  return ad_with_style(1);
}
inline e2sm_dapp_action_definition_s ad_style2()
{
  return ad_with_style(2);
}
inline e2sm_dapp_action_definition_s ad_style_max()
{
  return ad_with_style(4294967295LL);
}

// ---- indication header ---------------------------------------------------

inline e2sm_dapp_ind_hdr_s ih1_full()
{
  e2sm_dapp_ind_hdr_s v;
  auto&               f = v.ric_ind_hdr_formats.set_ind_hdr_format1();
  f.ran_function_id     = 255;
  f.dapp_id             = 7;
  f.node_type           = 2;
  set_plmn(f.node_plmn_id, 0x00, 0xf1, 0x10);
  f.node_nb_id            = 1234;
  f.node_cu_du_id_present = true;
  f.node_cu_du_id         = 5;
  f.timestamp_present     = true;
  f.timestamp             = 1700000000123456789LL;
  f.sequence_id_present   = true;
  f.sequence_id           = 42;
  return v;
}

inline e2sm_dapp_ind_hdr_s ih1_min()
{
  e2sm_dapp_ind_hdr_s v;
  auto&               f = v.ric_ind_hdr_formats.set_ind_hdr_format1();
  f.ran_function_id     = 255;
  f.dapp_id             = 1;
  f.node_type           = 0;
  set_plmn(f.node_plmn_id, 0x13, 0x00, 0x14);
  f.node_nb_id = 0;
  return v;
}

inline e2sm_dapp_ind_hdr_s ih1_bounds()
{
  e2sm_dapp_ind_hdr_s v;
  auto&               f = v.ric_ind_hdr_formats.set_ind_hdr_format1();
  f.ran_function_id     = 0;
  f.dapp_id             = 4294967295ULL;
  f.node_type           = 255;
  set_plmn(f.node_plmn_id, 0xff, 0xff, 0xff);
  f.node_nb_id            = 4294967295ULL;
  f.node_cu_du_id_present = true;
  f.node_cu_du_id         = 4294967295ULL;
  f.timestamp_present     = true;
  f.timestamp             = INT64_MAX;
  f.sequence_id_present   = true;
  f.sequence_id           = INT64_MAX;
  return v;
}

inline e2sm_dapp_ind_hdr_s ih2_full()
{
  e2sm_dapp_ind_hdr_s v;
  auto&               f = v.ric_ind_hdr_formats.set_ind_hdr_format2();
  f.node_type           = 2;
  set_plmn(f.node_plmn_id, 0x00, 0xf1, 0x10);
  f.node_nb_id            = 1234;
  f.node_cu_du_id_present = true;
  f.node_cu_du_id         = 5;
  f.timestamp_present     = true;
  f.timestamp             = 1700000000123456789LL;
  f.sequence_id_present   = true;
  f.sequence_id           = 42;
  return v;
}

inline e2sm_dapp_ind_hdr_s ih2_min()
{
  e2sm_dapp_ind_hdr_s v;
  auto&               f = v.ric_ind_hdr_formats.set_ind_hdr_format2();
  f.node_type           = 1;
  set_plmn(f.node_plmn_id, 0x13, 0x00, 0x14);
  f.node_nb_id = 99;
  return v;
}

// ---- indication message --------------------------------------------------

inline e2sm_dapp_ind_msg_s im1_with(const std::vector<uint8_t>& bytes)
{
  e2sm_dapp_ind_msg_s v;
  auto&               f = v.ric_ind_msg_formats.set_ind_msg_format1();
  f.data_size           = static_cast<uint16_t>(bytes.size());
  f.data.resize(static_cast<uint32_t>(bytes.size()));
  for (size_t i = 0; i < bytes.size(); ++i) {
    f.data[static_cast<uint32_t>(i)] = bytes[i];
  }
  return v;
}
inline e2sm_dapp_ind_msg_s im1_small()
{
  return im1_with({0xde, 0xad, 0xbe, 0xef});
}
inline e2sm_dapp_ind_msg_s im1_one()
{
  return im1_with({0x00});
}
inline e2sm_dapp_ind_msg_s im1_max()
{
  return im1_with(pattern(32768, 7, 3));
}

inline e2sm_dapp_ind_msg_s im2_with(const dapp_e3_subscription_list_l& subs)
{
  e2sm_dapp_ind_msg_s v;
  auto&               f      = v.ric_ind_msg_formats.set_ind_msg_format2();
  f.dapp_e3_subscriptions    = subs;
  return v;
}
inline e2sm_dapp_ind_msg_s im2_empty()
{
  return im2_with(make_subs({}, {}));
}
inline e2sm_dapp_ind_msg_s im2_one()
{
  return im2_with(make_subs({1}, {{1}}));
}
inline e2sm_dapp_ind_msg_s im2_multi()
{
  return im2_with(make_subs({7, 4294967295ULL}, {{1, 2, 3}, {0, 4294967295ULL}}));
}

/// n items, item i with dapp_id i and the single function i + 1. Item 0 carries
/// first_item_functions functions, values i * 1000.
inline e2sm_dapp_ind_msg_s im2_many(size_t n, size_t first_item_functions)
{
  std::vector<uint64_t>              ids;
  std::vector<std::vector<uint64_t>> rfs;
  for (size_t i = 0; i < n; ++i) {
    ids.push_back(i);
    if (i == 0) {
      std::vector<uint64_t> first;
      for (size_t j = 0; j < first_item_functions; ++j) {
        first.push_back(j * 1000);
      }
      rfs.push_back(first);
    } else {
      rfs.push_back({i + 1});
    }
  }
  return im2_with(make_subs(ids, rfs));
}
inline e2sm_dapp_ind_msg_s im2_255()
{
  return im2_many(255, 63);
}
inline e2sm_dapp_ind_msg_s im2_256()
{
  return im2_many(256, 64);
}

// ---- control header ------------------------------------------------------

inline e2sm_dapp_ctrl_hdr_s ch1_full()
{
  e2sm_dapp_ctrl_hdr_s v;
  auto&                f = v.ric_ctrl_hdr_formats.ctrl_hdr_format1();
  f.ran_function_id      = 255;
  f.dapp_id              = 7;
  f.timestamp_present    = true;
  f.timestamp            = 1700000000123456789LL;
  f.sequence_id_present  = true;
  f.sequence_id          = 42;
  return v;
}
inline e2sm_dapp_ctrl_hdr_s ch1_min()
{
  e2sm_dapp_ctrl_hdr_s v;
  auto&                f = v.ric_ctrl_hdr_formats.ctrl_hdr_format1();
  f.ran_function_id      = 1;
  f.dapp_id              = 2;
  return v;
}
inline e2sm_dapp_ctrl_hdr_s ch1_bounds()
{
  e2sm_dapp_ctrl_hdr_s v;
  auto&                f = v.ric_ctrl_hdr_formats.ctrl_hdr_format1();
  f.ran_function_id      = 4294967295ULL;
  f.dapp_id              = 0;
  f.timestamp_present    = true;
  f.timestamp            = INT64_MAX;
  f.sequence_id_present  = true;
  f.sequence_id          = INT64_MAX;
  return v;
}

// ---- control message -----------------------------------------------------

inline e2sm_dapp_ctrl_msg_s cm1_with(const std::vector<uint8_t>& bytes)
{
  e2sm_dapp_ctrl_msg_s v;
  auto&                f = v.ric_ctrl_msg_formats.ctrl_msg_format1();
  f.data_size            = static_cast<uint16_t>(bytes.size());
  f.data.resize(static_cast<uint32_t>(bytes.size()));
  for (size_t i = 0; i < bytes.size(); ++i) {
    f.data[static_cast<uint32_t>(i)] = bytes[i];
  }
  return v;
}
inline e2sm_dapp_ctrl_msg_s cm1_small()
{
  return cm1_with({0x01, 0x02, 0x03});
}
inline e2sm_dapp_ctrl_msg_s cm1_one()
{
  return cm1_with({0xff});
}
// flexric's encoder buffer is 16 KiB, so the golden vector stops at 16000 bytes
// although the grammar allows 32768.
inline e2sm_dapp_ctrl_msg_s cm1_large()
{
  return cm1_with(pattern(16000, 13, 5));
}

// ---- control outcome -----------------------------------------------------

inline e2sm_dapp_ctrl_outcome_s co1_with(bool                       with_ts_seq,
                                         const std::vector<uint8_t>& payload)
{
  e2sm_dapp_ctrl_outcome_s v;
  auto&                    f = v.ric_ctrl_outcome_formats.ctrl_outcome_format1();
  if (with_ts_seq) {
    f.timestamp_present   = true;
    f.timestamp           = 1700000000123456789LL;
    f.sequence_id_present = true;
    f.sequence_id         = 42;
  }
  if (!payload.empty()) {
    f.e3_ctrl_outcome_present = true;
    E2SM_DAPP_SET_OCTETS(f.e3_ctrl_outcome, payload.data(), payload.size());
  }
  return v;
}
inline std::vector<uint8_t> co_payload()
{
  return {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0};
}
inline e2sm_dapp_ctrl_outcome_s co1_full()
{
  return co1_with(true, co_payload());
}
inline e2sm_dapp_ctrl_outcome_s co1_min()
{
  return co1_with(false, {});
}
inline e2sm_dapp_ctrl_outcome_s co1_tsseq()
{
  return co1_with(true, {});
}
inline e2sm_dapp_ctrl_outcome_s co1_payload()
{
  return co1_with(false, co_payload());
}

// ---- RAN function definition ---------------------------------------------

inline e2sm_dapp_ran_function_definition_s fd_min()
{
  e2sm_dapp_ran_function_definition_s v;
  E2SM_DAPP_SET_STRING(v.ran_function_name.ran_function_short_name, "E2SM-DAPP");
  E2SM_DAPP_SET_STRING(v.ran_function_name.ran_function_e2sm_o_id, "1.3.6.1.4.1.53148.1.1.255.3");
  E2SM_DAPP_SET_STRING(v.ran_function_name.ran_function_description, "DAPP Service Model for E2/E3 bridge");
  return v;
}

inline void add_report_styles(e2sm_dapp_ran_function_definition_s& v)
{
  v.ran_function_definition_report_present = true;
  auto& styles                             = v.ran_function_definition_report.ric_report_style_list;
  styles.resize(2);
  styles[0].ric_report_style_type = 1;
  E2SM_DAPP_SET_STRING(styles[0].ric_report_style_name, "E3 Data Report");
  styles[0].ric_ind_hdr_format_type = 1;
  styles[0].ric_ind_msg_format_type = 1;
  styles[1].ric_report_style_type   = 2;
  E2SM_DAPP_SET_STRING(styles[1].ric_report_style_name, "E3 Subscription Map");
  styles[1].ric_ind_hdr_format_type       = 2;
  styles[1].ric_ind_msg_format_type       = 2;
  styles[1].dapp_e3_subscriptions_present = true;
  styles[1].dapp_e3_subscriptions         = make_subs({7, 9}, {{1, 2}, {3}});
}

inline void add_ctrl_style(e2sm_dapp_ran_function_definition_s& v, bool with_subs)
{
  v.ran_function_definition_ctrl_present = true;
  auto& styles                           = v.ran_function_definition_ctrl.ric_ctrl_style_list;
  styles.resize(1);
  styles[0].ric_ctrl_style_type = 1;
  E2SM_DAPP_SET_STRING(styles[0].ric_ctrl_style_name, "dApp Control");
  styles[0].ric_ctrl_hdr_format_type     = 1;
  styles[0].ric_ctrl_msg_format_type     = 1;
  styles[0].ric_ctrl_outcome_format_type = 1;
  if (with_subs) {
    styles[0].dapp_e3_subscriptions_present = true;
    styles[0].dapp_e3_subscriptions         = make_subs({7}, {{1, 2}});
  }
}

inline e2sm_dapp_ran_function_definition_s fd_report()
{
  auto v = fd_min();
  add_report_styles(v);
  return v;
}
inline e2sm_dapp_ran_function_definition_s fd_full()
{
  auto v = fd_min();
  v.ran_function_definition_event_trigger_present = true;
  add_report_styles(v);
  add_ctrl_style(v, true);
  return v;
}
inline e2sm_dapp_ran_function_definition_s fd_ctrl()
{
  auto v = fd_min();
  v.ran_function_definition_event_trigger_present = true;
  add_ctrl_style(v, false);
  return v;
}

} // namespace e2sm_dapp_fixtures
