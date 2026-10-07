/**
 * @file e2sm_dapp.hpp
 * @brief E2SM-DAPP information elements and their APER codec (C++ API).
 *
 * E2SM-DAPP is the E2 service model (RAN function id 255, OID
 * 1.3.6.1.4.1.53148.1.1.255.3) that carries dApp traffic between an E2 node and
 * an xApp. Its grammar ships with libe3 as
 * `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`, and the specification is in
 * `docs/e2sm-dapp/`.
 *
 * The type and field names in this header deliberately mirror OCUDU's
 * `asn1::e2sm_dapp` (`include/ocudu/asn1/e2sm/e2sm_dapp.h`), so that code
 * building or reading a message looks the same in both stacks. The codec behind
 * it is asn1c here and OCUDU's own ASN.1 runtime there; the two are checked
 * against each other with golden vectors. Differences from the OCUDU interface:
 *  - only the eight top-level types (see below) have `pack` and `unpack`, and they
 *    work on `std::vector<uint8_t>` instead of `bit_ref`;
 *  - the ASN.1 helper templates (`printable_string`, `fixed_octstring`, ...) are
 *    thin wrappers over `std::string` and `std::vector`;
 *  - there is no `to_json`;
 *  - `operator==` is provided on every type.
 *
 * Top-level types, each the content of an E2AP OCTET STRING:
 * `e2sm_dapp_event_trigger_s`, `e2sm_dapp_action_definition_s`,
 * `e2sm_dapp_ind_hdr_s`, `e2sm_dapp_ind_msg_s`, `e2sm_dapp_ctrl_hdr_s`,
 * `e2sm_dapp_ctrl_msg_s`, `e2sm_dapp_ctrl_outcome_s` and
 * `e2sm_dapp_ran_function_definition_s`.
 *
 * Built only when LIBE3_ENABLE_E2SM_DAPP is on (it needs LIBE3_ENABLE_ASN1).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIBE3_E2SM_DAPP_HPP
#define LIBE3_E2SM_DAPP_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace libe3 {
namespace e2sm_dapp {

/** Result of `pack` and `unpack`. Same values as OCUDU's `OCUDUASN_CODE`. */
enum asn_code : int {
    ASN_ERROR_ENCODE_FAIL = -2, ///< a value is outside its ASN.1 constraint, or the buffer cannot be built
    ASN_ERROR_DECODE_FAIL = -1, ///< the bytes are not a valid APER encoding of the type
    ASN_SUCCESS           = 0
};

// ---------------------------------------------------------------------------
// Helper types mirroring the ones OCUDU's ASN.1 runtime provides. The bounds are
// part of the type for documentation; they are enforced by `pack`, not by the
// containers.
// ---------------------------------------------------------------------------

/// Mirrors OCUDU's `enumerated<Opts, Ext>`: derives from the options struct and converts to its enum.
template <class EnumType, bool Ext = false>
class enumerated : public EnumType
{
public:
    static const bool has_ext = Ext; ///< true when the ASN.1 enumeration is extensible

    /// Holds `nulltype`, no alternative.
    enumerated() { EnumType::value = EnumType::nulltype; }
    /// @param v value to hold
    enumerated(typename EnumType::options v) { EnumType::value = v; }
    /// @return the held value
    operator typename EnumType::options() const { return EnumType::value; }
};

/// PrintableString (SIZE(LB..UB)).
template <uint32_t LB, uint32_t UB, bool Ext = false, bool Aligned = true>
class printable_string
{
public:
    /// @param s value to hold
    /// @return this string
    printable_string& from_string(const std::string& s)
    {
        str_ = s;
        return *this;
    }
    /// @return the held value
    const std::string& to_string() const { return str_; }
    /// @return length in characters
    size_t             size() const { return str_.size(); }
    /// @return length in characters
    size_t             length() const { return str_.size(); }
    /// @param o string to compare with
    /// @return true when both hold the same characters
    bool               operator==(const printable_string& o) const { return str_ == o.str_; }
    /// @param o string to compare with
    /// @return true when the strings differ
    bool               operator!=(const printable_string& o) const { return !(*this == o); }

private:
    std::string str_;
};

