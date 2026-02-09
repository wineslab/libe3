/**
 * @file e3_encoder.cpp
 * @brief Base encoder implementation and convenience methods
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_encoder.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

// Convenience method implementations

EncodeResult<EncodedMessage> E3Encoder::encode_setup_request(
    uint32_t message_id,
    const std::string& e3ap_protocol_version,
    const std::string& dapp_name,
    const std::string& dapp_version,
    const std::string& vendor
) {
    Pdu pdu(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.e3ap_protocol_version = e3ap_protocol_version;
    req.dapp_name = dapp_name;
    req.dapp_version = dapp_version;
    req.vendor = vendor;
    pdu.message_id = message_id;
    pdu.choice = req;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_setup_response(
    uint32_t message_id,
    uint32_t request_id,
    ResponseCode response_code,
    const std::optional<std::string>& e3ap_protocol_version,
    const std::optional<uint32_t>& dapp_identifier,
    const std::string& ran_identifier,
    const std::vector<RanFunctionDef>& ran_function_list
) {
    Pdu pdu(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.request_id = request_id;
    resp.response_code = response_code;
    resp.e3ap_protocol_version = e3ap_protocol_version;
    resp.dapp_identifier = dapp_identifier;
    resp.ran_identifier = ran_identifier;
    resp.ran_function_list = ran_function_list;
    pdu.message_id = message_id;
    pdu.choice = resp;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_subscription_request(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint32_t>& telemetry_identifier_list,
    const std::vector<uint32_t>& control_identifier_list,
    const std::optional<uint32_t>& subscription_time
) {
    Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.dapp_identifier = dapp_identifier;
    req.ran_function_identifier = ran_function_identifier;
    req.telemetry_identifier_list = telemetry_identifier_list;
    req.control_identifier_list = control_identifier_list;
    req.subscription_time = subscription_time;
    pdu.message_id = message_id;
    pdu.choice = req;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_subscription_delete(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t subscription_id
) {
    Pdu pdu(PduType::SUBSCRIPTION_DELETE);
    SubscriptionDelete del;
    del.dapp_identifier = dapp_identifier;
    del.subscription_id = subscription_id;
    pdu.message_id = message_id;
    pdu.choice = del;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_subscription_response(
    uint32_t message_id,
    uint32_t request_id,
    uint32_t dapp_identifier,
    ResponseCode response_code,
    const std::optional<uint32_t>& subscription_id
) {
    Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.request_id = request_id;
    resp.dapp_identifier = dapp_identifier;
    resp.response_code = response_code;
    resp.subscription_id = subscription_id;
    pdu.message_id = message_id;
    pdu.choice = resp;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_indication_message(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& protocol_data
) {
    Pdu pdu(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = dapp_identifier;
    msg.ran_function_identifier = ran_function_identifier;
    msg.protocol_data = protocol_data;
    pdu.message_id = message_id;
    pdu.choice = msg;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_dapp_control_action(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    uint32_t control_identifier,
    const std::vector<uint8_t>& action_data
) {
    Pdu pdu(PduType::DAPP_CONTROL_ACTION);
    DAppControlAction action;
    action.dapp_identifier = dapp_identifier;
    action.ran_function_identifier = ran_function_identifier;
    action.control_identifier = control_identifier;
    action.action_data = action_data;
    pdu.message_id = message_id;
    pdu.choice = action;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_dapp_report(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& report_data
) {
    Pdu pdu(PduType::DAPP_REPORT);
    DAppReport report;
    report.dapp_identifier = dapp_identifier;
    report.ran_function_identifier = ran_function_identifier;
    report.report_data = report_data;
    pdu.message_id = message_id;
    pdu.choice = report;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_xapp_control_action(
    uint32_t message_id,
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& xapp_control_data
) {
    Pdu pdu(PduType::XAPP_CONTROL_ACTION);
    XAppControlAction action;
    action.dapp_identifier = dapp_identifier;
    action.ran_function_identifier = ran_function_identifier;
    action.xapp_control_data = xapp_control_data;
    pdu.message_id = message_id;
    pdu.choice = action;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_message_ack(
    uint32_t message_id,
    uint32_t request_id,
    ResponseCode response_code
) {
    Pdu pdu(PduType::MESSAGE_ACK);
    MessageAck ack;
    ack.request_id = request_id;
    ack.response_code = response_code;
    pdu.message_id = message_id;
    pdu.choice = ack;
    return encode(pdu);
}

} // namespace libe3
