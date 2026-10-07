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
    static const bool has_ext = Ext;

    enumerated() { EnumType::value = EnumType::nulltype; }
    enumerated(typename EnumType::options v) { EnumType::value = v; }
    operator typename EnumType::options() const { return EnumType::value; }
};

/// PrintableString (SIZE(LB..UB)).
template <uint32_t LB, uint32_t UB, bool Ext = false, bool Aligned = true>
class printable_string
{
public:
    printable_string& from_string(const std::string& s)
    {
        str_ = s;
        return *this;
    }
    const std::string& to_string() const { return str_; }
    size_t             size() const { return str_.size(); }
    size_t             length() const { return str_.size(); }
    bool               operator==(const printable_string& o) const { return str_ == o.str_; }
    bool               operator!=(const printable_string& o) const { return !(*this == o); }

private:
    std::string str_;
};

/// OCTET STRING (SIZE(N)).
template <uint32_t N, bool Aligned = true>
class fixed_octstring : public std::array<uint8_t, N>
{
public:
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

constexpr int64_t max_e2sm_dapp_ind_msg_size             = 32768;
constexpr int64_t max_e2sm_dapp_ctrl_msg_size            = 32768;
constexpr int64_t maxnoof_dapps                          = 256;
constexpr int64_t maxnoof_subscribed_e3_ran_functions    = 64;
constexpr int64_t maxnoof_dapp_report_styles             = 2;
constexpr int64_t maxnoof_dapp_control_styles            = 1;

// ---------------------------------------------------------------------------
// Information elements.
// ---------------------------------------------------------------------------

#define LIBE3_E2SM_DAPP_NEQ(T) \
    bool operator!=(const T& o) const { return !(*this == o); }

// RANfunction-Name ::= SEQUENCE
struct ran_function_name_s {
    bool                                  ext                           = false;
    bool                                  ran_function_instance_present = false;
    printable_string<1, 150, true, true>  ran_function_short_name;
    printable_string<1, 1000, true, true> ran_function_e2sm_o_id;
    printable_string<1, 150, true, true>  ran_function_description;
    int64_t                               ran_function_instance = 0;

    bool operator==(const ran_function_name_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_name_s)
};

// E2SM-DAPP-EventTrigger-Format1 ::= SEQUENCE {}
struct e2sm_dapp_event_trigger_format1_s {
    bool operator==(const e2sm_dapp_event_trigger_format1_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_event_trigger_format1_s)
};

// E2SM-DAPP-EventTrigger ::= SEQUENCE (top-level)
struct e2sm_dapp_event_trigger_s {
    struct ric_event_trigger_formats_c_ {
        struct types_opts {
            enum options { event_trigger_format1, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const { return "eventTrigger-Format1"; }
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>;

        types type() const { return types::event_trigger_format1; }
        // getters
        e2sm_dapp_event_trigger_format1_s&       event_trigger_format1() { return c; }
        const e2sm_dapp_event_trigger_format1_s& event_trigger_format1() const { return c; }

        bool operator==(const ric_event_trigger_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_event_trigger_formats_c_)

    private:
        e2sm_dapp_event_trigger_format1_s c;
    };

    bool                         ext = false;
    ric_event_trigger_formats_c_ ric_event_trigger_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_event_trigger_s& o) const
    {
        return ric_event_trigger_formats == o.ric_event_trigger_formats;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_event_trigger_s)
};

// E2SM-DAPP-ActionDefinition-Format1 ::= SEQUENCE {}
struct e2sm_dapp_action_definition_format1_s {
    bool operator==(const e2sm_dapp_action_definition_format1_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_action_definition_format1_s)
};

// E2SM-DAPP-ActionDefinition ::= SEQUENCE (top-level)
struct e2sm_dapp_action_definition_s {
    struct action_definition_formats_c_ {
        struct types_opts {
            enum options { action_definition_format1, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const { return "actionDefinition-Format1"; }
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>;

        types type() const { return types::action_definition_format1; }
        // getters
        e2sm_dapp_action_definition_format1_s&       action_definition_format1() { return c; }
        const e2sm_dapp_action_definition_format1_s& action_definition_format1() const { return c; }

