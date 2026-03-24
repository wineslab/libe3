/**
 * @file json_encoder.cpp
 * @brief JSON Encoder implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "json_encoder.hpp"
#include "libe3/logger.hpp"
#include <iomanip>
#include <sstream>

namespace libe3 {

namespace {

constexpr const char* LOG_TAG = "JsonEnc";

const char* pdu_type_to_json_string(PduType type) noexcept {
    switch (type) {
        case PduType::SETUP_REQUEST:          return "setupRequest";
        case PduType::SETUP_RESPONSE:         return "setupResponse";
        case PduType::SUBSCRIPTION_REQUEST:   return "subscriptionRequest";
        case PduType::SUBSCRIPTION_DELETE:    return "subscriptionDelete";
        case PduType::SUBSCRIPTION_RESPONSE:  return "subscriptionResponse";
        case PduType::INDICATION_MESSAGE:     return "indicationMessage";
        case PduType::DAPP_CONTROL_ACTION:    return "dAppControlAction";
        case PduType::DAPP_REPORT:            return "dAppReport";
        case PduType::XAPP_CONTROL_ACTION:    return "xAppControlAction";
        case PduType::RELEASE_MESSAGE:        return "releaseMessage";
        case PduType::MESSAGE_ACK:            return "messageAck";
        default:                              return "unknown";
    }
}

} // anonymous namespace


// ============================================================================
// Helper methods for type conversions
// ============================================================================

PduType JsonE3Encoder::string_to_pdu_type(const std::string& s) const {
    if (s == "setupRequest")          return PduType::SETUP_REQUEST;
    if (s == "setupResponse")         return PduType::SETUP_RESPONSE;
    if (s == "subscriptionRequest")   return PduType::SUBSCRIPTION_REQUEST;
    if (s == "subscriptionDelete")    return PduType::SUBSCRIPTION_DELETE;
    if (s == "subscriptionResponse")  return PduType::SUBSCRIPTION_RESPONSE;
    if (s == "indicationMessage")     return PduType::INDICATION_MESSAGE;
    if (s == "dAppControlAction")     return PduType::DAPP_CONTROL_ACTION;
    if (s == "dAppReport")            return PduType::DAPP_REPORT;
    if (s == "xAppControlAction")     return PduType::XAPP_CONTROL_ACTION;
    if (s == "releaseMessage")        return PduType::RELEASE_MESSAGE;
    if (s == "messageAck")            return PduType::MESSAGE_ACK;
    return PduType::SETUP_REQUEST; // Default
}

ErrorCode JsonE3Encoder::string_to_error_code(const std::string& s) const {
    if (s == "SUCCESS") return ErrorCode::SUCCESS;
    if (s == "INVALID_PARAM") return ErrorCode::INVALID_PARAM;
    if (s == "TIMEOUT") return ErrorCode::TIMEOUT;
    if (s == "NOT_FOUND") return ErrorCode::NOT_FOUND;
    return ErrorCode::GENERIC_ERROR;
}

// ============================================================================
// Binary encoding helpers
// ============================================================================

std::string JsonE3Encoder::binary_to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream ss;
    for (uint8_t b : data) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

std::vector<uint8_t> JsonE3Encoder::hex_to_binary(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        result.push_back(static_cast<uint8_t>(
            std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return result;
}

// ============================================================================
// Encoding helpers for each PDU type
// ============================================================================

nlohmann::json JsonE3Encoder::encode_setup_request(const SetupRequest& req) const {
    nlohmann::json j;
    j["e3apProtocolVersion"] = req.e3ap_protocol_version;
    j["dAppName"] = req.dapp_name;
    j["dAppVersion"] = req.dapp_version;
    j["vendor"] = req.vendor;
    
    return j;
}

nlohmann::json JsonE3Encoder::encode_setup_response(const SetupResponse& resp) const {
    nlohmann::json j;
    j["requestId"] = resp.request_id;
    j["responseCode"] = (resp.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    if (resp.e3ap_protocol_version.has_value()) {
        j["e3apProtocolVersion"] = resp.e3ap_protocol_version.value();
    }
    if (resp.dapp_identifier.has_value()) {
        j["dAppIdentifier"] = resp.dapp_identifier.value();
    }
    j["ranIdentifier"] = resp.ran_identifier;
    if (!resp.ran_function_list.empty()) {
        nlohmann::json ran_funcs = nlohmann::json::array();
        for (const auto& func : resp.ran_function_list) {
            nlohmann::json func_obj;
            func_obj["ranFunctionIdentifier"] = func.ran_function_identifier;
            func_obj["telemetryIdentifierList"] = func.telemetry_identifier_list;
            func_obj["controlIdentifierList"] = func.control_identifier_list;
            func_obj["ranFunctionData"] = binary_to_hex(func.ran_function_data);
            ran_funcs.push_back(func_obj);
        }
        j["ranFunctionList"] = ran_funcs;
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_request(const SubscriptionRequest& req) const {
    nlohmann::json j;
    j["dAppIdentifier"] = req.dapp_identifier;
    j["ranFunctionIdentifier"] = req.ran_function_identifier;
    j["telemetryIdentifierList"] = req.telemetry_identifier_list;
    j["controlIdentifierList"] = req.control_identifier_list;
    if (req.subscription_time.has_value()) {
        j["subscriptionTime"] = req.subscription_time.value();
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_delete(const SubscriptionDelete& del) const {
    nlohmann::json j;
    j["dAppIdentifier"] = del.dapp_identifier;
    j["subscriptionId"] = del.subscription_id;
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_response(const SubscriptionResponse& resp) const {
    nlohmann::json j;
    j["requestId"] = resp.request_id;
    j["dAppIdentifier"] = resp.dapp_identifier;
    j["responseCode"] = (resp.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    if (resp.subscription_id.has_value()) {
        j["subscriptionId"] = resp.subscription_id.value();
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_indication_message(const IndicationMessage& msg) const {
    nlohmann::json j;
    j["dAppIdentifier"] = msg.dapp_identifier;
    j["ranFunctionIdentifier"] = msg.ran_function_identifier;
    j["protocolData"] = binary_to_hex(msg.protocol_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_dapp_control_action(const DAppControlAction& action) const {
    nlohmann::json j;
    j["dAppIdentifier"] = action.dapp_identifier;
    j["ranFunctionIdentifier"] = action.ran_function_identifier;
    j["controlIdentifier"] = action.control_identifier;
    j["actionData"] = binary_to_hex(action.action_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_dapp_report(const DAppReport& report) const {
    nlohmann::json j;
    j["dAppIdentifier"] = report.dapp_identifier;
    j["ranFunctionIdentifier"] = report.ran_function_identifier;
    j["reportData"] = binary_to_hex(report.report_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_xapp_control_action(const XAppControlAction& action) const {
    nlohmann::json j;
    j["dAppIdentifier"] = action.dapp_identifier;
    j["ranFunctionIdentifier"] = action.ran_function_identifier;
    j["xAppControlData"] = binary_to_hex(action.xapp_control_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_message_ack(const MessageAck& ack) const {
    nlohmann::json j;
    j["requestId"] = ack.request_id;
    j["responseCode"] = (ack.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    return j;
}

// ============================================================================
// Decoding helpers for each PDU type
// ============================================================================

SetupRequest JsonE3Encoder::decode_setup_request(const nlohmann::json& j) const {
    SetupRequest req;
    req.e3ap_protocol_version = j.value("e3apProtocolVersion", "");
    req.dapp_name = j.value("dAppName", "");
    req.dapp_version = j.value("dAppVersion", "");
    req.vendor = j.value("vendor", "");
    
    return req;
}

SetupResponse JsonE3Encoder::decode_setup_response(const nlohmann::json& j) const {
    SetupResponse resp;
    resp.request_id = j.value("requestId", 0u);
    
    std::string response_code_str = j.value("responseCode", "negative");
    resp.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    
    if (j.contains("e3apProtocolVersion")) {
        resp.e3ap_protocol_version = j["e3apProtocolVersion"].get<std::string>();
    }
    if (j.contains("dAppIdentifier")) {
        resp.dapp_identifier = j["dAppIdentifier"].get<uint32_t>();
    }
    resp.ran_identifier = j.value("ranIdentifier", "");
    if (j.contains("ranFunctionList")) {
        for (const auto& func_obj : j["ranFunctionList"]) {
            RanFunctionDef func;
            func.ran_function_identifier = func_obj.value("ranFunctionIdentifier", 0u);
            func.telemetry_identifier_list = func_obj.value("telemetryIdentifierList", std::vector<uint32_t>{});
            func.control_identifier_list = func_obj.value("controlIdentifierList", std::vector<uint32_t>{});
            func.ran_function_data = hex_to_binary(func_obj.value("ranFunctionData", ""));
            resp.ran_function_list.push_back(func);
        }
    }
    return resp;
}

SubscriptionRequest JsonE3Encoder::decode_subscription_request(const nlohmann::json& j) const {
    SubscriptionRequest req;
    req.dapp_identifier = j.value("dAppIdentifier", 0u);
    req.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);
    req.telemetry_identifier_list = j.value("telemetryIdentifierList", std::vector<uint32_t>{});
    req.control_identifier_list = j.value("controlIdentifierList", std::vector<uint32_t>{});
    if (j.contains("subscriptionTime")) {
        req.subscription_time = j["subscriptionTime"].get<uint32_t>();
    }
    return req;
}

SubscriptionDelete JsonE3Encoder::decode_subscription_delete(const nlohmann::json& j) const {
    SubscriptionDelete del;
    del.dapp_identifier = j.value("dAppIdentifier", 0u);
    del.subscription_id = j.value("subscriptionId", 0u);
    return del;
}

SubscriptionResponse JsonE3Encoder::decode_subscription_response(const nlohmann::json& j) const {
    SubscriptionResponse resp;
    resp.request_id = j.value("requestId", 0u);
    resp.dapp_identifier = j.value("dAppIdentifier", 0u);
    std::string response_code_str = j.value("responseCode", "negative");
    resp.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    if (j.contains("subscriptionId")) {
        resp.subscription_id = j["subscriptionId"].get<uint32_t>();
    }
    return resp;
}

IndicationMessage JsonE3Encoder::decode_indication_message(const nlohmann::json& j) const {
    IndicationMessage msg;
    msg.dapp_identifier = j.value("dAppIdentifier", 0u);
    msg.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);
    msg.protocol_data = hex_to_binary(j.value("protocolData", ""));
    return msg;
}

DAppControlAction JsonE3Encoder::decode_dapp_control_action(const nlohmann::json& j) const {
    DAppControlAction action;
    action.dapp_identifier = j.value("dAppIdentifier", 0u);
    action.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);
    action.control_identifier = j.value("controlIdentifier", 0u);
    action.action_data = hex_to_binary(j.value("actionData", ""));
    return action;
}

DAppReport JsonE3Encoder::decode_dapp_report(const nlohmann::json& j) const {
    DAppReport report;
    report.dapp_identifier = j.value("dAppIdentifier", 0u);
    report.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);
    report.report_data = hex_to_binary(j.value("reportData", ""));
    return report;
}

XAppControlAction JsonE3Encoder::decode_xapp_control_action(const nlohmann::json& j) const {
    XAppControlAction action;
    action.dapp_identifier = j.value("dAppIdentifier", 0u);
    action.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);
    action.xapp_control_data = hex_to_binary(j.value("xAppControlData", ""));
    return action;
}

MessageAck JsonE3Encoder::decode_message_ack(const nlohmann::json& j) const {
    MessageAck ack;
    ack.request_id = j.value("requestId", 0u);
    std::string response_code_str = j.value("responseCode", "negative");
    ack.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    return ack;
}

// ReleaseMessage encode/decode
nlohmann::json JsonE3Encoder::encode_release_message(const ReleaseMessage& msg) const {
    nlohmann::json j;
    j["dAppIdentifier"] = msg.dapp_identifier;
    return j;
}

ReleaseMessage JsonE3Encoder::decode_release_message(const nlohmann::json& j) const {
    ReleaseMessage msg;
    msg.dapp_identifier = j.value("dAppIdentifier", 0u);
    return msg;
}

// ============================================================================
// Main encode/decode methods
// ============================================================================

EncodeResult<EncodedMessage> JsonE3Encoder::encode(const Pdu& pdu) {
    try {
        nlohmann::json root;
        root["type"] = pdu_type_to_json_string(pdu.type);
        root["id"] = pdu.message_id;
        root["timestamp"] = pdu.timestamp;
        
        // Encode payload fields directly into root (flat format)
        std::visit([this, &root](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            nlohmann::json fields;
            if constexpr (std::is_same_v<T, SetupRequest>) {
                fields = encode_setup_request(arg);
            }
            else if constexpr (std::is_same_v<T, SetupResponse>) {
                fields = encode_setup_response(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionRequest>) {
                fields = encode_subscription_request(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionDelete>) {
                fields = encode_subscription_delete(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionResponse>) {
                fields = encode_subscription_response(arg);
            }
            else if constexpr (std::is_same_v<T, IndicationMessage>) {
                fields = encode_indication_message(arg);
            }
            else if constexpr (std::is_same_v<T, DAppControlAction>) {
                fields = encode_dapp_control_action(arg);
            }
            else if constexpr (std::is_same_v<T, DAppReport>) {
                fields = encode_dapp_report(arg);
            }
            else if constexpr (std::is_same_v<T, XAppControlAction>) {
                fields = encode_xapp_control_action(arg);
            }
            else if constexpr (std::is_same_v<T, ReleaseMessage>) {
                fields = encode_release_message(arg);
            }
            else if constexpr (std::is_same_v<T, MessageAck>) {
                fields = encode_message_ack(arg);
            }
            if (nested_mode_) {
                root["data"] = fields;
            } else {
                root.update(fields);
            }
        }, pdu.choice);
        
        std::string json_str = root.dump();
        EncodedMessage msg;
        msg.buffer.assign(json_str.begin(), json_str.end());
        msg.format = EncodingFormat::JSON;
        
        E3_LOG_TRACE(LOG_TAG) << "Encoded " << pdu_type_to_string(pdu.type) 
                              << " (" << msg.size() << " bytes)";
        
        return msg;
    }
    catch (const std::exception& e) {
        E3_LOG_ERROR(LOG_TAG) << "JSON encode error: " << e.what();
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }
}

EncodeResult<Pdu> JsonE3Encoder::decode(const EncodedMessage& encoded) {
    return decode(encoded.data(), encoded.size());
}

EncodeResult<Pdu> JsonE3Encoder::decode(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        E3_LOG_ERROR(LOG_TAG) << "Cannot decode empty data";
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
    
    try {
        std::string json_str(reinterpret_cast<const char*>(data), size);
        
        E3_LOG_TRACE(LOG_TAG) << "Decoding JSON (" << size << " bytes)";
        
        nlohmann::json root = nlohmann::json::parse(json_str);
        
        Pdu pdu;
        
        // Accept both nested envelope (pdu_type/message_id/data) and flat format (type/id)
        std::string pdu_type_str = root.value("pdu_type", root.value("type", ""));
        pdu.type = string_to_pdu_type(pdu_type_str);
        pdu.message_id = root.value("message_id", root.value("id", 0u));
        pdu.timestamp = root.value("timestamp", 0ull);
        
        // Use nested "data" object if present, otherwise treat root as the data object (flat format)
        nested_mode_ = root.contains("data");
        const nlohmann::json& j = nested_mode_ ? root["data"] : root;
        
        // Decode based on PDU type
        switch (pdu.type) {
            case PduType::SETUP_REQUEST:
                pdu.choice = decode_setup_request(j);
                break;
            case PduType::SETUP_RESPONSE:
                pdu.choice = decode_setup_response(j);
                break;
            case PduType::SUBSCRIPTION_REQUEST:
                pdu.choice = decode_subscription_request(j);
                break;
            case PduType::SUBSCRIPTION_DELETE:
                pdu.choice = decode_subscription_delete(j);
                break;
            case PduType::SUBSCRIPTION_RESPONSE:
                pdu.choice = decode_subscription_response(j);
                break;
            case PduType::INDICATION_MESSAGE:
                pdu.choice = decode_indication_message(j);
                break;
            case PduType::DAPP_CONTROL_ACTION:
                pdu.choice = decode_dapp_control_action(j);
                break;
            case PduType::DAPP_REPORT:
                pdu.choice = decode_dapp_report(j);
                break;
            case PduType::XAPP_CONTROL_ACTION:
                pdu.choice = decode_xapp_control_action(j);
                break;
            case PduType::RELEASE_MESSAGE:
                pdu.choice = decode_release_message(j);
                break;
            case PduType::MESSAGE_ACK:
                pdu.choice = decode_message_ack(j);
                break;
            default:
                E3_LOG_ERROR(LOG_TAG) << "Unknown PDU type: " << pdu_type_str;
                return tl::unexpected(ErrorCode::DECODE_FAILED);
        }
        
        E3_LOG_TRACE(LOG_TAG) << "Decoded " << pdu_type_to_string(pdu.type);
        return pdu;
    }
    catch (const nlohmann::json::parse_error& e) {
        E3_LOG_ERROR(LOG_TAG) << "JSON parse error: " << e.what();
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
    catch (const std::exception& e) {
        E3_LOG_ERROR(LOG_TAG) << "JSON decode error: " << e.what();
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
}

} // namespace libe3
