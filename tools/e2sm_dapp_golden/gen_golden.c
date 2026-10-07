// SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
// SPDX-License-Identifier: Apache-2.0
//
// Dumps the golden APER encodings of every E2SM-DAPP message, produced by
// flexric's own dapp_enc_*_asn() functions, and checks that flexric's decoder
// returns an equal structure for each. Output: one "GOLDEN <name> <hex>" line per
// case, and "GOLDEN_HASH <name> <len> <fnv1a64>" for payloads too large to inline.
//
// The inputs below are the contract that libe3's and OCUDU's tests rebuild by
// hand, so keep the case names and values in step with those tests.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sm/dapp_sm/dapp_sm_id.h"
#include "sm/dapp_sm/dec/dapp_dec_asn.h"
#include "sm/dapp_sm/enc/dapp_enc_asn.h"
#include "sm/dapp_sm/ie/dapp_data_ie.h"
#include "util/byte_array.h"

static void emit(const char* name, byte_array_t ba)
{
  assert(ba.buf != NULL && ba.len > 0);
  if (ba.len > 512) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < ba.len; ++i) {
      h ^= ba.buf[i];
      h *= 1099511628211ULL;
    }
    printf("GOLDEN_HASH %s %zu %016llx\n", name, ba.len, (unsigned long long)h);
  } else {
    printf("GOLDEN %s ", name);
    for (size_t i = 0; i < ba.len; ++i)
      printf("%02x", ba.buf[i]);
    printf("\n");
  }
}

static uint8_t* pattern(size_t n, unsigned mul, unsigned add)
{
  uint8_t* p = malloc(n);
  for (size_t i = 0; i < n; ++i)
    p[i] = (uint8_t)(i * mul + add);
  return p;
}

static dapp_e3_subscription_list_t make_subs(size_t n_items, uint32_t const* ids, size_t const* n_rf, uint32_t const* const* rfs)
{
  dapp_e3_subscription_list_t l = {0};
  l.sz_dapp_e3_subscriptions = n_items;
  l.dapp_e3_subscriptions = calloc(n_items ? n_items : 1, sizeof(dapp_e3_subscription_item_t));
  for (size_t i = 0; i < n_items; ++i) {
    l.dapp_e3_subscriptions[i].dapp_id = ids[i];
    l.dapp_e3_subscriptions[i].sz_subscribed_e3_ran_functions = n_rf[i];
    l.dapp_e3_subscriptions[i].subscribed_e3_ran_functions = calloc(n_rf[i], sizeof(uint32_t));
    memcpy(l.dapp_e3_subscriptions[i].subscribed_e3_ran_functions, rfs[i], n_rf[i] * sizeof(uint32_t));
  }
  return l;
}

#define CHECK_RT(dec, eq, free_, v)                                                        \
  do {                                                                                     \
    byte_array_t ba_ = v_ba;                                                            \
    __typeof__(v) d_ = dec(ba_.len, ba_.buf);                                              \
    assert(eq(&(v), &d_) && "flexric decode(encode(x)) != x");                             \
    free_(&d_);                                                                            \
  } while (0)

