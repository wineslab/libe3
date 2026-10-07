/**
 * @file e2sm_dapp_c.cpp
 * @brief C API of the E2SM-DAPP codec, implemented on top of the C++ one.
 *
 * Every function converts the C struct to its C++ counterpart (or back), calls
 * pack() / unpack(), and maps the result to an e3_error_t. All memory handed to
 * the caller is malloc'ed, so the free_* functions and plain `free` both work.
 * Nothing here lets a C++ exception cross the C boundary.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e2sm_dapp_c.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "libe3/e2sm_dapp.hpp"

namespace {

namespace dapp = libe3::e2sm_dapp;

// Grammar limits that bound the size of what is copied from caller memory, so
// that an absurd length is rejected before it is read, not after.
constexpr size_t k_max_payload    = 32768;
constexpr size_t k_max_name       = 150;
constexpr size_t k_max_oid        = 1000;
constexpr size_t k_max_subs       = 256;
constexpr size_t k_max_rf_per_sub = 64;
constexpr size_t k_max_report_sty = 2;
constexpr size_t k_max_ctrl_sty   = 1;

/// An error to report from inside a conversion; caught at the API boundary.
struct CError {
    e3_error_t code;
};

[[noreturn]] void fail(e3_error_t code)
{
    throw CError{code};
}

// ---------------------------------------------------------------------------
// Small allocation helpers (malloc, so that `free` releases them).
// ---------------------------------------------------------------------------

template <class T>
T* alloc_array(size_t n)
{
    if (n == 0) {
        return nullptr;
    }
    void* p = std::calloc(n, sizeof(T));
    if (p == nullptr) {
        fail(E3_SM_ERROR_MEMORY);
    }
    return static_cast<T*>(p);
}

void set_bytes(libe3_e2sm_dapp_bytes_t& dst, const uint8_t* src, size_t n)
{
    dst.data = alloc_array<uint8_t>(n);
    dst.len  = n;
    if (n != 0) {
        std::memcpy(dst.data, src, n);
    }
}

void set_bytes(libe3_e2sm_dapp_bytes_t& dst, const std::string& s)
{
    set_bytes(dst, reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

// ---------------------------------------------------------------------------
// C -> C++
// ---------------------------------------------------------------------------

std::vector<uint8_t> payload_from(const libe3_e2sm_dapp_bytes_t& b)
{
    // An empty payload is a value the encoder rejects (SIZE(1..32768)): let it.
    if (b.len > k_max_payload) {
        fail(E3_ENCODE_FAILED);
    }
    if (b.len != 0 && b.data == nullptr) {
        fail(E3_INVALID_PARAM);
    }
    return b.len == 0 ? std::vector<uint8_t>() : std::vector<uint8_t>(b.data, b.data + b.len);
}

std::string string_from(const libe3_e2sm_dapp_bytes_t& b, size_t max)
{
    if (b.len > max) {
        fail(E3_ENCODE_FAILED);
    }
    if (b.len != 0 && b.data == nullptr) {
        fail(E3_INVALID_PARAM);
    }
    return b.len == 0 ? std::string() : std::string(reinterpret_cast<const char*>(b.data), b.len);
}

dapp::dapp_e3_subscription_list_l subs_from(const libe3_e2sm_dapp_sub_list_t& s)
{
    if (s.n_items > k_max_subs) {
        fail(E3_ENCODE_FAILED);
    }
    if (s.n_items != 0 && s.items == nullptr) {
        fail(E3_INVALID_PARAM);
    }
    dapp::dapp_e3_subscription_list_l out;
    out.resize(s.n_items);
    for (size_t i = 0; i < s.n_items; ++i) {
        const auto& in = s.items[i];
        if (in.n_ran_functions > k_max_rf_per_sub) {
            fail(E3_ENCODE_FAILED);
        }
        if (in.n_ran_functions != 0 && in.ran_functions == nullptr) {
            fail(E3_INVALID_PARAM);
        }
        out[i].dapp_id = in.dapp_id;
        out[i].subscribed_e3_ran_functions.assign(in.ran_functions, in.ran_functions + in.n_ran_functions);
    }
    return out;
}

void to_cpp(const libe3_e2sm_dapp_ind_hdr_format1_t& in, dapp::e2sm_dapp_ind_hdr_format1_s& out)
{
    out.ran_function_id = in.ran_function_id;
    out.dapp_id         = in.dapp_id;
    out.node_type       = in.node_type;
    std::memcpy(out.node_plmn_id.data(), in.node_plmn_id, 3);
    out.node_nb_id            = in.node_nb_id;
    out.node_cu_du_id_present = in.has_node_cu_du_id;
    out.node_cu_du_id         = in.node_cu_du_id;
    out.timestamp_present     = in.has_timestamp;
    out.timestamp             = in.timestamp;
    out.sequence_id_present   = in.has_sequence_id;
    out.sequence_id           = in.sequence_id;
}

void to_cpp(const libe3_e2sm_dapp_ind_hdr_format2_t& in, dapp::e2sm_dapp_ind_hdr_format2_s& out)
{
    out.node_type = in.node_type;
    std::memcpy(out.node_plmn_id.data(), in.node_plmn_id, 3);
    out.node_nb_id            = in.node_nb_id;
    out.node_cu_du_id_present = in.has_node_cu_du_id;
    out.node_cu_du_id         = in.node_cu_du_id;
    out.timestamp_present     = in.has_timestamp;
    out.timestamp             = in.timestamp;
    out.sequence_id_present   = in.has_sequence_id;
    out.sequence_id           = in.sequence_id;
}

dapp::ran_function_name_s name_from(const libe3_e2sm_dapp_ran_function_name_t& in)
{
    dapp::ran_function_name_s out;
    out.ran_function_short_name.from_string(string_from(in.short_name, k_max_name));
    out.ran_function_e2sm_o_id.from_string(string_from(in.e2sm_oid, k_max_oid));
    out.ran_function_description.from_string(string_from(in.description, k_max_name));
    out.ran_function_instance_present = in.has_instance;
    out.ran_function_instance         = in.instance;
    return out;
}

// ---------------------------------------------------------------------------
// C++ -> C (decode results). Allocations go into `out` as they are made, so the
// caller's free_* can release a half-filled struct.
// ---------------------------------------------------------------------------

void subs_to_c(const dapp::dapp_e3_subscription_list_l& in, libe3_e2sm_dapp_sub_list_t& out)
{
    out.items = alloc_array<libe3_e2sm_dapp_sub_item_t>(in.size());
    out.n_items = 0;
    for (size_t i = 0; i < in.size(); ++i) {
        auto& o  = out.items[i];
        o.dapp_id = static_cast<uint32_t>(in[i].dapp_id);
        o.ran_functions = alloc_array<uint32_t>(in[i].subscribed_e3_ran_functions.size());
        o.n_ran_functions = in[i].subscribed_e3_ran_functions.size();
        for (size_t j = 0; j < o.n_ran_functions; ++j) {
            o.ran_functions[j] = static_cast<uint32_t>(in[i].subscribed_e3_ran_functions[j]);
        }
        out.n_items = i + 1;
    }
}

void to_c(const dapp::e2sm_dapp_ind_hdr_format1_s& in, libe3_e2sm_dapp_ind_hdr_format1_t& out)
{
    out.ran_function_id = static_cast<uint32_t>(in.ran_function_id);
    out.dapp_id         = static_cast<uint32_t>(in.dapp_id);
    out.node_type       = in.node_type;
    std::memcpy(out.node_plmn_id, in.node_plmn_id.data(), 3);
    out.node_nb_id        = static_cast<uint32_t>(in.node_nb_id);
    out.has_node_cu_du_id = in.node_cu_du_id_present;
    out.node_cu_du_id     = in.node_cu_du_id_present ? static_cast<uint32_t>(in.node_cu_du_id) : 0;
    out.has_timestamp     = in.timestamp_present;
    out.timestamp         = in.timestamp_present ? in.timestamp : 0;
    out.has_sequence_id   = in.sequence_id_present;
    out.sequence_id       = in.sequence_id_present ? in.sequence_id : 0;
}

void to_c(const dapp::e2sm_dapp_ind_hdr_format2_s& in, libe3_e2sm_dapp_ind_hdr_format2_t& out)
{
    out.node_type = in.node_type;
    std::memcpy(out.node_plmn_id, in.node_plmn_id.data(), 3);
    out.node_nb_id        = static_cast<uint32_t>(in.node_nb_id);
    out.has_node_cu_du_id = in.node_cu_du_id_present;
    out.node_cu_du_id     = in.node_cu_du_id_present ? static_cast<uint32_t>(in.node_cu_du_id) : 0;
    out.has_timestamp     = in.timestamp_present;
    out.timestamp         = in.timestamp_present ? in.timestamp : 0;
    out.has_sequence_id   = in.sequence_id_present;
    out.sequence_id       = in.sequence_id_present ? in.sequence_id : 0;
}

// ---------------------------------------------------------------------------
// Free helpers.
// ---------------------------------------------------------------------------

void release(libe3_e2sm_dapp_bytes_t& b)
{
    std::free(b.data);
    b.data = nullptr;
    b.len  = 0;
}

void release(libe3_e2sm_dapp_sub_list_t& l)
{
    if (l.items != nullptr) {
        for (size_t i = 0; i < l.n_items; ++i) {
            std::free(l.items[i].ran_functions);
        }
        std::free(l.items);
    }
    l.items   = nullptr;
    l.n_items = 0;
}

void release_def(libe3_e2sm_dapp_ran_function_definition_t& v)
{
    release(v.name.short_name);
    release(v.name.e2sm_oid);
    release(v.name.description);
    if (v.report_styles != nullptr) {
        for (size_t i = 0; i < v.n_report_styles; ++i) {
            release(v.report_styles[i].name);
            release(v.report_styles[i].dapp_e3_subscriptions);
        }
        std::free(v.report_styles);
    }
    if (v.ctrl_styles != nullptr) {
        for (size_t i = 0; i < v.n_ctrl_styles; ++i) {
            release(v.ctrl_styles[i].name);
            release(v.ctrl_styles[i].dapp_e3_subscriptions);
        }
        std::free(v.ctrl_styles);
    }
    std::memset(&v, 0, sizeof(v));
}

// ---------------------------------------------------------------------------
// The shared bodies of the encode_* and decode_* functions.
// ---------------------------------------------------------------------------

/// Pack `v` into a malloc'ed buffer in `*out`.
e3_error_t pack_to_c(const std::vector<uint8_t>& buf, libe3_e2sm_dapp_bytes_t* out)
{
    set_bytes(*out, buf.data(), buf.size());
    return E3_SUCCESS;
}

template <class Cpp, class CIn, class Convert>
e3_error_t encode_impl(const CIn* in, libe3_e2sm_dapp_bytes_t* out, Convert convert)
{
    if (in == nullptr || out == nullptr) {
        return E3_INVALID_PARAM;
    }
    out->data = nullptr;
    out->len  = 0;
    try {
        Cpp v;
        convert(*in, v);
        std::vector<uint8_t> buf;
        if (v.pack(buf) != dapp::ASN_SUCCESS) {
            return E3_ENCODE_FAILED;
        }
        return pack_to_c(buf, out);
    } catch (const CError& e) {
        release(*out);
        return e.code;
    } catch (const std::bad_alloc&) {
        release(*out);
        return E3_SM_ERROR_MEMORY;
    } catch (...) {
        release(*out);
        return E3_INTERNAL_ERROR;
    }
}

template <class Cpp, class COut, class Convert, class Release>
e3_error_t decode_impl(const uint8_t* buf, size_t len, COut* out, Convert convert, Release release_out)
{
    if (out == nullptr) {
        return E3_INVALID_PARAM;
    }
    std::memset(out, 0, sizeof(*out));
    if (buf == nullptr) {
        return E3_INVALID_PARAM;
    }
    try {
        Cpp v;
        if (v.unpack(buf, len) != dapp::ASN_SUCCESS) {
            return E3_DECODE_FAILED;
        }
        convert(v, *out);
        return E3_SUCCESS;
    } catch (const CError& e) {
        release_out(*out);
        std::memset(out, 0, sizeof(*out));
        return e.code;
    } catch (const std::bad_alloc&) {
        release_out(*out);
        std::memset(out, 0, sizeof(*out));
        return E3_SM_ERROR_MEMORY;
    } catch (...) {
        release_out(*out);
        std::memset(out, 0, sizeof(*out));
        return E3_INTERNAL_ERROR;
    }
}

} // namespace

extern "C" {

void libe3_e2sm_dapp_bytes_free(libe3_e2sm_dapp_bytes_t* b)
{
    if (b != nullptr) {
        release(*b);
    }
}

// ---- event trigger ----

e3_error_t libe3_e2sm_dapp_encode_event_trigger(const libe3_e2sm_dapp_event_trigger_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_event_trigger_s>(
        in, out, [](const libe3_e2sm_dapp_event_trigger_t& s, dapp::e2sm_dapp_event_trigger_s&) {
            if (s.format != LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1) {
                fail(E3_INVALID_PARAM);
            }
        });
}

e3_error_t libe3_e2sm_dapp_decode_event_trigger(const uint8_t* buf, size_t len, libe3_e2sm_dapp_event_trigger_t* out)
{
    return decode_impl<dapp::e2sm_dapp_event_trigger_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_event_trigger_s&, libe3_e2sm_dapp_event_trigger_t& d) {
            d.format = LIBE3_E2SM_DAPP_EVENT_TRIGGER_FORMAT_1;
        },
        [](libe3_e2sm_dapp_event_trigger_t&) {});
}

void libe3_e2sm_dapp_free_event_trigger(libe3_e2sm_dapp_event_trigger_t* v)
{
    if (v != nullptr) {
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- action definition ----

e3_error_t libe3_e2sm_dapp_encode_action_definition(const libe3_e2sm_dapp_action_definition_t* in,
                                                    libe3_e2sm_dapp_bytes_t*                   out)
{
    return encode_impl<dapp::e2sm_dapp_action_definition_s>(
        in, out, [](const libe3_e2sm_dapp_action_definition_t& s, dapp::e2sm_dapp_action_definition_s& d) {
            if (s.format != LIBE3_E2SM_DAPP_ACTION_DEFINITION_FORMAT_1) {
                fail(E3_INVALID_PARAM);
            }
            d.ric_style_type = s.ric_style_type;
        });
}

e3_error_t libe3_e2sm_dapp_decode_action_definition(const uint8_t* buf, size_t len, libe3_e2sm_dapp_action_definition_t* out)
{
    return decode_impl<dapp::e2sm_dapp_action_definition_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_action_definition_s& s, libe3_e2sm_dapp_action_definition_t& d) {
            d.format         = LIBE3_E2SM_DAPP_ACTION_DEFINITION_FORMAT_1;
            d.ric_style_type = s.ric_style_type;
        },
        [](libe3_e2sm_dapp_action_definition_t&) {});
}

void libe3_e2sm_dapp_free_action_definition(libe3_e2sm_dapp_action_definition_t* v)
{
    if (v != nullptr) {
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- indication header ----

e3_error_t libe3_e2sm_dapp_encode_ind_hdr(const libe3_e2sm_dapp_ind_hdr_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_ind_hdr_s>(
        in, out, [](const libe3_e2sm_dapp_ind_hdr_t& s, dapp::e2sm_dapp_ind_hdr_s& d) {
            switch (s.format) {
                case LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1:
                    to_cpp(s.frmt_1, d.ric_ind_hdr_formats.set_ind_hdr_format1());
                    break;
                case LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2:
                    to_cpp(s.frmt_2, d.ric_ind_hdr_formats.set_ind_hdr_format2());
                    break;
                default:
                    fail(E3_INVALID_PARAM);
            }
        });
}

e3_error_t libe3_e2sm_dapp_decode_ind_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_hdr_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ind_hdr_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ind_hdr_s& s, libe3_e2sm_dapp_ind_hdr_t& d) {
            using types = dapp::e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types;
            if (s.ric_ind_hdr_formats.type().value == types::ind_hdr_format1) {
                d.format = LIBE3_E2SM_DAPP_IND_HDR_FORMAT_1;
                to_c(s.ric_ind_hdr_formats.ind_hdr_format1(), d.frmt_1);
            } else {
                d.format = LIBE3_E2SM_DAPP_IND_HDR_FORMAT_2;
                to_c(s.ric_ind_hdr_formats.ind_hdr_format2(), d.frmt_2);
            }
        },
        [](libe3_e2sm_dapp_ind_hdr_t&) {});
}

void libe3_e2sm_dapp_free_ind_hdr(libe3_e2sm_dapp_ind_hdr_t* v)
{
    if (v != nullptr) {
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- indication message ----

e3_error_t libe3_e2sm_dapp_encode_ind_msg(const libe3_e2sm_dapp_ind_msg_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_ind_msg_s>(
        in, out, [](const libe3_e2sm_dapp_ind_msg_t& s, dapp::e2sm_dapp_ind_msg_s& d) {
            switch (s.format) {
                case LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1: {
                    auto& f = d.ric_ind_msg_formats.set_ind_msg_format1();
                    static_cast<std::vector<uint8_t>&>(f.data) = payload_from(s.frmt_1.data);
                    // data-size is the payload length, as in the C API's contract.
                    f.data_size = static_cast<uint16_t>(f.data.size());
                    break;
                }
                case LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2:
                    d.ric_ind_msg_formats.set_ind_msg_format2().dapp_e3_subscriptions =
                        subs_from(s.frmt_2.dapp_e3_subscriptions);
                    break;
                default:
                    fail(E3_INVALID_PARAM);
            }
        });
}

e3_error_t libe3_e2sm_dapp_decode_ind_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ind_msg_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ind_msg_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ind_msg_s& s, libe3_e2sm_dapp_ind_msg_t& d) {
            using types = dapp::e2sm_dapp_ind_msg_s::ric_ind_msg_formats_c_::types;
            if (s.ric_ind_msg_formats.type().value == types::ind_msg_format1) {
                d.format        = LIBE3_E2SM_DAPP_IND_MSG_FORMAT_1;
                const auto& dat = s.ric_ind_msg_formats.ind_msg_format1().data;
                set_bytes(d.frmt_1.data, dat.data(), dat.size());
            } else {
                d.format = LIBE3_E2SM_DAPP_IND_MSG_FORMAT_2;
                subs_to_c(s.ric_ind_msg_formats.ind_msg_format2().dapp_e3_subscriptions,
                          d.frmt_2.dapp_e3_subscriptions);
            }
        },
        [](libe3_e2sm_dapp_ind_msg_t& d) {
            release(d.frmt_1.data);
            release(d.frmt_2.dapp_e3_subscriptions);
        });
}

void libe3_e2sm_dapp_free_ind_msg(libe3_e2sm_dapp_ind_msg_t* v)
{
    if (v != nullptr) {
        release(v->frmt_1.data);
        release(v->frmt_2.dapp_e3_subscriptions);
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- control header ----

e3_error_t libe3_e2sm_dapp_encode_ctrl_hdr(const libe3_e2sm_dapp_ctrl_hdr_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_ctrl_hdr_s>(
        in, out, [](const libe3_e2sm_dapp_ctrl_hdr_t& s, dapp::e2sm_dapp_ctrl_hdr_s& d) {
            if (s.format != LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1) {
                fail(E3_INVALID_PARAM);
            }
            auto& f               = d.ric_ctrl_hdr_formats.ctrl_hdr_format1();
            f.ran_function_id     = s.ran_function_id;
            f.dapp_id             = s.dapp_id;
            f.timestamp_present   = s.has_timestamp;
            f.timestamp           = s.timestamp;
            f.sequence_id_present = s.has_sequence_id;
            f.sequence_id         = s.sequence_id;
        });
}

e3_error_t libe3_e2sm_dapp_decode_ctrl_hdr(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_hdr_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ctrl_hdr_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ctrl_hdr_s& s, libe3_e2sm_dapp_ctrl_hdr_t& d) {
            const auto& f       = s.ric_ctrl_hdr_formats.ctrl_hdr_format1();
            d.format            = LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1;
            d.ran_function_id   = static_cast<uint32_t>(f.ran_function_id);
            d.dapp_id           = static_cast<uint32_t>(f.dapp_id);
            d.has_timestamp     = f.timestamp_present;
            d.timestamp         = f.timestamp_present ? f.timestamp : 0;
            d.has_sequence_id   = f.sequence_id_present;
            d.sequence_id       = f.sequence_id_present ? f.sequence_id : 0;
        },
        [](libe3_e2sm_dapp_ctrl_hdr_t&) {});
}

void libe3_e2sm_dapp_free_ctrl_hdr(libe3_e2sm_dapp_ctrl_hdr_t* v)
{
    if (v != nullptr) {
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- control message ----

e3_error_t libe3_e2sm_dapp_encode_ctrl_msg(const libe3_e2sm_dapp_ctrl_msg_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_ctrl_msg_s>(
        in, out, [](const libe3_e2sm_dapp_ctrl_msg_t& s, dapp::e2sm_dapp_ctrl_msg_s& d) {
            if (s.format != LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1) {
                fail(E3_INVALID_PARAM);
            }
            auto& f     = d.ric_ctrl_msg_formats.ctrl_msg_format1();
            static_cast<std::vector<uint8_t>&>(f.data) = payload_from(s.data);
            f.data_size = static_cast<uint16_t>(f.data.size());
        });
}

e3_error_t libe3_e2sm_dapp_decode_ctrl_msg(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_msg_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ctrl_msg_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ctrl_msg_s& s, libe3_e2sm_dapp_ctrl_msg_t& d) {
            const auto& dat = s.ric_ctrl_msg_formats.ctrl_msg_format1().data;
            d.format        = LIBE3_E2SM_DAPP_CTRL_MSG_FORMAT_1;
            set_bytes(d.data, dat.data(), dat.size());
        },
        [](libe3_e2sm_dapp_ctrl_msg_t& d) { release(d.data); });
}

void libe3_e2sm_dapp_free_ctrl_msg(libe3_e2sm_dapp_ctrl_msg_t* v)
{
    if (v != nullptr) {
        release(v->data);
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- control outcome ----

e3_error_t libe3_e2sm_dapp_encode_ctrl_outcome(const libe3_e2sm_dapp_ctrl_outcome_t* in, libe3_e2sm_dapp_bytes_t* out)
{
    return encode_impl<dapp::e2sm_dapp_ctrl_outcome_s>(
        in, out, [](const libe3_e2sm_dapp_ctrl_outcome_t& s, dapp::e2sm_dapp_ctrl_outcome_s& d) {
            if (s.format != LIBE3_E2SM_DAPP_CTRL_OUTCOME_FORMAT_1) {
                fail(E3_INVALID_PARAM);
            }
            auto& f               = d.ric_ctrl_outcome_formats.ctrl_outcome_format1();
            f.timestamp_present   = s.has_timestamp;
            f.timestamp           = s.timestamp;
            f.sequence_id_present = s.has_sequence_id;
            f.sequence_id         = s.sequence_id;
            if (s.has_e3_control_outcome) {
                if (s.e3_control_outcome.len != 0 && s.e3_control_outcome.data == nullptr) {
                    fail(E3_INVALID_PARAM);
                }
                f.e3_ctrl_outcome_present = true;
                f.e3_ctrl_outcome.from_bytes(s.e3_control_outcome.data, s.e3_control_outcome.len);
            }
        });
}

e3_error_t libe3_e2sm_dapp_decode_ctrl_outcome(const uint8_t* buf, size_t len, libe3_e2sm_dapp_ctrl_outcome_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ctrl_outcome_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ctrl_outcome_s& s, libe3_e2sm_dapp_ctrl_outcome_t& d) {
            const auto& f          = s.ric_ctrl_outcome_formats.ctrl_outcome_format1();
            d.format               = LIBE3_E2SM_DAPP_CTRL_OUTCOME_FORMAT_1;
            d.has_timestamp        = f.timestamp_present;
            d.timestamp            = f.timestamp_present ? f.timestamp : 0;
            d.has_sequence_id      = f.sequence_id_present;
            d.sequence_id          = f.sequence_id_present ? f.sequence_id : 0;
            d.has_e3_control_outcome = f.e3_ctrl_outcome_present;
            if (f.e3_ctrl_outcome_present) {
                set_bytes(d.e3_control_outcome, f.e3_ctrl_outcome.data(), f.e3_ctrl_outcome.size());
            }
        },
        [](libe3_e2sm_dapp_ctrl_outcome_t& d) { release(d.e3_control_outcome); });
}

void libe3_e2sm_dapp_free_ctrl_outcome(libe3_e2sm_dapp_ctrl_outcome_t* v)
{
    if (v != nullptr) {
        release(v->e3_control_outcome);
        std::memset(v, 0, sizeof(*v));
    }
}

// ---- RAN function definition ----

e3_error_t libe3_e2sm_dapp_encode_ran_function_definition(const libe3_e2sm_dapp_ran_function_definition_t* in,
                                                          libe3_e2sm_dapp_bytes_t*                         out)
{
    return encode_impl<dapp::e2sm_dapp_ran_function_definition_s>(
        in,
        out,
        [](const libe3_e2sm_dapp_ran_function_definition_t& s, dapp::e2sm_dapp_ran_function_definition_s& d) {
            d.ran_function_name                              = name_from(s.name);
            d.ran_function_definition_event_trigger_present = s.has_event_trigger;

            d.ran_function_definition_report_present = s.has_report;
            if (s.has_report) {
                if (s.n_report_styles > k_max_report_sty) {
                    fail(E3_ENCODE_FAILED);
                }
                if (s.n_report_styles != 0 && s.report_styles == nullptr) {
                    fail(E3_INVALID_PARAM);
                }
                auto& list = d.ran_function_definition_report.ric_report_style_list;
                list.resize(s.n_report_styles);
                for (size_t i = 0; i < s.n_report_styles; ++i) {
                    const auto& in_i = s.report_styles[i];
                    auto&       o    = list[i];
                    o.ric_report_style_type = in_i.report_style_type;
                    o.ric_report_style_name.from_string(string_from(in_i.name, k_max_name));
                    o.ric_ind_hdr_format_type       = in_i.ind_hdr_format_type;
                    o.ric_ind_msg_format_type       = in_i.ind_msg_format_type;
                    o.dapp_e3_subscriptions_present = in_i.has_dapp_e3_subscriptions;
                    if (in_i.has_dapp_e3_subscriptions) {
                        o.dapp_e3_subscriptions = subs_from(in_i.dapp_e3_subscriptions);
                    }
                }
            }

            d.ran_function_definition_ctrl_present = s.has_ctrl;
            if (s.has_ctrl) {
                if (s.n_ctrl_styles > k_max_ctrl_sty) {
                    fail(E3_ENCODE_FAILED);
                }
                if (s.n_ctrl_styles != 0 && s.ctrl_styles == nullptr) {
                    fail(E3_INVALID_PARAM);
                }
                auto& list = d.ran_function_definition_ctrl.ric_ctrl_style_list;
                list.resize(s.n_ctrl_styles);
                for (size_t i = 0; i < s.n_ctrl_styles; ++i) {
                    const auto& in_i = s.ctrl_styles[i];
                    auto&       o    = list[i];
                    o.ric_ctrl_style_type = in_i.ctrl_style_type;
                    o.ric_ctrl_style_name.from_string(string_from(in_i.name, k_max_name));
                    o.ric_ctrl_hdr_format_type      = in_i.ctrl_hdr_format_type;
                    o.ric_ctrl_msg_format_type      = in_i.ctrl_msg_format_type;
                    o.ric_ctrl_outcome_format_type  = in_i.ctrl_outcome_format_type;
                    o.dapp_e3_subscriptions_present = in_i.has_dapp_e3_subscriptions;
                    if (in_i.has_dapp_e3_subscriptions) {
                        o.dapp_e3_subscriptions = subs_from(in_i.dapp_e3_subscriptions);
                    }
                }
            }
        });
}

e3_error_t libe3_e2sm_dapp_decode_ran_function_definition(const uint8_t*                             buf,
                                                          size_t                                     len,
                                                          libe3_e2sm_dapp_ran_function_definition_t* out)
{
    return decode_impl<dapp::e2sm_dapp_ran_function_definition_s>(
        buf,
        len,
        out,
        [](const dapp::e2sm_dapp_ran_function_definition_s& s, libe3_e2sm_dapp_ran_function_definition_t& d) {
            const auto& n = s.ran_function_name;
            set_bytes(d.name.short_name, n.ran_function_short_name.to_string());
            set_bytes(d.name.e2sm_oid, n.ran_function_e2sm_o_id.to_string());
            set_bytes(d.name.description, n.ran_function_description.to_string());
            d.name.has_instance = n.ran_function_instance_present;
            d.name.instance     = n.ran_function_instance_present ? n.ran_function_instance : 0;

            d.has_event_trigger = s.ran_function_definition_event_trigger_present;

            d.has_report = s.ran_function_definition_report_present;
            if (d.has_report) {
                const auto& list = s.ran_function_definition_report.ric_report_style_list;
                d.report_styles  = alloc_array<libe3_e2sm_dapp_report_style_t>(list.size());
                d.n_report_styles = list.size();
                for (size_t i = 0; i < list.size(); ++i) {
                    auto& o                = d.report_styles[i];
                    o.report_style_type    = list[i].ric_report_style_type;
                    o.ind_hdr_format_type  = list[i].ric_ind_hdr_format_type;
                    o.ind_msg_format_type  = list[i].ric_ind_msg_format_type;
                    set_bytes(o.name, list[i].ric_report_style_name.to_string());
                    o.has_dapp_e3_subscriptions = list[i].dapp_e3_subscriptions_present;
                    if (o.has_dapp_e3_subscriptions) {
                        subs_to_c(list[i].dapp_e3_subscriptions, o.dapp_e3_subscriptions);
                    }
                }
            }

            d.has_ctrl = s.ran_function_definition_ctrl_present;
            if (d.has_ctrl) {
                const auto& list = s.ran_function_definition_ctrl.ric_ctrl_style_list;
                d.ctrl_styles    = alloc_array<libe3_e2sm_dapp_ctrl_style_t>(list.size());
                d.n_ctrl_styles  = list.size();
                for (size_t i = 0; i < list.size(); ++i) {
                    auto& o                    = d.ctrl_styles[i];
                    o.ctrl_style_type          = list[i].ric_ctrl_style_type;
                    o.ctrl_hdr_format_type     = list[i].ric_ctrl_hdr_format_type;
                    o.ctrl_msg_format_type     = list[i].ric_ctrl_msg_format_type;
                    o.ctrl_outcome_format_type = list[i].ric_ctrl_outcome_format_type;
                    set_bytes(o.name, list[i].ric_ctrl_style_name.to_string());
                    o.has_dapp_e3_subscriptions = list[i].dapp_e3_subscriptions_present;
                    if (o.has_dapp_e3_subscriptions) {
                        subs_to_c(list[i].dapp_e3_subscriptions, o.dapp_e3_subscriptions);
                    }
                }
            }
        },
        release_def);
}

void libe3_e2sm_dapp_free_ran_function_definition(libe3_e2sm_dapp_ran_function_definition_t* v)
{
    if (v != nullptr) {
        release_def(*v);
    }
}

} // extern "C"