/// OCTET STRING (SIZE(N)).
template <uint32_t N, bool Aligned = true>
class fixed_octstring : public std::array<uint8_t, N>
{
public:
    /// Zero-filled.
    fixed_octstring() { this->fill(0); }
};

/// OCTET STRING (SIZE(LB..UB)).
template <uint32_t LB, uint32_t UB, bool Aligned = true>
class bounded_octstring : public std::vector<uint8_t>
{
};

/// OCTET STRING without a size constraint.
template <bool Aligned = true>
class unbounded_octstring : public std::vector<uint8_t>
{
public:
    /// @param data first byte to copy
    /// @param len number of bytes to copy
    /// @return this string
    unbounded_octstring& from_bytes(const uint8_t* data, size_t len)
    {
        assign(data, data + len);
        return *this;
    }
};

/// SEQUENCE OF with a size constraint known only at pack time.
template <class T>
class dyn_array : public std::vector<T>
{
};

/// SEQUENCE OF with an upper bound UB.
template <class T, uint32_t UB>
class bounded_array : public std::vector<T>
{
};

// ---------------------------------------------------------------------------
// Constants (maxE2SMDAPPIndMsgSize and friends).
// ---------------------------------------------------------------------------

constexpr int64_t max_e2sm_dapp_ind_msg_size             = 32768; ///< Largest indication message data, in octets
constexpr int64_t max_e2sm_dapp_ctrl_msg_size            = 32768; ///< Largest control message data, in octets
constexpr int64_t maxnoof_dapps                          = 256; ///< Most dApps in a dApp E3 subscription list
constexpr int64_t maxnoof_subscribed_e3_ran_functions    = 64; ///< Most E3 RAN functions one dApp can subscribe to
constexpr int64_t maxnoof_dapp_report_styles             = 2; ///< Most report styles in a RAN function definition
constexpr int64_t maxnoof_dapp_control_styles            = 1; ///< Most control styles in a RAN function definition

// ---------------------------------------------------------------------------
// Information elements.
// ---------------------------------------------------------------------------

/** Defines `operator!=` as the negation of `operator==`.
 * @param T the type that has an `operator==` */
#define LIBE3_E2SM_DAPP_NEQ(T) \
    bool operator!=(const T& o) const { return !(*this == o); }

/// Name, OID and description of the RAN function (ASN.1 `RANfunction-Name ::= SEQUENCE`).
struct ran_function_name_s {
    bool                                  ext                           = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                                  ran_function_instance_present = false; ///< true when `ran_function_instance` is present
    printable_string<1, 150, true, true>  ran_function_short_name; ///< Short name (`E2SM-DAPP`), 1 to 150 characters
    printable_string<1, 1000, true, true> ran_function_e2sm_o_id; ///< Service model OID, 1 to 1000 characters
    printable_string<1, 150, true, true>  ran_function_description; ///< Description, 1 to 150 characters
    int64_t                               ran_function_instance = 0; ///< Instance number; not used, a sender shall not set it

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const ran_function_name_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_name_s)
};

/// Event trigger format 1: no fields, the RAN reports when it has something to report (ASN.1 `E2SM-DAPP-EventTrigger-Format1 ::= SEQUENCE {}`).
struct e2sm_dapp_event_trigger_format1_s {
    /** @return true, the type has no fields */
    bool operator==(const e2sm_dapp_event_trigger_format1_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_event_trigger_format1_s)
};

