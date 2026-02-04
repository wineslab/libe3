/**
 * @file asn1_encoder.hpp
 * @brief ASN.1 APER Encoder for E3AP PDUs
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_ASN1_ENCODER_HPP
#define LIBE3_ASN1_ENCODER_HPP

#include "libe3/e3_encoder.hpp"

// Forward declarations for ASN.1 generated types
extern "C" {
struct E3_PDU;
}

namespace libe3 {

/**
 * @brief ASN.1 APER encoder implementation
 *
 * Uses asn1c-generated code to encode/decode E3AP PDUs using
 * Aligned Packed Encoding Rules (APER).
 */
class Asn1E3Encoder : public E3Encoder {
public:
    Asn1E3Encoder() = default;
    ~Asn1E3Encoder() override = default;

    // E3Encoder interface
    EncodeResult<EncodedMessage> encode(const Pdu& pdu) override;
    EncodeResult<Pdu> decode(const EncodedMessage& encoded) override;
    EncodeResult<Pdu> decode(const uint8_t* data, size_t size) override;
    EncodingFormat format() const noexcept override { return EncodingFormat::ASN1; }

private:
    static constexpr size_t BUFFER_SIZE = 65536;

    // Encoding helpers
    E3_PDU* pdu_to_asn1(const Pdu& pdu) const;
    Pdu asn1_to_pdu(const E3_PDU* asn1_pdu) const;
};

} // namespace libe3

#endif // LIBE3_ASN1_ENCODER_HPP
