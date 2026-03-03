/**
 * @file types.hpp
 * @brief E3AP Protocol Types - Vendor-neutral type definitions
 *
 * This header defines all E3AP protocol types and structures used throughout
 * the libe3 library. These types are designed to be RAN-agnostic and can be
 * used by any RAN vendor integrating with the E3AP protocol.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_TYPES_HPP
#define LIBE3_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <chrono>
#include <memory>

#include "libe3/error_codes.h"

namespace libe3 {

/**
 * @brief E3AP encoding formats supported by the library
 */
enum class EncodingFormat : uint8_t {
    ASN1 = 0,   ///< ASN.1 PER encoding (standard O-RAN format)
    JSON = 1    ///< JSON encoding (for development/debugging)
};

/**
 * @brief Link layer types (matches Python E3LinkLayer)
 */
enum class E3LinkLayer : uint8_t {
    ZMQ = 0,     ///< ZeroMQ-based transport
    POSIX = 1    ///< POSIX socket-based transport
};

/**
 * @brief Transport layer types (matches Python E3TransportLayer)
 */
enum class E3TransportLayer : uint8_t {
    SCTP = 0,    ///< SCTP (O-RAN standard)
    TCP = 1,     ///< TCP
    IPC = 2      ///< Unix Domain Sockets / IPC
};

/**
 * @brief E3AP action types for Setup and Subscription requests
 */
enum class ActionType : uint8_t {
    INSERT = 0,    ///< Insert/create new entry
    UPDATE = 1,    ///< Update existing entry
    DELETE = 2     ///< Delete existing entry
};

/**
 * @brief E3AP response codes
 */
enum class ResponseCode : uint8_t {
    POSITIVE = 0,  ///< Request accepted
    NEGATIVE = 1   ///< Request rejected
};

/**
 * @brief E3AP PDU types
 */
enum class PduType : uint8_t {
    SETUP_REQUEST = 0,
    SETUP_RESPONSE = 1,
    SUBSCRIPTION_REQUEST = 2,
    SUBSCRIPTION_DELETE = 3,
    SUBSCRIPTION_RESPONSE = 4,
    INDICATION_MESSAGE = 5,
    DAPP_CONTROL_ACTION = 6,
    DAPP_REPORT = 7,
    XAPP_CONTROL_ACTION = 8,
    RELEASE_MESSAGE = 9,
    MESSAGE_ACK = 10
};

/**
 * @brief Agent state machine states
 */
enum class AgentState : uint8_t {
    UNINITIALIZED = 0,  ///< Agent not yet initialized
    INITIALIZED = 1,    ///< Agent initialized but not connected
    CONNECTING = 2,     ///< Connection in progress
    CONNECTED = 3,      ///< Connected and ready for operations
    RUNNING = 4,        ///< Main processing loop active
    STOPPING = 5,       ///< Shutdown in progress
    STOPPED = 6,        ///< Agent fully stopped
    ERROR = 7           ///< Error state
};

/**
 * @brief Error codes returned by libe3 operations
 * Use shared error-code list (see libe3/error_codes.h)
 */
enum class ErrorCode : int {
#define X(name, val) name = val,
    LIBE3_ERROR_CODE_LIST
#undef X
};

/**
 * @brief Convert ErrorCode to string name (C++ wrapper)
 */
inline const char* ErrorCodeToString(ErrorCode code) {
    switch (static_cast<int>(code)) {
#define X(name, val) case val: return #name;
        LIBE3_ERROR_CODE_LIST
#undef X
        default: return "UNKNOWN_ERROR_CODE";
    }
}

// Maximum sizes for E3AP data fields (aligned with original C implementation)
constexpr size_t MAX_PROTOCOL_DATA_SIZE = 32768;
constexpr size_t MAX_ACTION_DATA_SIZE = 32768;
constexpr size_t MAX_DAPP_REPORT_DATA_SIZE = 32768;
constexpr size_t MAX_XAPP_CTRL_DATA_SIZE = 32768;
constexpr size_t MAX_RAN_FUNCTIONS = 255;
constexpr size_t DEFAULT_BUFFER_SIZE = 60000;

// Protocol version
constexpr uint32_t LIBE3_PROTOCOL_VERSION = 1;

/**
 * @brief Encoded message wrapper
 *
 * Unified wrapper for encoded data regardless of format used.
 */