/// Event trigger definition of a subscription (ASN.1 `E2SM-DAPP-EventTrigger ::= SEQUENCE (top-level)`).
struct e2sm_dapp_event_trigger_s {
    /// Formats of the event trigger.
    struct ric_event_trigger_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { event_trigger_format1, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const { return "eventTrigger-Format1"; }
            /** @return the number of the alternative, from 1 */
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** @return the selected alternative */
        types type() const { return types::event_trigger_format1; }
        // getters
        /** Content of event trigger format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_event_trigger_format1_s&       event_trigger_format1() { return c; }
        /** Content of event trigger format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_event_trigger_format1_s& event_trigger_format1() const { return c; }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_event_trigger_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_event_trigger_formats_c_)

    private:
        e2sm_dapp_event_trigger_format1_s c;
    };

    bool                         ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_event_trigger_formats_c_ ric_event_trigger_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_event_trigger_s& o) const
    {
        return ric_event_trigger_formats == o.ric_event_trigger_formats;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_event_trigger_s)
};

/// Action definition format 1: no fields (ASN.1 `E2SM-DAPP-ActionDefinition-Format1 ::= SEQUENCE {}`).
struct e2sm_dapp_action_definition_format1_s {
    /** @return true, the type has no fields */
    bool operator==(const e2sm_dapp_action_definition_format1_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_action_definition_format1_s)
};

/// Action definition of a subscription, which selects the report style (ASN.1 `E2SM-DAPP-ActionDefinition ::= SEQUENCE (top-level)`).
struct e2sm_dapp_action_definition_s {
    /// Formats of the action definition.
    struct action_definition_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { action_definition_format1, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const { return "actionDefinition-Format1"; }
            /** @return the number of the alternative, from 1 */
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** @return the selected alternative */
        types type() const { return types::action_definition_format1; }
        // getters
        /** Content of action definition format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_action_definition_format1_s&       action_definition_format1() { return c; }
        /** Content of action definition format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_action_definition_format1_s& action_definition_format1() const { return c; }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const action_definition_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(action_definition_formats_c_)

    private:
        e2sm_dapp_action_definition_format1_s c;
    };

    bool                         ext            = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    int64_t                      ric_style_type = 0; ///< Report style the subscription selects (1 or 2); it selects the indication formats
    action_definition_formats_c_ action_definition_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_action_definition_s& o) const
    {
        return ric_style_type == o.ric_style_type && action_definition_formats == o.action_definition_formats;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_action_definition_s)
};

/// The E3 RAN functions one dApp has subscribed to (ASN.1 `DAppE3Subscription-Item ::= SEQUENCE`).
struct dapp_e3_subscription_item_s {
    using subscribed_e3_ran_functions_l_ = bounded_array<uint64_t, 64>; ///< List of E3 RAN function ids

    bool                           ext     = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    uint64_t                       dapp_id = 0; ///< The dApp
    subscribed_e3_ran_functions_l_ subscribed_e3_ran_functions; ///< E3 RAN functions the dApp has subscribed to (1 to 64)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const dapp_e3_subscription_item_s& o) const
    {
        return dapp_id == o.dapp_id && subscribed_e3_ran_functions == o.subscribed_e3_ran_functions;
    }
    LIBE3_E2SM_DAPP_NEQ(dapp_e3_subscription_item_s)
};

/// The dApp E3 subscription map, 0 to 256 items (ASN.1 `DAppE3Subscription-List ::= SEQUENCE (SIZE (0..256)) OF DAppE3Subscription-Item`).
using dapp_e3_subscription_list_l = dyn_array<dapp_e3_subscription_item_s>;

/// One report style of the RAN function definition (ASN.1 `RANFunctionDefinition-Report-Item ::= SEQUENCE`).
struct ran_function_definition_report_item_s {
    bool                                 ext                           = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                                 dapp_e3_subscriptions_present = false; ///< true when `dapp_e3_subscriptions` is present
    int64_t                              ric_report_style_type         = 0; ///< Report style number (1 or 2)
    printable_string<1, 150, true, true> ric_report_style_name; ///< Report style name, 1 to 150 characters
    int64_t                              ric_ind_hdr_format_type = 0; ///< Indication header format the style produces
    int64_t                              ric_ind_msg_format_type = 0; ///< Indication message format the style produces
    dapp_e3_subscription_list_l          dapp_e3_subscriptions; ///< dApp E3 subscription map (0 to 256 items)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const ran_function_definition_report_item_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_report_item_s)
};

/// One control style of the RAN function definition (ASN.1 `RANFunctionDefinition-Control-Item ::= SEQUENCE`).
struct ran_function_definition_ctrl_item_s {
    bool                                 ext                           = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                                 dapp_e3_subscriptions_present = false; ///< true when `dapp_e3_subscriptions` is present
    int64_t                              ric_ctrl_style_type           = 0; ///< Control style number (1)
    printable_string<1, 150, true, true> ric_ctrl_style_name; ///< Control style name, 1 to 150 characters
    int64_t                              ric_ctrl_hdr_format_type     = 0; ///< Control header format the style uses
    int64_t                              ric_ctrl_msg_format_type     = 0; ///< Control message format the style uses
    int64_t                              ric_ctrl_outcome_format_type = 0; ///< Control outcome format the style uses
    dapp_e3_subscription_list_l          dapp_e3_subscriptions; ///< dApp E3 subscription map (0 to 256 items)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const ran_function_definition_ctrl_item_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_ctrl_item_s)
};

/// Event trigger section of the RAN function definition: empty, its presence says the function has an event trigger (ASN.1 `RANFunctionDefinition-EventTrigger ::= SEQUENCE`).
struct ran_function_definition_event_trigger_s {
    bool ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`

    /** @return true, the type has no fields */
    bool operator==(const ran_function_definition_event_trigger_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_event_trigger_s)
};

/// Report section of the RAN function definition (ASN.1 `RANFunctionDefinition-Report ::= SEQUENCE`).
struct ran_function_definition_report_s {
    using ric_report_style_list_l_ = dyn_array<ran_function_definition_report_item_s>; ///< List of report styles