        bool operator==(const action_definition_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(action_definition_formats_c_)

    private:
        e2sm_dapp_action_definition_format1_s c;
    };

    bool                         ext            = false;
    int64_t                      ric_style_type = 0;
    action_definition_formats_c_ action_definition_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_action_definition_s& o) const
    {
        return ric_style_type == o.ric_style_type && action_definition_formats == o.action_definition_formats;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_action_definition_s)
};

// DAppE3Subscription-Item ::= SEQUENCE
struct dapp_e3_subscription_item_s {
    using subscribed_e3_ran_functions_l_ = bounded_array<uint64_t, 64>;

    bool                           ext     = false;
    uint64_t                       dapp_id = 0;
    subscribed_e3_ran_functions_l_ subscribed_e3_ran_functions;

    bool operator==(const dapp_e3_subscription_item_s& o) const
    {
        return dapp_id == o.dapp_id && subscribed_e3_ran_functions == o.subscribed_e3_ran_functions;
    }
    LIBE3_E2SM_DAPP_NEQ(dapp_e3_subscription_item_s)
};

// DAppE3Subscription-List ::= SEQUENCE (SIZE (0..256)) OF DAppE3Subscription-Item
using dapp_e3_subscription_list_l = dyn_array<dapp_e3_subscription_item_s>;

// RANFunctionDefinition-Report-Item ::= SEQUENCE
struct ran_function_definition_report_item_s {
    bool                                 ext                           = false;
    bool                                 dapp_e3_subscriptions_present = false;
    int64_t                              ric_report_style_type         = 0;
    printable_string<1, 150, true, true> ric_report_style_name;
    int64_t                              ric_ind_hdr_format_type = 0;
    int64_t                              ric_ind_msg_format_type = 0;
    dapp_e3_subscription_list_l          dapp_e3_subscriptions;

    bool operator==(const ran_function_definition_report_item_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_report_item_s)
};

// RANFunctionDefinition-Control-Item ::= SEQUENCE
struct ran_function_definition_ctrl_item_s {
    bool                                 ext                           = false;
    bool                                 dapp_e3_subscriptions_present = false;
    int64_t                              ric_ctrl_style_type           = 0;
    printable_string<1, 150, true, true> ric_ctrl_style_name;
    int64_t                              ric_ctrl_hdr_format_type     = 0;
    int64_t                              ric_ctrl_msg_format_type     = 0;
    int64_t                              ric_ctrl_outcome_format_type = 0;
    dapp_e3_subscription_list_l          dapp_e3_subscriptions;

    bool operator==(const ran_function_definition_ctrl_item_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_ctrl_item_s)
};

// RANFunctionDefinition-EventTrigger ::= SEQUENCE
struct ran_function_definition_event_trigger_s {
    bool ext = false;

    bool operator==(const ran_function_definition_event_trigger_s&) const { return true; }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_event_trigger_s)
};

// RANFunctionDefinition-Report ::= SEQUENCE
struct ran_function_definition_report_s {
    using ric_report_style_list_l_ = dyn_array<ran_function_definition_report_item_s>;

    bool                     ext = false;
    ric_report_style_list_l_ ric_report_style_list;

    bool operator==(const ran_function_definition_report_s& o) const
    {
        return ric_report_style_list == o.ric_report_style_list;
    }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_report_s)
};

// RANFunctionDefinition-Control ::= SEQUENCE
struct ran_function_definition_ctrl_s {
    using ric_ctrl_style_list_l_ = dyn_array<ran_function_definition_ctrl_item_s>;

    bool                   ext = false;
    ric_ctrl_style_list_l_ ric_ctrl_style_list;

