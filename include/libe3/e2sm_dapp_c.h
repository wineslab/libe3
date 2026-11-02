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
 * Return codes are the values of @ref e3_error_t. They are 0 on success, and
 * E3_INVALID_PARAM, E3_ENCODE_FAILED, E3_DECODE_FAILED or E3_SM_ERROR_MEMORY on failure.
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
    uint8_t* data; ///< The bytes, heap allocated
    size_t   len; ///< Number of bytes in `data`
} libe3_e2sm_dapp_bytes_t;

/** Release `b->data` and zero `*b`.
 * @param b bytes to release, may be NULL */
void libe3_e2sm_dapp_bytes_free(libe3_e2sm_dapp_bytes_t* b);

/* ---- dApp E3 subscription list (DAppE3Subscription-List, up to 256 items) ---- */

/** The E3 RAN functions one dApp has subscribed to. */
typedef struct {
    uint32_t  dapp_id; ///< The dApp
    size_t    n_ran_functions;  /**< 1..64 when encoding */
    uint32_t* ran_functions;    /**< SubscribedE3RANFunction-ID values */
} libe3_e2sm_dapp_sub_item_t;

/** The dApp E3 subscription map. */
typedef struct {
    size_t                      n_items; /**< 0..256 */
    libe3_e2sm_dapp_sub_item_t* items; ///< Array of `n_items` subscription items
} libe3_e2sm_dapp_sub_list_t;

/* ---- event trigger ---- */

/** Formats of the event trigger. */
typedef enum {
    LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1 = 1 ///< Format 1
} libe3_e2sm_dapp_event_trigger_format_e;

/** Event trigger definition of a subscription. */
typedef struct {
    libe3_e2sm_dapp_event_trigger_format_e format; /**< format 1 carries no fields */
} libe3_e2sm_dapp_event_trigger_t;

/* ---- action definition ---- */

/** Formats of the action definition. */
typedef enum {
    LIBE3_E2SM_DAPP_ACTION_DEFINITION_FORMAT_1 = 1 ///< Format 1
} libe3_e2sm_dapp_action_definition_format_e;

/** Action definition of a subscription, which selects the report style. */
typedef struct {
    libe3_e2sm_dapp_action_definition_format_e format; /**< format 1 carries no fields */
    int64_t                                    ric_style_type; ///< Report style the subscription selects (1 or 2); it selects the indication formats
} libe3_e2sm_dapp_action_definition_t;

/* ---- indication header ---- */

/** Formats of the ind hdr. */
typedef enum {
    LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1 = 1, ///< Format 1
    LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2 = 2 ///< Format 2
} libe3_e2sm_dapp_ind_hdr_format_e;

/** Indication header format 1: the header of a dApp report. */
typedef struct {
    uint32_t ran_function_id; ///< E3 RAN function id of the report, not the E2 RAN function id
    uint32_t dapp_id; ///< The dApp that sent the report
    uint8_t  node_type; ///< Type of the reporting node (0 to 255)
    uint8_t  node_plmn_id[3]; ///< PLMN id of the node, 3 octets
    uint32_t node_nb_id; ///< gNB id of the node
    bool     has_node_cu_du_id; ///< true when `node_cu_du_id` is present
    uint32_t node_cu_du_id; ///< CU or DU id inside the node
    bool     has_timestamp; ///< true when `timestamp` is present
    int64_t  timestamp; /**< ns since the Unix epoch, CLOCK_REALTIME */
    bool     has_sequence_id; ///< true when `sequence_id` is present
    int64_t  sequence_id; ///< Correlation id of the dApp for this report, copied from the E3 report
} libe3_e2sm_dapp_ind_hdr_format1_t;

/** Indication header format 2: the header of a subscription map. */
typedef struct {
    uint8_t  node_type; ///< Type of the reporting node (0 to 255)
    uint8_t  node_plmn_id[3]; ///< PLMN id of the node, 3 octets
    uint32_t node_nb_id; ///< gNB id of the node
    bool     has_node_cu_du_id; ///< true when `node_cu_du_id` is present
    uint32_t node_cu_du_id; ///< CU or DU id inside the node
    bool     has_timestamp; ///< true when `timestamp` is present
    int64_t  timestamp; ///< When the RAN built the indication, in nanoseconds on its realtime clock
    bool     has_sequence_id; ///< true when `sequence_id` is present
    int64_t  sequence_id; ///< Correlation id; a subscription map has no procedure, so a RAN leaves it absent
} libe3_e2sm_dapp_ind_hdr_format2_t;

/** Indication header. */
typedef struct {
    libe3_e2sm_dapp_ind_hdr_format_e  format; ///< Selected format
    libe3_e2sm_dapp_ind_hdr_format1_t frmt_1; /**< valid when format == FORMAT_1 */
    libe3_e2sm_dapp_ind_hdr_format2_t frmt_2; /**< valid when format == FORMAT_2 */
} libe3_e2sm_dapp_ind_hdr_t;

