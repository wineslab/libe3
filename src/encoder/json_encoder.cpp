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

} // anonymous namespace


// ============================================================================
// Helper methods for type conversions
// ============================================================================

PduType JsonE3Encoder::string_to_pdu_type(const std::string& s) const {
    if (s == "SetupRequest") return PduType::SETUP_REQUEST;
    if (s == "SetupResponse") return PduType::SETUP_RESPONSE;
    if (s == "SubscriptionRequest") return PduType::SUBSCRIPTION_REQUEST;
    if (s == "SubscriptionDelete") return PduType::SUBSCRIPTION_DELETE;
    if (s == "SubscriptionResponse") return PduType::SUBSCRIPTION_RESPONSE;
    if (s == "IndicationMessage") return PduType::INDICATION_MESSAGE;
    if (s == "DAppControlAction") return PduType::DAPP_CONTROL_ACTION;
    if (s == "DAppReport") return PduType::DAPP_REPORT;
    if (s == "XAppControlAction") return PduType::XAPP_CONTROL_ACTION;
    if (s == "ReleaseMessage") return PduType::RELEASE_MESSAGE;
    if (s == "MessageAck") return PduType::MESSAGE_ACK;
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
    j["e3ap_protocol_version"] = req.e3ap_protocol_version;
    j["dapp_name"] = req.dapp_name;
    j["dapp_version"] = req.dapp_version;
    j["vendor"] = req.vendor;
    
    return j;
}

