/**
 * @file bench_encoding_wire.cpp
 * @brief Wire-size sweeps of the E3AP encoders, and the sizes of the E2SM-DAPP
 *        elements around an E3 payload. Sizes only: nothing is timed and no
 *        socket is opened.
 *
 * One mode per run, CSV on stdout, warnings on stderr. Every PDU is decoded
 * and re-encoded and a byte mismatch is reported. Rows come in two variants,
 * `timestamp` on (the library default) and off (Pdu::timestamp = 0).
 *
 *   --payload  timestamp,encoding,message_type,payload_bytes,encoded_bytes,overhead_pct
 *              size against opaque payload size, for the four payload PDUs.
 *              JSON payloads are {"p":"aaa..."} of exactly that many bytes.
 *   --values   timestamp,encoding,message_type,scenario,param,encoded_bytes
 *              min / typical / max per PDU, and one-parameter sweeps (ids,
 *              id-list lengths, RAN function count, optional presence).
 *              NA: the encoder rejected the PDU (the APER encoder has a fixed
 *              output buffer).
 *   --fields   encoding,message_type,field,kind,tag_bytes,length_bytes,
 *              value_bytes,syntax_bytes
 *              APER only, by differential encoding (field present vs absent).
 *              scripts/encoding_fields.py produces the JSON and protobuf rows.
 *   --e2sm     element,variant,payload_bytes,encoded_bytes
 *              E2SM-DAPP elements (built from tests/e2sm_dapp_fixtures.inc)
 *              and the size of each golden vector.
 *   --dump     encoding,message_type,case,hex
 *              the encoded PDUs, for offline field parsing and compression.
 *
 * The exit status is 1 when a round trip or a pack failed, so `ctest` runs
 * `--values` as a check; 2 for a missing mode.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/e3_encoder.hpp>

#ifdef LIBE3_BENCH_E2SM_DAPP
#include <libe3/e2sm_dapp.hpp>
#define E2SM_DAPP_NS libe3::e2sm_dapp
#define E2SM_DAPP_SET_STRING(dst, text) dst.from_string(text)
#define E2SM_DAPP_SET_OCTETS(dst, ptr, n) dst.from_bytes(ptr, n)
#include "../e2sm_dapp_fixtures.inc"
#include "../e2sm_dapp_golden.hpp"
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace libe3;

namespace {

constexpr size_t kTypical = SIZE_MAX;
bool g_failed = false;  // a round trip or an E2SM-DAPP pack went wrong
constexpr uint32_t kMaxMsgId = 4294967295u;

// Every field of every PDU that a sweep varies; fields a PDU lacks are ignored.
struct Params {
    uint32_t msg_id = 1, dapp = 1, rf = 1, ctrl = 1, req = 1, seq = 1;
    uint32_t id_val = 1;                  // value of every list element
    size_t payload = kTypical;            // opaque bytes; JSON: bytes of JSON text
    bool ctrl_seq = false;                // DAppControlAction sequence id
    size_t ver = 5, name = 9, dver = 5, vendor = 8, ran_id = 13;
    size_t n_rf = 1, rf_tel = 1, rf_ctl = 1;
    std::optional<size_t> setup_ver = 5;
    std::optional<uint32_t> setup_dapp = 1;
    size_t tel = 1, ctl = 1;
    std::optional<uint32_t> sub_time = 0, period = 1000;
    uint32_t sub_id = 1;
    std::optional<uint32_t> resp_sub = 1, resp_rf, resp_period;
    std::optional<size_t> granted_tel, granted_ctl;
};

struct Enc {
    const char* name;
    bool json;
    std::unique_ptr<E3Encoder> e;
};

std::vector<uint8_t> make_payload(bool json, size_t n) {
    if (json) {
        if (n == kTypical) {
            const char* s = R"({"data1":42,"timestamp":1234567890})";
            return {s, s + std::strlen(s)};
        }
        std::string t = n == 2 ? "{}" : "{\"p\":\"" + std::string(n - 8, 'a') + "\"}";
        return {t.begin(), t.end()};
    }
    std::vector<uint8_t> v(n == kTypical ? 64 : n);
    for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>(i * 7 + 3);
    return v;
}

std::string str(size_t n) {
    std::string s(n, 'x');
    for (size_t i = 0; i < n; ++i) s[i] = static_cast<char>('a' + i % 26);
    return s;
}

std::vector<uint32_t> ids(size_t n, uint32_t v) { return std::vector<uint32_t>(n, v); }

Pdu build(PduType t, const Params& p, bool json) {
    Pdu pdu(t);
    pdu.message_id = p.msg_id;
    auto data = [&] { return make_payload(json, p.payload); };
    switch (t) {
    case PduType::SETUP_REQUEST: {
        SetupRequest r;
        r.e3ap_protocol_version = str(p.ver);
        r.dapp_name = str(p.name);
        r.dapp_version = str(p.dver);
        r.vendor = str(p.vendor);
        pdu.choice = r;
        break;
    }
    case PduType::SETUP_RESPONSE: {
        SetupResponse r;
        r.request_id = p.req;
        r.response_code = ResponseCode::POSITIVE;
        r.ran_identifier = str(p.ran_id);
        if (p.setup_ver) r.e3ap_protocol_version = str(*p.setup_ver);
        r.dapp_identifier = p.setup_dapp;
        for (size_t i = 0; i < p.n_rf; ++i) {
            RanFunctionDef d;
            d.ran_function_identifier = p.rf;
            d.telemetry_identifier_list = ids(p.rf_tel, p.id_val);
            d.control_identifier_list = ids(p.rf_ctl, p.id_val);
            d.ran_function_data = data();
            r.ran_function_list.push_back(std::move(d));
        }
        pdu.choice = r;
        break;
    }
    case PduType::SUBSCRIPTION_REQUEST: {
        SubscriptionRequest r;
        r.dapp_identifier = p.dapp;
        r.ran_function_identifier = p.rf;
        r.telemetry_identifier_list = ids(p.tel, p.id_val);
        r.control_identifier_list = ids(p.ctl, p.id_val);
        r.subscription_time = p.sub_time;
        r.periodicity = p.period;
        pdu.choice = r;
        break;
    }
    case PduType::SUBSCRIPTION_DELETE: {
        SubscriptionDelete d;
        d.dapp_identifier = p.dapp;
        d.subscription_id = p.sub_id;
        pdu.choice = d;
        break;
    }
    case PduType::SUBSCRIPTION_RESPONSE: {
        SubscriptionResponse r;
        r.request_id = p.req;
        r.dapp_identifier = p.dapp;
        r.response_code = ResponseCode::POSITIVE;
        r.subscription_id = p.resp_sub;
        if (p.granted_tel) r.telemetry_identifier_list = ids(*p.granted_tel, p.id_val);
        if (p.granted_ctl) r.control_identifier_list = ids(*p.granted_ctl, p.id_val);
        r.ran_function_identifier = p.resp_rf;
        r.periodicity = p.resp_period;
        pdu.choice = r;
        break;
    }
    case PduType::INDICATION_MESSAGE: {
        IndicationMessage m;
        m.dapp_identifier = p.dapp;
        m.ran_function_identifier = p.rf;
        m.protocol_data = data();
        pdu.choice = m;
        break;
    }
    case PduType::DAPP_CONTROL_ACTION: {
        DAppControlAction a;
        a.dapp_identifier = p.dapp;
        a.ran_function_identifier = p.rf;
        a.control_identifier = p.ctrl;
        a.action_data = data();
        a.sequence_id = p.ctrl_seq ? p.seq : 0;
        pdu.choice = a;
        break;
    }
    case PduType::DAPP_REPORT: {
        DAppReport r;
        r.dapp_identifier = p.dapp;
        r.ran_function_identifier = p.rf;
        r.report_data = data();
        r.sequence_id = p.seq;
        pdu.choice = r;
        break;
    }
    case PduType::XAPP_CONTROL_ACTION: {
        XAppControlAction a;
        a.dapp_identifier = p.dapp;
        a.ran_function_identifier = p.rf;
        a.xapp_control_data = data();
        a.sequence_id = p.seq;
        pdu.choice = a;
        break;
    }
    case PduType::RELEASE_MESSAGE: {
        ReleaseMessage r;
        r.dapp_identifier = p.dapp;
        pdu.choice = r;
        break;
    }
    case PduType::MESSAGE_ACK: {
        MessageAck a;
        a.request_id = p.req;
        a.response_code = ResponseCode::POSITIVE;
        pdu.choice = a;
        break;
    }
    default:
        break;
    }
    return pdu;
}

struct TypeName {
    PduType type;
    const char* name;
};
const TypeName kTypes[] = {
    {PduType::SETUP_REQUEST, "SetupRequest"},
    {PduType::SETUP_RESPONSE, "SetupResponse"},
    {PduType::SUBSCRIPTION_REQUEST, "SubscriptionRequest"},
    {PduType::SUBSCRIPTION_DELETE, "SubscriptionDelete"},
    {PduType::SUBSCRIPTION_RESPONSE, "SubscriptionResponse"},
    {PduType::INDICATION_MESSAGE, "IndicationMessage"},
    {PduType::DAPP_CONTROL_ACTION, "DAppControlAction"},
    {PduType::DAPP_REPORT, "DAppReport"},
    {PduType::XAPP_CONTROL_ACTION, "XAppControlAction"},
    {PduType::RELEASE_MESSAGE, "ReleaseMessage"},
    {PduType::MESSAGE_ACK, "MessageAck"},
};
const PduType kPayloadTypes[] = {PduType::INDICATION_MESSAGE, PduType::DAPP_CONTROL_ACTION,
                                 PduType::DAPP_REPORT, PduType::XAPP_CONTROL_ACTION};

const char* type_name(PduType t) {
    for (const auto& tn : kTypes)
        if (tn.type == t) return tn.name;
    return "?";
}

std::vector<Enc> make_encoders() {
    std::vector<Enc> v;
    auto add = [&](EncodingFormat f, const char* n, bool json) {
        auto e = create_encoder(f);
        if (e) v.push_back({n, json, std::move(e)});
        else std::fprintf(stderr, "WARN: no encoder for %s\n", n);
    };
#ifdef LIBE3_ENABLE_ASN1
    add(EncodingFormat::ASN1, "asn1", false);
#endif
#ifdef LIBE3_ENABLE_PROTOBUF
    add(EncodingFormat::PROTOBUF, "protobuf", false);
#endif
#ifdef LIBE3_ENABLE_JSON
    add(EncodingFormat::JSON, "json", true);
#endif
    return v;
}

// Encoded bytes of the PDU, or nullopt when it does not encode.
std::optional<std::vector<uint8_t>> encode(Enc& e, Pdu pdu, bool ts, const char* what) {
    if (!ts) pdu.timestamp = 0;
    auto enc = e.e->encode(pdu);
    if (!enc) {
        std::fprintf(stderr, "WARN: encode failed %s/%s\n", e.name, what);
        return std::nullopt;
    }
    auto back = e.e->decode(*enc);
    bool same = false;
    if (back) {
        auto again = e.e->encode(*back);
        same = again && again->buffer == enc->buffer;
    }
    if (!same) {
        std::fprintf(stderr, "WARN: round trip mismatch %s/%s\n", e.name, what);
        g_failed = true;
    }
    return enc->buffer;
}

std::optional<size_t> size_of(Enc& e, PduType t, const Params& p, bool ts, const std::string& what) {
    auto b = encode(e, build(t, p, e.json), ts, (std::string(type_name(t)) + ":" + what).c_str());
    if (!b) return std::nullopt;
    return b->size();
}

const char* ts_name(bool ts) { return ts ? "on" : "off"; }

// ---------------------------------------------------------------------------

void run_payload(std::vector<Enc>& encs) {
    std::printf("timestamp,encoding,message_type,payload_bytes,encoded_bytes,overhead_pct\n");
    const size_t sizes[] = {1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    for (bool ts : {true, false})
        for (auto& e : encs)
            for (PduType t : kPayloadTypes)
                for (size_t n : sizes) {
                    size_t actual = e.json && n == 1 ? 2 : n;
                    Params p;
                    p.payload = actual;
                    auto sz = size_of(e, t, p, ts, std::to_string(n));
                    if (!sz) continue;
                    std::printf("%s,%s,%s,%zu,%zu,%.2f\n", ts_name(ts), e.name, type_name(t), actual, *sz,
                                100.0 * (static_cast<double>(*sz) - static_cast<double>(actual)) /
                                    static_cast<double>(actual));
                }
}

Params min_params() {
    Params p;
    p.msg_id = p.dapp = p.rf = p.ctrl = p.req = p.seq = 1;
    p.ctrl_seq = false;
    p.ver = p.name = p.dver = p.vendor = p.ran_id = 1;
    p.n_rf = 0;
    p.setup_ver.reset();
    p.setup_dapp.reset();
    p.tel = p.ctl = 0;
    p.sub_time.reset();
    p.period.reset();
    p.resp_sub.reset();
    return p;
}

Params max_params() {
    Params p;
    p.msg_id = p.req = p.seq = kMaxMsgId;
    p.dapp = p.rf = p.ctrl = p.sub_id = 65535;
    p.id_val = 65535;
    p.ctrl_seq = true;
    p.ver = p.dver = 32;
    p.name = p.vendor = p.ran_id = 64;
    p.n_rf = 64;
    p.rf_tel = p.rf_ctl = 256;
    p.setup_ver = 32;
    p.setup_dapp = 65535;
    p.tel = p.ctl = 256;
    p.sub_time = 86400;
    p.period = 60000000;
    p.resp_sub = 65535;
    p.resp_rf = 65535;
    p.resp_period = 60000000;
    p.granted_tel = p.granted_ctl = 256;
    return p;
}

void run_values(std::vector<Enc>& encs) {
    std::printf("timestamp,encoding,message_type,scenario,param,encoded_bytes\n");
    for (bool ts : {true, false})
        for (auto& e : encs) {
            auto row = [&](PduType t, const char* scenario, const std::string& param, const Params& p) {
                auto sz = size_of(e, t, p, ts, std::string(scenario) + "=" + param);
                std::printf("%s,%s,%s,%s,%s,%s\n", ts_name(ts), e.name, type_name(t), scenario,
                            param.c_str(), sz ? std::to_string(*sz).c_str() : "NA");
            };
            for (const auto& tn : kTypes) {
                row(tn.type, "min", "", min_params());
                row(tn.type, "typical", "", Params());
                row(tn.type, "max", "", max_params());
            }
            for (size_t n : {1, 2, 4, 8, 16, 32, 64}) {
                Params p;
                p.n_rf = n;
                row(PduType::SETUP_RESPONSE, "rf_count", std::to_string(n), p);
            }
            for (size_t n : {1, 2, 4, 8, 16, 32, 64}) {
                Params p;
                p.n_rf = n;
                p.rf_tel = p.rf_ctl = 256;
                p.id_val = 65535;
                row(PduType::SETUP_RESPONSE, "rf_count_full_lists", std::to_string(n), p);
            }
            for (size_t n : {0, 1, 2, 4, 8, 16, 32, 64, 128, 256}) {
                Params p;
                p.rf_tel = p.rf_ctl = n;
                row(PduType::SETUP_RESPONSE, "rf_list_len", std::to_string(n), p);
                p = Params();
                p.tel = p.ctl = n;
                row(PduType::SUBSCRIPTION_REQUEST, "id_list_len", std::to_string(n), p);
                p.id_val = 65535;
                row(PduType::SUBSCRIPTION_REQUEST, "id_list_len_wide", std::to_string(n), p);
                p = Params();
                p.granted_tel = p.granted_ctl = n;
                row(PduType::SUBSCRIPTION_RESPONSE, "granted_list_len", std::to_string(n), p);
            }
            row(PduType::SUBSCRIPTION_RESPONSE, "granted_list_len", "absent", Params());
            for (uint32_t v : {1u, 127u, 128u, 255u, 256u, 65535u}) {
                Params p;
                p.dapp = v;
                row(PduType::RELEASE_MESSAGE, "dapp_id", std::to_string(v), p);
            }
            for (uint32_t v : {1u, 127u, 128u, 255u, 256u, 16383u, 16384u, 65535u, 65536u, 16777215u,
                               16777216u, kMaxMsgId}) {
                Params p;
                p.msg_id = v;
                row(PduType::RELEASE_MESSAGE, "msg_id", std::to_string(v), p);
            }
            struct Opt {
                PduType t;
                const char* name;
                std::function<void(Params&, bool)> set;
            };
            const Opt opts[] = {
                {PduType::SETUP_RESPONSE, "opt_e3ap_protocol_version",
                 [](Params& p, bool on) { p.setup_ver = on ? std::optional<size_t>(5) : std::nullopt; }},
                {PduType::SETUP_RESPONSE, "opt_dapp_identifier",
                 [](Params& p, bool on) { p.setup_dapp = on ? std::optional<uint32_t>(1) : std::nullopt; }},
                {PduType::SUBSCRIPTION_REQUEST, "opt_subscription_time",
                 [](Params& p, bool on) { p.sub_time = on ? std::optional<uint32_t>(0) : std::nullopt; }},
                {PduType::SUBSCRIPTION_REQUEST, "opt_periodicity",
                 [](Params& p, bool on) { p.period = on ? std::optional<uint32_t>(1000) : std::nullopt; }},
                {PduType::SUBSCRIPTION_RESPONSE, "opt_subscription_id",
                 [](Params& p, bool on) { p.resp_sub = on ? std::optional<uint32_t>(1) : std::nullopt; }},
                {PduType::SUBSCRIPTION_RESPONSE, "opt_granted_telemetry",
                 [](Params& p, bool on) { p.granted_tel = on ? std::optional<size_t>(1) : std::nullopt; }},
                {PduType::SUBSCRIPTION_RESPONSE, "opt_granted_control",
                 [](Params& p, bool on) { p.granted_ctl = on ? std::optional<size_t>(1) : std::nullopt; }},
                {PduType::SUBSCRIPTION_RESPONSE, "opt_ran_function_identifier",
                 [](Params& p, bool on) { p.resp_rf = on ? std::optional<uint32_t>(1) : std::nullopt; }},
                {PduType::SUBSCRIPTION_RESPONSE, "opt_periodicity",
                 [](Params& p, bool on) { p.resp_period = on ? std::optional<uint32_t>(1000) : std::nullopt; }},
                {PduType::DAPP_CONTROL_ACTION, "opt_sequence_id",
                 [](Params& p, bool on) { p.ctrl_seq = on; }},
            };
            for (const auto& o : opts)
                for (bool on : {false, true}) {
                    Params p;
                    o.set(p, on);
                    row(o.t, o.name, on ? "present" : "absent", p);
                }
        }
}

void run_fields(std::vector<Enc>& encs) {
    std::printf("encoding,message_type,field,kind,tag_bytes,length_bytes,value_bytes,syntax_bytes\n");
    for (auto& e : encs) {
        if (std::strcmp(e.name, "asn1") != 0) continue;
        auto row = [&](PduType t, const std::string& field, const char* kind, long v) {
            std::printf("%s,%s,%s,%s,0,0,%ld,0\n", e.name, type_name(t), field.c_str(), kind, v);
        };
        for (const auto& tn : kTypes) {
            const PduType t = tn.type;
            const Params base;
            auto total = size_of(e, t, base, true, "total");
            auto no_ts = size_of(e, t, base, false, "no_ts");
            if (!total || !no_ts) continue;
            long sum = 0;
            auto part = [&](const std::string& field, const char* kind, long v) {
                row(t, field, kind, v);
                sum += v;
            };
            part("timestamp", "timestamp", static_cast<long>(*total) - static_cast<long>(*no_ts));

            const size_t n = base.payload == kTypical ? 64 : base.payload;
            if (t == PduType::SETUP_RESPONSE || t == PduType::INDICATION_MESSAGE ||
                t == PduType::DAPP_CONTROL_ACTION || t == PduType::DAPP_REPORT ||
                t == PduType::XAPP_CONTROL_ACTION)
                part("payload", "payload", static_cast<long>(n * (t == PduType::SETUP_RESPONSE ? base.n_rf : 1)));

            struct Opt {
                PduType t;
                const char* name;
                std::function<void(Params&, bool)> set;
                bool typical_present;
            };
            const Opt opts[] = {
                {PduType::SETUP_RESPONSE, "e3ap_protocol_version",
                 [](Params& p, bool on) { p.setup_ver = on ? std::optional<size_t>(5) : std::nullopt; }, true},
                {PduType::SETUP_RESPONSE, "dapp_identifier",
                 [](Params& p, bool on) { p.setup_dapp = on ? std::optional<uint32_t>(1) : std::nullopt; }, true},
                {PduType::SUBSCRIPTION_REQUEST, "subscription_time",
                 [](Params& p, bool on) { p.sub_time = on ? std::optional<uint32_t>(0) : std::nullopt; }, true},
                {PduType::SUBSCRIPTION_REQUEST, "periodicity",
                 [](Params& p, bool on) { p.period = on ? std::optional<uint32_t>(1000) : std::nullopt; }, true},
                {PduType::SUBSCRIPTION_RESPONSE, "subscription_id",
                 [](Params& p, bool on) { p.resp_sub = on ? std::optional<uint32_t>(1) : std::nullopt; }, true},
                {PduType::SUBSCRIPTION_RESPONSE, "granted_telemetry_list",
                 [](Params& p, bool on) { p.granted_tel = on ? std::optional<size_t>(1) : std::nullopt; }, false},
                {PduType::SUBSCRIPTION_RESPONSE, "granted_control_list",
                 [](Params& p, bool on) { p.granted_ctl = on ? std::optional<size_t>(1) : std::nullopt; }, false},
                {PduType::SUBSCRIPTION_RESPONSE, "ran_function_identifier",
                 [](Params& p, bool on) { p.resp_rf = on ? std::optional<uint32_t>(1) : std::nullopt; }, false},
                {PduType::SUBSCRIPTION_RESPONSE, "periodicity",
                 [](Params& p, bool on) { p.resp_period = on ? std::optional<uint32_t>(1000) : std::nullopt; }, false},
                {PduType::DAPP_CONTROL_ACTION, "sequence_id",
                 [](Params& p, bool on) { p.ctrl_seq = on; }, false},
            };
            for (const auto& o : opts) {
                if (o.t != t) continue;
                Params off = base, on = base;
                o.set(off, false);
                o.set(on, true);
                auto a = size_of(e, t, off, true, o.name);
                auto b = size_of(e, t, on, true, o.name);
                if (!a || !b) continue;
                const long d = static_cast<long>(*b) - static_cast<long>(*a);
                if (o.typical_present) part(o.name, "optional", d);
                else row(t, o.name, "optional_if_present", d);
            }

            if (t == PduType::SUBSCRIPTION_REQUEST) {
                Params empty = base;
                empty.tel = empty.ctl = 0;
                auto z = size_of(e, t, empty, true, "lists_empty");
                if (z) part("id_list_elements", "list", static_cast<long>(*total) - static_cast<long>(*z));
            }
            if (t == PduType::SETUP_RESPONSE) {
                Params empty = base;
                empty.rf_tel = empty.rf_ctl = 0;
                auto z = size_of(e, t, empty, true, "lists_empty");
                if (z) part("id_list_elements", "list", static_cast<long>(*total) - static_cast<long>(*z));
            }
            row(t, "ids_lengths_preamble", "residual", static_cast<long>(*total) - sum);
        }
    }
}

void run_dump(std::vector<Enc>& encs) {
    std::printf("encoding,message_type,case,hex\n");
    auto hex = [](const std::vector<uint8_t>& b) {
        std::string s;
        char c[3];
        for (uint8_t x : b) {
            std::snprintf(c, sizeof c, "%02x", x);
            s += c;
        }
        return s;
    };
    // Incompressible payload, so that a compressor only sees the framing.
    auto random_payload = [](bool json, size_t n) {
        uint64_t x = 0x9e3779b97f4a7c15ull;
        auto next = [&] {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            return static_cast<uint8_t>(x >> 24);
        };
        std::vector<uint8_t> v;
        if (json) {
            const char* hexd = "0123456789abcdef";
            std::string s = "{\"p\":\"";
            while (s.size() + 2 < n) s += hexd[next() & 15];
            s += "\"}";
            v.assign(s.begin(), s.end());
        } else {
            for (size_t i = 0; i < n; ++i) v.push_back(next());
        }
        return v;
    };
    for (auto& e : encs) {
        for (const auto& tn : kTypes) {
            auto b = encode(e, build(tn.type, Params(), e.json), true, tn.name);
            if (b) std::printf("%s,%s,typical,%s\n", e.name, tn.name, hex(*b).c_str());
        }
        for (size_t n : {64, 1024, 16384}) {
            for (const auto& tn : kTypes) {
                bool carries = false;
                for (PduType t : kPayloadTypes) carries |= t == tn.type;
                if (n != 64 && !carries) continue;
                Pdu pdu = build(tn.type, Params(), e.json);
                auto data = random_payload(e.json, n);
                if (auto* m = std::get_if<IndicationMessage>(&pdu.choice)) m->protocol_data = data;
                else if (auto* m = std::get_if<DAppControlAction>(&pdu.choice)) m->action_data = data;
                else if (auto* m = std::get_if<DAppReport>(&pdu.choice)) m->report_data = data;
                else if (auto* m = std::get_if<XAppControlAction>(&pdu.choice)) m->xapp_control_data = data;
                else if (auto* m = std::get_if<SetupResponse>(&pdu.choice)) {
                    for (auto& f : m->ran_function_list) f.ran_function_data = data;
                }
                auto b = encode(e, pdu, true, tn.name);
                if (b) std::printf("%s,%s,rand_%zu,%s\n", e.name, tn.name, n, hex(*b).c_str());
            }
        }
    }
}

#ifdef LIBE3_BENCH_E2SM_DAPP
template <class T>
size_t packed(const T& v) {
    std::vector<uint8_t> out;
    if (v.pack(out) != libe3::e2sm_dapp::ASN_SUCCESS) {
        std::fprintf(stderr, "WARN: E2SM-DAPP pack failed\n");
        g_failed = true;
        return 0;
    }
    return out.size();
}

void run_e2sm() {
    namespace fx = e2sm_dapp_fixtures;
    std::printf("element,variant,payload_bytes,encoded_bytes\n");
    auto row = [](const char* el, const std::string& var, long payload, size_t bytes) {
        std::printf("%s,%s,%s,%zu\n", el, var.c_str(), payload < 0 ? "" : std::to_string(payload).c_str(),
                    bytes);
    };
    const size_t ih1_full = packed(fx::ih1_full()), ih1_min = packed(fx::ih1_min());
    const size_t ch1_full = packed(fx::ch1_full()), ch1_min = packed(fx::ch1_min());
    row("ind_hdr_format1", "full", -1, ih1_full);
    row("ind_hdr_format1", "min", -1, ih1_min);
    row("ind_hdr_format2", "full", -1, packed(fx::ih2_full()));
    row("ind_hdr_format2", "min", -1, packed(fx::ih2_min()));
    row("ctrl_hdr_format1", "full", -1, ch1_full);
    row("ctrl_hdr_format1", "min", -1, ch1_min);
    row("ind_msg_format2", "empty", -1, packed(fx::im2_empty()));
    row("ind_msg_format2", "one", -1, packed(fx::im2_one()));
    row("ind_msg_format2", "multi", -1, packed(fx::im2_multi()));
    row("ctrl_outcome_format1", "full", -1, packed(fx::co1_full()));
    row("ctrl_outcome_format1", "min", -1, packed(fx::co1_min()));
    row("ctrl_outcome_format1", "tsseq", -1, packed(fx::co1_tsseq()));
    row("ctrl_outcome_format1", "payload", -1, packed(fx::co1_payload()));
    for (size_t n : {1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768}) {
        const auto bytes = fx::pattern(n, 7, 3);
        const size_t im = packed(fx::im1_with(bytes)), cm = packed(fx::cm1_with(bytes));
        const long p = static_cast<long>(n);
        row("ind_msg_format1", "payload", p, im);
        row("ctrl_msg_format1", "payload", p, cm);
        row("ind_total", "hdr1_full+msg1", p, ih1_full + im);
        row("ind_total", "hdr1_min+msg1", p, ih1_min + im);
        row("ctrl_total", "hdr1_full+msg1", p, ch1_full + cm);
        row("ctrl_total", "hdr1_min+msg1", p, ch1_min + cm);
    }
    for (const auto& g : libe3_e2sm_dapp_test::golden_vectors)
        row("golden", g.name, -1, std::strlen(g.hex) / 2);
    for (const auto& g : libe3_e2sm_dapp_test::golden_hashes) row("golden", g.name, -1, g.length);
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "--e2sm") {
#ifdef LIBE3_BENCH_E2SM_DAPP
        run_e2sm();
        return g_failed ? 1 : 0;
#else
        std::fprintf(stderr, "built without the E2SM-DAPP codec\n");
        return 1;
#endif
    }
    auto encs = make_encoders();
    if (mode == "--payload") run_payload(encs);
    else if (mode == "--values") run_values(encs);
    else if (mode == "--fields") run_fields(encs);
    else if (mode == "--dump") run_dump(encs);
    else {
        std::fprintf(stderr, "usage: %s --payload|--values|--fields|--e2sm|--dump\n", argv[0]);
        return 2;
    }
    return g_failed ? 1 : 0;
}