/* ---- indication message ---- */

/** Formats of the ind msg. */
typedef enum {
    LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1 = 1, ///< Format 1
    LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2 = 2 ///< Format 2
} libe3_e2sm_dapp_ind_msg_format_e;

/** Indication message format 1: the dApp report payload. */
typedef struct {
    libe3_e2sm_dapp_bytes_t data; /**< 1..32768 bytes; the opaque E3 payload. data-size is len. */
} libe3_e2sm_dapp_ind_msg_format1_t;

/** Indication message format 2: the dApp E3 subscription map. */
typedef struct {
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions; ///< Current dApp E3 subscription map; can be empty
} libe3_e2sm_dapp_ind_msg_format2_t;

/** Indication message. */
typedef struct {
    libe3_e2sm_dapp_ind_msg_format_e  format; ///< Selected format
    libe3_e2sm_dapp_ind_msg_format1_t frmt_1; ///< Content when `format` is FORMAT_1
    libe3_e2sm_dapp_ind_msg_format2_t frmt_2; ///< Content when `format` is FORMAT_2
} libe3_e2sm_dapp_ind_msg_t;

/* ---- control header ---- */

/** Formats of the ctrl hdr. */
typedef enum {
    LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1 = 1 ///< Format 1
} libe3_e2sm_dapp_ctrl_hdr_format_e;

/** Control header, format 1: where a control goes. */
typedef struct {
    libe3_e2sm_dapp_ctrl_hdr_format_e format; ///< Selected format
    uint32_t                          ran_function_id; ///< E3 RAN function id the control is for
    uint32_t                          dapp_id; ///< The dApp that is the target of the control
    bool                              has_timestamp; ///< true when `timestamp` is present
    int64_t                           timestamp; ///< When the xApp built the control, in nanoseconds on the realtime clock of its host
    bool                              has_sequence_id; ///< true when `sequence_id` is present
    int64_t                           sequence_id; ///< Id of the procedure, echoed from the indication header; absent or 0 means the RAN cannot defer the acknowledge
} libe3_e2sm_dapp_ctrl_hdr_t;

/* ---- control message ---- */

/** Formats of the ctrl msg. */
typedef enum {
    LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1 = 1 ///< Format 1
} libe3_e2sm_dapp_ctrl_msg_format_e;

/** Control message, format 1: the xApp control payload. */
typedef struct {
    libe3_e2sm_dapp_ctrl_msg_format_e format; ///< Selected format
    libe3_e2sm_dapp_bytes_t           data; /**< 1..32768 bytes; the opaque E3 payload. data-size is len. */
} libe3_e2sm_dapp_ctrl_msg_t;

/* ---- control outcome ---- */

/** Formats of the ctrl outcome. */
typedef enum {
    LIBE3_E2SM_DAPP_CTRL_OUTCOME_FORMAT_1 = 1 ///< Format 1
} libe3_e2sm_dapp_ctrl_outcome_format_e;

/** Control outcome, format 1: the answer to a control. */
typedef struct {
    libe3_e2sm_dapp_ctrl_outcome_format_e format; ///< Selected format
    bool                                  has_timestamp; ///< true when `timestamp` is present
    int64_t                               timestamp; ///< When the RAN built the outcome, in nanoseconds on its realtime clock
    bool                                  has_sequence_id; ///< true when `sequence_id` is present
    int64_t                               sequence_id; ///< Procedure this outcome answers, copied from the control header
    bool                                  has_e3_control_outcome; ///< true when `e3_control_outcome` is present
    libe3_e2sm_dapp_bytes_t               e3_control_outcome; /**< opaque; decoded by the SM named in the control's ran-function-id */
} libe3_e2sm_dapp_ctrl_outcome_t;

/* ---- RAN function definition ---- */

/** One report style of the RAN function definition. */
typedef struct {
    int64_t                    report_style_type; ///< Report style number (1 or 2)
    libe3_e2sm_dapp_bytes_t    name; /**< PrintableString, 1..150 */
    int64_t                    ind_hdr_format_type; ///< Indication header format the style produces
    int64_t                    ind_msg_format_type; ///< Indication message format the style produces
    bool                       has_dapp_e3_subscriptions; ///< true when `dapp_e3_subscriptions` is present
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions; ///< dApp E3 subscription map
} libe3_e2sm_dapp_report_style_t;

/** One control style of the RAN function definition. */
typedef struct {
    int64_t                    ctrl_style_type; ///< Control style number (1)
    libe3_e2sm_dapp_bytes_t    name; /**< PrintableString, 1..150 */
    int64_t                    ctrl_hdr_format_type; ///< Control header format the style uses
    int64_t                    ctrl_msg_format_type; ///< Control message format the style uses
    int64_t                    ctrl_outcome_format_type; ///< Control outcome format the style uses
    bool                       has_dapp_e3_subscriptions; ///< true when `dapp_e3_subscriptions` is present
    libe3_e2sm_dapp_sub_list_t dapp_e3_subscriptions; ///< dApp E3 subscription map
} libe3_e2sm_dapp_ctrl_style_t;

