/**
 * @file e2sm_dapp_c.h
 * @brief E2SM-DAPP information elements and their APER codec (C API).
 *
 * The C counterpart of @ref e2sm_dapp.hpp, for RAN stacks written in C (flexric,
 * and any stack that links libe3 from C). The structs carry the same information
 * as the C++ types, with one change: every OPTIONAL field has an explicit
 * `has_*` flag, and 0 is an ordinary value, never a stand-in for "absent".
 *
 * Ownership:
 *  - Inputs to `libe3_e2sm_dapp_encode_*` belong to the caller and are only read.
 *  - `libe3_e2sm_dapp_encode_*` allocates `out->data`; release it with
 *    @ref libe3_e2sm_dapp_bytes_free.
 *  - `libe3_e2sm_dapp_decode_*` allocates every pointer inside `*out`; release the
 *    whole struct with the matching `libe3_e2sm_dapp_free_*`. On failure `*out` is
 *    zeroed and nothing needs freeing.
 *  - Every `free` function zeroes the struct, so freeing twice is harmless.
 *
 * Return codes are @ref e3_error_t: 0 on success, and E3_INVALID_PARAM,
 * E3_ENCODE_FAILED, E3_DECODE_FAILED or E3_SM_ERROR_MEMORY on failure.
 *
 * Built only when LIBE3_ENABLE_E2SM_DAPP is on (it needs LIBE3_ENABLE_ASN1).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIBE3_E2SM_DAPP_C_H
#define LIBE3_E2SM_DAPP_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libe3/error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Heap bytes: an OCTET STRING, a PrintableString (not NUL-terminated), or an encoded PDU. */
typedef struct {
    uint8_t* data;
    size_t   len;
} libe3_e2sm_dapp_bytes_t;

/** Release `b->data` and zero `*b`. */
void libe3_e2sm_dapp_bytes_free(libe3_e2sm_dapp_bytes_t* b);

/* ---- dApp E3 subscription list (DAppE3Subscription-List, up to 256 items) ---- */

typedef struct {
    uint32_t  dapp_id;
    size_t    n_ran_functions;  /**< 1..64 when encoding */
    uint32_t* ran_functions;    /**< SubscribedE3RANFunction-ID values */
} libe3_e2sm_dapp_sub_item_t;

typedef struct {
    size_t                      n_items; /**< 0..256 */
    libe3_e2sm_dapp_sub_item_t* items;
} libe3_e2sm_dapp_sub_list_t;

/* ---- event trigger ---- */

typedef enum { LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1 = 1 } libe3_e2sm_dapp_event_trigger_format_e;

typedef struct {
    libe3_e2sm_dapp_event_trigger_format_e format; /**< format 1 carries no fields */
} libe3_e2sm_dapp_event_trigger_t;

/* ---- action definition ---- */

typedef enum { LIBE3_E2SM_DAPP_ACTION_DEFINITION_FORMAT_1 = 1 } libe3_e2sm_dapp_action_definition_format_e;

typedef struct {
    libe3_e2sm_dapp_action_definition_format_e format; /**< format 1 carries no fields */
    int64_t                                    ric_style_type;
} libe3_e2sm_dapp_action_definition_t;

/* ---- indication header ---- */

typedef enum { LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1 = 1, LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2 = 2 } libe3_e2sm_dapp_ind_hdr_format_e;

typedef struct {
    uint32_t ran_function_id;
    uint32_t dapp_id;
    uint8_t  node_type;
    uint8_t  node_plmn_id[3];
    uint32_t node_nb_id;
    bool     has_node_cu_du_id;
    uint32_t node_cu_du_id;
    bool     has_timestamp;
    int64_t  timestamp; /**< ns since the Unix epoch, CLOCK_REALTIME */
    bool     has_sequence_id;
    int64_t  sequence_id;
} libe3_e2sm_dapp_ind_hdr_format1_t;