struct EncodedMessage {
    std::vector<uint8_t> buffer;  ///< Encoded data buffer
    EncodingFormat format;        ///< Format used for encoding

    EncodedMessage() = default;
    EncodedMessage(std::vector<uint8_t> buf, EncodingFormat fmt)
        : buffer(std::move(buf)), format(fmt) {}
    
    size_t size() const noexcept { return buffer.size(); }
    bool empty() const noexcept { return buffer.empty(); }
    const uint8_t* data() const noexcept { return buffer.data(); }
    uint8_t* data() noexcept { return buffer.data(); }
};

/**
 * @brief RAN Function definition (SM identification)
 */
struct RanFunctionDefinition {
    uint32_t ran_function_id{0};
    std::string sm_name;
    std::string sm_version;
};

/**
 * @brief E3AP Setup Request structure
 */
struct SetupRequest {
    std::string e3ap_protocol_version;  ///< E3AP protocol version (e.g., "0.0.0")
    std::string dapp_name;               ///< Name of the dApp
    std::string dapp_version;            ///< Version of the dApp (e.g., "0.0.0")
    std::string vendor;                  ///< Vendor name (max 30 chars)
};

/**
 * @brief RAN Function Definition for Setup Response
 */
struct RanFunctionDef {
    uint32_t ran_function_identifier{0};
    std::vector<uint32_t> telemetry_identifier_list;  ///< List of telemetry identifiers
    std::vector<uint32_t> control_identifier_list;    ///< List of control identifiers
    std::vector<uint8_t> ran_function_data;
};

/**
 * @brief E3AP Setup Response structure
 */
struct SetupResponse {
    uint32_t request_id{0};                      ///< ID of the corresponding SetupRequest
    ResponseCode response_code{ResponseCode::NEGATIVE}; ///< Response code (positive/negative)
    std::optional<std::string> e3ap_protocol_version;   ///< E3AP protocol version (optional)
    std::optional<uint32_t> dapp_identifier;            ///< Assigned dApp identifier (optional)
    std::string ran_identifier;                         ///< RAN identifier (mandatory)
    std::vector<RanFunctionDef> ran_function_list;      ///< List of available RAN functions (optional)
};

/**
 * @brief E3AP Subscription Request structure
 */
struct SubscriptionRequest {
    uint32_t dapp_identifier{0};                     ///< dApp identifier
    uint32_t ran_function_identifier{0};             ///< RAN function to subscribe to
    std::vector<uint32_t> telemetry_identifier_list; ///< List of telemetry identifiers
    std::vector<uint32_t> control_identifier_list;   ///< List of control identifiers
    std::optional<uint32_t> subscription_time;       ///< How long to keep the subscription (0-3600 sec)
    std::optional<uint32_t> periodicity;             ///< Periodicity of data delivery (0-10000 microseconds)
};

/**
 * @brief E3AP Subscription Delete structure
 */
struct SubscriptionDelete {
    uint32_t dapp_identifier{0};         ///< dApp identifier
    uint32_t subscription_id{0};         ///< Subscription ID to delete
};

/**
 * @brief E3AP Subscription Response structure
 */
struct SubscriptionResponse {
    uint32_t request_id{0};                      ///< ID of the corresponding SubscriptionRequest
    uint32_t dapp_identifier{0};                  ///< dApp identifier
    ResponseCode response_code{ResponseCode::NEGATIVE}; ///< Response code (positive/negative)
    std::optional<uint32_t> subscription_id;     ///< Subscription ID (optional)
};

/**
 * @brief E3AP Indication Message structure
 */
struct IndicationMessage {
    uint32_t dapp_identifier{0};
    uint32_t ran_function_identifier{0}; ///< RAN function identifier
    std::vector<uint8_t> protocol_data;
};

/**
 * @brief E3AP dApp Control Action structure
 */
struct DAppControlAction {
    uint32_t dapp_identifier{0};
    uint32_t ran_function_identifier{0};
    uint32_t control_identifier{0};      ///< Control identifier
    std::vector<uint8_t> action_data;
};

/**
 * @brief E3AP Message Acknowledgment structure
 */
struct MessageAck {
    uint32_t request_id{0};              ///< ID of the request being acknowledged
    ResponseCode response_code{ResponseCode::NEGATIVE}; ///< Response code (positive/negative)
};

/**
 * @brief E3AP dApp Report structure
 */
