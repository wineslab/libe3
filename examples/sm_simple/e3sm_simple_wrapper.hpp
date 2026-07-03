/*
 * Wrapper helpers for encoding/decoding the Simple Service Model
 * (placed in examples so they don't affect the main library)
 *
 * The Service Model payload encoding is independent of the E3AP transport
 * encoding, but the example selects it from the same EncodingFormat so a run
 * started with `-e json` or `-e protobuf` uses that encoding end-to-end
 * (envelope and SM payload). The JSON and protobuf branches are only
 * available when libe3 was built with LIBE3_ENABLE_JSON /
 * LIBE3_ENABLE_PROTOBUF respectively; otherwise these helpers use ASN.1 APER.
 */
#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <string>

#include <libe3/types.hpp>

namespace libe3_examples {

struct SimpleIndication {
    uint32_t data1;
    std::optional<uint32_t> timestamp;
};

struct SimpleDAppReport {
    int bin1;
};

struct SimpleConfigControl {
    bool enable;
};

// Encode Simple-Indication into SM payload bytes
bool encode_simple_indication(const SimpleIndication& in, std::vector<uint8_t>& out,
                              libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Decode Simple-Indication from SM payload bytes
bool decode_simple_indication(const std::vector<uint8_t>& in, SimpleIndication& out,
                              libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Encode Simple-Control (samplingThreshold)
bool encode_simple_control(int samplingThreshold, std::vector<uint8_t>& out,
                           libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Decode Simple-Control -> samplingThreshold
bool decode_simple_control(const std::vector<uint8_t>& in, int& samplingThreshold,
                           libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Encode Simple-RanFunctionData into SM payload bytes
bool encode_ran_function_data(const std::string name, std::vector<uint8_t>& out,
                              libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Decode Simple-DAppReport from SM payload bytes (dApp -> RAN report)
bool decode_simple_dapp_report(const std::vector<uint8_t>& in, SimpleDAppReport& out,
                               libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Encode Simple-ConfigControl into SM payload bytes
bool encode_simple_config_control(const SimpleConfigControl& in, std::vector<uint8_t>& out,
                                  libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Decode Simple-ConfigControl from SM payload bytes
bool decode_simple_config_control(const std::vector<uint8_t>& in, SimpleConfigControl& out,
                                  libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

} // namespace libe3_examples