typedef struct {
    uint8_t  node_type;
    uint8_t  node_plmn_id[3];
    uint32_t node_nb_id;
    bool     has_node_cu_du_id;
    uint32_t node_cu_du_id;
    bool     has_timestamp;
    int64_t  timestamp;
    bool     has_sequence_id;
    int64_t  sequence_id;
} libe3_e2sm_dapp_ind_hdr_format2_t;

typedef struct {
    libe3_e2sm_dapp_ind_hdr_format_e  format;
    libe3_e2sm_dapp_ind_hdr_format1_t frmt_1; /**< valid when format == FORMAT_1 */
    libe3_e2sm_dapp_ind_hdr_format2_t frmt_2; /**< valid when format == FORMAT_2 */
} libe3_e2sm_dapp_ind_hdr_t;

/* ---- indication message ---- */

typedef enum { LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1 = 1, LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2 = 2 } libe3_e2sm_dapp_ind_msg_format_e;

typedef struct {
    libe3_e2sm_dapp_bytes_t data; /**< 1..32768 bytes; the opaque E3 payload. data-size is len. */
} libe3_e2sm_dapp_ind_msg_format1_t;

typedef struct {
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions;
} libe3_e2sm_dapp_ind_msg_format2_t;

typedef struct {
    libe3_e2sm_dapp_ind_msg_format_e  format;
    libe3_e2sm_dapp_ind_msg_format1_t frmt_1;
    libe3_e2sm_dapp_ind_msg_format2_t frmt_2;
} libe3_e2sm_dapp_ind_msg_t;

/* ---- control header ---- */

typedef enum { LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1 = 1 } libe3_e2sm_dapp_ctrl_hdr_format_e;

typedef struct {
    libe3_e2sm_dapp_ctrl_hdr_format_e format;
    uint32_t                          ran_function_id;
    uint32_t                          dapp_id;
    bool                              has_timestamp;
    int64_t                           timestamp;
    bool                              has_sequence_id;
    int64_t                           sequence_id;
} libe3_e2sm_dapp_ctrl_hdr_t;

/* ---- control message ---- */

typedef enum { LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1 = 1 } libe3_e2sm_dapp_ctrl_msg_format_e;

typedef struct {
    libe3_e2sm_dapp_ctrl_msg_format_e format;
    libe3_e2sm_dapp_bytes_t           data; /**< 1..32768 bytes; the opaque E3 payload. data-size is len. */
} libe3_e2sm_dapp_ctrl_msg_t;

/* ---- control outcome ---- */

typedef enum { LIBE3_E2SM_DAPP_CTRL_OUTCOME_FORMAT_1 = 1 } libe3_e2sm_dapp_ctrl_outcome_format_e;

typedef struct {
    libe3_e2sm_dapp_ctrl_outcome_format_e format;
    bool                                  has_timestamp;
    int64_t                               timestamp;
    bool                                  has_sequence_id;
    int64_t                               sequence_id;
    bool                                  has_e3_control_outcome;
    libe3_e2sm_dapp_bytes_t               e3_control_outcome; /**< opaque; decoded by the SM named in the control's ran-function-id */
} libe3_e2sm_dapp_ctrl_outcome_t;

/* ---- RAN function definition ---- */

typedef struct {
    int64_t                    report_style_type;
    libe3_e2sm_dapp_bytes_t    name; /**< PrintableString, 1..150 */
    int64_t                    ind_hdr_format_type;
    int64_t                    ind_msg_format_type;
    bool                       has_dapp_e3_subscriptions;
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions;
} libe3_e2sm_dapp_report_style_t;

typedef struct {
    int64_t                    ctrl_style_type;
    libe3_e2sm_dapp_bytes_t    name; /**< PrintableString, 1..150 */
    int64_t                    ctrl_hdr_format_type;
    int64_t                    ctrl_msg_format_type;
    int64_t                    ctrl_outcome_format_type;
    bool                       has_dapp_e3_subscriptions;
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions;
} libe3_e2sm_dapp_ctrl_style_t;