int main(void)
{
  /* --- event trigger --- */
  {
    e2sm_dapp_event_trigger_t v = {.format = FORMAT_1_E2SM_DAPP_EV_TRIGGER_FORMAT};
    byte_array_t v_ba = dapp_enc_event_trigger_asn(&v);
    emit("et_f1", v_ba);
    CHECK_RT(dapp_dec_event_trigger_asn, eq_e2sm_dapp_event_trigger, free_e2sm_dapp_event_trigger, v);
  }

  /* --- action definition --- */
  {
    uint32_t styles[] = {1, 2, 4294967295u};
    const char* names[] = {"ad_style1", "ad_style2", "ad_style_max"};
    for (int i = 0; i < 3; ++i) {
      e2sm_dapp_action_def_t v = {.format = FORMAT_1_E2SM_DAPP_ACTION_DEF, .ric_style_type = styles[i]};
      byte_array_t v_ba = dapp_enc_action_def_asn(&v);
      emit(names[i], v_ba);
      CHECK_RT(dapp_dec_action_def_asn, eq_e2sm_dapp_action_def, free_e2sm_dapp_action_def, v);
    }
  }

  /* --- indication header format 1 --- */
  {
    e2sm_dapp_ind_hdr_t full = {.format = FORMAT_1_E2SM_DAPP_IND_HDR,
                                .frmt_1 = {.ran_function_id = 255,
                                           .dapp_id = 7,
                                           .node_type = 2,
                                           .node_plmn_id = {0x00, 0xf1, 0x10},
                                           .node_nb_id = 1234,
                                           .node_cu_du_id_present = true,
                                           .node_cu_du_id = 5,
                                           .timestamp_ns = 1700000000123456789LL,
                                           .sequence_id = 42}};
    byte_array_t v_ba = dapp_enc_ind_hdr_asn(&full);
    emit("ih1_full", v_ba);
    CHECK_RT(dapp_dec_ind_hdr_asn, eq_e2sm_dapp_ind_hdr, free_e2sm_dapp_ind_hdr, full);

    e2sm_dapp_ind_hdr_t min = {.format = FORMAT_1_E2SM_DAPP_IND_HDR,
                               .frmt_1 = {.ran_function_id = 255,
                                          .dapp_id = 1,
                                          .node_type = 0,
                                          .node_plmn_id = {0x13, 0x00, 0x14},
                                          .node_nb_id = 0}};
    v_ba = dapp_enc_ind_hdr_asn(&min);
    emit("ih1_min", v_ba);
    CHECK_RT(dapp_dec_ind_hdr_asn, eq_e2sm_dapp_ind_hdr, free_e2sm_dapp_ind_hdr, min);

    e2sm_dapp_ind_hdr_t bounds = {.format = FORMAT_1_E2SM_DAPP_IND_HDR,
                                  .frmt_1 = {.ran_function_id = 0,
                                             .dapp_id = 4294967295u,
                                             .node_type = 255,
                                             .node_plmn_id = {0xff, 0xff, 0xff},
                                             .node_nb_id = 4294967295u,
                                             .node_cu_du_id_present = true,
                                             .node_cu_du_id = 4294967295u,
                                             .timestamp_ns = INT64_MAX,
                                             .sequence_id = INT64_MAX}};
    v_ba = dapp_enc_ind_hdr_asn(&bounds);
    emit("ih1_bounds", v_ba);
    CHECK_RT(dapp_dec_ind_hdr_asn, eq_e2sm_dapp_ind_hdr, free_e2sm_dapp_ind_hdr, bounds);
  }

  /* --- indication header format 2 --- */
  {
    e2sm_dapp_ind_hdr_t full = {.format = FORMAT_2_E2SM_DAPP_IND_HDR,
                                .frmt_2 = {.node_type = 2,
                                           .node_plmn_id = {0x00, 0xf1, 0x10},
                                           .node_nb_id = 1234,
                                           .node_cu_du_id_present = true,
                                           .node_cu_du_id = 5,
                                           .timestamp_ns = 1700000000123456789LL,
                                           .sequence_id = 42}};
    byte_array_t v_ba = dapp_enc_ind_hdr_asn(&full);
    emit("ih2_full", v_ba);
    CHECK_RT(dapp_dec_ind_hdr_asn, eq_e2sm_dapp_ind_hdr, free_e2sm_dapp_ind_hdr, full);

    e2sm_dapp_ind_hdr_t min = {.format = FORMAT_2_E2SM_DAPP_IND_HDR,
                               .frmt_2 = {.node_type = 1, .node_plmn_id = {0x13, 0x00, 0x14}, .node_nb_id = 99}};
    v_ba = dapp_enc_ind_hdr_asn(&min);
    emit("ih2_min", v_ba);
    CHECK_RT(dapp_dec_ind_hdr_asn, eq_e2sm_dapp_ind_hdr, free_e2sm_dapp_ind_hdr, min);
  }

  /* --- indication message format 1 --- */
  {
    uint8_t small[] = {0xde, 0xad, 0xbe, 0xef};
    e2sm_dapp_ind_msg_t v = {.format = FORMAT_1_E2SM_DAPP_IND_MSG, .frmt_1 = {.data_size = 4, .data = small}};
    byte_array_t v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im1_small", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);

    uint8_t one[] = {0x00};
    v.frmt_1.data_size = 1;
    v.frmt_1.data = one;
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im1_one", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);

    uint8_t* big = pattern(32768, 7, 3);
    v.frmt_1.data_size = 32768;
    v.frmt_1.data = big;
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im1_max", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);
    free(big);
  }

  /* --- indication message format 2 --- */
  {
    e2sm_dapp_ind_msg_t v = {.format = FORMAT_2_E2SM_DAPP_IND_MSG, .frmt_2 = {.dapp_e3_subs = {0}}};
    byte_array_t v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im2_empty", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);

    uint32_t id1[] = {1};
    size_t n1[] = {1};
    uint32_t rf1[] = {1};
    uint32_t const* rfs1[] = {rf1};
    v.frmt_2.dapp_e3_subs = make_subs(1, id1, n1, rfs1);
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im2_one", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);
    free_dapp_e3_subscription_list(&v.frmt_2.dapp_e3_subs);

    uint32_t id2[] = {7, 4294967295u};
    size_t n2[] = {3, 2};
    uint32_t rfa[] = {1, 2, 3};
    uint32_t rfb[] = {0, 4294967295u};
    uint32_t const* rfs2[] = {rfa, rfb};
    v.frmt_2.dapp_e3_subs = make_subs(2, id2, n2, rfs2);
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im2_multi", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);
    free_dapp_e3_subscription_list(&v.frmt_2.dapp_e3_subs);

    /* Large lists: maxnoofDApps = 256 items, and 64 functions on one of them.
     * flexric's decoder asserts n < 256, so the grammar's own maximum of 256 can
     * be encoded but not decoded by flexric (see the discrepancies doc). The
     * 256-item case is therefore emitted without flexric's round trip. */
    enum { N = 256 };
    uint32_t ids[N];
    size_t ns[N];
    uint32_t rfbuf[N][1];
    uint32_t const* rfsN[N];
    uint32_t rf64[64];
    for (int i = 0; i < 64; ++i)
      rf64[i] = (uint32_t)(i * 1000);
    for (int i = 0; i < N; ++i) {
      ids[i] = (uint32_t)i;
      ns[i] = 1;
      rfbuf[i][0] = (uint32_t)(i + 1);
      rfsN[i] = rfbuf[i];
    }
    ns[0] = 64;
    rfsN[0] = rf64;

    ns[0] = 63; /* flexric's decoder also asserts nrf < 64 */
    v.frmt_2.dapp_e3_subs = make_subs(255, ids, ns, rfsN);
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im2_255", v_ba);
    CHECK_RT(dapp_dec_ind_msg_asn, eq_e2sm_dapp_ind_msg, free_e2sm_dapp_ind_msg, v);
    free_dapp_e3_subscription_list(&v.frmt_2.dapp_e3_subs);

    ns[0] = 64;
    v.frmt_2.dapp_e3_subs = make_subs(N, ids, ns, rfsN);
    v_ba = dapp_enc_ind_msg_asn(&v);
    emit("im2_256", v_ba);
    /* not freed: free_dapp_e3_subscription_list asserts sz < 256 as well */
  }

  /* --- control header --- */
  {
    e2sm_dapp_ctrl_hdr_t full = {.format = FORMAT_1_E2SM_DAPP_CTRL_HDR,
                                 .frmt_1 = {.ran_function_id = 255, .dapp_id = 7, .timestamp_ns = 1700000000123456789LL, .sequence_id = 42}};
    byte_array_t v_ba = dapp_enc_ctrl_hdr_asn(&full);
    emit("ch1_full", v_ba);
    CHECK_RT(dapp_dec_ctrl_hdr_asn, eq_e2sm_dapp_ctrl_hdr, free_e2sm_dapp_ctrl_hdr, full);

    e2sm_dapp_ctrl_hdr_t min = {.format = FORMAT_1_E2SM_DAPP_CTRL_HDR, .frmt_1 = {.ran_function_id = 1, .dapp_id = 2}};
    v_ba = dapp_enc_ctrl_hdr_asn(&min);
    emit("ch1_min", v_ba);
    CHECK_RT(dapp_dec_ctrl_hdr_asn, eq_e2sm_dapp_ctrl_hdr, free_e2sm_dapp_ctrl_hdr, min);

    e2sm_dapp_ctrl_hdr_t bounds = {.format = FORMAT_1_E2SM_DAPP_CTRL_HDR,
                                   .frmt_1 = {.ran_function_id = 4294967295u, .dapp_id = 0, .timestamp_ns = INT64_MAX, .sequence_id = INT64_MAX}};
    v_ba = dapp_enc_ctrl_hdr_asn(&bounds);
    emit("ch1_bounds", v_ba);
    CHECK_RT(dapp_dec_ctrl_hdr_asn, eq_e2sm_dapp_ctrl_hdr, free_e2sm_dapp_ctrl_hdr, bounds);
  }

  /* --- control message (flexric's buffer is 16 KiB, so "large" stays under it) --- */
  {
    uint8_t small[] = {0x01, 0x02, 0x03};
    e2sm_dapp_ctrl_msg_t v = {.format = FORMAT_1_E2SM_DAPP_CTRL_MSG, .frmt_1 = {.data_size = 3, .data = small}};
    byte_array_t v_ba = dapp_enc_ctrl_msg_asn(&v);
    emit("cm1_small", v_ba);
    CHECK_RT(dapp_dec_ctrl_msg_asn, eq_e2sm_dapp_ctrl_msg, free_e2sm_dapp_ctrl_msg, v);

    uint8_t one[] = {0xff};
    v.frmt_1.data_size = 1;
    v.frmt_1.data = one;
    v_ba = dapp_enc_ctrl_msg_asn(&v);
    emit("cm1_one", v_ba);
    CHECK_RT(dapp_dec_ctrl_msg_asn, eq_e2sm_dapp_ctrl_msg, free_e2sm_dapp_ctrl_msg, v);

    uint8_t* big = pattern(16000, 13, 5);
    v.frmt_1.data_size = 16000;
    v.frmt_1.data = big;
    v_ba = dapp_enc_ctrl_msg_asn(&v);
    emit("cm1_large", v_ba);
    CHECK_RT(dapp_dec_ctrl_msg_asn, eq_e2sm_dapp_ctrl_msg, free_e2sm_dapp_ctrl_msg, v);
    free(big);
  }

  /* --- control outcome --- */
  {
    uint8_t payload[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0};
    e2sm_dapp_ctrl_out_t full = {.format = FORMAT_1_E2SM_DAPP_CTRL_OUT,
                                 .frmt_1 = {.timestamp_ns = 1700000000123456789LL,
                                            .sequence_id = 42,
                                            .e3_control_outcome = payload,
                                            .e3_control_outcome_size = 10}};
    byte_array_t v_ba = dapp_enc_ctrl_out_asn(&full);
    emit("co1_full", v_ba);
    CHECK_RT(dapp_dec_ctrl_out_asn, eq_e2sm_dapp_ctrl_out, free_e2sm_dapp_ctrl_out, full);

    e2sm_dapp_ctrl_out_t min = {.format = FORMAT_1_E2SM_DAPP_CTRL_OUT};
    v_ba = dapp_enc_ctrl_out_asn(&min);
    emit("co1_min", v_ba);
    CHECK_RT(dapp_dec_ctrl_out_asn, eq_e2sm_dapp_ctrl_out, free_e2sm_dapp_ctrl_out, min);

    e2sm_dapp_ctrl_out_t tsseq = {.format = FORMAT_1_E2SM_DAPP_CTRL_OUT,
                                  .frmt_1 = {.timestamp_ns = 1700000000123456789LL, .sequence_id = 42}};
    v_ba = dapp_enc_ctrl_out_asn(&tsseq);
    emit("co1_tsseq", v_ba);
    CHECK_RT(dapp_dec_ctrl_out_asn, eq_e2sm_dapp_ctrl_out, free_e2sm_dapp_ctrl_out, tsseq);

    e2sm_dapp_ctrl_out_t pay = {.format = FORMAT_1_E2SM_DAPP_CTRL_OUT,
                                .frmt_1 = {.e3_control_outcome = payload, .e3_control_outcome_size = 10}};
    v_ba = dapp_enc_ctrl_out_asn(&pay);
    emit("co1_payload", v_ba);
    CHECK_RT(dapp_dec_ctrl_out_asn, eq_e2sm_dapp_ctrl_out, free_e2sm_dapp_ctrl_out, pay);
  }

  /* --- RAN function definition --- */
  {
    e2sm_dapp_func_def_t fd = {0};
    fd.name.name = cp_str_to_ba(SM_DAPP_SHORT_NAME);
    fd.name.oid = cp_str_to_ba(SM_DAPP_OID);
    fd.name.description = cp_str_to_ba(SM_DAPP_DESCRIPTION);

    byte_array_t v_ba = dapp_enc_func_def_asn(&fd);
    emit("fd_min", v_ba);
    CHECK_RT(dapp_dec_func_def_asn, eq_e2sm_dapp_func_def, free_e2sm_dapp_func_def, fd);

    /* report only: two styles, the second with a subscription map */
    uint32_t id[] = {7, 9};
    size_t n[] = {2, 1};
    uint32_t rfa[] = {1, 2};
    uint32_t rfb[] = {3};
    uint32_t const* rfs[] = {rfa, rfb};

    fd.report = calloc(1, sizeof(*fd.report));
    fd.report->sz_seq_report_sty = 2;
    fd.report->seq_report_sty = calloc(2, sizeof(seq_report_sty_dapp_sm_t));
    fd.report->seq_report_sty[0] = (seq_report_sty_dapp_sm_t){
        .report_type = DAPP_RIC_STYLE_E3_DATA_REPORT, .name = cp_str_to_ba("E3 Data Report"), .ind_hdr_type = 1, .ind_msg_type = 1};
    fd.report->seq_report_sty[1] = (seq_report_sty_dapp_sm_t){.report_type = DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP,
                                                              .name = cp_str_to_ba("E3 Subscription Map"),
                                                              .ind_hdr_type = 2,
                                                              .ind_msg_type = 2,
                                                              .dapp_e3_subs = calloc(1, sizeof(dapp_e3_subscription_list_t))};
    *fd.report->seq_report_sty[1].dapp_e3_subs = make_subs(2, id, n, rfs);
    v_ba = dapp_enc_func_def_asn(&fd);
    emit("fd_report", v_ba);
    CHECK_RT(dapp_dec_func_def_asn, eq_e2sm_dapp_func_def, free_e2sm_dapp_func_def, fd);

    /* full: event trigger + report + control (one style, with a subscription map) */
    fd.ev_trig = calloc(1, sizeof(*fd.ev_trig));
    fd.ctrl = calloc(1, sizeof(*fd.ctrl));
    fd.ctrl->sz_seq_ctrl_style = 1;
    fd.ctrl->seq_ctrl_style = calloc(1, sizeof(seq_ctrl_style_dapp_sm_t));
    fd.ctrl->seq_ctrl_style[0] = (seq_ctrl_style_dapp_sm_t){.style_type = 1,
                                                            .name = cp_str_to_ba("dApp Control"),
                                                            .hdr = 1,
                                                            .msg = 1,
                                                            .out_frmt = 1,
                                                            .dapp_e3_subs = calloc(1, sizeof(dapp_e3_subscription_list_t))};
    *fd.ctrl->seq_ctrl_style[0].dapp_e3_subs = make_subs(1, id, n, rfs);
    v_ba = dapp_enc_func_def_asn(&fd);
    emit("fd_full", v_ba);
    CHECK_RT(dapp_dec_func_def_asn, eq_e2sm_dapp_func_def, free_e2sm_dapp_func_def, fd);

    /* control only, no subscription map */
    free_ran_func_def_report_dapp_sm(fd.report);
    free(fd.report);
    fd.report = NULL;
    free_dapp_e3_subscription_list(fd.ctrl->seq_ctrl_style[0].dapp_e3_subs);
    free(fd.ctrl->seq_ctrl_style[0].dapp_e3_subs);
    fd.ctrl->seq_ctrl_style[0].dapp_e3_subs = NULL;
    v_ba = dapp_enc_func_def_asn(&fd);
    emit("fd_ctrl", v_ba);
    CHECK_RT(dapp_dec_func_def_asn, eq_e2sm_dapp_func_def, free_e2sm_dapp_func_def, fd);
  }

  return 0;
}