    bool operator==(const ran_function_definition_ctrl_s& o) const
    {
        return ric_ctrl_style_list == o.ric_ctrl_style_list;
    }
    LIBE3_E2SM_DAPP_NEQ(ran_function_definition_ctrl_s)
};

// E2SM-DAPP-RANFunctionDefinition ::= SEQUENCE (top-level)
struct e2sm_dapp_ran_function_definition_s {
    bool                                    ext                                           = false;
    bool                                    ran_function_definition_event_trigger_present = false;
    bool                                    ran_function_definition_report_present        = false;
    bool                                    ran_function_definition_ctrl_present          = false;
    ran_function_name_s                     ran_function_name;
    ran_function_definition_event_trigger_s ran_function_definition_event_trigger;
    ran_function_definition_report_s        ran_function_definition_report;
    ran_function_definition_ctrl_s          ran_function_definition_ctrl;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_ran_function_definition_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ran_function_definition_s)
};

// E2SM-DAPP-IndicationHeader-Format1 ::= SEQUENCE
struct e2sm_dapp_ind_hdr_format1_s {
    bool                     ext                   = false;
    bool                     node_cu_du_id_present = false;
    bool                     timestamp_present     = false;
    bool                     sequence_id_present   = false;
    uint64_t                 ran_function_id       = 0;
    uint64_t                 dapp_id               = 0;
    uint8_t                  node_type             = 0;
    fixed_octstring<3, true> node_plmn_id;
    uint64_t                 node_nb_id    = 0;
    uint64_t                 node_cu_du_id = 0;
    int64_t                  timestamp     = 0;
    int64_t                  sequence_id   = 0;

    bool operator==(const e2sm_dapp_ind_hdr_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_format1_s)
};

// E2SM-DAPP-IndicationHeader-Format2 ::= SEQUENCE
struct e2sm_dapp_ind_hdr_format2_s {
    bool                     ext                   = false;
    bool                     node_cu_du_id_present = false;
    bool                     timestamp_present     = false;
    bool                     sequence_id_present   = false;
    uint8_t                  node_type             = 0;
    fixed_octstring<3, true> node_plmn_id;
    uint64_t                 node_nb_id    = 0;
    uint64_t                 node_cu_du_id = 0;
    int64_t                  timestamp     = 0;
    int64_t                  sequence_id   = 0;

    bool operator==(const e2sm_dapp_ind_hdr_format2_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_format2_s)
};

// E2SM-DAPP-IndicationHeader ::= SEQUENCE (top-level)
struct e2sm_dapp_ind_hdr_s {
    struct ric_ind_hdr_formats_c_ {
        struct types_opts {
            enum options { ind_hdr_format1, ind_hdr_format2, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const
            {
                static const char* names[] = {"indicationHeader-Format1", "indicationHeader-Format2"};
                return value < nulltype ? names[value] : "nulltype";
            }
            uint8_t to_number() const { return static_cast<uint8_t>(value + 1); }
        };
        using types = enumerated<types_opts, true>;

        void set(types::options e = types::nulltype)
        {
            type_ = e;
            switch (e) {
                case types::ind_hdr_format1: c_.emplace<1>(); break;
                case types::ind_hdr_format2: c_.emplace<2>(); break;
                default: c_.emplace<0>(); break;
            }
        }
        types type() const { return type_; }
        // getters; throw std::bad_variant_access when the other alternative is set
        e2sm_dapp_ind_hdr_format1_s&       ind_hdr_format1() { return std::get<1>(c_); }
        e2sm_dapp_ind_hdr_format2_s&       ind_hdr_format2() { return std::get<2>(c_); }
        const e2sm_dapp_ind_hdr_format1_s& ind_hdr_format1() const { return std::get<1>(c_); }
        const e2sm_dapp_ind_hdr_format2_s& ind_hdr_format2() const { return std::get<2>(c_); }
        e2sm_dapp_ind_hdr_format1_s&       set_ind_hdr_format1()
        {
            set(types::ind_hdr_format1);
            return std::get<1>(c_);
        }
        e2sm_dapp_ind_hdr_format2_s& set_ind_hdr_format2()
        {
            set(types::ind_hdr_format2);
            return std::get<2>(c_);
        }

        bool operator==(const ric_ind_hdr_formats_c_& o) const { return type_.value == o.type_.value && c_ == o.c_; }
        LIBE3_E2SM_DAPP_NEQ(ric_ind_hdr_formats_c_)

    private:
        types                                                                         type_;
        std::variant<std::monostate, e2sm_dapp_ind_hdr_format1_s, e2sm_dapp_ind_hdr_format2_s> c_;
    };

