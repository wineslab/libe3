/* Example wrapper helpers that use ASN.1 generated types (and, when libe3 is
 * built with JSON or protobuf support, JSON text payloads or the generated
 * Simple SM protobuf types). These are compiled only into the examples binary
 * and are not part of the main libe3 public API.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "e3sm_simple_wrapper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "libe3/asn1_layout_check.h"
#include "Simple-Indication.h"
#include "Simple-Control.h"
#include "Simple-ConfigControl.h"
#include "Simple-RanFunctionData.h"
#include "Simple-DAppReport.h"
#include "aper_encoder.h"
#include "aper_decoder.h"

#ifdef __cplusplus
}
#endif

#ifdef LIBE3_ENABLE_PROTOBUF
#include "e3sm_simple.pb.h"
#endif

#ifdef LIBE3_ENABLE_JSON
#include <nlohmann/json.hpp>
#endif

#include <cstring>
#include <cstdlib>
#include <memory>

namespace libe3_examples {

std::vector<uint8_t> make_prb_block_payload(unsigned n_prbs) {
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(n_prbs) * 2);
    for (unsigned i = 0; i < n_prbs; ++i) {
        out.push_back(static_cast<uint8_t>(i & 0xFF));
        out.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
    }
    return out;
}

std::vector<uint8_t> make_pseudo_random_payload(size_t n_bytes) {
    std::vector<uint8_t> out(n_bytes);
    uint32_t x = 0x2545F491u;  // fixed seed: the same bytes every run
    for (auto& b : out) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        b = static_cast<uint8_t>(x >> 8);
    }
    return out;
}

#ifdef LIBE3_ENABLE_PROTOBUF
namespace smpb = libe3::sm::simple::v1;
#endif

namespace {
// Frees an OCTET_STRING_t allocated by make_octets (the encoder side builds
// the C structs by hand and owns their buffers).
struct OctetsDeleter {
    void operator()(OCTET_STRING_t* o) const {
        if (!o) return;
        free(o->buf);
        free(o);
    }
};
using OctetsPtr = std::unique_ptr<OCTET_STRING_t, OctetsDeleter>;

OctetsPtr make_octets(const std::vector<uint8_t>& v) {
    OctetsPtr o(static_cast<OCTET_STRING_t*>(calloc(1, sizeof(OCTET_STRING_t))));
    if (!o) return nullptr;
    o->buf = static_cast<uint8_t*>(malloc(v.size()));
    if (!o->buf) return nullptr;
    memcpy(o->buf, v.data(), v.size());
    o->size = v.size();
    return o;
}

// Fixed headroom for the fields around the payload in an APER encoding.
constexpr size_t kAperHeadroom = 64;
}  // namespace

#ifdef LIBE3_ENABLE_JSON
namespace {
// JSON SM payloads are UTF-8 JSON text bytes with camelCase keys, matching
// the JSON envelope encoder convention (the envelope nests the indication
// payload as a JSON object on the wire).
inline void json_to_bytes(const nlohmann::json& j, std::vector<uint8_t>& out) {
    const std::string s = j.dump();
    out.assign(s.begin(), s.end());
}

inline bool bytes_to_json(const std::vector<uint8_t>& in, nlohmann::json& j) {
    j = nlohmann::json::parse(in.begin(), in.end(), nullptr, false);
    return !j.is_discarded();
}
}  // namespace
#endif

bool encode_simple_indication(const SimpleIndication& in, std::vector<uint8_t>& out,
                              libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleIndication m;
        m.set_data1(in.data1);
        if (in.timestamp.has_value()) m.set_timestamp(*in.timestamp);
        if (!in.payload.empty()) m.set_payload(in.payload.data(), in.payload.size());
        std::string s;
        if (!m.SerializeToString(&s)) return false;
        out.assign(s.begin(), s.end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        j["data1"] = in.data1;
        if (in.timestamp.has_value()) j["timestamp"] = *in.timestamp;
        // An array of integers, the shape the real SMs' JSON arrays take.
        if (!in.payload.empty()) j["payload"] = in.payload;
        json_to_bytes(j, out);
        return true;
    }
#endif
    Simple_Indication_t si;
    memset(&si, 0, sizeof(si));
    si.data1 = in.data1;
    if (in.timestamp.has_value()) {
        si.timestamp = (long *)malloc(sizeof(long));
        if(!si.timestamp) return false;
        *si.timestamp = in.timestamp.value();
    }

    OctetsPtr payload;
    if (!in.payload.empty()) {
        payload = make_octets(in.payload);
        if (!payload) {
            free(si.timestamp);
            return false;
        }
        si.payload = payload.get();
    }

    std::vector<uint8_t> buffer(kAperHeadroom + in.payload.size());
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_Indication, NULL, &si, buffer.data(), buffer.size());
    if (ret.encoded == -1) {
        free(si.timestamp);
        return false;
    }
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer.begin(), buffer.begin() + bytes);

    free(si.timestamp);
    return true;
}

bool decode_simple_indication(const std::vector<uint8_t>& in, SimpleIndication& out,
                              libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleIndication m;
        if (!m.ParseFromArray(in.data(), static_cast<int>(in.size()))) return false;
        out.data1 = m.data1();
        if (m.has_timestamp()) out.timestamp = m.timestamp();
        else out.timestamp.reset();
        out.payload.assign(m.payload().begin(), m.payload().end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        if (!bytes_to_json(in, j) || !j.contains("data1")) return false;
        out.data1 = j["data1"].get<uint32_t>();
        if (j.contains("timestamp")) out.timestamp = j["timestamp"].get<uint32_t>();
        else out.timestamp.reset();
        if (j.contains("payload")) out.payload = j["payload"].get<std::vector<uint8_t>>();
        else out.payload.clear();
        return true;
    }
#endif
    Simple_Indication_t *si = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_Indication, (void **)&si, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !si) return false;

    out.data1 = (uint32_t)si->data1;
    if (si->timestamp) out.timestamp = static_cast<uint32_t>(*si->timestamp);
    else out.timestamp.reset();
    if (si->payload) out.payload.assign(si->payload->buf, si->payload->buf + si->payload->size);
    else out.payload.clear();

    ASN_STRUCT_FREE(asn_DEF_Simple_Indication, si);
    return true;
}

bool decode_simple_control(const std::vector<uint8_t>& in, int& samplingThreshold,
                           libe3::EncodingFormat enc, std::vector<uint8_t>* payload) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleControl m;
        if (!m.ParseFromArray(in.data(), static_cast<int>(in.size()))) return false;
        if (!m.has_sampling_threshold()) return false;
        samplingThreshold = static_cast<int>(m.sampling_threshold());
        if (payload) payload->assign(m.payload().begin(), m.payload().end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        if (!bytes_to_json(in, j) || !j.contains("samplingThreshold")) return false;
        samplingThreshold = j["samplingThreshold"].get<int>();
        if (payload) {
            if (j.contains("payload")) *payload = j["payload"].get<std::vector<uint8_t>>();
            else payload->clear();
        }
        return true;
    }
#endif
    Simple_Control_t *sc = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_Control, (void **)&sc, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !sc) return false;

    if (payload) {
        if (sc->payload) payload->assign(sc->payload->buf, sc->payload->buf + sc->payload->size);
        else payload->clear();
    }
    if (sc->samplingThreshold) samplingThreshold = (int)*sc->samplingThreshold;
    else {
        ASN_STRUCT_FREE(asn_DEF_Simple_Control, sc);
        return false;
    }

    ASN_STRUCT_FREE(asn_DEF_Simple_Control, sc);
    return true;
}

bool encode_simple_control(int samplingThreshold, std::vector<uint8_t>& out,
                           libe3::EncodingFormat enc, const std::vector<uint8_t>& payload) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleControl m;
        m.set_sampling_threshold(static_cast<uint32_t>(samplingThreshold));
        if (!payload.empty()) m.set_payload(payload.data(), payload.size());
        std::string s;
        if (!m.SerializeToString(&s)) return false;
        out.assign(s.begin(), s.end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        j["samplingThreshold"] = samplingThreshold;
        if (!payload.empty()) j["payload"] = payload;
        json_to_bytes(j, out);
        return true;
    }
#endif
    Simple_Control_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.samplingThreshold = (long *)malloc(sizeof(long));
    if (!sc.samplingThreshold) return false;
    *sc.samplingThreshold = samplingThreshold;

    OctetsPtr octets;
    if (!payload.empty()) {
        octets = make_octets(payload);
        if (!octets) {
            free(sc.samplingThreshold);
            return false;
        }
        sc.payload = octets.get();
    }

    std::vector<uint8_t> buffer(kAperHeadroom + payload.size());
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_Control, NULL, &sc, buffer.data(), buffer.size());
    if (ret.encoded == -1) {
        free(sc.samplingThreshold);
        return false;
    }
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer.begin(), buffer.begin() + bytes);

    free(sc.samplingThreshold);
    return true;
}

bool encode_ran_function_data(const std::string name, std::vector<uint8_t>& out,
                              libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleRanFunctionData m;
        m.set_name(name);
        std::string s;
        if (!m.SerializeToString(&s)) return false;
        out.assign(s.begin(), s.end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        j["name"] = name;
        json_to_bytes(j, out);
        return true;
    }
#endif
    Simple_RanFunctionData_t srd;
    memset(&srd, 0, sizeof(srd));

    srd.name.buf = (uint8_t *)malloc(name.size());
    if (!srd.name.buf) return false;
    memcpy(srd.name.buf, name.data(), name.size());
    srd.name.size = name.size();

    uint8_t buffer[256];
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_RanFunctionData, NULL, &srd, buffer, sizeof(buffer));
    if (ret.encoded == -1) {
        if (srd.name.buf) free(srd.name.buf);
        return false;
    }
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer, buffer + bytes);

    if (srd.name.buf) free(srd.name.buf);
    return true;
}

bool decode_simple_dapp_report(const std::vector<uint8_t>& in, SimpleDAppReport& out,
                               libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleDAppReport m;
        if (!m.ParseFromArray(in.data(), static_cast<int>(in.size()))) return false;
        out.bin1 = static_cast<int>(m.bin1());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        if (!bytes_to_json(in, j) || !j.contains("bin1")) return false;
        out.bin1 = j["bin1"].get<int>();
        return true;
    }
#endif
    Simple_DAppReport_t* rep = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_DAppReport, (void**)&rep, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !rep) return false;
    out.bin1 = rep->bin1;
    ASN_STRUCT_FREE(asn_DEF_Simple_DAppReport, rep);
    return true;
}

bool encode_simple_config_control(const SimpleConfigControl& in, std::vector<uint8_t>& out,
                                  libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleConfigControl m;
        m.set_enable(in.enable);
        std::string s;
        if (!m.SerializeToString(&s)) return false;
        out.assign(s.begin(), s.end());
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        j["enable"] = in.enable;
        json_to_bytes(j, out);
        return true;
    }
#endif
    Simple_ConfigControl_t scc;
    memset(&scc, 0, sizeof(scc));
    scc.enable = in.enable ? 1 : 0;

    uint8_t buffer[32];
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_ConfigControl, NULL, &scc, buffer, sizeof(buffer));
    if (ret.encoded == -1) {
        return false;
    }
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer, buffer + bytes);
    return true;
}

bool decode_simple_config_control(const std::vector<uint8_t>& in, SimpleConfigControl& out,
                                  libe3::EncodingFormat enc) {
    (void)enc;
#ifdef LIBE3_ENABLE_PROTOBUF
    if (enc == libe3::EncodingFormat::PROTOBUF) {
        smpb::SimpleConfigControl m;
        if (!m.ParseFromArray(in.data(), static_cast<int>(in.size()))) return false;
        out.enable = m.enable();
        return true;
    }
#endif
#ifdef LIBE3_ENABLE_JSON
    if (enc == libe3::EncodingFormat::JSON) {
        nlohmann::json j;
        if (!bytes_to_json(in, j) || !j.contains("enable")) return false;
        out.enable = j["enable"].get<bool>();
        return true;
    }
#endif
    Simple_ConfigControl_t* scc = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_ConfigControl, (void**)&scc, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !scc) return false;
    out.enable = scc->enable ? true : false;
    ASN_STRUCT_FREE(asn_DEF_Simple_ConfigControl, scc);
    return true;
}

} // namespace libe3_examples
