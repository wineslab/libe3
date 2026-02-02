/**
 * @file encoder_factory.cpp
 * @brief Factory for creating encoder instances
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_encoder.hpp"
#include "asn1_encoder.hpp"
#include "json_encoder.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

std::unique_ptr<E3Encoder> create_encoder(EncodingFormat format) {
    switch (format) {
        case EncodingFormat::ASN1:
            return std::make_unique<Asn1E3Encoder>();
        
        case EncodingFormat::JSON:
            return std::make_unique<JsonE3Encoder>();
        
        default:
            return nullptr;
    }
}

} // namespace libe3