    bool                   ext = false;
    ric_ind_hdr_formats_c_ ric_ind_hdr_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_ind_hdr_s& o) const { return ric_ind_hdr_formats == o.ric_ind_hdr_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_hdr_s)
};

// E2SM-DAPP-IndicationMessage-Format1 ::= SEQUENCE
struct e2sm_dapp_ind_msg_format1_s {
    bool                              ext       = false;
    uint16_t                          data_size = 0;
    bounded_octstring<1, 32768, true> data;

    bool operator==(const e2sm_dapp_ind_msg_format1_s& o) const { return data_size == o.data_size && data == o.data; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_format1_s)
};

// E2SM-DAPP-IndicationMessage-Format2 ::= SEQUENCE
struct e2sm_dapp_ind_msg_format2_s {
    bool                        ext = false;
    dapp_e3_subscription_list_l dapp_e3_subscriptions;

    bool operator==(const e2sm_dapp_ind_msg_format2_s& o) const
    {
        return dapp_e3_subscriptions == o.dapp_e3_subscriptions;
    }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_format2_s)
};

// E2SM-DAPP-IndicationMessage ::= SEQUENCE (top-level)
struct e2sm_dapp_ind_msg_s {
    struct ric_ind_msg_formats_c_ {
        struct types_opts {
            enum options { ind_msg_format1, ind_msg_format2, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const
            {
                static const char* names[] = {"indicationMessage-Format1", "indicationMessage-Format2"};
                return value < nulltype ? names[value] : "nulltype";
            }
            uint8_t to_number() const { return static_cast<uint8_t>(value + 1); }
        };
        using types = enumerated<types_opts, true>;

        void set(types::options e = types::nulltype)
        {
            type_ = e;
            switch (e) {
                case types::ind_msg_format1: c_.emplace<1>(); break;
                case types::ind_msg_format2: c_.emplace<2>(); break;
                default: c_.emplace<0>(); break;
            }
        }
        types type() const { return type_; }
        // getters; throw std::bad_variant_access when the other alternative is set
        e2sm_dapp_ind_msg_format1_s&       ind_msg_format1() { return std::get<1>(c_); }
        e2sm_dapp_ind_msg_format2_s&       ind_msg_format2() { return std::get<2>(c_); }
        const e2sm_dapp_ind_msg_format1_s& ind_msg_format1() const { return std::get<1>(c_); }
        const e2sm_dapp_ind_msg_format2_s& ind_msg_format2() const { return std::get<2>(c_); }
        e2sm_dapp_ind_msg_format1_s&       set_ind_msg_format1()
        {
            set(types::ind_msg_format1);
            return std::get<1>(c_);
        }
        e2sm_dapp_ind_msg_format2_s& set_ind_msg_format2()
        {
            set(types::ind_msg_format2);
            return std::get<2>(c_);
        }

        bool operator==(const ric_ind_msg_formats_c_& o) const { return type_.value == o.type_.value && c_ == o.c_; }
        LIBE3_E2SM_DAPP_NEQ(ric_ind_msg_formats_c_)

    private:
        types                                                                         type_;
        std::variant<std::monostate, e2sm_dapp_ind_msg_format1_s, e2sm_dapp_ind_msg_format2_s> c_;
    };

    bool                   ext = false;
    ric_ind_msg_formats_c_ ric_ind_msg_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_ind_msg_s& o) const { return ric_ind_msg_formats == o.ric_ind_msg_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ind_msg_s)
};

// E2SM-DAPP-ControlHeader-Format1 ::= SEQUENCE
struct e2sm_dapp_ctrl_hdr_format1_s {
    bool     ext                 = false;
    bool     timestamp_present   = false;
    bool     sequence_id_present = false;
    uint64_t ran_function_id     = 0;
    uint64_t dapp_id             = 0;
    int64_t  timestamp           = 0;
    int64_t  sequence_id         = 0;

    bool operator==(const e2sm_dapp_ctrl_hdr_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_hdr_format1_s)
};

// E2SM-DAPP-ControlHeader ::= SEQUENCE (top-level)
struct e2sm_dapp_ctrl_hdr_s {
    struct ric_ctrl_hdr_formats_c_ {
        struct types_opts {
            enum options { ctrl_hdr_format1, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const { return "controlHeader-Format1"; }
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>;