struct DAppReport {
    uint32_t dapp_identifier{0};
    uint32_t ran_function_identifier{0};
    std::vector<uint8_t> report_data;
};

/**
 * @brief E3AP xApp Control Action structure
 */
struct XAppControlAction {
    uint32_t dapp_identifier{0};
    uint32_t ran_function_identifier{0};
    std::vector<uint8_t> xapp_control_data;
};

/**
 * @brief E3AP Release Message structure
 */
struct ReleaseMessage {
    uint32_t dapp_identifier{0};         ///< dApp identifier
};

/**
 * @brief Generic E3AP PDU using std::variant for type-safe union
 */
using PduChoice = std::variant<
    SetupRequest,
    SetupResponse,
    SubscriptionRequest,
    SubscriptionDelete,
    SubscriptionResponse,
    IndicationMessage,
    DAppControlAction,
    DAppReport,
    XAppControlAction,
    ReleaseMessage,
    MessageAck
>;

/**
 * @brief Generic E3AP PDU structure
 */
struct Pdu {
    PduType type{PduType::SETUP_REQUEST};
    PduChoice choice;
    uint32_t message_id{0};        ///< Unique message identifier
    uint64_t timestamp{0};         ///< Message timestamp (milliseconds since epoch)

    Pdu() : timestamp(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count())) {}
    
    explicit Pdu(PduType t) : type(t), timestamp(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count())) {}
    
    template<typename T>
    Pdu(PduType t, T&& data) : type(t), choice(std::forward<T>(data)),
        timestamp(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count())) {}

    /**
     * @brief Get PDU data with type checking
     */
    template<typename T>
    T* get_if() noexcept {
        return std::get_if<T>(&choice);
    }

    template<typename T>
    const T* get_if() const noexcept {
        return std::get_if<T>(&choice);
    }
};

/**
 * @brief Configuration for E3Agent
 */
struct E3Config {
    // RAN identification
    std::string ran_identifier;

    //  E3AP version
    std::string e3ap_version{"1.0.0"};

    // Transport configuration
    E3LinkLayer link_layer{E3LinkLayer::ZMQ};
    E3TransportLayer transport_layer{E3TransportLayer::IPC};
    
    // Ports
    uint16_t setup_port{9990};
    uint16_t subscriber_port{9999};
    uint16_t publisher_port{9991};

    // Endpoints
    std::string setup_endpoint{"ipc:///tmp/dapps/setup"};
    std::string subscriber_endpoint{"ipc:///tmp/dapps/dapp_socket"};
    std::string publisher_endpoint{"ipc:///tmp/dapps/e3_socket"};
    
    // Encoding format
#if defined(LIBE3_ENABLE_ASN1)
    EncodingFormat encoding{EncodingFormat::ASN1};
#elif defined(LIBE3_ENABLE_JSON)
    EncodingFormat encoding{EncodingFormat::JSON};
#else // Fallback
    EncodingFormat encoding{EncodingFormat::ASN1};
#endif
    
    // Timeouts (milliseconds)
    uint32_t connect_timeout_ms{5000};
    uint32_t recv_timeout_ms{1000};
    uint32_t send_timeout_ms{1000};
    
    // Buffer sizes
    size_t receive_buffer_size{DEFAULT_BUFFER_SIZE};
    size_t send_buffer_size{DEFAULT_BUFFER_SIZE};
    
    // Threading
    size_t io_threads{2};
    
    // Logging level (0=none, 1=error, 2=warn, 3=info, 4=debug, 5=trace)
    int log_level{3};
};

/**
 * @brief Timestamp type using steady_clock for monotonic time
 */
using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

/**
 * @brief dApp registration entry
 */
struct DAppEntry {
    uint32_t dapp_identifier{0};
    Timestamp registered_time;
};

/**
 * @brief Subscription entry representing dApp-RAN function association
 */
struct SubscriptionEntry {
    uint32_t dapp_identifier{0};
    uint32_t ran_function_id{0};
    Timestamp created_time;
};

// Utility functions for type conversions

/**
 * @brief Convert PduType to string representation
 */
