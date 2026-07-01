/**
 * @file bench_encoding_size.cpp
 * @brief Measure the wire size (bytes) of each E3AP PDU type under every
 *        enabled encoding (ASN.1 APER, JSON; protobuf when available).
 *
 * Outputs a CSV to stdout with columns:
 *   encoding, message_type, encoded_bytes
 *
 * Usage:
 *   ./bench_encoding_size
 *   ./bench_encoding_size | python scripts/collect_baseline.py \
 *       --type encoding --output data/baseline/encoding/encoding.csv
 *
 * Representative values are used for each field; opaque payload fields
 * (protocol_data, action_data, etc.) use a 64-byte pattern so that the
 * numbers reflect realistic in-field usage without being dominated by
 * the payload itself.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/e3_encoder.hpp>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace libe3;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// 64-byte representative payload for opaque binary fields (ASN.1 path).
std::vector<uint8_t> small_payload() {
    std::vector<uint8_t> v(64);
    for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>(i);
    return v;
}

// JSON-formatted payload for IndicationMessage.protocol_data when encoding==JSON.
// The JSON encoder embeds protocol_data as a nested JSON object (not hex).
// This represents a realistic Simple SM indication payload.
std::vector<uint8_t> json_sm_payload() {
    const char* s = R"({"data1":42,"timestamp":1234567890})";
    return std::vector<uint8_t>(s, s + std::strlen(s));
}

// Encode one PDU with the given encoder and print the CSV row.
void measure(E3Encoder& enc, const char* encoding_name,
             const char* msg_type, const Pdu& pdu) {
    auto result = enc.encode(pdu);
    if (!result.has_value()) {
        std::fprintf(stderr, "WARN: encode failed for %s/%s\n",
                     encoding_name, msg_type);
        return;
    }
    std::printf("%s,%s,%zu\n",
                encoding_name, msg_type, result->buffer.size());
}

// Run all 11 PDU types through one encoder instance.
// json_indication: when true, use a JSON-formatted indication payload instead of
// raw bytes (the JSON encoder embeds protocol_data as a nested JSON object).
void run_encoder(E3Encoder& enc, const char* encoding_name, bool json_indication = false) {
    auto payload = small_payload();
    auto ind_payload = json_indication ? json_sm_payload() : payload;

    // 1. SetupRequest
    {
        Pdu pdu(PduType::SETUP_REQUEST);
        SetupRequest req;
        req.e3ap_protocol_version = "1.0.0";
        req.dapp_name             = "BenchDApp";
        req.dapp_version          = "1.0.0";
        req.vendor                = "WinesLab";
        pdu.choice = req;
        pdu.message_id = 1;
        measure(enc, encoding_name, "SetupRequest", pdu);
    }

    // 2. SetupResponse
    {
        Pdu pdu(PduType::SETUP_RESPONSE);
        SetupResponse resp;
        resp.request_id            = 1;
        resp.response_code         = ResponseCode::POSITIVE;
        resp.ran_identifier        = "bench-ran-001";
        resp.e3ap_protocol_version = "1.0.0";
        resp.dapp_identifier       = 1;
        RanFunctionDef rfdef;
        rfdef.ran_function_identifier = 1;
        rfdef.telemetry_identifier_list = {1};
        rfdef.control_identifier_list   = {1};
        rfdef.ran_function_data = payload;
        resp.ran_function_list.push_back(rfdef);
        pdu.choice = resp;
        pdu.message_id = 2;
        measure(enc, encoding_name, "SetupResponse", pdu);
    }

    // 3. SubscriptionRequest
    {
        Pdu pdu(PduType::SUBSCRIPTION_REQUEST);
        SubscriptionRequest req;
        req.dapp_identifier           = 1;
        req.ran_function_identifier   = 1;
        req.telemetry_identifier_list = {1};
        req.control_identifier_list   = {1};
        req.subscription_time         = 0;
        req.periodicity               = 1000;
        pdu.choice = req;
        pdu.message_id = 3;
        measure(enc, encoding_name, "SubscriptionRequest", pdu);
    }

    // 4. SubscriptionDelete
    {
        Pdu pdu(PduType::SUBSCRIPTION_DELETE);
        SubscriptionDelete del;
        del.dapp_identifier  = 1;
        del.subscription_id  = 1;
        pdu.choice = del;
        pdu.message_id = 4;
        measure(enc, encoding_name, "SubscriptionDelete", pdu);
    }

    // 5. SubscriptionResponse
    {
        Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
        SubscriptionResponse resp;
        resp.request_id       = 3;
        resp.dapp_identifier  = 1;
        resp.response_code    = ResponseCode::POSITIVE;
        resp.subscription_id  = 1;
        pdu.choice = resp;
        pdu.message_id = 5;
        measure(enc, encoding_name, "SubscriptionResponse", pdu);
    }

    // 6. IndicationMessage
    {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        IndicationMessage msg;
        msg.dapp_identifier        = 1;
        msg.ran_function_identifier = 1;
        msg.protocol_data          = ind_payload;
        pdu.choice = msg;
        pdu.message_id = 6;
        measure(enc, encoding_name, "IndicationMessage", pdu);
    }

    // 7. DAppControlAction
    {
        Pdu pdu(PduType::DAPP_CONTROL_ACTION);
        DAppControlAction action;
        action.dapp_identifier        = 1;
        action.ran_function_identifier = 1;
        action.control_identifier     = 1;
        action.action_data            = payload;
        pdu.choice = action;
        pdu.message_id = 7;
        measure(enc, encoding_name, "DAppControlAction", pdu);
    }

    // 8. DAppReport
    {
        Pdu pdu(PduType::DAPP_REPORT);
        DAppReport rep;
        rep.dapp_identifier        = 1;
        rep.ran_function_identifier = 1;
        rep.report_data            = payload;
        pdu.choice = rep;
        pdu.message_id = 8;
        measure(enc, encoding_name, "DAppReport", pdu);
    }

    // 9. XAppControlAction
    {
        Pdu pdu(PduType::XAPP_CONTROL_ACTION);
        XAppControlAction xaction;
        xaction.dapp_identifier        = 1;
        xaction.ran_function_identifier = 1;
        xaction.xapp_control_data      = payload;
        pdu.choice = xaction;
        pdu.message_id = 9;
        measure(enc, encoding_name, "XAppControlAction", pdu);
    }

    // 10. ReleaseMessage
    {
        Pdu pdu(PduType::RELEASE_MESSAGE);
        ReleaseMessage rel;
        rel.dapp_identifier = 1;
        pdu.choice = rel;
        pdu.message_id = 10;
        measure(enc, encoding_name, "ReleaseMessage", pdu);
    }

    // 11. MessageAck
    {
        Pdu pdu(PduType::MESSAGE_ACK);
        MessageAck ack;
        ack.request_id    = 1;
        ack.response_code = ResponseCode::POSITIVE;
        pdu.choice = ack;
        pdu.message_id = 11;
        measure(enc, encoding_name, "MessageAck", pdu);
    }
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("encoding,message_type,encoded_bytes\n");

#ifdef LIBE3_ENABLE_ASN1
    {
        auto enc = create_encoder(EncodingFormat::ASN1);
        if (enc) run_encoder(*enc, "asn1");
    }
#endif

#ifdef LIBE3_ENABLE_JSON
    {
        auto enc = create_encoder(EncodingFormat::JSON);
        if (enc) run_encoder(*enc, "json", /*json_indication=*/true);
    }
#endif

    return 0;
}