/** Name, OID and description of the RAN function. */
typedef struct {
    libe3_e2sm_dapp_bytes_t short_name;  /**< PrintableString, 1..150 */
    libe3_e2sm_dapp_bytes_t e2sm_oid;    /**< PrintableString, 1..1000 */
    libe3_e2sm_dapp_bytes_t description; /**< PrintableString, 1..150 */
    bool                    has_instance; ///< true when `instance` is present
    int64_t                 instance; ///< Instance number; not used, a sender shall not set it
} libe3_e2sm_dapp_ran_function_name_t;

/** RAN function definition, announced in E2 Setup and RIC Service Update. */
typedef struct {
    libe3_e2sm_dapp_ran_function_name_t name; ///< Name of the RAN function
    bool                                has_event_trigger; /**< the event trigger item carries no fields */
    bool                                has_report; ///< true when the report section is present
    size_t                              n_report_styles; /**< 1..2 when has_report */
    libe3_e2sm_dapp_report_style_t*     report_styles; ///< Array of `n_report_styles` report styles
    bool                                has_ctrl; ///< true when the control section is present
    size_t                              n_ctrl_styles; /**< 1 when has_ctrl */
    libe3_e2sm_dapp_ctrl_style_t*       ctrl_styles; ///< Array of `n_ctrl_styles` control styles
} libe3_e2sm_dapp_ran_function_definition_t;

/* ---- codec ---- */

/** Encode a event trigger as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_event_trigger(const libe3_e2sm_dapp_event_trigger_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a event trigger from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_event_trigger(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_event_trigger(const uint8_t* buf, size_t len, libe3_e2sm_dapp_event_trigger_t* out);
/** Release everything a decode of a event trigger allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_event_trigger(libe3_e2sm_dapp_event_trigger_t* v);

/** Encode a action definition as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_action_definition(const libe3_e2sm_dapp_action_definition_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a action definition from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_action_definition(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_action_definition(const uint8_t* buf, size_t len, libe3_e2sm_dapp_action_definition_t* out);
/** Release everything a decode of a action definition allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_action_definition(libe3_e2sm_dapp_action_definition_t* v);

/** Encode a indication header as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ind_hdr(const libe3_e2sm_dapp_ind_hdr_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a indication header from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ind_hdr(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ind_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_hdr_t* out);
/** Release everything a decode of a indication header allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ind_hdr(libe3_e2sm_dapp_ind_hdr_t* v);

/** Encode a indication message as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ind_msg(const libe3_e2sm_dapp_ind_msg_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a indication message from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ind_msg(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ind_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_msg_t* out);
/** Release everything a decode of a indication message allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ind_msg(libe3_e2sm_dapp_ind_msg_t* v);

/** Encode a control header as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ctrl_hdr(const libe3_e2sm_dapp_ctrl_hdr_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a control header from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ctrl_hdr(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ctrl_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_hdr_t* out);
/** Release everything a decode of a control header allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ctrl_hdr(libe3_e2sm_dapp_ctrl_hdr_t* v);

/** Encode a control message as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ctrl_msg(const libe3_e2sm_dapp_ctrl_msg_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a control message from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ctrl_msg(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ctrl_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_msg_t* out);
/** Release everything a decode of a control message allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ctrl_msg(libe3_e2sm_dapp_ctrl_msg_t* v);

/** Encode a control outcome as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ctrl_outcome(const libe3_e2sm_dapp_ctrl_outcome_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a control outcome from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ctrl_outcome(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ctrl_outcome(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_outcome_t* out);
/** Release everything a decode of a control outcome allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ctrl_outcome(libe3_e2sm_dapp_ctrl_outcome_t* v);

/** Encode a RAN function definition as APER.
 * @param in value to encode, only read
 * @param out receives the bytes; release them with libe3_e2sm_dapp_bytes_free()
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_encode_ran_function_definition(const libe3_e2sm_dapp_ran_function_definition_t* in, libe3_e2sm_dapp_bytes_t* out);
/** Decode a RAN function definition from APER bytes.
 * @param buf bytes to decode
 * @param len number of bytes
 * @param out receives the value; release it with libe3_e2sm_dapp_free_ran_function_definition(). Zeroed on failure.
 * @return 0 on success, an e3_error_t on failure */
e3_error_t libe3_e2sm_dapp_decode_ran_function_definition(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ran_function_definition_t* out);
/** Release everything a decode of a RAN function definition allocated, and zero the struct. Freeing twice is harmless.
 * @param v value to release, may be NULL */
void       libe3_e2sm_dapp_free_ran_function_definition(libe3_e2sm_dapp_ran_function_definition_t* v);

#ifdef __cplusplus
}
#endif

#endif /* LIBE3_E2SM_DAPP_C_H */
