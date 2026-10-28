/**
 * @file e2sm_dapp.cpp
 * @brief E2SM-DAPP APER codec: the C++ structs of e2sm_dapp.hpp <-> asn1c <-> bytes.
 *
 * Encoding builds an asn1c struct from the C++ one, checks every constraint of
 * the grammar by hand first (asn1c does not reject an out-of-range value
 * cleanly, it can fault inside the encoder or silently use an extension
 * encoding), runs asn_check_constraints as a second net, and encodes APER
 * aligned into a growing std::vector. Decoding runs aper_decode, requires
 * RC_OK, and converts back, applying the same constraints. The asn1c struct is
 * freed on every path.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e2sm_dapp.hpp"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "DAppE3Subscription-Item.h"
#include "DAppE3Subscription-List.h"
#include "E2SM-DAPP-ActionDefinition-Format1.h"
#include "E2SM-DAPP-ActionDefinition.h"
#include "E2SM-DAPP-ControlHeader-Format1.h"
#include "E2SM-DAPP-ControlHeader.h"
#include "E2SM-DAPP-ControlMessage-Format1.h"
#include "E2SM-DAPP-ControlMessage.h"
#include "E2SM-DAPP-ControlOutcome-Format1.h"
#include "E2SM-DAPP-ControlOutcome.h"
#include "E2SM-DAPP-EventTrigger-Format1.h"
#include "E2SM-DAPP-EventTrigger.h"
#include "E2SM-DAPP-IndicationHeader-Format1.h"
#include "E2SM-DAPP-IndicationHeader-Format2.h"
#include "E2SM-DAPP-IndicationHeader.h"
#include "E2SM-DAPP-IndicationMessage-Format1.h"
#include "E2SM-DAPP-IndicationMessage-Format2.h"
#include "E2SM-DAPP-IndicationMessage.h"
#include "E2SM-DAPP-RANFunctionDefinition.h"
#include "RANFunctionDefinition-Control-Item.h"
#include "RANFunctionDefinition-Control.h"
#include "RANFunctionDefinition-EventTrigger.h"
#include "RANFunctionDefinition-Report-Item.h"
#include "RANFunctionDefinition-Report.h"
#include "RANfunction-Name.h"
#include "aper_decoder.h"
#include "asn_application.h"
#include "constraints.h"

namespace libe3 {
namespace e2sm_dapp {

// ---------------------------------------------------------------------------
// operator== of the types whose definition is not in the header. An OPTIONAL
// field that is absent has no value, so its payload is compared only when the
// *_present flag is set; `ext` is never compared.
// ---------------------------------------------------------------------------

bool ran_function_name_s::operator==(const ran_function_name_s& o) const
{
    return ran_function_instance_present == o.ran_function_instance_present &&
           ran_function_short_name == o.ran_function_short_name &&
           ran_function_e2sm_o_id == o.ran_function_e2sm_o_id &&
           ran_function_description == o.ran_function_description &&
           (!ran_function_instance_present || ran_function_instance == o.ran_function_instance);
}

bool ran_function_definition_report_item_s::operator==(const ran_function_definition_report_item_s& o) const
{
    return dapp_e3_subscriptions_present == o.dapp_e3_subscriptions_present &&
           ric_report_style_type == o.ric_report_style_type && ric_report_style_name == o.ric_report_style_name &&
           ric_ind_hdr_format_type == o.ric_ind_hdr_format_type &&
           ric_ind_msg_format_type == o.ric_ind_msg_format_type &&
           (!dapp_e3_subscriptions_present || dapp_e3_subscriptions == o.dapp_e3_subscriptions);
}

bool ran_function_definition_ctrl_item_s::operator==(const ran_function_definition_ctrl_item_s& o) const
{
    return dapp_e3_subscriptions_present == o.dapp_e3_subscriptions_present &&
           ric_ctrl_style_type == o.ric_ctrl_style_type && ric_ctrl_style_name == o.ric_ctrl_style_name &&
           ric_ctrl_hdr_format_type == o.ric_ctrl_hdr_format_type &&
           ric_ctrl_msg_format_type == o.ric_ctrl_msg_format_type &&
           ric_ctrl_outcome_format_type == o.ric_ctrl_outcome_format_type &&
           (!dapp_e3_subscriptions_present || dapp_e3_subscriptions == o.dapp_e3_subscriptions);
}

bool e2sm_dapp_ran_function_definition_s::operator==(const e2sm_dapp_ran_function_definition_s& o) const
{
    return ran_function_definition_event_trigger_present == o.ran_function_definition_event_trigger_present &&
           ran_function_definition_report_present == o.ran_function_definition_report_present &&
           ran_function_definition_ctrl_present == o.ran_function_definition_ctrl_present &&
           ran_function_name == o.ran_function_name &&
           (!ran_function_definition_report_present ||
            ran_function_definition_report == o.ran_function_definition_report) &&
           (!ran_function_definition_ctrl_present || ran_function_definition_ctrl == o.ran_function_definition_ctrl);
}

bool e2sm_dapp_ind_hdr_format1_s::operator==(const e2sm_dapp_ind_hdr_format1_s& o) const
{
    return node_cu_du_id_present == o.node_cu_du_id_present && timestamp_present == o.timestamp_present &&
           sequence_id_present == o.sequence_id_present && ran_function_id == o.ran_function_id &&
           dapp_id == o.dapp_id && node_type == o.node_type && node_plmn_id == o.node_plmn_id &&
           node_nb_id == o.node_nb_id && (!node_cu_du_id_present || node_cu_du_id == o.node_cu_du_id) &&
           (!timestamp_present || timestamp == o.timestamp) && (!sequence_id_present || sequence_id == o.sequence_id);
}

bool e2sm_dapp_ind_hdr_format2_s::operator==(const e2sm_dapp_ind_hdr_format2_s& o) const
{
    return node_cu_du_id_present == o.node_cu_du_id_present && timestamp_present == o.timestamp_present &&
           sequence_id_present == o.sequence_id_present && node_type == o.node_type &&
           node_plmn_id == o.node_plmn_id && node_nb_id == o.node_nb_id &&
           (!node_cu_du_id_present || node_cu_du_id == o.node_cu_du_id) &&
           (!timestamp_present || timestamp == o.timestamp) && (!sequence_id_present || sequence_id == o.sequence_id);
}

bool e2sm_dapp_ctrl_hdr_format1_s::operator==(const e2sm_dapp_ctrl_hdr_format1_s& o) const
{
    return timestamp_present == o.timestamp_present && sequence_id_present == o.sequence_id_present &&
           ran_function_id == o.ran_function_id && dapp_id == o.dapp_id &&
           (!timestamp_present || timestamp == o.timestamp) && (!sequence_id_present || sequence_id == o.sequence_id);
}

bool e2sm_dapp_ctrl_outcome_format1_s::operator==(const e2sm_dapp_ctrl_outcome_format1_s& o) const
{
    return timestamp_present == o.timestamp_present && sequence_id_present == o.sequence_id_present &&
           e3_ctrl_outcome_present == o.e3_ctrl_outcome_present &&
           (!timestamp_present || timestamp == o.timestamp) && (!sequence_id_present || sequence_id == o.sequence_id) &&
           (!e3_ctrl_outcome_present || e3_ctrl_outcome == o.e3_ctrl_outcome);
}

namespace {

// ---------------------------------------------------------------------------
// Grammar limits.
// ---------------------------------------------------------------------------

constexpr uint64_t k_uint32_max     = 0xFFFFFFFFULL;
constexpr size_t   k_max_payload    = static_cast<size_t>(max_e2sm_dapp_ind_msg_size);  // == ctrl size
constexpr size_t   k_max_subs       = static_cast<size_t>(maxnoof_dapps);
constexpr size_t   k_max_rf_per_sub = static_cast<size_t>(maxnoof_subscribed_e3_ran_functions);
constexpr size_t   k_max_report_sty = static_cast<size_t>(maxnoof_dapp_report_styles);
constexpr size_t   k_max_ctrl_sty   = static_cast<size_t>(maxnoof_dapp_control_styles);
constexpr size_t   k_name_max       = 150;
constexpr size_t   k_oid_max        = 1000;

static_assert(max_e2sm_dapp_ind_msg_size == max_e2sm_dapp_ctrl_msg_size, "one payload bound is used for both");

// ---------------------------------------------------------------------------
// Value checks shared by encode (reject) and decode (reject).
// ---------------------------------------------------------------------------

/// INTEGER (0..4294967295): ids, node-nb-id, node-cu-du-id, subscribed function ids.
bool in_u32(uint64_t v)
{
    return v <= k_uint32_max;
}

/// Unconstrained INTEGER, held by asn1c in a `long`.
bool fits_long(int64_t v)
{
    return static_cast<int64_t>(static_cast<long>(v)) == v;
}

/// The PrintableString alphabet: letters, digits, space and ' ( ) + , - . / : = ?
bool is_printable_char(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
        case ' ':
        case '\'':
        case '(':
        case ')':
        case '+':
        case ',':
        case '-':
        case '.':
        case '/':
        case ':':
        case '=':
        case '?':
            return true;
        default:
            return false;
    }
}

bool valid_printable(const char* s, size_t n, size_t lo, size_t hi)
{
    if (n < lo || n > hi) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (!is_printable_char(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// asn1c struct ownership.
// ---------------------------------------------------------------------------

template <class T>
T* zalloc()
{
    return static_cast<T*>(std::calloc(1, sizeof(T)));
}

/// Owns an asn1c struct and frees it, children included, on every exit path.
template <class T>
class Owned
{
public:
    explicit Owned(const asn_TYPE_descriptor_t& td) : td_(td) {}
    ~Owned() { reset(nullptr); }
    Owned(const Owned&)            = delete;
    Owned& operator=(const Owned&) = delete;

    /// Allocate the zeroed struct. False when memory is exhausted.
    bool alloc()
    {
        reset(zalloc<T>());
        return p_ != nullptr;
    }
    /// Take over a struct that asn1c allocated (decode, also when it failed half way).
    void adopt(T* p) { reset(p); }

    T* get() const { return p_; }
    T* operator->() const { return p_; }

private:
    void reset(T* p)
    {
        if (p_ != nullptr) {
            td_.op->free_struct(&td_, p_, ASFM_FREE_EVERYTHING);
        }
        p_ = p;
    }

    const asn_TYPE_descriptor_t& td_;
    T*                           p_ = nullptr;
};

/// OCTET STRING <- bytes. Never hands asn1c a null pointer, which would mean "clear".
bool set_octets(OCTET_STRING_t& dst, const uint8_t* data, size_t n)
{
    if (n > static_cast<size_t>(INT32_MAX)) {
        return false;
    }
    static const char k_empty[1] = {0};
    const char*       p          = (n == 0) ? k_empty : reinterpret_cast<const char*>(data);
    return OCTET_STRING_fromBuf(&dst, p, static_cast<int>(n)) == 0;
}

bool set_printable(PrintableString_t& dst, const std::string& s, size_t lo, size_t hi)
{
    if (!valid_printable(s.data(), s.size(), lo, hi)) {
        return false;
    }
    return set_octets(dst, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

/// OPTIONAL INTEGER held in a `long` (timestamp, sequence-id, instance).
bool set_opt_long(long*& dst, bool present, int64_t v)
{
    if (!present) {
        return true;
    }
    if (!fits_long(v)) {
        return false;
    }
    dst = zalloc<long>();
    if (dst == nullptr) {
        return false;
    }
    *dst = static_cast<long>(v);
    return true;
}

bool set_u32(unsigned long& dst, uint64_t v)
{
    if (!in_u32(v)) {
        return false;
    }
    dst = static_cast<unsigned long>(v);
    return true;
}

bool set_long(long& dst, int64_t v)
{
    if (!fits_long(v)) {
        return false;
    }
    dst = static_cast<long>(v);
    return true;
}

// ---------------------------------------------------------------------------
// C++ -> asn1c builders. Each returns false for a value outside the grammar or
// for exhausted memory; the caller's Owned frees whatever was already attached.
// ---------------------------------------------------------------------------

bool build_subs(const dapp_e3_subscription_list_l& src, DAppE3Subscription_List_t& dst)
{
    if (src.size() > k_max_subs) {
        return false;
    }
    for (const auto& s : src) {
        if (s.subscribed_e3_ran_functions.size() < 1 || s.subscribed_e3_ran_functions.size() > k_max_rf_per_sub) {
            return false;
        }
        auto* item = zalloc<DAppE3Subscription_Item_t>();
        if (item == nullptr) {
            return false;
        }
        if (asn_sequence_add(&dst, item) != 0) {
            std::free(item);
            return false;
        }
        if (!set_u32(item->dapp_id, s.dapp_id)) {
            return false;
        }
        for (uint64_t rf : s.subscribed_e3_ran_functions) {
            auto* v = zalloc<SubscribedE3RANFunction_ID_t>();
            if (v == nullptr) {
                return false;
            }
            if (asn_sequence_add(&item->subscribed_e3_ran_functions, v) != 0) {
                std::free(v);
                return false;
            }
            if (!set_u32(*v, rf)) {
                return false;
            }
        }
    }
    return true;
}

/// An OPTIONAL DAppE3Subscription-List behind a pointer.
bool build_opt_subs(bool present, const dapp_e3_subscription_list_l& src, DAppE3Subscription_List_t*& dst)
{
    if (!present) {
        return true;
    }
    dst = zalloc<DAppE3Subscription_List_t>();
    return dst != nullptr && build_subs(src, *dst);
}

bool build(const ran_function_name_s& s, RANfunction_Name_t& d)
{
    return set_printable(d.ranFunction_ShortName, s.ran_function_short_name.to_string(), 1, k_name_max) &&
           set_printable(d.ranFunction_E2SM_OID, s.ran_function_e2sm_o_id.to_string(), 1, k_oid_max) &&
           set_printable(d.ranFunction_Description, s.ran_function_description.to_string(), 1, k_name_max) &&
           set_opt_long(d.ranFunction_Instance, s.ran_function_instance_present, s.ran_function_instance);
}

bool build(const ran_function_definition_report_item_s& s, RANFunctionDefinition_Report_Item_t& d)
{
    return set_long(d.ric_ReportStyle_Type, s.ric_report_style_type) &&
           set_printable(d.ric_ReportStyle_Name, s.ric_report_style_name.to_string(), 1, k_name_max) &&
           set_long(d.ric_IndicationHeaderFormat_Type, s.ric_ind_hdr_format_type) &&
           set_long(d.ric_IndicationMessageFormat_Type, s.ric_ind_msg_format_type) &&
           build_opt_subs(s.dapp_e3_subscriptions_present, s.dapp_e3_subscriptions, d.dappE3Subscriptions);
}

bool build(const ran_function_definition_ctrl_item_s& s, RANFunctionDefinition_Control_Item_t& d)
{
    return set_long(d.ric_ControlStyle_Type, s.ric_ctrl_style_type) &&
           set_printable(d.ric_ControlStyle_Name, s.ric_ctrl_style_name.to_string(), 1, k_name_max) &&
           set_long(d.ric_ControlHeaderFormat_Type, s.ric_ctrl_hdr_format_type) &&
           set_long(d.ric_ControlMessageFormat_Type, s.ric_ctrl_msg_format_type) &&
           set_long(d.ric_ControlOutcomeFormat_Type, s.ric_ctrl_outcome_format_type) &&
           build_opt_subs(s.dapp_e3_subscriptions_present, s.dapp_e3_subscriptions, d.dappE3Subscriptions);
}

bool build(const ran_function_definition_report_s& s, RANFunctionDefinition_Report_t& d)
{
    if (s.ric_report_style_list.size() < 1 || s.ric_report_style_list.size() > k_max_report_sty) {
        return false;
    }
    for (const auto& it : s.ric_report_style_list) {
        auto* item = zalloc<RANFunctionDefinition_Report_Item_t>();
        if (item == nullptr) {
            return false;
        }
        if (asn_sequence_add(&d.ric_ReportStyle_List, item) != 0) {
            std::free(item);
            return false;
        }
        if (!build(it, *item)) {
            return false;
        }
    }
    return true;
}

bool build(const ran_function_definition_ctrl_s& s, RANFunctionDefinition_Control_t& d)
{
    if (s.ric_ctrl_style_list.size() < 1 || s.ric_ctrl_style_list.size() > k_max_ctrl_sty) {
        return false;
    }
    for (const auto& it : s.ric_ctrl_style_list) {
        auto* item = zalloc<RANFunctionDefinition_Control_Item_t>();
        if (item == nullptr) {
            return false;
        }
        if (asn_sequence_add(&d.ric_ControlStyle_List, item) != 0) {
            std::free(item);
            return false;
        }
        if (!build(it, *item)) {
            return false;
        }
    }
    return true;
}

bool build(const e2sm_dapp_ran_function_definition_s& s, E2SM_DAPP_RANFunctionDefinition_t& d)
{
    if (!build(s.ran_function_name, d.ranFunction_Name)) {
        return false;
    }
    if (s.ran_function_definition_event_trigger_present) {
        d.ranFunctionDefinition_EventTrigger = zalloc<RANFunctionDefinition_EventTrigger_t>();
        if (d.ranFunctionDefinition_EventTrigger == nullptr) {
            return false;
        }
    }
    if (s.ran_function_definition_report_present) {
        d.ranFunctionDefinition_Report = zalloc<RANFunctionDefinition_Report_t>();
        if (d.ranFunctionDefinition_Report == nullptr ||
            !build(s.ran_function_definition_report, *d.ranFunctionDefinition_Report)) {
            return false;
        }
    }
    if (s.ran_function_definition_ctrl_present) {
        d.ranFunctionDefinition_Control = zalloc<RANFunctionDefinition_Control_t>();
        if (d.ranFunctionDefinition_Control == nullptr ||
            !build(s.ran_function_definition_ctrl, *d.ranFunctionDefinition_Control)) {
            return false;
        }
    }
    return true;
}

bool build(const e2sm_dapp_ind_hdr_format1_s& s, E2SM_DAPP_IndicationHeader_Format1_t& d)
{
    if (!set_u32(d.ran_function_id, s.ran_function_id) || !set_u32(d.dapp_id, s.dapp_id) ||
        !set_u32(d.node_nb_id, s.node_nb_id)) {
        return false;
    }
    d.node_type = static_cast<long>(s.node_type);
    if (!set_octets(d.node_plmn_id, s.node_plmn_id.data(), s.node_plmn_id.size())) {
        return false;
    }
    if (s.node_cu_du_id_present) {
        d.node_cu_du_id = zalloc<unsigned long>();
        if (d.node_cu_du_id == nullptr || !set_u32(*d.node_cu_du_id, s.node_cu_du_id)) {
            return false;
        }
    }
    return set_opt_long(d.timestamp, s.timestamp_present, s.timestamp) &&
           set_opt_long(d.sequence_id, s.sequence_id_present, s.sequence_id);
}

bool build(const e2sm_dapp_ind_hdr_format2_s& s, E2SM_DAPP_IndicationHeader_Format2_t& d)
{
    if (!set_u32(d.node_nb_id, s.node_nb_id)) {
        return false;
    }
    d.node_type = static_cast<long>(s.node_type);
    if (!set_octets(d.node_plmn_id, s.node_plmn_id.data(), s.node_plmn_id.size())) {
        return false;
    }
    if (s.node_cu_du_id_present) {
        d.node_cu_du_id = zalloc<unsigned long>();
        if (d.node_cu_du_id == nullptr || !set_u32(*d.node_cu_du_id, s.node_cu_du_id)) {
            return false;
        }
    }
    return set_opt_long(d.timestamp, s.timestamp_present, s.timestamp) &&
           set_opt_long(d.sequence_id, s.sequence_id_present, s.sequence_id);
}

/// data-size INTEGER (0..32768) and data OCTET STRING (SIZE(1..32768)), as in both message formats.
bool build_data(uint16_t data_size, const std::vector<uint8_t>& data, long& dst_size, OCTET_STRING_t& dst_data)
{
    if (data_size > k_max_payload || data.empty() || data.size() > k_max_payload) {
        return false;
    }
    dst_size = static_cast<long>(data_size);
    return set_octets(dst_data, data.data(), data.size());
}

bool build(const e2sm_dapp_ctrl_hdr_format1_s& s, E2SM_DAPP_ControlHeader_Format1_t& d)
{
    return set_u32(d.ran_function_id, s.ran_function_id) && set_u32(d.dapp_id, s.dapp_id) &&
           set_opt_long(d.timestamp, s.timestamp_present, s.timestamp) &&
           set_opt_long(d.sequence_id, s.sequence_id_present, s.sequence_id);
}

bool build(const e2sm_dapp_ctrl_outcome_format1_s& s, E2SM_DAPP_ControlOutcome_Format1_t& d)
{
    if (!set_opt_long(d.timestamp, s.timestamp_present, s.timestamp) ||
        !set_opt_long(d.sequence_id, s.sequence_id_present, s.sequence_id)) {
        return false;
    }
    if (s.e3_ctrl_outcome_present) {
        d.e3_control_outcome = zalloc<OCTET_STRING_t>();
        if (d.e3_control_outcome == nullptr ||
            !set_octets(*d.e3_control_outcome, s.e3_ctrl_outcome.data(), s.e3_ctrl_outcome.size())) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Encoding to a growing buffer.
// ---------------------------------------------------------------------------

struct Sink {
    std::vector<uint8_t>* out;
    bool                  failed;
};

/// asn1c output callback. It never returns an error (asn_encode would treat that
/// as a malformed struct): an allocation failure is recorded and checked after.
int sink_cb(const void* data, size_t size, void* key)
{
    auto* sink = static_cast<Sink*>(key);
    if (sink->failed) {
        return 0;
    }
    try {
        const auto* p = static_cast<const uint8_t*>(data);
        sink->out->insert(sink->out->end(), p, p + size);
    } catch (...) {
        sink->failed = true;
    }
    return 0;
}

asn_code encode_pdu(const asn_TYPE_descriptor_t& td, const void* sptr, std::vector<uint8_t>& out)
{
    char   errbuf[256];
    size_t errlen = sizeof(errbuf);
    if (asn_check_constraints(&td, sptr, errbuf, &errlen) != 0) {
        return ASN_ERROR_ENCODE_FAIL;
    }

    Sink           sink{&out, false};
    asn_enc_rval_t er = asn_encode(nullptr, ATS_ALIGNED_BASIC_PER, &td, sptr, sink_cb, &sink);
    if (er.encoded < 0 || sink.failed || out.size() != static_cast<size_t>(er.encoded) || out.empty()) {
        out.clear();
        return ASN_ERROR_ENCODE_FAIL;
    }
    return ASN_SUCCESS;
}

/// The shared body of every pack(): `fill` builds the asn1c struct, which is then encoded and freed.
template <class T, class Fill>
asn_code pack_pdu(const asn_TYPE_descriptor_t& td, std::vector<uint8_t>& out, Fill fill)
{
    out.clear();
    try {
        Owned<T> pdu(td);
        if (!pdu.alloc() || !fill(*pdu.get())) {
            return ASN_ERROR_ENCODE_FAIL;
        }
        return encode_pdu(td, pdu.get(), out);
    } catch (...) {
        out.clear();
        return ASN_ERROR_ENCODE_FAIL;
    }
}

// ---------------------------------------------------------------------------
// asn1c -> C++ converters. Anything outside the grammar's root constraints is
// rejected, so that a struct that unpack() returns can be pack()ed again.
// ---------------------------------------------------------------------------

bool get_u32(unsigned long v, uint64_t& dst)
{
    if (static_cast<uint64_t>(v) > k_uint32_max) {
        return false;
    }
    dst = static_cast<uint64_t>(v);
    return true;
}

bool get_printable(const PrintableString_t& s, size_t lo, size_t hi, std::string& dst)
{
    if (s.buf == nullptr && s.size != 0) {
        return false;
    }
    const char* p = reinterpret_cast<const char*>(s.buf);
    if (!valid_printable(p, s.size, lo, hi)) {
        return false;
    }
    dst.assign(p, s.size);
    return true;
}

bool get_subs(const DAppE3Subscription_List_t& src, dapp_e3_subscription_list_l& dst)
{
    if (src.list.count < 0 || static_cast<size_t>(src.list.count) > k_max_subs) {
        return false;
    }
    dst.clear();
    dst.resize(static_cast<size_t>(src.list.count));
    for (int i = 0; i < src.list.count; ++i) {
        const DAppE3Subscription_Item_t* item = src.list.array[i];
        if (item == nullptr) {
            return false;
        }
        auto&     d     = dst[static_cast<size_t>(i)];
        const int count = item->subscribed_e3_ran_functions.list.count;
        if (!get_u32(item->dapp_id, d.dapp_id) || count < 1 || static_cast<size_t>(count) > k_max_rf_per_sub) {
            return false;
        }
        d.subscribed_e3_ran_functions.resize(static_cast<size_t>(count));
        for (int j = 0; j < count; ++j) {
            const SubscribedE3RANFunction_ID_t* rf = item->subscribed_e3_ran_functions.list.array[j];
            if (rf == nullptr || !get_u32(*rf, d.subscribed_e3_ran_functions[static_cast<size_t>(j)])) {
                return false;
            }
        }
    }
    return true;
}

bool get_opt_subs(const DAppE3Subscription_List_t* src, bool& present, dapp_e3_subscription_list_l& dst)
{
    present = (src != nullptr);
    return !present || get_subs(*src, dst);
}

bool get_node_type(long v, uint8_t& dst)
{
    if (v < 0 || v > 255) {
        return false;
    }
    dst = static_cast<uint8_t>(v);
    return true;
}

bool get_plmn(const OCTET_STRING_t& s, fixed_octstring<3, true>& dst)
{
    if (s.size != 3 || s.buf == nullptr) {
        return false;
    }
    std::memcpy(dst.data(), s.buf, 3);
    return true;
}

bool get_opt_u32(const unsigned long* src, bool& present, uint64_t& dst)
{
    present = (src != nullptr);
    return !present || get_u32(*src, dst);
}

void get_opt_long(const long* src, bool& present, int64_t& dst)
{
    present = (src != nullptr);
    if (present) {
        dst = static_cast<int64_t>(*src);
    }
}

bool get_data(long data_size, const OCTET_STRING_t& data, uint16_t& dst_size, std::vector<uint8_t>& dst_data)
{
    if (data_size < 0 || static_cast<unsigned long>(data_size) > k_max_payload || data.buf == nullptr ||
        data.size < 1 || data.size > k_max_payload) {
        return false;
    }
    dst_size = static_cast<uint16_t>(data_size);
    dst_data.assign(data.buf, data.buf + data.size);
    return true;
}

bool from_asn(const RANfunction_Name_t& s, ran_function_name_s& d)
{
    std::string a, b, c;
    if (!get_printable(s.ranFunction_ShortName, 1, k_name_max, a) ||
        !get_printable(s.ranFunction_E2SM_OID, 1, k_oid_max, b) ||
        !get_printable(s.ranFunction_Description, 1, k_name_max, c)) {
        return false;
    }
    d.ran_function_short_name.from_string(a);
    d.ran_function_e2sm_o_id.from_string(b);
    d.ran_function_description.from_string(c);
    get_opt_long(s.ranFunction_Instance, d.ran_function_instance_present, d.ran_function_instance);
    return true;
}

bool from_asn(const RANFunctionDefinition_Report_Item_t& s, ran_function_definition_report_item_s& d)
{
    std::string name;
    if (!get_printable(s.ric_ReportStyle_Name, 1, k_name_max, name)) {
        return false;
    }
    d.ric_report_style_type = s.ric_ReportStyle_Type;
    d.ric_report_style_name.from_string(name);
    d.ric_ind_hdr_format_type = s.ric_IndicationHeaderFormat_Type;
    d.ric_ind_msg_format_type = s.ric_IndicationMessageFormat_Type;
    return get_opt_subs(s.dappE3Subscriptions, d.dapp_e3_subscriptions_present, d.dapp_e3_subscriptions);
}

bool from_asn(const RANFunctionDefinition_Control_Item_t& s, ran_function_definition_ctrl_item_s& d)
{
    std::string name;
    if (!get_printable(s.ric_ControlStyle_Name, 1, k_name_max, name)) {
        return false;
    }
    d.ric_ctrl_style_type = s.ric_ControlStyle_Type;
    d.ric_ctrl_style_name.from_string(name);
    d.ric_ctrl_hdr_format_type     = s.ric_ControlHeaderFormat_Type;
    d.ric_ctrl_msg_format_type     = s.ric_ControlMessageFormat_Type;
    d.ric_ctrl_outcome_format_type = s.ric_ControlOutcomeFormat_Type;
    return get_opt_subs(s.dappE3Subscriptions, d.dapp_e3_subscriptions_present, d.dapp_e3_subscriptions);
}

bool from_asn(const E2SM_DAPP_RANFunctionDefinition_t& s, e2sm_dapp_ran_function_definition_s& d)
{
    if (!from_asn(s.ranFunction_Name, d.ran_function_name)) {
        return false;
    }
    d.ran_function_definition_event_trigger_present = (s.ranFunctionDefinition_EventTrigger != nullptr);

    d.ran_function_definition_report_present = (s.ranFunctionDefinition_Report != nullptr);
    if (d.ran_function_definition_report_present) {
        const auto& l = s.ranFunctionDefinition_Report->ric_ReportStyle_List.list;
        if (l.count < 1 || static_cast<size_t>(l.count) > k_max_report_sty) {
            return false;
        }
        d.ran_function_definition_report.ric_report_style_list.resize(static_cast<size_t>(l.count));
        for (int i = 0; i < l.count; ++i) {
            if (l.array[i] == nullptr ||
                !from_asn(*l.array[i], d.ran_function_definition_report.ric_report_style_list[static_cast<size_t>(i)])) {
                return false;
            }
        }
    }

    d.ran_function_definition_ctrl_present = (s.ranFunctionDefinition_Control != nullptr);
    if (d.ran_function_definition_ctrl_present) {
        const auto& l = s.ranFunctionDefinition_Control->ric_ControlStyle_List.list;
        if (l.count < 1 || static_cast<size_t>(l.count) > k_max_ctrl_sty) {
            return false;
        }
        d.ran_function_definition_ctrl.ric_ctrl_style_list.resize(static_cast<size_t>(l.count));
        for (int i = 0; i < l.count; ++i) {
            if (l.array[i] == nullptr ||
                !from_asn(*l.array[i], d.ran_function_definition_ctrl.ric_ctrl_style_list[static_cast<size_t>(i)])) {
                return false;
            }
        }
    }
    return true;
}

bool from_asn(const E2SM_DAPP_IndicationHeader_Format1_t& s, e2sm_dapp_ind_hdr_format1_s& d)
{
    if (!get_u32(s.ran_function_id, d.ran_function_id) || !get_u32(s.dapp_id, d.dapp_id) ||
        !get_node_type(s.node_type, d.node_type) || !get_plmn(s.node_plmn_id, d.node_plmn_id) ||
        !get_u32(s.node_nb_id, d.node_nb_id) ||
        !get_opt_u32(s.node_cu_du_id, d.node_cu_du_id_present, d.node_cu_du_id)) {
        return false;
    }
    get_opt_long(s.timestamp, d.timestamp_present, d.timestamp);
    get_opt_long(s.sequence_id, d.sequence_id_present, d.sequence_id);
    return true;
}

bool from_asn(const E2SM_DAPP_IndicationHeader_Format2_t& s, e2sm_dapp_ind_hdr_format2_s& d)
{
    if (!get_node_type(s.node_type, d.node_type) || !get_plmn(s.node_plmn_id, d.node_plmn_id) ||
        !get_u32(s.node_nb_id, d.node_nb_id) ||
        !get_opt_u32(s.node_cu_du_id, d.node_cu_du_id_present, d.node_cu_du_id)) {
        return false;
    }
    get_opt_long(s.timestamp, d.timestamp_present, d.timestamp);
    get_opt_long(s.sequence_id, d.sequence_id_present, d.sequence_id);
    return true;
}

bool from_asn(const E2SM_DAPP_ControlHeader_Format1_t& s, e2sm_dapp_ctrl_hdr_format1_s& d)
{
    if (!get_u32(s.ran_function_id, d.ran_function_id) || !get_u32(s.dapp_id, d.dapp_id)) {
        return false;
    }
    get_opt_long(s.timestamp, d.timestamp_present, d.timestamp);
    get_opt_long(s.sequence_id, d.sequence_id_present, d.sequence_id);
    return true;
}

bool from_asn(const E2SM_DAPP_ControlOutcome_Format1_t& s, e2sm_dapp_ctrl_outcome_format1_s& d)
{
    get_opt_long(s.timestamp, d.timestamp_present, d.timestamp);
    get_opt_long(s.sequence_id, d.sequence_id_present, d.sequence_id);
    d.e3_ctrl_outcome_present = (s.e3_control_outcome != nullptr);
    if (d.e3_ctrl_outcome_present) {
        if (s.e3_control_outcome->buf == nullptr && s.e3_control_outcome->size != 0) {
            return false;
        }
        d.e3_ctrl_outcome.from_bytes(s.e3_control_outcome->buf, s.e3_control_outcome->size);
    }
    return true;
}

/// The shared body of every unpack(): decode, convert into a temporary, move it in on success only.
template <class T, class Cpp, class Convert>
asn_code unpack_pdu(const asn_TYPE_descriptor_t& td, const uint8_t* data, size_t len, Cpp& self, Convert convert)
{
    if (data == nullptr || len == 0) {
        return ASN_ERROR_DECODE_FAIL;
    }
    try {
        Owned<T> pdu(td);
        void*    p  = nullptr;
        auto     rv = aper_decode(nullptr, &td, &p, data, len, 0, 0);
        // asn1c leaves a half-built struct behind on failure: always take it over to free it.
        pdu.adopt(static_cast<T*>(p));
        if (rv.code != RC_OK || pdu.get() == nullptr) {
            return ASN_ERROR_DECODE_FAIL;
        }
        Cpp tmp;
        if (!convert(*pdu.get(), tmp)) {
            return ASN_ERROR_DECODE_FAIL;
        }
        self = std::move(tmp);
        return ASN_SUCCESS;
    } catch (...) {
        return ASN_ERROR_DECODE_FAIL;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Event trigger
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_event_trigger_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_EventTrigger_t>(asn_DEF_E2SM_DAPP_EventTrigger, out, [](E2SM_DAPP_EventTrigger_t& d) {
        d.ric_eventTrigger_formats.present = E2SM_DAPP_EventTrigger__ric_eventTrigger_formats_PR_eventTrigger_Format1;
        d.ric_eventTrigger_formats.choice.eventTrigger_Format1 = zalloc<E2SM_DAPP_EventTrigger_Format1_t>();
        return d.ric_eventTrigger_formats.choice.eventTrigger_Format1 != nullptr;
    });
}

asn_code e2sm_dapp_event_trigger_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_EventTrigger_t>(
        asn_DEF_E2SM_DAPP_EventTrigger, data, len, *this, [](const E2SM_DAPP_EventTrigger_t& s, e2sm_dapp_event_trigger_s&) {
            return s.ric_eventTrigger_formats.present ==
                   E2SM_DAPP_EventTrigger__ric_eventTrigger_formats_PR_eventTrigger_Format1;
        });
}

// ---------------------------------------------------------------------------
// Action definition
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_action_definition_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_ActionDefinition_t>(
        asn_DEF_E2SM_DAPP_ActionDefinition, out, [this](E2SM_DAPP_ActionDefinition_t& d) {
            if (!set_long(d.ric_Style_Type, ric_style_type)) {
                return false;
            }
            d.actionDefinition_formats.present =
                E2SM_DAPP_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format1;
            d.actionDefinition_formats.choice.actionDefinition_Format1 =
                zalloc<E2SM_DAPP_ActionDefinition_Format1_t>();
            return d.actionDefinition_formats.choice.actionDefinition_Format1 != nullptr;
        });
}

asn_code e2sm_dapp_action_definition_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_ActionDefinition_t>(
        asn_DEF_E2SM_DAPP_ActionDefinition,
        data,
        len,
        *this,
        [](const E2SM_DAPP_ActionDefinition_t& s, e2sm_dapp_action_definition_s& d) {
            if (s.actionDefinition_formats.present !=
                E2SM_DAPP_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format1) {
                return false;
            }
            d.ric_style_type = static_cast<int64_t>(s.ric_Style_Type);
            return true;
        });
}

// ---------------------------------------------------------------------------
// Indication header
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ind_hdr_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_IndicationHeader_t>(
        asn_DEF_E2SM_DAPP_IndicationHeader, out, [this](E2SM_DAPP_IndicationHeader_t& d) {
            using types = ric_ind_hdr_formats_c_::types;
            switch (ric_ind_hdr_formats.type().value) {
                case types::ind_hdr_format1: {
                    auto* f = zalloc<E2SM_DAPP_IndicationHeader_Format1_t>();
                    d.ric_indicationHeader_formats.present =
                        E2SM_DAPP_IndicationHeader__ric_indicationHeader_formats_PR_indicationHeader_Format1;
                    d.ric_indicationHeader_formats.choice.indicationHeader_Format1 = f;
                    return f != nullptr && build(ric_ind_hdr_formats.ind_hdr_format1(), *f);
                }
                case types::ind_hdr_format2: {
                    auto* f = zalloc<E2SM_DAPP_IndicationHeader_Format2_t>();
                    d.ric_indicationHeader_formats.present =
                        E2SM_DAPP_IndicationHeader__ric_indicationHeader_formats_PR_indicationHeader_Format2;
                    d.ric_indicationHeader_formats.choice.indicationHeader_Format2 = f;
                    return f != nullptr && build(ric_ind_hdr_formats.ind_hdr_format2(), *f);
                }
                default:
                    return false;
            }
        });
}

asn_code e2sm_dapp_ind_hdr_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_IndicationHeader_t>(
        asn_DEF_E2SM_DAPP_IndicationHeader,
        data,
        len,
        *this,
        [](const E2SM_DAPP_IndicationHeader_t& s, e2sm_dapp_ind_hdr_s& d) {
            switch (s.ric_indicationHeader_formats.present) {
                case E2SM_DAPP_IndicationHeader__ric_indicationHeader_formats_PR_indicationHeader_Format1:
                    return s.ric_indicationHeader_formats.choice.indicationHeader_Format1 != nullptr &&
                           from_asn(*s.ric_indicationHeader_formats.choice.indicationHeader_Format1,
                               d.ric_ind_hdr_formats.set_ind_hdr_format1());
                case E2SM_DAPP_IndicationHeader__ric_indicationHeader_formats_PR_indicationHeader_Format2:
                    return s.ric_indicationHeader_formats.choice.indicationHeader_Format2 != nullptr &&
                           from_asn(*s.ric_indicationHeader_formats.choice.indicationHeader_Format2,
                               d.ric_ind_hdr_formats.set_ind_hdr_format2());
                default:
                    return false;
            }
        });
}

// ---------------------------------------------------------------------------
// Indication message
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ind_msg_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_IndicationMessage_t>(
        asn_DEF_E2SM_DAPP_IndicationMessage, out, [this](E2SM_DAPP_IndicationMessage_t& d) {
            using types = ric_ind_msg_formats_c_::types;
            switch (ric_ind_msg_formats.type().value) {
                case types::ind_msg_format1: {
                    auto* f = zalloc<E2SM_DAPP_IndicationMessage_Format1_t>();
                    d.ric_indicationMessage_formats.present =
                        E2SM_DAPP_IndicationMessage__ric_indicationMessage_formats_PR_indicationMessage_Format1;
                    d.ric_indicationMessage_formats.choice.indicationMessage_Format1 = f;
                    const auto& s = ric_ind_msg_formats.ind_msg_format1();
                    return f != nullptr && build_data(s.data_size, s.data, f->data_size, f->data);
                }
                case types::ind_msg_format2: {
                    auto* f = zalloc<E2SM_DAPP_IndicationMessage_Format2_t>();
                    d.ric_indicationMessage_formats.present =
                        E2SM_DAPP_IndicationMessage__ric_indicationMessage_formats_PR_indicationMessage_Format2;
                    d.ric_indicationMessage_formats.choice.indicationMessage_Format2 = f;
                    return f != nullptr &&
                           build_subs(ric_ind_msg_formats.ind_msg_format2().dapp_e3_subscriptions,
                                      f->dappE3Subscriptions);
                }
                default:
                    return false;
            }
        });
}

asn_code e2sm_dapp_ind_msg_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_IndicationMessage_t>(
        asn_DEF_E2SM_DAPP_IndicationMessage,
        data,
        len,
        *this,
        [](const E2SM_DAPP_IndicationMessage_t& s, e2sm_dapp_ind_msg_s& d) {
            switch (s.ric_indicationMessage_formats.present) {
                case E2SM_DAPP_IndicationMessage__ric_indicationMessage_formats_PR_indicationMessage_Format1: {
                    const auto* f = s.ric_indicationMessage_formats.choice.indicationMessage_Format1;
                    auto&       c = d.ric_ind_msg_formats.set_ind_msg_format1();
                    return f != nullptr && get_data(f->data_size, f->data, c.data_size, c.data);
                }
                case E2SM_DAPP_IndicationMessage__ric_indicationMessage_formats_PR_indicationMessage_Format2: {
                    const auto* f = s.ric_indicationMessage_formats.choice.indicationMessage_Format2;
                    auto&       c = d.ric_ind_msg_formats.set_ind_msg_format2();
                    return f != nullptr && get_subs(f->dappE3Subscriptions, c.dapp_e3_subscriptions);
                }
                default:
                    return false;
            }
        });
}

// ---------------------------------------------------------------------------
// Control header
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ctrl_hdr_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_ControlHeader_t>(
        asn_DEF_E2SM_DAPP_ControlHeader, out, [this](E2SM_DAPP_ControlHeader_t& d) {
            auto* f = zalloc<E2SM_DAPP_ControlHeader_Format1_t>();
            d.ric_controlHeader_formats.present =
                E2SM_DAPP_ControlHeader__ric_controlHeader_formats_PR_controlHeader_Format1;
            d.ric_controlHeader_formats.choice.controlHeader_Format1 = f;
            return f != nullptr && build(ric_ctrl_hdr_formats.ctrl_hdr_format1(), *f);
        });
}

asn_code e2sm_dapp_ctrl_hdr_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_ControlHeader_t>(
        asn_DEF_E2SM_DAPP_ControlHeader,
        data,
        len,
        *this,
        [](const E2SM_DAPP_ControlHeader_t& s, e2sm_dapp_ctrl_hdr_s& d) {
            return s.ric_controlHeader_formats.present ==
                       E2SM_DAPP_ControlHeader__ric_controlHeader_formats_PR_controlHeader_Format1 &&
                   s.ric_controlHeader_formats.choice.controlHeader_Format1 != nullptr &&
                   from_asn(*s.ric_controlHeader_formats.choice.controlHeader_Format1,
                       d.ric_ctrl_hdr_formats.ctrl_hdr_format1());
        });
}

// ---------------------------------------------------------------------------
// Control message
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ctrl_msg_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_ControlMessage_t>(
        asn_DEF_E2SM_DAPP_ControlMessage, out, [this](E2SM_DAPP_ControlMessage_t& d) {
            auto* f = zalloc<E2SM_DAPP_ControlMessage_Format1_t>();
            d.ric_controlMessage_formats.present =
                E2SM_DAPP_ControlMessage__ric_controlMessage_formats_PR_controlMessage_Format1;
            d.ric_controlMessage_formats.choice.controlMessage_Format1 = f;
            const auto& s = ric_ctrl_msg_formats.ctrl_msg_format1();
            return f != nullptr && build_data(s.data_size, s.data, f->data_size, f->data);
        });
}

asn_code e2sm_dapp_ctrl_msg_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_ControlMessage_t>(
        asn_DEF_E2SM_DAPP_ControlMessage,
        data,
        len,
        *this,
        [](const E2SM_DAPP_ControlMessage_t& s, e2sm_dapp_ctrl_msg_s& d) {
            if (s.ric_controlMessage_formats.present !=
                E2SM_DAPP_ControlMessage__ric_controlMessage_formats_PR_controlMessage_Format1) {
                return false;
            }
            const auto* f = s.ric_controlMessage_formats.choice.controlMessage_Format1;
            auto&       c = d.ric_ctrl_msg_formats.ctrl_msg_format1();
            return f != nullptr && get_data(f->data_size, f->data, c.data_size, c.data);
        });
}

// ---------------------------------------------------------------------------
// Control outcome
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ctrl_outcome_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_ControlOutcome_t>(
        asn_DEF_E2SM_DAPP_ControlOutcome, out, [this](E2SM_DAPP_ControlOutcome_t& d) {
            auto* f = zalloc<E2SM_DAPP_ControlOutcome_Format1_t>();
            d.ric_controlOutcome_formats.present =
                E2SM_DAPP_ControlOutcome__ric_controlOutcome_formats_PR_controlOutcome_Format1;
            d.ric_controlOutcome_formats.choice.controlOutcome_Format1 = f;
            return f != nullptr && build(ric_ctrl_outcome_formats.ctrl_outcome_format1(), *f);
        });
}

asn_code e2sm_dapp_ctrl_outcome_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_ControlOutcome_t>(
        asn_DEF_E2SM_DAPP_ControlOutcome,
        data,
        len,
        *this,
        [](const E2SM_DAPP_ControlOutcome_t& s, e2sm_dapp_ctrl_outcome_s& d) {
            return s.ric_controlOutcome_formats.present ==
                       E2SM_DAPP_ControlOutcome__ric_controlOutcome_formats_PR_controlOutcome_Format1 &&
                   s.ric_controlOutcome_formats.choice.controlOutcome_Format1 != nullptr &&
                   from_asn(*s.ric_controlOutcome_formats.choice.controlOutcome_Format1,
                       d.ric_ctrl_outcome_formats.ctrl_outcome_format1());
        });
}

// ---------------------------------------------------------------------------
// RAN function definition
// ---------------------------------------------------------------------------

asn_code e2sm_dapp_ran_function_definition_s::pack(std::vector<uint8_t>& out) const
{
    return pack_pdu<E2SM_DAPP_RANFunctionDefinition_t>(
        asn_DEF_E2SM_DAPP_RANFunctionDefinition,
        out,
        [this](E2SM_DAPP_RANFunctionDefinition_t& d) { return build(*this, d); });
}

asn_code e2sm_dapp_ran_function_definition_s::unpack(const uint8_t* data, size_t len)
{
    return unpack_pdu<E2SM_DAPP_RANFunctionDefinition_t>(
        asn_DEF_E2SM_DAPP_RANFunctionDefinition,
        data,
        len,
        *this,
        [](const E2SM_DAPP_RANFunctionDefinition_t& s, e2sm_dapp_ran_function_definition_s& d) {
            return from_asn(s, d);
        });
}

} // namespace e2sm_dapp
} // namespace libe3