    bool                     ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_report_style_list_l_ ric_report_style_list; ///< Report styles the RAN supports (1 to 2)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const ran_function_definition_report_s& o) const
    {
        return ric_report_style_list == o.ric_report_style_list;
    }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_report_s)
};

/// Control section of the RAN function definition (ASN.1 `RANFunctionDefinition-Control ::= SEQUENCE`).
struct ran_function_definition_ctrl_s {
    using ric_ctrl_style_list_l_ = dyn_array<ran_function_definition_ctrl_item_s>; ///< List of control styles

    bool                   ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ctrl_style_list_l_ ric_ctrl_style_list; ///< Control styles the RAN supports (exactly 1)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const ran_function_definition_ctrl_s& o) const
    {
        return ric_ctrl_style_list == o.ric_ctrl_style_list;
    }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_ctrl_s)
};

/// RAN function definition, announced in E2 Setup and RIC Service Update (ASN.1 `E2SM-DAPP-RANFunctionDefinition ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ran_function_definition_s {
    bool                                    ext                                           = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                                    ran_function_definition_event_trigger_present = false; ///< true when `ran_function_definition_event_trigger` is present
    bool                                    ran_function_definition_report_present        = false; ///< true when `ran_function_definition_report` is present
    bool                                    ran_function_definition_ctrl_present          = false; ///< true when `ran_function_definition_ctrl` is present
    ran_function_name_s                     ran_function_name; ///< Name of the RAN function
    ran_function_definition_event_trigger_s ran_function_definition_event_trigger; ///< Event trigger section, empty
    ran_function_definition_report_s        ran_function_definition_report; ///< Report section
    ran_function_definition_ctrl_s          ran_function_definition_ctrl; ///< Control section

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ran_function_definition_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ran_function_definition_s)
};

