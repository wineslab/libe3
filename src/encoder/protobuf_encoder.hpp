/**
 * @file protobuf_encoder.hpp
 * @brief Protocol Buffers encoder for E3AP PDUs
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_PROTOBUF_ENCODER_HPP
#define LIBE3_PROTOBUF_ENCODER_HPP

#include "libe3/e3_encoder.hpp"

// Forward declaration of the generated protobuf message (defined in e3ap-1.0.0.pb.h).
namespace libe3 {
namespace e3ap {
namespace v1 {
class E3Pdu;
}
}
}

namespace libe3 {

/**
 * @brief Protocol Buffers encoder implementation
 *
 * Uses protoc-generated C++ code to encode/decode E3AP PDUs with Protocol
 * Buffers wire format.
 */
class ProtobufE3Encoder : public E3Encoder {
public:
    ProtobufE3Encoder() = default;
    ~ProtobufE3Encoder() override = default;

    // E3Encoder interface
    EncodeResult<EncodedMessage> encode(const Pdu& pdu) override;
    EncodeResult<Pdu> decode(const EncodedMessage& encoded) override;
    EncodeResult<Pdu> decode(const uint8_t* data, size_t size) override;
    EncodingFormat format() const noexcept override { return EncodingFormat::PROTOBUF; }

private:
    // Conversion helpers between the C++ Pdu struct and the generated message.
    bool pdu_to_proto(const Pdu& pdu, e3ap::v1::E3Pdu& out) const;
    Pdu proto_to_pdu(const e3ap::v1::E3Pdu& proto) const;
};

} // namespace libe3

#endif // LIBE3_PROTOBUF_ENCODER_HPP
