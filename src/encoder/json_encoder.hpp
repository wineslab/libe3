/**
 * @file json_encoder.hpp
 * @brief JSON Encoder for E3AP PDUs
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_JSON_ENCODER_HPP
#define LIBE3_JSON_ENCODER_HPP

#include "libe3/e3_encoder.hpp"
#include <nlohmann/json.hpp>

namespace libe3 {

/**
 * @brief JSON encoder for E3AP PDUs
 *
 * Provides JSON encoding/decoding for development and debugging.
 */
class JsonE3Encoder : public E3Encoder {
public:
    JsonE3Encoder() = default;
    ~JsonE3Encoder() override = default;

    EncodeResult<EncodedMessage> encode(const Pdu& pdu) override;
    EncodeResult<Pdu> decode(const EncodedMessage& encoded) override;
    EncodeResult<Pdu> decode(const uint8_t* data, size_t size) override;
    EncodingFormat format() const noexcept override { return EncodingFormat::JSON; }

private:
    // Helper methods for encoding PDU types to JSON
    nlohmann::json encode_setup_request(const SetupRequest& req) const;
    nlohmann::json encode_setup_response(const SetupResponse& resp) const;
    nlohmann::json encode_subscription_request(const SubscriptionRequest& req) const;
    nlohmann::json encode_subscription_delete(const SubscriptionDelete& del) const;
    nlohmann::json encode_subscription_response(const SubscriptionResponse& resp) const;
    nlohmann::json encode_indication_message(const IndicationMessage& msg) const;
    nlohmann::json encode_dapp_control_action(const DAppControlAction& action) const;
    nlohmann::json encode_dapp_report(const DAppReport& report) const;
    nlohmann::json encode_xapp_control_action(const XAppControlAction& action) const;
    nlohmann::json encode_release_message(const ReleaseMessage& msg) const;
    nlohmann::json encode_message_ack(const MessageAck& ack) const;

    // Helper methods for decoding JSON to PDU types
    SetupRequest decode_setup_request(const nlohmann::json& j) const;
    SetupResponse decode_setup_response(const nlohmann::json& j) const;
    SubscriptionRequest decode_subscription_request(const nlohmann::json& j) const;
    SubscriptionDelete decode_subscription_delete(const nlohmann::json& j) const;
    SubscriptionResponse decode_subscription_response(const nlohmann::json& j) const;
    IndicationMessage decode_indication_message(const nlohmann::json& j) const;
    DAppControlAction decode_dapp_control_action(const nlohmann::json& j) const;
    DAppReport decode_dapp_report(const nlohmann::json& j) const;
    XAppControlAction decode_xapp_control_action(const nlohmann::json& j) const;
    ReleaseMessage decode_release_message(const nlohmann::json& j) const;
    MessageAck decode_message_ack(const nlohmann::json& j) const;

    // Binary data encoding helpers (hex string)
    static std::string binary_to_hex(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> hex_to_binary(const std::string& hex);

    // Helper methods for type conversions
    PduType string_to_pdu_type(const std::string& s) const;
    ErrorCode string_to_error_code(const std::string& s) const;
};

} // namespace libe3

#endif // LIBE3_JSON_ENCODER_HPP