nlohmann::json JsonE3Encoder::encode_setup_response(const SetupResponse& resp) const {
    nlohmann::json j;
    j["request_id"] = resp.request_id;
    j["response_code"] = (resp.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    if (resp.e3ap_protocol_version.has_value()) {
        j["e3ap_protocol_version"] = resp.e3ap_protocol_version.value();
    }
    if (resp.dapp_identifier.has_value()) {
        j["dapp_identifier"] = resp.dapp_identifier.value();
    }
    j["ran_identifier"] = resp.ran_identifier;
    if (!resp.ran_function_list.empty()) {
        nlohmann::json ran_funcs = nlohmann::json::array();
        for (const auto& func : resp.ran_function_list) {
            nlohmann::json func_obj;
            func_obj["ran_function_identifier"] = func.ran_function_identifier;
            func_obj["telemetry_identifier_list"] = func.telemetry_identifier_list;
            func_obj["control_identifier_list"] = func.control_identifier_list;
            func_obj["ran_function_data"] = binary_to_hex(func.ran_function_data);
            ran_funcs.push_back(func_obj);
        }
        j["ran_function_list"] = ran_funcs;
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_request(const SubscriptionRequest& req) const {
    nlohmann::json j;
    j["dapp_identifier"] = req.dapp_identifier;
    j["ran_function_identifier"] = req.ran_function_identifier;
    j["telemetry_identifier_list"] = req.telemetry_identifier_list;
    j["control_identifier_list"] = req.control_identifier_list;
    if (req.subscription_time.has_value()) {
        j["subscription_time"] = req.subscription_time.value();
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_delete(const SubscriptionDelete& del) const {
    nlohmann::json j;
    j["dapp_identifier"] = del.dapp_identifier;
    j["subscription_id"] = del.subscription_id;
    return j;
}

nlohmann::json JsonE3Encoder::encode_subscription_response(const SubscriptionResponse& resp) const {
    nlohmann::json j;
    j["request_id"] = resp.request_id;
    j["dapp_identifier"] = resp.dapp_identifier;
    j["response_code"] = (resp.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    if (resp.subscription_id.has_value()) {
        j["subscription_id"] = resp.subscription_id.value();
    }
    return j;
}

nlohmann::json JsonE3Encoder::encode_indication_message(const IndicationMessage& msg) const {
    nlohmann::json j;
    j["dapp_identifier"] = msg.dapp_identifier;
    j["ran_function_identifier"] = msg.ran_function_identifier;
    j["protocol_data"] = binary_to_hex(msg.protocol_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_dapp_control_action(const DAppControlAction& action) const {
    nlohmann::json j;
    j["dapp_identifier"] = action.dapp_identifier;
    j["ran_function_identifier"] = action.ran_function_identifier;
    j["control_identifier"] = action.control_identifier;
    j["action_data"] = binary_to_hex(action.action_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_dapp_report(const DAppReport& report) const {
    nlohmann::json j;
    j["dapp_identifier"] = report.dapp_identifier;
    j["ran_function_identifier"] = report.ran_function_identifier;
    j["report_data"] = binary_to_hex(report.report_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_xapp_control_action(const XAppControlAction& action) const {
    nlohmann::json j;
    j["dapp_identifier"] = action.dapp_identifier;
    j["ran_function_identifier"] = action.ran_function_identifier;
    j["xapp_control_data"] = binary_to_hex(action.xapp_control_data);
    return j;
}

nlohmann::json JsonE3Encoder::encode_message_ack(const MessageAck& ack) const {
    nlohmann::json j;
    j["request_id"] = ack.request_id;
    j["response_code"] = (ack.response_code == ResponseCode::POSITIVE) ? "positive" : "negative";
    return j;
}

// ============================================================================
// Decoding helpers for each PDU type
// ============================================================================

SetupRequest JsonE3Encoder::decode_setup_request(const nlohmann::json& j) const {
    SetupRequest req;
    req.e3ap_protocol_version = j.value("e3ap_protocol_version", "");
    req.dapp_name = j.value("dapp_name", "");
    req.dapp_version = j.value("dapp_version", "");
    req.vendor = j.value("vendor", "");
    
    return req;
}

SetupResponse JsonE3Encoder::decode_setup_response(const nlohmann::json& j) const {
    SetupResponse resp;
    resp.request_id = j.value("request_id", 0u);
    
    std::string response_code_str = j.value("response_code", "negative");
    resp.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    
    if (j.contains("e3ap_protocol_version")) {
        resp.e3ap_protocol_version = j["e3ap_protocol_version"].get<std::string>();
    }
    if (j.contains("dapp_identifier")) {
        resp.dapp_identifier = j["dapp_identifier"].get<uint32_t>();
    }
    resp.ran_identifier = j.value("ran_identifier", "");
    if (j.contains("ran_function_list")) {
        for (const auto& func_obj : j["ran_function_list"]) {
            RanFunctionDef func;
            func.ran_function_identifier = func_obj.value("ran_function_identifier", 0u);
            func.telemetry_identifier_list = func_obj.value("telemetry_identifier_list", std::vector<uint32_t>{});
            func.control_identifier_list = func_obj.value("control_identifier_list", std::vector<uint32_t>{});
            func.ran_function_data = hex_to_binary(func_obj.value("ran_function_data", ""));
            resp.ran_function_list.push_back(func);
        }
    }
    return resp;
}

SubscriptionRequest JsonE3Encoder::decode_subscription_request(const nlohmann::json& j) const {
    SubscriptionRequest req;
    req.dapp_identifier = j.value("dapp_identifier", 0u);
    req.ran_function_identifier = j.value("ran_function_identifier", 0u);
    req.telemetry_identifier_list = j.value("telemetry_identifier_list", std::vector<uint32_t>{});
    req.control_identifier_list = j.value("control_identifier_list", std::vector<uint32_t>{});
    if (j.contains("subscription_time")) {
        req.subscription_time = j["subscription_time"].get<uint32_t>();
    }
    return req;
}

SubscriptionDelete JsonE3Encoder::decode_subscription_delete(const nlohmann::json& j) const {
    SubscriptionDelete del;
    del.dapp_identifier = j.value("dapp_identifier", 0u);
    del.subscription_id = j.value("subscription_id", 0u);
    return del;
}

SubscriptionResponse JsonE3Encoder::decode_subscription_response(const nlohmann::json& j) const {
    SubscriptionResponse resp;
    resp.request_id = j.value("request_id", 0u);
    resp.dapp_identifier = j.value("dapp_identifier", 0u);
    std::string response_code_str = j.value("response_code", "negative");
    resp.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    if (j.contains("subscription_id")) {
        resp.subscription_id = j["subscription_id"].get<uint32_t>();
    }
    return resp;
}

IndicationMessage JsonE3Encoder::decode_indication_message(const nlohmann::json& j) const {
    IndicationMessage msg;
    msg.dapp_identifier = j.value("dapp_identifier", 0u);
    msg.ran_function_identifier = j.value("ran_function_identifier", 0u);
    msg.protocol_data = hex_to_binary(j.value("protocol_data", ""));
    return msg;
}

DAppControlAction JsonE3Encoder::decode_dapp_control_action(const nlohmann::json& j) const {
    DAppControlAction action;
    action.dapp_identifier = j.value("dapp_identifier", 0u);
    action.ran_function_identifier = j.value("ran_function_identifier", 0u);
    action.control_identifier = j.value("control_identifier", 0u);
    action.action_data = hex_to_binary(j.value("action_data", ""));
    return action;
}

DAppReport JsonE3Encoder::decode_dapp_report(const nlohmann::json& j) const {
    DAppReport report;
    report.dapp_identifier = j.value("dapp_identifier", 0u);
    report.ran_function_identifier = j.value("ran_function_identifier", 0u);
    report.report_data = hex_to_binary(j.value("report_data", ""));
    return report;
}

XAppControlAction JsonE3Encoder::decode_xapp_control_action(const nlohmann::json& j) const {
    XAppControlAction action;
    action.dapp_identifier = j.value("dapp_identifier", 0u);
    action.ran_function_identifier = j.value("ran_function_identifier", 0u);
    action.xapp_control_data = hex_to_binary(j.value("xapp_control_data", ""));
    return action;
}

MessageAck JsonE3Encoder::decode_message_ack(const nlohmann::json& j) const {
    MessageAck ack;
    ack.request_id = j.value("request_id", 0u);
    std::string response_code_str = j.value("response_code", "negative");
    ack.response_code = (response_code_str == "positive") ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
    return ack;
}

// ReleaseMessage encode/decode
nlohmann::json JsonE3Encoder::encode_release_message(const ReleaseMessage& msg) const {
    nlohmann::json j;
    j["dapp_identifier"] = msg.dapp_identifier;
    return j;
}

ReleaseMessage JsonE3Encoder::decode_release_message(const nlohmann::json& j) const {
    ReleaseMessage msg;
    msg.dapp_identifier = j.value("dapp_identifier", 0u);
    return msg;
}

// ============================================================================
// Main encode/decode methods
// ============================================================================

EncodeResult<EncodedMessage> JsonE3Encoder::encode(const Pdu& pdu) {
    try {
        nlohmann::json root;
        root["pdu_type"] = pdu_type_to_string(pdu.type);
        root["message_id"] = pdu.message_id;
        root["timestamp"] = pdu.timestamp;
        
        // Encode the data based on PDU type
        nlohmann::json data;
        std::visit([this, &data](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, SetupRequest>) {
                data = encode_setup_request(arg);
            }
            else if constexpr (std::is_same_v<T, SetupResponse>) {
                data = encode_setup_response(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionRequest>) {
                data = encode_subscription_request(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionDelete>) {
                data = encode_subscription_delete(arg);
            }
            else if constexpr (std::is_same_v<T, SubscriptionResponse>) {
                data = encode_subscription_response(arg);
            }
            else if constexpr (std::is_same_v<T, IndicationMessage>) {
                data = encode_indication_message(arg);
            }
            else if constexpr (std::is_same_v<T, DAppControlAction>) {
                data = encode_dapp_control_action(arg);
            }
            else if constexpr (std::is_same_v<T, DAppReport>) {
                data = encode_dapp_report(arg);
            }
            else if constexpr (std::is_same_v<T, XAppControlAction>) {
                data = encode_xapp_control_action(arg);
            }
            else if constexpr (std::is_same_v<T, ReleaseMessage>) {
                data = encode_release_message(arg);
            }
            else if constexpr (std::is_same_v<T, MessageAck>) {
                data = encode_message_ack(arg);
            }
        }, pdu.choice);
        
        root["data"] = data;
        
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
        
        // Get PDU type
        std::string pdu_type_str = root.value("pdu_type", "");
        pdu.type = string_to_pdu_type(pdu_type_str);
        pdu.message_id = root.value("message_id", 0u);
        pdu.timestamp = root.value("timestamp", 0ull);
        
        // Get data object
        if (!root.contains("data")) {
            E3_LOG_ERROR(LOG_TAG) << "Missing 'data' field in JSON";
            return tl::unexpected(ErrorCode::DECODE_FAILED);
        }
        
        const nlohmann::json& j = root["data"];
        
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