/// Indication header format 1: the header of a dApp report (ASN.1 `E2SM-DAPP-IndicationHeader-Format1 ::= SEQUENCE`).
struct e2sm_dapp_ind_hdr_format1_s {
    bool                     ext                   = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                     node_cu_du_id_present = false; ///< true when `node_cu_du_id` is present
    bool                     timestamp_present     = false; ///< true when `timestamp` is present
    bool                     sequence_id_present   = false; ///< true when `sequence_id` is present
    uint64_t                 ran_function_id       = 0; ///< E3 RAN function id of the report, not the E2 RAN function id
    uint64_t                 dapp_id               = 0; ///< The dApp that sent the report
    uint8_t                  node_type             = 0; ///< Type of the reporting node (0 to 255)
    fixed_octstring<3, true> node_plmn_id; ///< PLMN id of the node, 3 octets
    uint64_t                 node_nb_id    = 0; ///< gNB id of the node
    uint64_t                 node_cu_du_id = 0; ///< CU or DU id inside the node
    int64_t                  timestamp     = 0; ///< When the RAN built the indication, in nanoseconds on its realtime clock
    int64_t                  sequence_id   = 0; ///< Correlation id of the dApp for this report, copied from the E3 report

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ind_hdr_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_format1_s)
};

/// Indication header format 2: the header of a subscription map (ASN.1 `E2SM-DAPP-IndicationHeader-Format2 ::= SEQUENCE`).
struct e2sm_dapp_ind_hdr_format2_s {
    bool                     ext                   = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                     node_cu_du_id_present = false; ///< true when `node_cu_du_id` is present
    bool                     timestamp_present     = false; ///< true when `timestamp` is present
    bool                     sequence_id_present   = false; ///< true when `sequence_id` is present
    uint8_t                  node_type             = 0; ///< Type of the reporting node (0 to 255)
    fixed_octstring<3, true> node_plmn_id; ///< PLMN id of the node, 3 octets
    uint64_t                 node_nb_id    = 0; ///< gNB id of the node
    uint64_t                 node_cu_du_id = 0; ///< CU or DU id inside the node
    int64_t                  timestamp     = 0; ///< When the RAN built the indication, in nanoseconds on its realtime clock
    int64_t                  sequence_id   = 0; ///< Correlation id; a subscription map has no procedure, so a RAN leaves it absent

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ind_hdr_format2_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_format2_s)
};

/// Indication header (ASN.1 `E2SM-DAPP-IndicationHeader ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ind_hdr_s {
    /// Formats of the indication header.
    struct ric_ind_hdr_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { ind_hdr_format1, ind_hdr_format2, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const
            {
                static const char* names[] = {"indicationHeader-Format1", "indicationHeader-Format2"};
                return value < nulltype ? names[value] : "nulltype";
            }
            /** @return the number of the alternative, from 1 */
            uint8_t to_number() const { return static_cast<uint8_t>(value + 1); }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** Select an alternative and reset its content.
         * @param e alternative to select, `nulltype` for none */
        void set(types::options e = types::nulltype)
        {
            type_ = e;
            switch (e) {
                case types::ind_hdr_format1: c_.emplace<1>(); break;
                case types::ind_hdr_format2: c_.emplace<2>(); break;
                default: c_.emplace<0>(); break;
            }
        }
        /** @return the selected alternative */
        types type() const { return type_; }
        // getters; throw std::bad_variant_access when the other alternative is set
        /** Content of ind hdr format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ind_hdr_format1_s&       ind_hdr_format1() { return std::get<1>(c_); }
        /** Content of ind hdr format 2.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ind_hdr_format2_s&       ind_hdr_format2() { return std::get<2>(c_); }
        /** Content of ind hdr format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ind_hdr_format1_s& ind_hdr_format1() const { return std::get<1>(c_); }
        /** Content of ind hdr format 2.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ind_hdr_format2_s& ind_hdr_format2() const { return std::get<2>(c_); }
        /** Select ind hdr format 1.
         * @return its content, default constructed */
        e2sm_dapp_ind_hdr_format1_s&       set_ind_hdr_format1()
        {
            set(types::ind_hdr_format1);
            return std::get<1>(c_);
        }
        /** Select ind hdr format 2.
         * @return its content, default constructed */
        e2sm_dapp_ind_hdr_format2_s& set_ind_hdr_format2()
        {
            set(types::ind_hdr_format2);
            return std::get<2>(c_);
        }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_ind_hdr_formats_c_& o) const { return type_.value == o.type_.value && c_ == o.c_; }
        LIBE3_E2SM_DAPP_NEQ(ric_ind_hdr_formats_c_)

    private:
        types                                                                         type_;
        std::variant<std::monostate, e2sm_dapp_ind_hdr_format1_s, e2sm_dapp_ind_hdr_format2_s> c_;
    };

    bool                   ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ind_hdr_formats_c_ ric_ind_hdr_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ind_hdr_s& o) const { return ric_ind_hdr_formats == o.ric_ind_hdr_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_s)
};

