/**
 * @file e3_encoder.hpp
 * @brief Abstract E3 Encoder interface for PDU encoding/decoding
 *
 * Defines the abstract interface for encoding and decoding E3AP PDUs.
 * Ported from the original C implementation's e3ap_handler functions.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_E3_ENCODER_HPP
#define LIBE3_E3_ENCODER_HPP

#include "types.hpp"
#include <memory>
#include <optional>
#include <tl/expected.hpp>

namespace libe3 {

/**
 * @brief Result type for encoding/decoding operations
 */
template<typename T>
using EncodeResult = tl::expected<T, ErrorCode>;

/**
 * @brief Abstract base class for E3AP encoders
 *
 * This class defines the interface for encoding and decoding E3AP PDUs.
 * The design allows for multiple encoding formats (ASN.1, JSON) while
 * maintaining the same API.
 *
 * Derived classes:
 * - ASN1E3Encoder: ASN.1 PER encoding (O-RAN standard)
 * - JsonE3Encoder: JSON encoding (for development/debugging)
 */
class E3Encoder {
public:
    virtual ~E3Encoder() = default;

    // Non-copyable, movable
    E3Encoder(const E3Encoder&) = delete;
    E3Encoder& operator=(const E3Encoder&) = delete;
    E3Encoder(E3Encoder&&) = default;
    E3Encoder& operator=(E3Encoder&&) = default;

    /**
     * @brief Encode a PDU to bytes
     * @param pdu PDU to encode
     * @return Encoded message on success, error code on failure
     */
    virtual EncodeResult<EncodedMessage> encode(const Pdu& pdu) = 0;

    /**
     * @brief Decode bytes to a PDU
     * @param encoded Encoded message to decode
     * @return Decoded PDU on success, error code on failure
     */
    virtual EncodeResult<Pdu> decode(const EncodedMessage& encoded) = 0;

    /**
     * @brief Decode bytes to a PDU
     * @param data Raw data buffer
     * @param size Size of data
     * @return Decoded PDU on success, error code on failure
     */
    virtual EncodeResult<Pdu> decode(const uint8_t* data, size_t size) = 0;

    /**
     * @brief Get the encoding format
     */
    virtual EncodingFormat format() const noexcept = 0;

    // Convenience methods for creating specific PDUs

    /**
     * @brief Create and encode a Setup Request PDU
     * @param message_id Unique message ID
     * @param e3ap_protocol_version E3AP protocol version string (e.g., "0.0.0")
     * @param dapp_name Name of the dApp
     * @param dapp_version Version of the dApp (e.g., "0.0.0")
     * @param vendor Vendor name (max 30 chars)
     */
    EncodeResult<EncodedMessage> encode_setup_request(
        uint32_t message_id,
        const std::string& e3ap_protocol_version,
        const std::string& dapp_name,
        const std::string& dapp_version,
        const std::string& vendor
    );

    /**
     * @brief Create and encode a Setup Response PDU
     * @param message_id Unique message ID
     * @param request_id ID of the corresponding SetupRequest
     * @param response_code Response code (positive/negative)
     * @param e3ap_protocol_version E3AP protocol version (optional)
     * @param dapp_identifier Assigned dApp identifier (optional)
     * @param ran_identifier RAN identifier
     * @param ran_function_list List of available RAN functions (optional)
     */
    EncodeResult<EncodedMessage> encode_setup_response(
        uint32_t message_id,
        uint32_t request_id,
        ResponseCode response_code,
        const std::optional<std::string>& e3ap_protocol_version = std::nullopt,
        const std::optional<uint32_t>& dapp_identifier = std::nullopt,
        const std::string& ran_identifier = "",
        const std::vector<RanFunctionDef>& ran_function_list = {}
    );

    /**
     * @brief Create and encode a Subscription Request PDU
     * @param message_id Unique message ID
     * @param dapp_identifier dApp identifier
     * @param ran_function_identifier RAN function to subscribe to
     * @param telemetry_identifier_list List of telemetry identifiers
     * @param control_identifier_list List of control identifiers
     * @param subscription_time How long to keep the subscription (0-3600 sec, optional)
     */
    EncodeResult<EncodedMessage> encode_subscription_request(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t ran_function_identifier,
        const std::vector<uint32_t>& telemetry_identifier_list,
        const std::vector<uint32_t>& control_identifier_list,
        const std::optional<uint32_t>& subscription_time = std::nullopt
    );

    /**
     * @brief Create and encode a Subscription Delete PDU
     * @param message_id Unique message ID
     * @param dapp_identifier dApp identifier
     * @param subscription_id Subscription ID to delete
     */
    EncodeResult<EncodedMessage> encode_subscription_delete(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t subscription_id
    );

    /**
     * @brief Create and encode a Subscription Response PDU
     * @param message_id Unique message ID
     * @param request_id ID of the corresponding SubscriptionRequest
     * @param dapp_identifier dApp identifier
     * @param response_code Response code (positive/negative)
     * @param subscription_id Subscription ID (optional)
     */
    EncodeResult<EncodedMessage> encode_subscription_response(
        uint32_t message_id,
        uint32_t request_id,
        uint32_t dapp_identifier,
        ResponseCode response_code,
        const std::optional<uint32_t>& subscription_id = std::nullopt
    );

    /**
     * @brief Create and encode an Indication Message PDU
     * @param message_id Unique message ID
     * @param dapp_identifier dApp identifier
     * @param ran_function_identifier RAN function identifier
     * @param protocol_data Protocol data
     */
    EncodeResult<EncodedMessage> encode_indication_message(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t ran_function_identifier,
        const std::vector<uint8_t>& protocol_data
    );

    /**
     * @brief Create and encode a dApp Control Action PDU
     * @param message_id Unique message ID
     * @param dapp_identifier dApp identifier
     * @param ran_function_identifier RAN function identifier
     * @param control_identifier Control identifier
     * @param action_data Action data
     */
    EncodeResult<EncodedMessage> encode_dapp_control_action(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t ran_function_identifier,
        uint32_t control_identifier,
        const std::vector<uint8_t>& action_data
    );

    /**
     * @brief Create and encode a dApp Report PDU
     * @param message_id Unique message ID
     */
    EncodeResult<EncodedMessage> encode_dapp_report(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t ran_function_identifier,
        const std::vector<uint8_t>& report_data
    );

    /**
     * @brief Create and encode an xApp Control Action PDU
     * @param message_id Unique message ID
     */
    EncodeResult<EncodedMessage> encode_xapp_control_action(
        uint32_t message_id,
        uint32_t dapp_identifier,
        uint32_t ran_function_identifier,
        const std::vector<uint8_t>& xapp_control_data
    );

    /**
     * @brief Create and encode a Message Acknowledgment PDU
     * @param message_id Unique message ID
     */
    EncodeResult<EncodedMessage> encode_message_ack(
        uint32_t message_id,
        uint32_t request_id,
        ResponseCode response_code
    );

protected:
    E3Encoder() = default;
};

/**
 * @brief Factory function to create appropriate encoder
 *
 * Creates an encoder instance based on the encoding format specified.
 *
 * @param format Encoding format to use
 * @return Unique pointer to created encoder, nullptr on failure
 */
std::unique_ptr<E3Encoder> create_encoder(EncodingFormat format);

} // namespace libe3

#endif // LIBE3_E3_ENCODER_HPP
