/*
 * Wrapper helpers for encoding/decoding the Simple Service Model
 * (placed in examples so they don't affect the main library)
 */
#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <string>

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

// Encode Simple-Indication into PER/UPER bytes
bool encode_simple_indication(const SimpleIndication& in, std::vector<uint8_t>& out);

// Decode Simple-Indication from PER/UPER bytes
bool decode_simple_indication(const std::vector<uint8_t>& in, SimpleIndication& out);

// Encode Simple-Control (samplingThreshold)
bool encode_simple_control(int samplingThreshold, std::vector<uint8_t>& out);

// Decode Simple-Control -> samplingThreshold
bool decode_simple_control(const std::vector<uint8_t>& in, int& samplingThreshold);

// Encode Simple-RanFunctionData into APER bytes
bool encode_ran_function_data(const std::string name, std::vector<uint8_t>& out);

// Decode Simple-DAppReport from APER bytes (dApp → RAN report)
bool decode_simple_dapp_report(const std::vector<uint8_t>& in, SimpleDAppReport& out);

// Encode Simple-ConfigControl into APER bytes
bool encode_simple_config_control(const SimpleConfigControl& in, std::vector<uint8_t>& out);

// Decode Simple-ConfigControl from APER bytes
bool decode_simple_config_control(const std::vector<uint8_t>& in, SimpleConfigControl& out);

} // namespace libe3_examples
