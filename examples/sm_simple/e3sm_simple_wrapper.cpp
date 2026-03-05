/* Example wrapper helpers that use ASN.1 generated types.
 * These are compiled only into the examples binary and are not part
 * of the main libe3 public API.
 */

#include "e3sm_simple_wrapper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

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

#include <cstring>
#include <cstdlib>

namespace libe3_examples {

bool encode_simple_indication(const SimpleIndication& in, std::vector<uint8_t>& out) {
    Simple_Indication_t si;
    memset(&si, 0, sizeof(si));
    si.data1 = in.data1;
    if (in.timestamp.has_value()) {
        si.timestamp = (long *)malloc(sizeof(long));
        if(!si.timestamp) return false;
        *si.timestamp = in.timestamp.value();
    }

    uint8_t buffer[1024];
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_Indication, NULL, &si, buffer, sizeof(buffer));
    if (ret.encoded == -1) {
        if (si.timestamp) free(si.timestamp);
        return false;
    }
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer, buffer + bytes);

    if (si.timestamp) free(si.timestamp);
    return true;
}

bool decode_simple_indication(const std::vector<uint8_t>& in, SimpleIndication& out) {
    Simple_Indication_t *si = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_Indication, (void **)&si, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !si) return false;

    out.data1 = (uint32_t)si->data1;
    if (si->timestamp) out.timestamp = static_cast<uint32_t>(*si->timestamp);
    else out.timestamp.reset();

    ASN_STRUCT_FREE(asn_DEF_Simple_Indication, si);
    return true;
}

bool decode_simple_control(const std::vector<uint8_t>& in, int& samplingThreshold) {
    Simple_Control_t *sc = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_Control, (void **)&sc, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !sc) return false;

    if (sc->samplingThreshold) samplingThreshold = (int)*sc->samplingThreshold;
    else {
        ASN_STRUCT_FREE(asn_DEF_Simple_Control, sc);
        return false;
    }

    ASN_STRUCT_FREE(asn_DEF_Simple_Control, sc);
    return true;
}

bool encode_simple_control(int samplingThreshold, std::vector<uint8_t>& out) {
    Simple_Control_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.samplingThreshold = (long *)malloc(sizeof(long));
    if (!sc.samplingThreshold) return false;
    *sc.samplingThreshold = samplingThreshold;

    uint8_t buffer[256];
    asn_enc_rval_t ret = aper_encode_to_buffer(&asn_DEF_Simple_Control, NULL, &sc, buffer, sizeof(buffer));
    if (ret.encoded == -1 ) return false;
    size_t bytes = (ret.encoded + 7) / 8;
    out.assign(buffer, buffer + bytes);

    if (sc.samplingThreshold) free(sc.samplingThreshold);
    return true;
}

bool encode_ran_function_data(const std::string name, std::vector<uint8_t>& out) {
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

bool decode_simple_dapp_report(const std::vector<uint8_t>& in, SimpleDAppReport& out) {
    Simple_DAppReport_t* rep = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_DAppReport, (void**)&rep, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !rep) return false;
    out.bin1 = rep->bin1;
    ASN_STRUCT_FREE(asn_DEF_Simple_DAppReport, rep);
    return true;
}

bool encode_simple_config_control(const SimpleConfigControl& in, std::vector<uint8_t>& out) {
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

bool decode_simple_config_control(const std::vector<uint8_t>& in, SimpleConfigControl& out) {
    Simple_ConfigControl_t* scc = nullptr;
    asn_dec_rval_t dr = aper_decode(NULL, &asn_DEF_Simple_ConfigControl, (void**)&scc, in.data(), in.size(), 0, 0);
    if (dr.code != RC_OK || !scc) return false;
    out.enable = scc->enable ? true : false;
    ASN_STRUCT_FREE(asn_DEF_Simple_ConfigControl, scc);
    return true;
}

} // namespace libe3_examples