/// Indication message format 1: the dApp report payload (ASN.1 `E2SM-DAPP-IndicationMessage-Format1 ::= SEQUENCE`).
struct e2sm_dapp_ind_msg_format1_s {
    bool                              ext       = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    uint16_t                          data_size = 0; ///< Number of octets in `data`; repeats the length of the octet string
    bounded_octstring<1, 32768, true> data; ///< Service model payload, copied unchanged from the E3 report (1 to 32768 octets)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ind_msg_format1_s& o) const { return data_size == o.data_size && data == o.data; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_format1_s)
};

/// Indication message format 2: the dApp E3 subscription map (ASN.1 `E2SM-DAPP-IndicationMessage-Format2 ::= SEQUENCE`).
struct e2sm_dapp_ind_msg_format2_s {
    bool                        ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    dapp_e3_subscription_list_l dapp_e3_subscriptions; ///< dApp E3 subscription map (0 to 256 items)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ind_msg_format2_s& o) const
    {
        return dapp_e3_subscriptions == o.dapp_e3_subscriptions;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_format2_s)
};

/// Indication message (ASN.1 `E2SM-DAPP-IndicationMessage ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ind_msg_s {
    /// Formats of the indication message.
    struct ric_ind_msg_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { ind_msg_format1, ind_msg_format2, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const
            {
                static const char* names[] = {"indicationMessage-Format1", "indicationMessage-Format2"};
                return value < nulltype ? names[value] : "nulltype";
            }
            /** @return the number of the alternative, from 1 */
            uint8_t to_number() const { return static_cast<uint8_t>(value + 1); }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** Select an alternative and reset its content.
         * @param e alternative to select, `nulltype` for none */
        void set(types::options e = types::nulltype)
        {
            type_ = e;
            switch (e) {
                case types::ind_msg_format1: c_.emplace<1>(); break;
                case types::ind_msg_format2: c_.emplace<2>(); break;
                default: c_.emplace<0>(); break;
            }
        }
        /** @return the selected alternative */
        types type() const { return type_; }
        // getters; throw std::bad_variant_access when the other alternative is set
        /** Content of ind msg format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ind_msg_format1_s&       ind_msg_format1() { return std::get<1>(c_); }
        /** Content of ind msg format 2.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ind_msg_format2_s&       ind_msg_format2() { return std::get<2>(c_); }
        /** Content of ind msg format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ind_msg_format1_s& ind_msg_format1() const { return std::get<1>(c_); }
        /** Content of ind msg format 2.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ind_msg_format2_s& ind_msg_format2() const { return std::get<2>(c_); }
        /** Select ind msg format 1.
         * @return its content, default constructed */
        e2sm_dapp_ind_msg_format1_s&       set_ind_msg_format1()
        {
            set(types::ind_msg_format1);
            return std::get<1>(c_);
        }
        /** Select ind msg format 2.
         * @return its content, default constructed */
        e2sm_dapp_ind_msg_format2_s& set_ind_msg_format2()
        {
            set(types::ind_msg_format2);
            return std::get<2>(c_);
        }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_ind_msg_formats_c_& o) const { return type_.value == o.type_.value && c_ == o.c_; }
        LIBE3_E2SM_DAPP_NEQ(ric_ind_msg_formats_c_)

    private:
        types                                                                         type_;
        std::variant<std::monostate, e2sm_dapp_ind_msg_format1_s, e2sm_dapp_ind_msg_format2_s> c_;
    };

    bool                   ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ind_msg_formats_c_ ric_ind_msg_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ind_msg_s& o) const { return ric_ind_msg_formats == o.ric_ind_msg_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_s)
};