inline const char* pdu_type_to_string(PduType type) noexcept {
    switch (type) {
        case PduType::SETUP_REQUEST: return "SetupRequest";
        case PduType::SETUP_RESPONSE: return "SetupResponse";
        case PduType::SUBSCRIPTION_REQUEST: return "SubscriptionRequest";
        case PduType::SUBSCRIPTION_DELETE: return "SubscriptionDelete";
        case PduType::SUBSCRIPTION_RESPONSE: return "SubscriptionResponse";
        case PduType::INDICATION_MESSAGE: return "IndicationMessage";
        case PduType::DAPP_CONTROL_ACTION: return "DAppControlAction";
        case PduType::DAPP_REPORT: return "DAppReport";
        case PduType::XAPP_CONTROL_ACTION: return "XAppControlAction";
        case PduType::RELEASE_MESSAGE: return "ReleaseMessage";
        case PduType::MESSAGE_ACK: return "MessageAck";
        default: return "unknown";
    }
}

/**
 * @brief Convert ActionType to string representation
 */
inline const char* action_type_to_string(ActionType type) noexcept {
    switch (type) {
        case ActionType::INSERT: return "insert";
        case ActionType::UPDATE: return "update";
        case ActionType::DELETE: return "delete";
        default: return "unknown";
    }
}

/**
 * @brief Convert ResponseCode to string representation
 */
inline const char* response_code_to_string(ResponseCode code) noexcept {
    switch (code) {
        case ResponseCode::POSITIVE: return "positive";
        case ResponseCode::NEGATIVE: return "negative";
        default: return "unknown";
    }
}

/**
 * @brief Convert ErrorCode to string representation
 */
inline const char* error_code_to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::SUCCESS: return "Success";
        case ErrorCode::INVALID_PARAM: return "Invalid parameter";
        case ErrorCode::NOT_INITIALIZED: return "Not initialized";
        case ErrorCode::ALREADY_INITIALIZED: return "Already initialized";
        case ErrorCode::NOT_CONNECTED: return "Not connected";
        case ErrorCode::CONNECTION_FAILED: return "Connection failed";
        case ErrorCode::TIMEOUT: return "Timeout";
        case ErrorCode::ENCODE_FAILED: return "Encode failed";
        case ErrorCode::DECODE_FAILED: return "Decode failed";
        case ErrorCode::SM_NOT_FOUND: return "Service Model not found";
        case ErrorCode::SM_ALREADY_REGISTERED: return "Service Model already registered";
        case ErrorCode::BUFFER_TOO_SMALL: return "Buffer too small";
        case ErrorCode::INTERNAL_ERROR: return "Internal error";
        case ErrorCode::SUBSCRIPTION_EXISTS: return "Subscription already exists";
        case ErrorCode::SUBSCRIPTION_NOT_FOUND: return "Subscription not found";
        case ErrorCode::DAPP_NOT_REGISTERED: return "dApp not registered";
        case ErrorCode::TRANSPORT_ERROR: return "Transport error";
        case ErrorCode::STATE_ERROR: return "State error";
        case ErrorCode::SM_START_FAILED: return "Service Model start failed";
        case ErrorCode::NOT_FOUND: return "Not found";
        case ErrorCode::CANCELLED: return "Operation cancelled";
        default: return "Unknown error";
    }
}

/**
 * @brief Convert AgentState to string representation
 */
inline const char* agent_state_to_string(AgentState state) noexcept {
    switch (state) {
        case AgentState::UNINITIALIZED: return "Uninitialized";
        case AgentState::INITIALIZED: return "Initialized";
        case AgentState::CONNECTING: return "Connecting";
        case AgentState::CONNECTED: return "Connected";
        case AgentState::RUNNING: return "Running";
        case AgentState::STOPPING: return "Stopping";
        case AgentState::STOPPED: return "Stopped";
        case AgentState::ERROR: return "Error";
        default: return "Unknown";
    }
}

/**
 * @brief Convert E3LinkLayer to string representation
 */
inline const char* link_layer_to_string(E3LinkLayer layer) noexcept {
    switch (layer) {
        case E3LinkLayer::ZMQ: return "zmq";
        case E3LinkLayer::POSIX: return "posix";
        default: return "unknown";
    }
}

/**
 * @brief Convert E3TransportLayer to string representation
 */
inline const char* transport_layer_to_string(E3TransportLayer layer) noexcept {
    switch (layer) {
        case E3TransportLayer::SCTP: return "sctp";
        case E3TransportLayer::TCP: return "tcp";
        case E3TransportLayer::IPC: return "ipc";
        default: return "unknown";
    }
}

} // namespace libe3

#endif // LIBE3_TYPES_HPP