        types type() const { return types::ctrl_hdr_format1; }
        // getters
        e2sm_dapp_ctrl_hdr_format1_s&       ctrl_hdr_format1() { return c; }
        const e2sm_dapp_ctrl_hdr_format1_s& ctrl_hdr_format1() const { return c; }

        bool operator==(const ric_ctrl_hdr_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_hdr_formats_c_)

    private:
        e2sm_dapp_ctrl_hdr_format1_s c;
    };

    bool                    ext = false;
    ric_ctrl_hdr_formats_c_ ric_ctrl_hdr_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_ctrl_hdr_s& o) const { return ric_ctrl_hdr_formats == o.ric_ctrl_hdr_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_hdr_s)
};

// E2SM-DAPP-ControlMessage-Format1 ::= SEQUENCE
struct e2sm_dapp_ctrl_msg_format1_s {
    bool                              ext       = false;
    uint16_t                          data_size = 0;
    bounded_octstring<1, 32768, true> data;

    bool operator==(const e2sm_dapp_ctrl_msg_format1_s& o) const { return data_size == o.data_size && data == o.data; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_msg_format1_s)
};

// E2SM-DAPP-ControlMessage ::= SEQUENCE (top-level)
struct e2sm_dapp_ctrl_msg_s {
    struct ric_ctrl_msg_formats_c_ {
        struct types_opts {
            enum options { ctrl_msg_format1, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const { return "controlMessage-Format1"; }
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>;

        types type() const { return types::ctrl_msg_format1; }
        // getters
        e2sm_dapp_ctrl_msg_format1_s&       ctrl_msg_format1() { return c; }
        const e2sm_dapp_ctrl_msg_format1_s& ctrl_msg_format1() const { return c; }

        bool operator==(const ric_ctrl_msg_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_msg_formats_c_)

    private:
        e2sm_dapp_ctrl_msg_format1_s c;
    };

    bool                    ext = false;
    ric_ctrl_msg_formats_c_ ric_ctrl_msg_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
    bool     operator==(const e2sm_dapp_ctrl_msg_s& o) const { return ric_ctrl_msg_formats == o.ric_ctrl_msg_formats; }
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_msg_s)
};

// E2SM-DAPP-ControlOutcome-Format1 ::= SEQUENCE
struct e2sm_dapp_ctrl_outcome_format1_s {
    bool                      ext                     = false;
    bool                      timestamp_present       = false;
    bool                      sequence_id_present     = false;
    bool                      e3_ctrl_outcome_present = false;
    int64_t                   timestamp               = 0;
    int64_t                   sequence_id             = 0;
    unbounded_octstring<true> e3_ctrl_outcome;

    bool operator==(const e2sm_dapp_ctrl_outcome_format1_s& o) const;
    LIBE3_E2SM_DAPP_NEQ(e2sm_dapp_ctrl_outcome_format1_s)
};

// E2SM-DAPP-ControlOutcome ::= SEQUENCE (top-level)
struct e2sm_dapp_ctrl_outcome_s {
    struct ric_ctrl_outcome_formats_c_ {
        struct types_opts {
            enum options { ctrl_outcome_format1, /*...*/ nulltype } value;
            typedef uint8_t number_type;

            const char* to_string() const { return "controlOutcome-Format1"; }
            uint8_t     to_number() const { return 1; }
        };
        using types = enumerated<types_opts, true>;

        types type() const { return types::ctrl_outcome_format1; }
        // getters
        e2sm_dapp_ctrl_outcome_format1_s&       ctrl_outcome_format1() { return c; }
        const e2sm_dapp_ctrl_outcome_format1_s& ctrl_outcome_format1() const { return c; }

        bool operator==(const ric_ctrl_outcome_formats_c_& o) const { return c == o.c; }
        LIBE3_E2SM_DAPP_NEQ(ric_ctrl_outcome_formats_c_)

    private:
        e2sm_dapp_ctrl_outcome_format1_s c;
    };

    bool                        ext = false;
    ric_ctrl_outcome_formats_c_ ric_ctrl_outcome_formats;

    asn_code pack(std::vector<uint8_t>& out) const;
    asn_code unpack(const uint8_t* data, size_t len);
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