/// Control header format 1: where a control goes (ASN.1 `E2SM-DAPP-ControlHeader-Format1 ::= SEQUENCE`).
struct e2sm_dapp_ctrl_hdr_format1_s {
    bool     ext                 = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool     timestamp_present   = false; ///< true when `timestamp` is present
    bool     sequence_id_present = false; ///< true when `sequence_id` is present
    uint64_t ran_function_id     = 0; ///< E3 RAN function id the control is for
    uint64_t dapp_id             = 0; ///< The dApp that is the target of the control
    int64_t  timestamp           = 0; ///< When the xApp built the control, in nanoseconds on the realtime clock of its host
    int64_t  sequence_id         = 0; ///< Id of the procedure, echoed from the indication header; absent or 0 means the RAN cannot defer the acknowledge

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ctrl_hdr_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_hdr_format1_s)
};

/// Control header (ASN.1 `E2SM-DAPP-ControlHeader ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ctrl_hdr_s {
    /// Formats of the control header.
    struct ric_ctrl_hdr_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { ctrl_hdr_format1, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const { return "controlHeader-Format1"; }
            /** @return the number of the alternative, from 1 */
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** @return the selected alternative */
        types type() const { return types::ctrl_hdr_format1; }
        // getters
        /** Content of ctrl hdr format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ctrl_hdr_format1_s&       ctrl_hdr_format1() { return c; }
        /** Content of ctrl hdr format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ctrl_hdr_format1_s& ctrl_hdr_format1() const { return c; }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_ctrl_hdr_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_hdr_formats_c_)

    private:
        e2sm_dapp_ctrl_hdr_format1_s c;
    };

    bool                    ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ctrl_hdr_formats_c_ ric_ctrl_hdr_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ctrl_hdr_s& o) const { return ric_ctrl_hdr_formats == o.ric_ctrl_hdr_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_hdr_s)
};

/// Control message format 1: the xApp control payload (ASN.1 `E2SM-DAPP-ControlMessage-Format1 ::= SEQUENCE`).
struct e2sm_dapp_ctrl_msg_format1_s {
    bool                              ext       = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    uint16_t                          data_size = 0; ///< Number of octets in `data`; repeats the length of the octet string
    bounded_octstring<1, 32768, true> data; ///< Service model payload, delivered unchanged to the dApp (1 to 32768 octets)

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ctrl_msg_format1_s& o) const { return data_size == o.data_size && data == o.data; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_msg_format1_s)
};

/// Control message (ASN.1 `E2SM-DAPP-ControlMessage ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ctrl_msg_s {
    /// Formats of the control message.
    struct ric_ctrl_msg_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { ctrl_msg_format1, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const { return "controlMessage-Format1"; }
            /** @return the number of the alternative, from 1 */
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** @return the selected alternative */
        types type() const { return types::ctrl_msg_format1; }
        // getters
        /** Content of ctrl msg format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ctrl_msg_format1_s&       ctrl_msg_format1() { return c; }
        /** Content of ctrl msg format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ctrl_msg_format1_s& ctrl_msg_format1() const { return c; }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_ctrl_msg_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_msg_formats_c_)

    private:
        e2sm_dapp_ctrl_msg_format1_s c;
    };

    bool                    ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ctrl_msg_formats_c_ ric_ctrl_msg_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ctrl_msg_s& o) const { return ric_ctrl_msg_formats == o.ric_ctrl_msg_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_msg_s)
};

/// Control outcome format 1: the answer to a control (ASN.1 `E2SM-DAPP-ControlOutcome-Format1 ::= SEQUENCE`).
struct e2sm_dapp_ctrl_outcome_format1_s {
    bool                      ext                     = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    bool                      timestamp_present       = false; ///< true when `timestamp` is present
    bool                      sequence_id_present     = false; ///< true when `sequence_id` is present
    bool                      e3_ctrl_outcome_present = false; ///< true when `e3_ctrl_outcome` is present
    int64_t                   timestamp               = 0; ///< When the RAN built the outcome, in nanoseconds on its realtime clock
    int64_t                   sequence_id             = 0; ///< Procedure this outcome answers, copied from the control header
    unbounded_octstring<true> e3_ctrl_outcome; ///< Account of the service model on applying the control; E2SM-DAPP does not interpret it

    /** @param o value to compare with
     * @return true when every field is equal */
    bool operator==(const e2sm_dapp_ctrl_outcome_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_outcome_format1_s)
};

