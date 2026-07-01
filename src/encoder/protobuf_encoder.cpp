/**
 * @file protobuf_encoder.cpp
 * @brief Protocol Buffers encoder for E3AP PDUs
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf_encoder.hpp"

#include "e3ap-1.0.0.pb.h"

#include <string>
#include <vector>

namespace libe3 {

namespace pb = e3ap::v1;

namespace {

pb::ResponseCode to_pb_response_code(ResponseCode rc) {
    return rc == ResponseCode::POSITIVE ? pb::RESPONSE_CODE_POSITIVE
                                        : pb::RESPONSE_CODE_NEGATIVE;
}

ResponseCode from_pb_response_code(pb::ResponseCode rc) {
    return rc == pb::RESPONSE_CODE_NEGATIVE ? ResponseCode::NEGATIVE
                                            : ResponseCode::POSITIVE;
}

std::vector<uint8_t> to_bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace

bool ProtobufE3Encoder::pdu_to_proto(const Pdu& pdu, pb::E3Pdu& out) const {
    out.set_id(pdu.message_id);
    out.set_timestamp(pdu.timestamp);

    switch (pdu.type) {
        case PduType::SETUP_REQUEST: {
            const auto* s = pdu.get_if<SetupRequest>();
            if (!s) return false;
            auto* m = out.mutable_setup_request();
            m->set_e3ap_protocol_version(s->e3ap_protocol_version);
            m->set_dapp_name(s->dapp_name);
            m->set_dapp_version(s->dapp_version);
            m->set_vendor(s->vendor);
            return true;
        }
        case PduType::SETUP_RESPONSE: {
            const auto* s = pdu.get_if<SetupResponse>();
            if (!s) return false;
            auto* m = out.mutable_setup_response();
            m->set_request_id(s->request_id);
            m->set_response_code(to_pb_response_code(s->response_code));
            if (s->e3ap_protocol_version) m->set_e3ap_protocol_version(*s->e3ap_protocol_version);
            if (s->dapp_identifier) m->set_dapp_identifier(*s->dapp_identifier);
            m->set_ran_identifier(s->ran_identifier);
            for (const auto& rf : s->ran_function_list) {
                auto* pf = m->add_ran_function_list();
                pf->set_ran_function_identifier(rf.ran_function_identifier);
                for (auto t : rf.telemetry_identifier_list) pf->add_telemetry_identifier_list(t);
                for (auto c : rf.control_identifier_list) pf->add_control_identifier_list(c);
                pf->set_ran_function_data(rf.ran_function_data.data(), rf.ran_function_data.size());
            }
            return true;
        }
        case PduType::SUBSCRIPTION_REQUEST: {
            const auto* s = pdu.get_if<SubscriptionRequest>();
            if (!s) return false;
            auto* m = out.mutable_subscription_request();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_ran_function_identifier(s->ran_function_identifier);
            for (auto t : s->telemetry_identifier_list) m->add_telemetry_identifier_list(t);
            for (auto c : s->control_identifier_list) m->add_control_identifier_list(c);
            if (s->subscription_time) m->set_subscription_time(*s->subscription_time);
            if (s->periodicity) m->set_periodicity(*s->periodicity);
            return true;
        }
        case PduType::SUBSCRIPTION_DELETE: {
            const auto* s = pdu.get_if<SubscriptionDelete>();
            if (!s) return false;
            auto* m = out.mutable_subscription_delete();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_subscription_id(s->subscription_id);
            return true;
        }
        case PduType::SUBSCRIPTION_RESPONSE: {
            const auto* s = pdu.get_if<SubscriptionResponse>();
            if (!s) return false;
            auto* m = out.mutable_subscription_response();
            m->set_request_id(s->request_id);
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_response_code(to_pb_response_code(s->response_code));
            if (s->subscription_id) m->set_subscription_id(*s->subscription_id);
            return true;
        }
        case PduType::INDICATION_MESSAGE: {
            const auto* s = pdu.get_if<IndicationMessage>();
            if (!s) return false;
            auto* m = out.mutable_indication_message();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_ran_function_identifier(s->ran_function_identifier);
            m->set_protocol_data(s->protocol_data.data(), s->protocol_data.size());
            return true;
        }
        case PduType::DAPP_CONTROL_ACTION: {
            const auto* s = pdu.get_if<DAppControlAction>();
            if (!s) return false;
            auto* m = out.mutable_dapp_control_action();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_ran_function_identifier(s->ran_function_identifier);
            m->set_control_identifier(s->control_identifier);
            m->set_action_data(s->action_data.data(), s->action_data.size());
            return true;
        }
        case PduType::DAPP_REPORT: {
            const auto* s = pdu.get_if<DAppReport>();
            if (!s) return false;
            auto* m = out.mutable_dapp_report();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_ran_function_identifier(s->ran_function_identifier);
            m->set_report_data(s->report_data.data(), s->report_data.size());
            return true;
        }
        case PduType::XAPP_CONTROL_ACTION: {
            const auto* s = pdu.get_if<XAppControlAction>();
            if (!s) return false;
            auto* m = out.mutable_xapp_control_action();
            m->set_dapp_identifier(s->dapp_identifier);
            m->set_ran_function_identifier(s->ran_function_identifier);
            m->set_xapp_control_data(s->xapp_control_data.data(), s->xapp_control_data.size());
            return true;
        }
        case PduType::RELEASE_MESSAGE: {
            const auto* s = pdu.get_if<ReleaseMessage>();
            if (!s) return false;
            auto* m = out.mutable_release_message();
            m->set_dapp_identifier(s->dapp_identifier);
            return true;
        }
        case PduType::MESSAGE_ACK: {
            const auto* s = pdu.get_if<MessageAck>();
            if (!s) return false;
            auto* m = out.mutable_message_ack();
            m->set_request_id(s->request_id);
            m->set_response_code(to_pb_response_code(s->response_code));
            return true;
        }
    }
    return false;
}

Pdu ProtobufE3Encoder::proto_to_pdu(const pb::E3Pdu& proto) const {
    Pdu pdu;
    pdu.message_id = proto.id();
    pdu.timestamp = proto.timestamp();

    switch (proto.msg_case()) {
        case pb::E3Pdu::kSetupRequest: {
            const auto& m = proto.setup_request();
            SetupRequest s;
            s.e3ap_protocol_version = m.e3ap_protocol_version();
            s.dapp_name = m.dapp_name();
            s.dapp_version = m.dapp_version();
            s.vendor = m.vendor();
            pdu.type = PduType::SETUP_REQUEST;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kSetupResponse: {
            const auto& m = proto.setup_response();
            SetupResponse s;
            s.request_id = m.request_id();
            s.response_code = from_pb_response_code(m.response_code());
            if (m.has_e3ap_protocol_version()) s.e3ap_protocol_version = m.e3ap_protocol_version();
            if (m.has_dapp_identifier()) s.dapp_identifier = m.dapp_identifier();
            s.ran_identifier = m.ran_identifier();
            for (int i = 0; i < m.ran_function_list_size(); ++i) {
                const auto& pf = m.ran_function_list(i);
                RanFunctionDef rf;
                rf.ran_function_identifier = pf.ran_function_identifier();
                for (int j = 0; j < pf.telemetry_identifier_list_size(); ++j)
                    rf.telemetry_identifier_list.push_back(pf.telemetry_identifier_list(j));
                for (int j = 0; j < pf.control_identifier_list_size(); ++j)
                    rf.control_identifier_list.push_back(pf.control_identifier_list(j));
                rf.ran_function_data = to_bytes(pf.ran_function_data());
                s.ran_function_list.push_back(std::move(rf));
            }
            pdu.type = PduType::SETUP_RESPONSE;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kSubscriptionRequest: {
            const auto& m = proto.subscription_request();
            SubscriptionRequest s;
            s.dapp_identifier = m.dapp_identifier();
            s.ran_function_identifier = m.ran_function_identifier();
            for (int j = 0; j < m.telemetry_identifier_list_size(); ++j)
                s.telemetry_identifier_list.push_back(m.telemetry_identifier_list(j));
            for (int j = 0; j < m.control_identifier_list_size(); ++j)
                s.control_identifier_list.push_back(m.control_identifier_list(j));
            if (m.has_subscription_time()) s.subscription_time = m.subscription_time();
            if (m.has_periodicity()) s.periodicity = m.periodicity();
            pdu.type = PduType::SUBSCRIPTION_REQUEST;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kSubscriptionDelete: {
            const auto& m = proto.subscription_delete();
            SubscriptionDelete s;
            s.dapp_identifier = m.dapp_identifier();
            s.subscription_id = m.subscription_id();
            pdu.type = PduType::SUBSCRIPTION_DELETE;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kSubscriptionResponse: {
            const auto& m = proto.subscription_response();
            SubscriptionResponse s;
            s.request_id = m.request_id();
            s.dapp_identifier = m.dapp_identifier();
            s.response_code = from_pb_response_code(m.response_code());
            if (m.has_subscription_id()) s.subscription_id = m.subscription_id();
            pdu.type = PduType::SUBSCRIPTION_RESPONSE;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kIndicationMessage: {
            const auto& m = proto.indication_message();
            IndicationMessage s;
            s.dapp_identifier = m.dapp_identifier();
            s.ran_function_identifier = m.ran_function_identifier();
            s.protocol_data = to_bytes(m.protocol_data());
            pdu.type = PduType::INDICATION_MESSAGE;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kDappControlAction: {
            const auto& m = proto.dapp_control_action();
            DAppControlAction s;
            s.dapp_identifier = m.dapp_identifier();
            s.ran_function_identifier = m.ran_function_identifier();
            s.control_identifier = m.control_identifier();
            s.action_data = to_bytes(m.action_data());
            pdu.type = PduType::DAPP_CONTROL_ACTION;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kDappReport: {
            const auto& m = proto.dapp_report();
            DAppReport s;
            s.dapp_identifier = m.dapp_identifier();
            s.ran_function_identifier = m.ran_function_identifier();
            s.report_data = to_bytes(m.report_data());
            pdu.type = PduType::DAPP_REPORT;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kXappControlAction: {
            const auto& m = proto.xapp_control_action();
            XAppControlAction s;
            s.dapp_identifier = m.dapp_identifier();
            s.ran_function_identifier = m.ran_function_identifier();
            s.xapp_control_data = to_bytes(m.xapp_control_data());
            pdu.type = PduType::XAPP_CONTROL_ACTION;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kReleaseMessage: {
            const auto& m = proto.release_message();
            ReleaseMessage s;
            s.dapp_identifier = m.dapp_identifier();
            pdu.type = PduType::RELEASE_MESSAGE;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::kMessageAck: {
            const auto& m = proto.message_ack();
            MessageAck s;
            s.request_id = m.request_id();
            s.response_code = from_pb_response_code(m.response_code());
            pdu.type = PduType::MESSAGE_ACK;
            pdu.choice = std::move(s);
            break;
        }
        case pb::E3Pdu::MSG_NOT_SET:
            break;
    }
    return pdu;
}

EncodeResult<EncodedMessage> ProtobufE3Encoder::encode(const Pdu& pdu) {
    pb::E3Pdu proto;
    if (!pdu_to_proto(pdu, proto)) {
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }
    std::string serialized;
    if (!proto.SerializeToString(&serialized)) {
        return tl::unexpected(ErrorCode::ENCODE_FAILED);
    }
    return EncodedMessage{std::vector<uint8_t>(serialized.begin(), serialized.end()),
                          EncodingFormat::PROTOBUF};
}

EncodeResult<Pdu> ProtobufE3Encoder::decode(const EncodedMessage& encoded) {
    return decode(encoded.buffer.data(), encoded.buffer.size());
}

EncodeResult<Pdu> ProtobufE3Encoder::decode(const uint8_t* data, size_t size) {
    if (data == nullptr && size != 0) {
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
    pb::E3Pdu proto;
    if (!proto.ParseFromArray(data, static_cast<int>(size))) {
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
    if (proto.msg_case() == pb::E3Pdu::MSG_NOT_SET) {
        return tl::unexpected(ErrorCode::DECODE_FAILED);
    }
    return proto_to_pdu(proto);
}

} // namespace libe3
