/**
 * @file e3_encoder.cpp
 * @brief Base encoder implementation and convenience methods
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_encoder.hpp"
#include "libe3/logger.hpp"
#include <atomic>

namespace libe3 {

namespace {
std::atomic<uint32_t> message_id_counter{1};
}

uint32_t E3Encoder::generate_message_id() {
    uint32_t id = message_id_counter.fetch_add(1, std::memory_order_relaxed);
    // Wrap around to stay within valid range (1-100)
    if (id > 100) {
        id = (id % 100) + 1;
        message_id_counter.store(id + 1, std::memory_order_relaxed);
    }
    return id;
}

// Convenience method implementations

EncodeResult<EncodedMessage> E3Encoder::encode_setup_request(
    uint32_t dapp_identifier,
    const std::vector<uint32_t>& ran_function_list,
    ActionType action_type
) {
    Pdu pdu(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.id = generate_message_id();
    req.dapp_identifier = dapp_identifier;
    req.ran_function_list = ran_function_list;
    req.type = action_type;
    pdu.choice = req;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_setup_response(
    uint32_t request_id,
    ResponseCode response_code,
    const std::vector<uint32_t>& ran_function_list
) {
    Pdu pdu(PduType::SETUP_RESPONSE);
    SetupResponse resp;
    resp.id = generate_message_id();
    resp.request_id = request_id;
    resp.response_code = response_code;
    resp.ran_function_list = ran_function_list;
    pdu.choice = resp;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_subscription_request(
    uint32_t dapp_identifier,
    ActionType action_type,
    uint32_t ran_function_identifier
) {
    Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
    SubscriptionRequest req;
    req.id = generate_message_id();
    req.dapp_identifier = dapp_identifier;
    req.type = action_type;
    req.ran_function_identifier = ran_function_identifier;
    pdu.choice = req;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_subscription_response(
    uint32_t request_id,
    ResponseCode response_code
) {
    Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.id = generate_message_id();
    resp.request_id = request_id;
    resp.response_code = response_code;
    pdu.choice = resp;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_indication_message(
    uint32_t dapp_identifier,
    const std::vector<uint8_t>& protocol_data
) {
    Pdu pdu(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.id = generate_message_id();
    msg.dapp_identifier = dapp_identifier;
    msg.protocol_data = protocol_data;
    pdu.choice = msg;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_control_action(
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& action_data
) {
    Pdu pdu(PduType::CONTROL_ACTION);
    ControlAction action;
    action.id = generate_message_id();
    action.dapp_identifier = dapp_identifier;
    action.ran_function_identifier = ran_function_identifier;
    action.action_data = action_data;
    pdu.choice = action;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_dapp_report(
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& report_data
) {
    Pdu pdu(PduType::DAPP_REPORT);
    DAppReport report;
    report.id = generate_message_id();
    report.dapp_identifier = dapp_identifier;
    report.ran_function_identifier = ran_function_identifier;
    report.report_data = report_data;
    pdu.choice = report;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_xapp_control_action(
    uint32_t dapp_identifier,
    uint32_t ran_function_identifier,
    const std::vector<uint8_t>& xapp_control_data
) {
    Pdu pdu(PduType::XAPP_CONTROL_ACTION);
    XAppControlAction action;
    action.id = generate_message_id();
    action.dapp_identifier = dapp_identifier;
    action.ran_function_identifier = ran_function_identifier;
    action.xapp_control_data = xapp_control_data;
    pdu.choice = action;
    return encode(pdu);
}

EncodeResult<EncodedMessage> E3Encoder::encode_message_ack(
    uint32_t request_id,
    ResponseCode response_code
) {
    Pdu pdu(PduType::MESSAGE_ACK);
    MessageAck ack;
    ack.id = generate_message_id();
    ack.request_id = request_id;
    ack.response_code = response_code;
    pdu.choice = ack;
    return encode(pdu);
}

} // namespace libe3
