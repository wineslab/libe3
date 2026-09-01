/**
 * @file json_encoder.cpp
 * @brief JSON Encoder implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "json_encoder.hpp"
#include "libe3/logger.hpp"
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace libe3 {

namespace {

constexpr const char* LOG_TAG = "JsonEnc";

std::string to_camel_case(const char* pascal) {
    std::string s(pascal);
    if (!s.empty()) s[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}

bool is_json_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

size_t skip_ws(const std::string& text, size_t pos) {
    while (pos < text.size() && is_json_ws(text[pos])) {
        ++pos;
    }
    return pos;
}

// Skips a JSON string starting at text[pos] == '"'. Returns the index just
// past the closing quote, or nullopt on truncation/malformed escapes.
std::optional<size_t> skip_json_string(const std::string& text, size_t pos) {
    const size_t n = text.size();
    if (pos >= n || text[pos] != '"') return std::nullopt;
    ++pos;
    while (pos < n) {
        char c = text[pos];
        if (c == '\\') {
            if (pos + 1 >= n) return std::nullopt;
            if (text[pos + 1] == 'u') {
                if (pos + 6 > n) return std::nullopt;  // \ u X X X X
                pos += 6;
                continue;
            }
            pos += 2;
            continue;
        }
        if (c == '"') {
            return pos + 1;
        }
        ++pos;
    }
    return std::nullopt;
}

// Skips one JSON value starting at text[pos] (already at its first byte).
// Returns the index just past the value, or nullopt on anything unexpected
// or truncated. Objects/arrays are skipped by depth-counting braces/brackets
// (with nested strings skipped via skip_json_string so a brace/bracket
// character inside a string is never mistaken for structure); scalars (
// numbers, true/false/null) are skipped up to the next structural delimiter
// or whitespace.
std::optional<size_t> skip_json_value(const std::string& text, size_t pos) {
    const size_t n = text.size();
    if (pos >= n) return std::nullopt;
    char c = text[pos];
    if (c == '"') {
        return skip_json_string(text, pos);
    }
    if (c == '{' || c == '[') {
        char open = c;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        ++pos;
        while (pos < n && depth > 0) {
            char cc = text[pos];
            if (cc == '"') {
                auto after = skip_json_string(text, pos);
                if (!after) return std::nullopt;
                pos = *after;
                continue;
            }
            if (cc == open) {
                ++depth;
            } else if (cc == close) {
                --depth;
            }
            ++pos;
        }
        if (depth != 0) return std::nullopt;
        return pos;
    }
    // Bare token: number, true, false, or null.
    while (pos < n) {
        char cc = text[pos];
        if (cc == ',' || cc == '}' || cc == ']' || is_json_ws(cc)) {
            break;
        }
        ++pos;
    }
    return pos;
}

// Locates the byte span [start, end) of the value bound to `key` as a
// top-level member of the JSON object beginning at text[obj_start]. `text`
// is assumed to have already been proven well-formed JSON by the caller
// (decode()'s prior nlohmann::json::parse succeeded), so this only has to
// locate an already-guaranteed-to-exist member, not defend against
// arbitrary malformed input; every access is still bounds-checked and any
// unexpected byte pattern bails out to nullopt (never guesses), so a bug
// here can only cost a caller its fast path, never correctness -- callers
// are expected to fall back to re-serializing the parsed subtree on a miss.
// Does not unescape keys: a key that itself contains an escape sequence
// simply won't match and triggers the fallback (no encoder in this file
// produces escaped key names).
std::optional<std::pair<size_t, size_t>> find_top_level_member_span(
        const std::string& text, size_t obj_start, const std::string& key) {
    const size_t n = text.size();
    if (obj_start >= n || text[obj_start] != '{') {
        return std::nullopt;
    }

    size_t pos = obj_start + 1;
    while (true) {
        pos = skip_ws(text, pos);
        if (pos >= n) return std::nullopt;
        if (text[pos] == '}') {
            return std::nullopt;  // key not found
        }
        if (text[pos] != '"') return std::nullopt;

        size_t key_start = pos + 1;
        auto after_key = skip_json_string(text, pos);
        if (!after_key) return std::nullopt;
        size_t key_end = *after_key - 1;  // index of the closing quote

        pos = skip_ws(text, *after_key);
        if (pos >= n || text[pos] != ':') return std::nullopt;
        pos = skip_ws(text, pos + 1);

        size_t value_start = pos;
        auto after_value = skip_json_value(text, pos);
        if (!after_value) return std::nullopt;

        bool key_matches = (key_end - key_start == key.size()) &&
                            text.compare(key_start, key.size(), key) == 0;
        if (key_matches) {
            return std::make_pair(value_start, *after_value);
        }

        pos = skip_ws(text, *after_value);
        if (pos < n && text[pos] == ',') {
            ++pos;
            continue;
        }
        return std::nullopt;  // '}' (key not found) or anything unexpected
    }
}

} // anonymous namespace


// ============================================================================
// Helper methods for type conversions
// ============================================================================

std::optional<PduType> JsonE3Encoder::string_to_pdu_type(const std::string& s) const {
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
    return std::nullopt;
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
    if (req.periodicity.has_value()) {
        j["periodicity"] = req.periodicity.value();
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

std::optional<nlohmann::json> JsonE3Encoder::encode_indication_message(const IndicationMessage& msg) const {
    if (!nlohmann::json::accept(msg.protocol_data)) {
        E3_LOG_ERROR(LOG_TAG) << "Malformed protocolData for indication message "
                              << "(ranFunctionIdentifier=" << msg.ran_function_identifier
                              << ", payload size=" << msg.protocol_data.size() << " bytes)";
        return std::nullopt;  // caller reports ErrorCode::ENCODE_FAILED
    }
    nlohmann::json j;
    j["dAppIdentifier"] = msg.dapp_identifier;
    j["ranFunctionIdentifier"] = msg.ran_function_identifier;
    // protocolData intentionally omitted: encode() splices the validated raw
    // bytes verbatim into the dumped envelope instead of parsing them into a
    // DOM here only to have root.dump() re-serialize them a moment later.
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
    if (j.contains("periodicity")) {
        req.periodicity = j["periodicity"].get<uint32_t>();
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

IndicationMessage JsonE3Encoder::decode_indication_message(const nlohmann::json& j,
                                                             const std::string& raw_json) const {
    IndicationMessage msg;
    msg.dapp_identifier = j.value("dAppIdentifier", 0u);
    msg.ran_function_identifier = j.value("ranFunctionIdentifier", 0u);

    size_t obj_start = raw_json.find_first_not_of(" \t\r\n");
    auto span = (obj_start == std::string::npos)
        ? std::nullopt
        : find_top_level_member_span(raw_json, obj_start, "protocolData");

    if (span) {
        // Verbatim splice: byte-identical to what the SM originally emitted,
        // instead of re-dumping the parsed subtree (which would silently
        // reorder keys / drop whitespace for non-canonical input).
        msg.protocol_data.assign(raw_json.begin() + static_cast<long>(span->first),
                                  raw_json.begin() + static_cast<long>(span->second));
    } else {
        E3_LOG_DEBUG(LOG_TAG) << "protocolData byte-range scan missed; falling back to DOM dump "
                                 "(ranFunctionIdentifier=" << msg.ran_function_identifier << ")";
        std::string dumped = j["protocolData"].dump();
        msg.protocol_data.assign(dumped.begin(), dumped.end());
    }
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
        root["type"] = to_camel_case(pdu_type_to_string(pdu.type));
        root["id"] = pdu.message_id;
        root["timestamp"] = pdu.timestamp;

        // Set only for IndicationMessage: the SM's already-serialized payload,
        // spliced verbatim into the dumped envelope below instead of being
        // carried through the nlohmann tree (see encode_indication_message()).
        std::optional<std::string> raw_protocol_data;
        bool indication_payload_invalid = false;

        // Encode payload fields directly into root (flat format)
        std::visit([this, &root, &raw_protocol_data, &indication_payload_invalid](auto&& arg) {
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
                auto encoded_fields = encode_indication_message(arg);
                if (!encoded_fields) {
                    indication_payload_invalid = true;  // already logged with attribution
                    return;
                }
                fields = *encoded_fields;
                raw_protocol_data.emplace(arg.protocol_data.begin(), arg.protocol_data.end());
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
            root.update(fields);
        }, pdu.choice);

        if (indication_payload_invalid) {
            return tl::unexpected(ErrorCode::ENCODE_FAILED);
        }

        std::string json_str = root.dump();

        if (raw_protocol_data) {
            // root.dump() sorts object keys lexicographically (documented
            // nlohmann behavior); with protocolData excluded from the tree,
            // "ranFunctionIdentifier" is exactly where protocolData would
            // have sorted to ('d' < 'p' < 'r'), so this reproduces the same
            // key position (and, for well-formed payloads, the same bytes)
            // as before.
            size_t pos = json_str.find("\"ranFunctionIdentifier\"");
            if (pos == std::string::npos) {
                E3_LOG_ERROR(LOG_TAG) << "Internal error: indication envelope missing "
                                         "ranFunctionIdentifier; cannot splice protocolData";
                return tl::unexpected(ErrorCode::ENCODE_FAILED);
            }
            json_str.insert(pos, "\"protocolData\":" + *raw_protocol_data + ",");
        }

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
        std::string pdu_type_str = root.value("type", "");
        auto pdu_type = string_to_pdu_type(pdu_type_str);
        if (!pdu_type) {
            E3_LOG_ERROR(LOG_TAG) << "Unrecognized PDU type: " << pdu_type_str;
            return tl::unexpected(ErrorCode::DECODE_FAILED);
        }
        pdu.type = *pdu_type;
        pdu.message_id = root.value("id", 0u);
        pdu.timestamp = root.value("timestamp", 0ull);
        
        if (root.contains("data")) {
            E3_LOG_ERROR(LOG_TAG) << "Nested \"data\" wrapper is not supported; use flat format";
            return tl::unexpected(ErrorCode::DECODE_FAILED);
        }
        
        // Decode based on PDU type
        switch (pdu.type) {
            case PduType::SETUP_REQUEST:
                pdu.choice = decode_setup_request(root);
                break;
            case PduType::SETUP_RESPONSE:
                pdu.choice = decode_setup_response(root);
                break;
            case PduType::SUBSCRIPTION_REQUEST:
                pdu.choice = decode_subscription_request(root);
                break;
            case PduType::SUBSCRIPTION_DELETE:
                pdu.choice = decode_subscription_delete(root);
                break;
            case PduType::SUBSCRIPTION_RESPONSE:
                pdu.choice = decode_subscription_response(root);
                break;
            case PduType::INDICATION_MESSAGE:
                pdu.choice = decode_indication_message(root, json_str);
                break;
            case PduType::DAPP_CONTROL_ACTION:
                pdu.choice = decode_dapp_control_action(root);
                break;
            case PduType::DAPP_REPORT:
                pdu.choice = decode_dapp_report(root);
                break;
            case PduType::XAPP_CONTROL_ACTION:
                pdu.choice = decode_xapp_control_action(root);
                break;
            case PduType::RELEASE_MESSAGE:
                pdu.choice = decode_release_message(root);
                break;
            case PduType::MESSAGE_ACK:
                pdu.choice = decode_message_ack(root);
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
