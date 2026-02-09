/**
 * @file asn1_encoder.cpp
 * @brief ASN.1 APER Encoder implementation
 *
 * Ported from the original C implementation's e3ap_handler.c ASN.1 functions.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "asn1_encoder.hpp"
#include "libe3/logger.hpp"

// ASN.1 generated headers
extern "C" {
#include "E3-PDU.h"
#include "E3-SetupRequest.h"
#include "E3-SetupResponse.h"
#include "E3-RanFunctionDefinition.h"
#include "E3-SubscriptionRequest.h"
#include "E3-SubscriptionDelete.h"
#include "E3-SubscriptionResponse.h"
#include "E3-IndicationMessage.h"
#include "E3-DAppControlAction.h"
#include "E3-DAppReport.h"
#include "E3-XAppControlAction.h"
#include "E3-ReleaseMessage.h"
#include "E3-MessageAck.h"
}

#include <cstring>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "Asn1Enc";
} // anonymous namespace

// ============================================================================
// Encoding
// ============================================================================

EncodeResult<EncodedMessage> Asn1E3Encoder::encode(const Pdu& pdu) {
    E3_PDU* asn1_pdu = pdu_to_asn1(pdu);
    if (!asn1_pdu) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to convert PDU to ASN.1";
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }

    // Allocate buffer for encoding
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    // Encode using ASN.1 APER
    asn_enc_rval_t enc_rval = aper_encode_to_buffer(
        &asn_DEF_E3_PDU,
        nullptr,
        asn1_pdu,
        buffer.data(),
        buffer.size()
    );
    
    if (enc_rval.encoded == -1) {
        E3_LOG_ERROR(LOG_TAG) << "APER encoding failed for type: " 
                              << (enc_rval.failed_type ? enc_rval.failed_type->name : "Unknown");
        ASN_STRUCT_FREE(asn_DEF_E3_PDU, asn1_pdu);
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }
    
    // Resize buffer to actual encoded size
    buffer.resize(static_cast<size_t>(enc_rval.encoded));
    
    ASN_STRUCT_FREE(asn_DEF_E3_PDU, asn1_pdu);
    
    EncodedMessage msg;
    msg.buffer = std::move(buffer);
    msg.format = EncodingFormat::ASN1;
    
    E3_LOG_DEBUG(LOG_TAG) << "Encoded PDU: " << enc_rval.encoded << " bytes";
    return msg;
}

// ============================================================================
// Decoding
// ============================================================================

EncodeResult<Pdu> Asn1E3Encoder::decode(const EncodedMessage& encoded) {
    return decode(encoded.buffer.data(), encoded.buffer.size());
}

EncodeResult<Pdu> Asn1E3Encoder::decode(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return tl::unexpected(ErrorCode::INVALID_PARAM);
    }
    
    // Decode ASN.1 buffer
    E3_PDU* asn1_pdu = nullptr;
    asn_dec_rval_t dec_rval = aper_decode(
        nullptr,
        &asn_DEF_E3_PDU,
        reinterpret_cast<void**>(&asn1_pdu),
        data,
        size,
        0, 0
    );
    
    if (dec_rval.code != RC_OK) {
        E3_LOG_ERROR(LOG_TAG) << "APER decoding failed with code " << dec_rval.code;
        if (asn1_pdu) {
            ASN_STRUCT_FREE(asn_DEF_E3_PDU, asn1_pdu);
        }
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }
    
    // Convert to generic PDU
    Pdu pdu = asn1_to_pdu(asn1_pdu);
    
    ASN_STRUCT_FREE(asn_DEF_E3_PDU, asn1_pdu);
    
    return pdu;
}

// ============================================================================
// PDU Conversion: Generic -> ASN.1
// ============================================================================

E3_PDU* Asn1E3Encoder::pdu_to_asn1(const Pdu& pdu) const {
    E3_PDU* asn1_pdu = static_cast<E3_PDU*>(calloc(1, sizeof(E3_PDU)));
    if (!asn1_pdu) {
        return nullptr;
    }
    
    // Set top-level message ID (E3-PDU.id)
    asn1_pdu->id = pdu.message_id;
    
    switch (pdu.type) {
        case PduType::SETUP_REQUEST: {
            const auto* req = std::get_if<SetupRequest>(&pdu.choice);
            if (!req) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_setupRequest;
            asn1_pdu->msg.choice.setupRequest = static_cast<E3_SetupRequest_t*>(
                calloc(1, sizeof(E3_SetupRequest_t)));
            if (!asn1_pdu->msg.choice.setupRequest) { free(asn1_pdu); return nullptr; }
            
            // Set e3apProtocolVersion string
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.setupRequest->e3apProtocolVersion,
                req->e3ap_protocol_version.c_str(),
                static_cast<int>(req->e3ap_protocol_version.size()));
            
            // Set dAppName string
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.setupRequest->dAppName,
                req->dapp_name.c_str(),
                static_cast<int>(req->dapp_name.size()));
            
            // Set dAppVersion string
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.setupRequest->dAppVersion,
                req->dapp_version.c_str(),
                static_cast<int>(req->dapp_version.size()));
            
            // Set vendor string
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.setupRequest->vendor,
                req->vendor.c_str(),
                static_cast<int>(req->vendor.size()));
            break;
        }
        
        case PduType::SETUP_RESPONSE: {
            const auto* resp = std::get_if<SetupResponse>(&pdu.choice);
            if (!resp) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_setupResponse;
            asn1_pdu->msg.choice.setupResponse = static_cast<E3_SetupResponse_t*>(
                calloc(1, sizeof(E3_SetupResponse_t)));
            if (!asn1_pdu->msg.choice.setupResponse) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.setupResponse->requestId = resp->request_id;
            asn1_pdu->msg.choice.setupResponse->responseCode = 
                (resp->response_code == ResponseCode::POSITIVE) ? 0 : 1;
            
            // Set optional e3apProtocolVersion
            if (resp->e3ap_protocol_version.has_value()) {
                asn1_pdu->msg.choice.setupResponse->e3apProtocolVersion = 
                    static_cast<OCTET_STRING_t*>(calloc(1, sizeof(OCTET_STRING_t)));
                OCTET_STRING_fromBuf(asn1_pdu->msg.choice.setupResponse->e3apProtocolVersion,
                    resp->e3ap_protocol_version.value().c_str(),
                    static_cast<int>(resp->e3ap_protocol_version.value().size()));
            }
            
            // Set optional dAppIdentifier
            if (resp->dapp_identifier.has_value()) {
                asn1_pdu->msg.choice.setupResponse->dAppIdentifier = 
                    static_cast<long*>(malloc(sizeof(long)));
                *asn1_pdu->msg.choice.setupResponse->dAppIdentifier = resp->dapp_identifier.value();
            }
            
            // Set mandatory ranIdentifier
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.setupResponse->ranIdentifier,
                resp->ran_identifier.c_str(),
                static_cast<int>(resp->ran_identifier.size()));
            
            // Set optional ranFunctionList
            if (!resp->ran_function_list.empty()) {
                for (const auto& func : resp->ran_function_list) {
                    E3_RanFunctionDefinition_t* ran_func = 
                        static_cast<E3_RanFunctionDefinition_t*>(calloc(1, sizeof(E3_RanFunctionDefinition_t)));
                    ran_func->ranFunctionIdentifier = func.ran_function_identifier;
                    
                    // Encode telemetryIdentifierList
                    for (uint32_t tel_id : func.telemetry_identifier_list) {
                        long* id = static_cast<long*>(malloc(sizeof(long)));
                        *id = tel_id;
                        ASN_SEQUENCE_ADD(&ran_func->telemetryIdentifierList, id);
                    }
                    
                    // Encode controlIdentifierList
                    for (uint32_t ctrl_id : func.control_identifier_list) {
                        long* id = static_cast<long*>(malloc(sizeof(long)));
                        *id = ctrl_id;
                        ASN_SEQUENCE_ADD(&ran_func->controlIdentifierList, id);
                    }
                    
                    OCTET_STRING_fromBuf(&ran_func->ranFunctionData,
                        reinterpret_cast<const char*>(func.ran_function_data.data()),
                        static_cast<int>(func.ran_function_data.size()));

                    // Ensure ranFunctionList container is allocated (optional field)
                    if (!asn1_pdu->msg.choice.setupResponse->ranFunctionList) {
                        asn1_pdu->msg.choice.setupResponse->ranFunctionList =
                            static_cast<decltype(asn1_pdu->msg.choice.setupResponse->ranFunctionList)>(
                                calloc(1, sizeof(*asn1_pdu->msg.choice.setupResponse->ranFunctionList)));
                    }

                    // Append the ran function into the sequence's internal list
                    ASN_SEQUENCE_ADD(&asn1_pdu->msg.choice.setupResponse->ranFunctionList->list, ran_func);
                }
            }
            break;
        }
        
        case PduType::SUBSCRIPTION_REQUEST: {
            const auto* req = std::get_if<SubscriptionRequest>(&pdu.choice);
            if (!req) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_subscriptionRequest;
            asn1_pdu->msg.choice.subscriptionRequest = static_cast<E3_SubscriptionRequest_t*>(
                calloc(1, sizeof(E3_SubscriptionRequest_t)));
            if (!asn1_pdu->msg.choice.subscriptionRequest) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.subscriptionRequest->dAppIdentifier = req->dapp_identifier;
            asn1_pdu->msg.choice.subscriptionRequest->ranFunctionIdentifier = req->ran_function_identifier;
            
            // Encode telemetryIdentifierList
            for (uint32_t tel_id : req->telemetry_identifier_list) {
                long* id = static_cast<long*>(malloc(sizeof(long)));
                *id = tel_id;
                ASN_SEQUENCE_ADD(&asn1_pdu->msg.choice.subscriptionRequest->telemetryIdentifierList, id);
            }
            
            // Encode controlIdentifierList
            for (uint32_t ctrl_id : req->control_identifier_list) {
                long* id = static_cast<long*>(malloc(sizeof(long)));
                *id = ctrl_id;
                ASN_SEQUENCE_ADD(&asn1_pdu->msg.choice.subscriptionRequest->controlIdentifierList, id);
            }
            
            // Set optional subscriptionTime
            if (req->subscription_time.has_value()) {
                asn1_pdu->msg.choice.subscriptionRequest->subscriptionTime = 
                    static_cast<long*>(malloc(sizeof(long)));
                *asn1_pdu->msg.choice.subscriptionRequest->subscriptionTime = req->subscription_time.value();
            }
            break;
        }
        
        case PduType::SUBSCRIPTION_DELETE: {
            const auto* del = std::get_if<SubscriptionDelete>(&pdu.choice);
            if (!del) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_subscriptionDelete;
            asn1_pdu->msg.choice.subscriptionDelete = static_cast<E3_SubscriptionDelete_t*>(
                calloc(1, sizeof(E3_SubscriptionDelete_t)));
            if (!asn1_pdu->msg.choice.subscriptionDelete) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.subscriptionDelete->dAppIdentifier = del->dapp_identifier;
            asn1_pdu->msg.choice.subscriptionDelete->subscriptionId = del->subscription_id;
            break;
        }
        
        case PduType::SUBSCRIPTION_RESPONSE: {
            const auto* resp = std::get_if<SubscriptionResponse>(&pdu.choice);
            if (!resp) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_subscriptionResponse;
            asn1_pdu->msg.choice.subscriptionResponse = static_cast<E3_SubscriptionResponse_t*>(
                calloc(1, sizeof(E3_SubscriptionResponse_t)));
            if (!asn1_pdu->msg.choice.subscriptionResponse) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.subscriptionResponse->requestId = resp->request_id;
            asn1_pdu->msg.choice.subscriptionResponse->dAppIdentifier = resp->dapp_identifier;
            asn1_pdu->msg.choice.subscriptionResponse->responseCode = 
                (resp->response_code == ResponseCode::POSITIVE) ? 0 : 1;
            
            // Set optional subscriptionId
            if (resp->subscription_id.has_value()) {
                asn1_pdu->msg.choice.subscriptionResponse->subscriptionId = 
                    static_cast<long*>(malloc(sizeof(long)));
                *asn1_pdu->msg.choice.subscriptionResponse->subscriptionId = resp->subscription_id.value();
            }
            break;
        }
        
        case PduType::INDICATION_MESSAGE: {
            const auto* msg = std::get_if<IndicationMessage>(&pdu.choice);
            if (!msg) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_indicationMessage;
            asn1_pdu->msg.choice.indicationMessage = static_cast<E3_IndicationMessage_t*>(
                calloc(1, sizeof(E3_IndicationMessage_t)));
            if (!asn1_pdu->msg.choice.indicationMessage) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.indicationMessage->dAppIdentifier = msg->dapp_identifier;
            asn1_pdu->msg.choice.indicationMessage->ranFunctionIdentifier = msg->ran_function_identifier;
            
            // Copy protocol data
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.indicationMessage->protocolData,
                reinterpret_cast<const char*>(msg->protocol_data.data()),
                static_cast<int>(msg->protocol_data.size()));
            break;
        }
        
        case PduType::DAPP_CONTROL_ACTION: {
            const auto* action = std::get_if<DAppControlAction>(&pdu.choice);
            if (!action) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_dAppControlAction;
            asn1_pdu->msg.choice.dAppControlAction = static_cast<E3_DAppControlAction_t*>(
                calloc(1, sizeof(E3_DAppControlAction_t)));
            if (!asn1_pdu->msg.choice.dAppControlAction) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.dAppControlAction->dAppIdentifier = action->dapp_identifier;
            asn1_pdu->msg.choice.dAppControlAction->ranFunctionIdentifier = action->ran_function_identifier;
            asn1_pdu->msg.choice.dAppControlAction->controlIdentifier = action->control_identifier;
            
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.dAppControlAction->actionData,
                reinterpret_cast<const char*>(action->action_data.data()),
                static_cast<int>(action->action_data.size()));
            break;
        }
        
        case PduType::DAPP_REPORT: {
            const auto* report = std::get_if<DAppReport>(&pdu.choice);
            if (!report) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_dAppReport;
            asn1_pdu->msg.choice.dAppReport = static_cast<E3_DAppReport_t*>(
                calloc(1, sizeof(E3_DAppReport_t)));
            if (!asn1_pdu->msg.choice.dAppReport) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.dAppReport->dAppIdentifier = report->dapp_identifier;
            asn1_pdu->msg.choice.dAppReport->ranFunctionIdentifier = report->ran_function_identifier;
            
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.dAppReport->reportData,
                reinterpret_cast<const char*>(report->report_data.data()),
                static_cast<int>(report->report_data.size()));
            break;
        }
        
        case PduType::XAPP_CONTROL_ACTION: {
            const auto* action = std::get_if<XAppControlAction>(&pdu.choice);
            if (!action) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_xAppControlAction;
            asn1_pdu->msg.choice.xAppControlAction = static_cast<E3_XAppControlAction_t*>(
                calloc(1, sizeof(E3_XAppControlAction_t)));
            if (!asn1_pdu->msg.choice.xAppControlAction) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.xAppControlAction->dAppIdentifier = action->dapp_identifier;
            asn1_pdu->msg.choice.xAppControlAction->ranFunctionIdentifier = action->ran_function_identifier;
            
            OCTET_STRING_fromBuf(&asn1_pdu->msg.choice.xAppControlAction->xAppControlData,
                reinterpret_cast<const char*>(action->xapp_control_data.data()),
                static_cast<int>(action->xapp_control_data.size()));
            break;
        }
        
        case PduType::MESSAGE_ACK: {
            const auto* ack = std::get_if<MessageAck>(&pdu.choice);
            if (!ack) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_messageAck;
            asn1_pdu->msg.choice.messageAck = static_cast<E3_MessageAck_t*>(
                calloc(1, sizeof(E3_MessageAck_t)));
            if (!asn1_pdu->msg.choice.messageAck) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.messageAck->requestId = ack->request_id;
            asn1_pdu->msg.choice.messageAck->responseCode = 
                (ack->response_code == ResponseCode::POSITIVE) ? 0 : 1;
            break;
        }
        
        case PduType::RELEASE_MESSAGE: {
            const auto* msg = std::get_if<ReleaseMessage>(&pdu.choice);
            if (!msg) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.present = E3_PDU__msg_PR_releaseMessage;
            asn1_pdu->msg.choice.releaseMessage = static_cast<E3_ReleaseMessage_t*>(
                calloc(1, sizeof(E3_ReleaseMessage_t)));
            if (!asn1_pdu->msg.choice.releaseMessage) { free(asn1_pdu); return nullptr; }
            
            asn1_pdu->msg.choice.releaseMessage->dAppIdentifier = msg->dapp_identifier;
            break;
        }
        
        default:
            free(asn1_pdu);
            return nullptr;
    }
    
    return asn1_pdu;
}

// ============================================================================
// PDU Conversion: ASN.1 -> Generic
// ============================================================================

Pdu Asn1E3Encoder::asn1_to_pdu(const E3_PDU* asn1_pdu) const {
    Pdu pdu;
    
    // Read top-level message ID (E3-PDU.id)
    pdu.message_id = static_cast<uint32_t>(asn1_pdu->id);
    
    switch (asn1_pdu->msg.present) {
        case E3_PDU__msg_PR_setupRequest: {
            pdu.type = PduType::SETUP_REQUEST;
            
            SetupRequest req;
            
            // Extract e3apProtocolVersion string
            const OCTET_STRING_t* proto_ver = &asn1_pdu->msg.choice.setupRequest->e3apProtocolVersion;
            req.e3ap_protocol_version.assign(reinterpret_cast<const char*>(proto_ver->buf), proto_ver->size);
            
            // Extract dAppName string
            const OCTET_STRING_t* dapp_name = &asn1_pdu->msg.choice.setupRequest->dAppName;
            req.dapp_name.assign(reinterpret_cast<const char*>(dapp_name->buf), dapp_name->size);
            
            // Extract dAppVersion string
            const OCTET_STRING_t* dapp_ver = &asn1_pdu->msg.choice.setupRequest->dAppVersion;
            req.dapp_version.assign(reinterpret_cast<const char*>(dapp_ver->buf), dapp_ver->size);
            
            // Extract vendor string
            const OCTET_STRING_t* vendor = &asn1_pdu->msg.choice.setupRequest->vendor;
            req.vendor.assign(reinterpret_cast<const char*>(vendor->buf), vendor->size);
            
            pdu.choice = req;
            break;
        }
        
        case E3_PDU__msg_PR_setupResponse: {
            pdu.type = PduType::SETUP_RESPONSE;
            
            SetupResponse resp;
            resp.request_id = static_cast<uint32_t>(asn1_pdu->msg.choice.setupResponse->requestId);
            resp.response_code = (asn1_pdu->msg.choice.setupResponse->responseCode == 0) 
                ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
            
            // Extract optional e3apProtocolVersion
            if (asn1_pdu->msg.choice.setupResponse->e3apProtocolVersion) {
                const OCTET_STRING_t* proto_ver = asn1_pdu->msg.choice.setupResponse->e3apProtocolVersion;
                resp.e3ap_protocol_version = std::string(
                    reinterpret_cast<const char*>(proto_ver->buf), proto_ver->size);
            }
            
            // Extract optional dAppIdentifier
            if (asn1_pdu->msg.choice.setupResponse->dAppIdentifier) {
                resp.dapp_identifier = *asn1_pdu->msg.choice.setupResponse->dAppIdentifier;
            }
            
            // Extract mandatory ranIdentifier
            const OCTET_STRING_t* ran_id = &asn1_pdu->msg.choice.setupResponse->ranIdentifier;
            resp.ran_identifier.assign(reinterpret_cast<const char*>(ran_id->buf), ran_id->size);
            
            // Extract ranFunctionList
            if (asn1_pdu->msg.choice.setupResponse->ranFunctionList) {
                int count = asn1_pdu->msg.choice.setupResponse->ranFunctionList->list.count;
                for (int i = 0; i < count; i++) {
                    E3_RanFunctionDefinition_t* asn_func = 
                        asn1_pdu->msg.choice.setupResponse->ranFunctionList->list.array[i];
                    RanFunctionDef func;
                    func.ran_function_identifier = static_cast<uint32_t>(asn_func->ranFunctionIdentifier);
                    
                    // Decode telemetryIdentifierList
                    int tel_count = asn_func->telemetryIdentifierList.list.count;
                    for (int j = 0; j < tel_count; j++) {
                        func.telemetry_identifier_list.push_back(
                            static_cast<uint32_t>(*asn_func->telemetryIdentifierList.list.array[j]));
                    }
                    
                    // Decode controlIdentifierList
                    int ctrl_count = asn_func->controlIdentifierList.list.count;
                    for (int j = 0; j < ctrl_count; j++) {
                        func.control_identifier_list.push_back(
                            static_cast<uint32_t>(*asn_func->controlIdentifierList.list.array[j]));
                    }
                    
                    func.ran_function_data.assign(
                        asn_func->ranFunctionData.buf,
                        asn_func->ranFunctionData.buf + asn_func->ranFunctionData.size);
                    resp.ran_function_list.push_back(func);
                }
            }
            
            pdu.choice = resp;
            break;
        }
        
        case E3_PDU__msg_PR_subscriptionRequest: {
            pdu.type = PduType::SUBSCRIPTION_REQUEST;
            
            SubscriptionRequest req;
            req.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionRequest->dAppIdentifier);
            req.ran_function_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionRequest->ranFunctionIdentifier);
            
            // Decode telemetryIdentifierList
            int tel_count = asn1_pdu->msg.choice.subscriptionRequest->telemetryIdentifierList.list.count;
            for (int i = 0; i < tel_count; i++) {
                req.telemetry_identifier_list.push_back(
                    static_cast<uint32_t>(*asn1_pdu->msg.choice.subscriptionRequest->telemetryIdentifierList.list.array[i]));
            }
            
            // Decode controlIdentifierList
            int ctrl_count = asn1_pdu->msg.choice.subscriptionRequest->controlIdentifierList.list.count;
            for (int i = 0; i < ctrl_count; i++) {
                req.control_identifier_list.push_back(
                    static_cast<uint32_t>(*asn1_pdu->msg.choice.subscriptionRequest->controlIdentifierList.list.array[i]));
            }
            
            // Decode optional subscriptionTime
            if (asn1_pdu->msg.choice.subscriptionRequest->subscriptionTime) {
                req.subscription_time = *asn1_pdu->msg.choice.subscriptionRequest->subscriptionTime;
            }
            
            pdu.choice = req;
            break;
        }
        
        case E3_PDU__msg_PR_subscriptionDelete: {
            pdu.type = PduType::SUBSCRIPTION_DELETE;
            
            SubscriptionDelete del;
            del.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionDelete->dAppIdentifier);
            del.subscription_id = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionDelete->subscriptionId);
            
            pdu.choice = del;
            break;
        }
        
        case E3_PDU__msg_PR_subscriptionResponse: {
            pdu.type = PduType::SUBSCRIPTION_RESPONSE;
            
            SubscriptionResponse resp;
            resp.request_id = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionResponse->requestId);
            resp.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.subscriptionResponse->dAppIdentifier);
            resp.response_code = (asn1_pdu->msg.choice.subscriptionResponse->responseCode == 0)
                ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
            
            // Decode optional subscriptionId
            if (asn1_pdu->msg.choice.subscriptionResponse->subscriptionId) {
                resp.subscription_id = *asn1_pdu->msg.choice.subscriptionResponse->subscriptionId;
            }
            
            pdu.choice = resp;
            break;
        }
        
        case E3_PDU__msg_PR_indicationMessage: {
            pdu.type = PduType::INDICATION_MESSAGE;
            
            IndicationMessage msg;
            msg.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.indicationMessage->dAppIdentifier);
            msg.ran_function_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.indicationMessage->ranFunctionIdentifier);
            
            // Copy protocol data
            const OCTET_STRING_t* data = &asn1_pdu->msg.choice.indicationMessage->protocolData;
            msg.protocol_data.assign(data->buf, data->buf + data->size);
            
            pdu.choice = msg;
            break;
        }
        
        case E3_PDU__msg_PR_dAppControlAction: {
            pdu.type = PduType::DAPP_CONTROL_ACTION;
            
            DAppControlAction action;
            action.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.dAppControlAction->dAppIdentifier);
            action.ran_function_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.dAppControlAction->ranFunctionIdentifier);
            action.control_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.dAppControlAction->controlIdentifier);
            
            const OCTET_STRING_t* data = &asn1_pdu->msg.choice.dAppControlAction->actionData;
            action.action_data.assign(data->buf, data->buf + data->size);
            
            pdu.choice = action;
            break;
        }
        
        case E3_PDU__msg_PR_dAppReport: {
            pdu.type = PduType::DAPP_REPORT;
            
            DAppReport report;
            report.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.dAppReport->dAppIdentifier);
            report.ran_function_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.dAppReport->ranFunctionIdentifier);
            
            const OCTET_STRING_t* data = &asn1_pdu->msg.choice.dAppReport->reportData;
            report.report_data.assign(data->buf, data->buf + data->size);
            
            pdu.choice = report;
            break;
        }
        
        case E3_PDU__msg_PR_xAppControlAction: {
            pdu.type = PduType::XAPP_CONTROL_ACTION;
            
            XAppControlAction action;
            action.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.xAppControlAction->dAppIdentifier);
            action.ran_function_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.xAppControlAction->ranFunctionIdentifier);
            
            const OCTET_STRING_t* data = &asn1_pdu->msg.choice.xAppControlAction->xAppControlData;
            action.xapp_control_data.assign(data->buf, data->buf + data->size);
            
            pdu.choice = action;
            break;
        }
        
        case E3_PDU__msg_PR_messageAck: {
            pdu.type = PduType::MESSAGE_ACK;
            
            MessageAck ack;
            ack.request_id = static_cast<uint32_t>(asn1_pdu->msg.choice.messageAck->requestId);
            ack.response_code = (asn1_pdu->msg.choice.messageAck->responseCode == 0)
                ? ResponseCode::POSITIVE : ResponseCode::NEGATIVE;
            
            pdu.choice = ack;
            break;
        }
        
        case E3_PDU__msg_PR_releaseMessage: {
            pdu.type = PduType::RELEASE_MESSAGE;
            
            ReleaseMessage msg;
            msg.dapp_identifier = static_cast<uint32_t>(asn1_pdu->msg.choice.releaseMessage->dAppIdentifier);
            
            pdu.choice = msg;
            break;
        }
        
        default:
            E3_LOG_ERROR(LOG_TAG) << "Unknown ASN.1 PDU type: " << asn1_pdu->msg.present;
            break;
    }
    
    return pdu;
}

} // namespace libe3
