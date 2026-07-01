/**
 * @file encoder_factory.cpp
 * @brief Factory for creating encoder instances
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_encoder.hpp"
#if LIBE3_ENABLE_ASN1
#include "asn1_encoder.hpp"
#endif
#if LIBE3_ENABLE_JSON
#include "json_encoder.hpp"
#endif
#if LIBE3_ENABLE_PROTOBUF
#include "protobuf_encoder.hpp"
#endif
#include "libe3/logger.hpp"

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "EncoderFactory";

// Human-readable name and the CMake flag that compiles in each format, used to
// produce an actionable error when a requested encoding was not built in.
struct EncodingInfo {
    const char* name;
    const char* enable_flag;
};

EncodingInfo encoding_info(EncodingFormat format) {
    switch (format) {
        case EncodingFormat::ASN1:
            return {"ASN.1", "LIBE3_ENABLE_ASN1"};
        case EncodingFormat::JSON:
            return {"JSON", "LIBE3_ENABLE_JSON"};
        case EncodingFormat::PROTOBUF:
            return {"Protocol Buffers", "LIBE3_ENABLE_PROTOBUF"};
    }
    return {"unknown", nullptr};
}
} // namespace

std::unique_ptr<E3Encoder> create_encoder(EncodingFormat format) {
    switch (format) {
#if LIBE3_ENABLE_ASN1
        case EncodingFormat::ASN1:
            return std::make_unique<Asn1E3Encoder>();
#endif
#if LIBE3_ENABLE_JSON
        case EncodingFormat::JSON:
            return std::make_unique<JsonE3Encoder>();
#endif
#if LIBE3_ENABLE_PROTOBUF
        case EncodingFormat::PROTOBUF:
            return std::make_unique<ProtobufE3Encoder>();
#endif
        default:
            break;
    }

    // The requested format's case was either compiled out (its LIBE3_ENABLE_*
    // flag was off in this build) or is not a known EncodingFormat value.
    const EncodingInfo info = encoding_info(format);
    if (info.enable_flag) {
        E3_LOG_ERROR(LOG_TAG)
            << "Encoding format " << info.name
            << " requested but not compiled into this libe3 build; rebuild with -D"
            << info.enable_flag << "=ON";
    } else {
        E3_LOG_ERROR(LOG_TAG) << "Unknown encoding format requested: "
                              << static_cast<unsigned>(format);
    }
    return nullptr;
}

} // namespace libe3