/// Control outcome (ASN.1 `E2SM-DAPP-ControlOutcome ::= SEQUENCE (top-level)`).
struct e2sm_dapp_ctrl_outcome_s {
    /// Formats of the control outcome.
    struct ric_ctrl_outcome_formats_c_ {
        /// Alternatives of the CHOICE, with their ASN.1 names and numbers.
        struct types_opts {
            /// Alternatives of the CHOICE; `nulltype` means none is set.
            enum options { ctrl_outcome_format1, /*...*/ nulltype };
            /// The selected alternative.
            options value;
            typedef uint8_t number_type; ///< Type of the alternative number

            /** @return the ASN.1 name of the alternative */
            const char* to_string() const { return "controlOutcome-Format1"; }
            /** @return the number of the alternative, from 1 */
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>; ///< The selected alternative as an enumerated value

        /** @return the selected alternative */
        types type() const { return types::ctrl_outcome_format1; }
        // getters
        /** Content of ctrl outcome format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        e2sm_dapp_ctrl_outcome_format1_s&       ctrl_outcome_format1() { return c; }
        /** Content of ctrl outcome format 1.
         * @return its content; throws std::bad_variant_access when it is not the selected alternative */
        const e2sm_dapp_ctrl_outcome_format1_s& ctrl_outcome_format1() const { return c; }

        /** @param o value to compare with
         * @return true when every field is equal */
        bool operator==(const ric_ctrl_outcome_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_outcome_formats_c_)

    private:
        e2sm_dapp_ctrl_outcome_format1_s c;
    };

    bool                        ext = false; ///< ASN.1 extension marker; ignored by `pack`, false after `unpack`
    ric_ctrl_outcome_formats_c_ ric_ctrl_outcome_formats; ///< Format of the element, a CHOICE with one alternative per format

    /** Encode as APER.
     * @param out receives the bytes, left empty on failure
     * @return ASN_SUCCESS, or ASN_ERROR_ENCODE_FAIL when a value is outside its constraint */
    asn_code pack(std::vector<uint8_t>& out) const;
    /** Decode APER bytes.
     * @param data bytes to decode
     * @param len number of bytes
     * @return ASN_SUCCESS, or ASN_ERROR_DECODE_FAIL when the bytes are not a valid encoding */
    asn_code unpack(const uint8_t* data, size_t len);
    /** @param o value to compare with
     * @return true when every field is equal */
    bool     operator==(const e2sm_dapp_ctrl_outcome_s& o) const
    {
        return ric_ctrl_outcome_formats == o.ric_ctrl_outcome_formats;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_outcome_s)
};

#undef LIBE3_E2SM_DAPP_NEQ

} // namespace e2sm_dapp
} // namespace libe3

#endif // LIBE3_E2SM_DAPP_HPP
