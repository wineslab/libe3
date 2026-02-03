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

    [[nodiscard]] EncodeResult<EncodedMessage> encode(const Pdu& pdu) override;
    [[nodiscard]] EncodeResult<Pdu> decode(const EncodedMessage& encoded) override;
    [[nodiscard]] EncodeResult<Pdu> decode(const uint8_t* data, size_t size) override;
    [[nodiscard]] EncodingFormat format() const noexcept override { return EncodingFormat::JSON; }

private:
    // Helper methods for encoding PDU types to JSON
    [[nodiscard]] nlohmann::json encode_setup_request(const SetupRequest& req) const;
    [[nodiscard]] nlohmann::json encode_setup_response(const SetupResponse& resp) const;
    [[nodiscard]] nlohmann::json encode_subscription_request(const SubscriptionRequest& req) const;
    [[nodiscard]] nlohmann::json encode_subscription_response(const SubscriptionResponse& resp) const;
    [[nodiscard]] nlohmann::json encode_indication_message(const IndicationMessage& msg) const;
    [[nodiscard]] nlohmann::json encode_control_action(const ControlAction& action) const;
    [[nodiscard]] nlohmann::json encode_dapp_report(const DAppReport& report) const;
    [[nodiscard]] nlohmann::json encode_xapp_control_action(const XAppControlAction& action) const;
    [[nodiscard]] nlohmann::json encode_release_message(const ReleaseMessage& msg) const;
    [[nodiscard]] nlohmann::json encode_message_ack(const MessageAck& ack) const;

    // Helper methods for decoding JSON to PDU types
    [[nodiscard]] SetupRequest decode_setup_request(const nlohmann::json& j) const;
    [[nodiscard]] SetupResponse decode_setup_response(const nlohmann::json& j) const;
    [[nodiscard]] SubscriptionRequest decode_subscription_request(const nlohmann::json& j) const;
    [[nodiscard]] SubscriptionResponse decode_subscription_response(const nlohmann::json& j) const;
    [[nodiscard]] IndicationMessage decode_indication_message(const nlohmann::json& j) const;
    [[nodiscard]] ControlAction decode_control_action(const nlohmann::json& j) const;
    [[nodiscard]] DAppReport decode_dapp_report(const nlohmann::json& j) const;
    [[nodiscard]] XAppControlAction decode_xapp_control_action(const nlohmann::json& j) const;
    [[nodiscard]] ReleaseMessage decode_release_message(const nlohmann::json& j) const;
    [[nodiscard]] MessageAck decode_message_ack(const nlohmann::json& j) const;

    // Binary data encoding helpers (hex string)
    [[nodiscard]] static std::string binary_to_hex(const std::vector<uint8_t>& data);
    [[nodiscard]] static std::vector<uint8_t> hex_to_binary(const std::string& hex);
};

} // namespace libe3

#endif // LIBE3_JSON_ENCODER_HPP
