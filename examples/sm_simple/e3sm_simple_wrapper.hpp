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
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
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
    /// Opaque bytes carried alongside data1/timestamp. Empty means the field is
    /// absent on the wire, so the encoding is byte-identical to a message that
    /// predates it. Used to give the benchmarks a real SM's payload size.
    std::vector<uint8_t> payload;
};

/// Raw payload of a Spectrum PRB-block control: the PRB indices 0..n_prbs-1 as
/// little-endian uint16, so 2 bytes per PRB (0 -> empty, 50 -> 100 B, 273 -> 546 B).
std::vector<uint8_t> make_prb_block_payload(unsigned n_prbs);

/// Deterministic pseudo-random bytes (xorshift, fixed seed), the stand-in for a
/// L1-KPM slot's raw payload. Not zeros, so JSON digit counts are realistic.
std::vector<uint8_t> make_pseudo_random_payload(size_t n_bytes);

struct SimpleDAppReport {
    int bin1;
};

struct SimpleConfigControl {
    bool enable;
};

// Encode Simple-Indication into SM payload bytes
bool encode_simple_indication(const SimpleIndication& in, std::vector<uint8_t>& out,
                              libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Decode Simple-Indication from SM payload bytes (out.payload is empty if absent)
bool decode_simple_indication(const std::vector<uint8_t>& in, SimpleIndication& out,
                              libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1);

// Encode Simple-Control (samplingThreshold, plus an optional opaque payload;
// an empty payload leaves the field absent)
bool encode_simple_control(int samplingThreshold, std::vector<uint8_t>& out,
                           libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1,
                           const std::vector<uint8_t>& payload = {});

// Decode Simple-Control -> samplingThreshold (and, when payload is non-null,
// the opaque payload, empty if absent)
bool decode_simple_control(const std::vector<uint8_t>& in, int& samplingThreshold,
                           libe3::EncodingFormat enc = libe3::EncodingFormat::ASN1,
                           std::vector<uint8_t>* payload = nullptr);

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
