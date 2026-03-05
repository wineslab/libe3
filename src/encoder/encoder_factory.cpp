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
#include "libe3/logger.hpp"

namespace libe3 {

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
        
        default:
            return nullptr;
    }
}

} // namespace libe3