typedef struct {
    libe3_e2sm_dapp_bytes_t short_name;  /**< PrintableString, 1..150 */
    libe3_e2sm_dapp_bytes_t e2sm_oid;    /**< PrintableString, 1..1000 */
    libe3_e2sm_dapp_bytes_t description; /**< PrintableString, 1..150 */
    bool                    has_instance;
    int64_t                 instance;
} libe3_e2sm_dapp_ran_function_name_t;

typedef struct {
    libe3_e2sm_dapp_ran_function_name_t name;
    bool                                has_event_trigger; /**< the event trigger item carries no fields */
    bool                                has_report;
    size_t                              n_report_styles; /**< 1..2 when has_report */
    libe3_e2sm_dapp_report_style_t*     report_styles;
    bool                                has_ctrl;
    size_t                              n_ctrl_styles; /**< 1 when has_ctrl */
    libe3_e2sm_dapp_ctrl_style_t*       ctrl_styles;
} libe3_e2sm_dapp_ran_function_definition_t;

/* ---- codec ---- */

e3_error_t libe3_e2sm_dapp_encode_event_trigger(const libe3_e2sm_dapp_event_trigger_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_event_trigger(const uint8_t* buf, size_t len, libe3_e2sm_dapp_event_trigger_t* out);
void       libe3_e2sm_dapp_free_event_trigger(libe3_e2sm_dapp_event_trigger_t* v);

e3_error_t libe3_e2sm_dapp_encode_action_definition(const libe3_e2sm_dapp_action_definition_t* in,
                                                    libe3_e2sm_dapp_bytes_t*                   out);
e3_error_t libe3_e2sm_dapp_decode_action_definition(const uint8_t* buf, size_t len, libe3_e2sm_dapp_action_definition_t* out);
void       libe3_e2sm_dapp_free_action_definition(libe3_e2sm_dapp_action_definition_t* v);

e3_error_t libe3_e2sm_dapp_encode_ind_hdr(const libe3_e2sm_dapp_ind_hdr_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_ind_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_hdr_t* out);
void       libe3_e2sm_dapp_free_ind_hdr(libe3_e2sm_dapp_ind_hdr_t* v);

e3_error_t libe3_e2sm_dapp_encode_ind_msg(const libe3_e2sm_dapp_ind_msg_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_ind_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_msg_t* out);
void       libe3_e2sm_dapp_free_ind_msg(libe3_e2sm_dapp_ind_msg_t* v);

e3_error_t libe3_e2sm_dapp_encode_ctrl_hdr(const libe3_e2sm_dapp_ctrl_hdr_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_ctrl_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_hdr_t* out);
void       libe3_e2sm_dapp_free_ctrl_hdr(libe3_e2sm_dapp_ctrl_hdr_t* v);

e3_error_t libe3_e2sm_dapp_encode_ctrl_msg(const libe3_e2sm_dapp_ctrl_msg_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_ctrl_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_msg_t* out);
void       libe3_e2sm_dapp_free_ctrl_msg(libe3_e2sm_dapp_ctrl_msg_t* v);

e3_error_t libe3_e2sm_dapp_encode_ctrl_outcome(const libe3_e2sm_dapp_ctrl_outcome_t* in, libe3_e2sm_dapp_bytes_t* out);
e3_error_t libe3_e2sm_dapp_decode_ctrl_outcome(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_outcome_t* out);
void       libe3_e2sm_dapp_free_ctrl_outcome(libe3_e2sm_dapp_ctrl_outcome_t* v);

e3_error_t libe3_e2sm_dapp_encode_ran_function_definition(const libe3_e2sm_dapp_ran_function_definition_t* in,
                                                          libe3_e2sm_dapp_bytes_t*                         out);
e3_error_t libe3_e2sm_dapp_decode_ran_function_definition(const uint8_t*                         buf,
                                                          size_t                                 len,
                                                          libe3_e2sm_dapp_ran_function_definition_t* out);
void       libe3_e2sm_dapp_free_ran_function_definition(libe3_e2sm_dapp_ran_function_definition_t* v);

#ifdef __cplusplus
}
#endif

#endif /* LIBE3_E2SM_DAPP_C_H */
