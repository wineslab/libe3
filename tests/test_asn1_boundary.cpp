/**
 * @file test_asn1_boundary.cpp
 * @brief Pins the APER bytes libe3 writes at the edges of every constraint.
 *
 * test_asn1_golden.cpp pins one typical PDU per message type. The edges are where
 * an asn1c change would alter bytes without touching those: ids at 0, 255, 256 and
 * 65535, a sequence id of 4294967295, timestamps up to 2^64-1, payload sizes around
 * 127, 128 and 16384, lists of 0 and 256 entries, optional fields present and
 * absent, and values just outside a range, which must stay refused. Each case is
 * pinned by its length, a checksum of every byte and its first 32 bytes, so the
 * 32 KiB payloads do not fill the file.
 *
 * A failure means the wire format changed. If that was intended, add a row to the
 * change ledger in docs/interop.md in the same PR; if not, the change is a bug.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace libe3;

namespace {

constexpr uint32_t kMessageId = 42;
constexpr uint64_t kTimestamp = 1756800000123456789ull;

struct Pin {
    const char* name;
    bool encodes;
    size_t size;
    uint32_t checksum;
    const char* head;
};

std::string to_hex(const uint8_t* data, size_t size) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < size; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0xF];
    }
    return out;
}

uint32_t fnv1a(const std::vector<uint8_t>& bytes) {
    uint32_t h = 2166136261u;
    for (uint8_t b : bytes) {
        h = (h ^ b) * 16777619u;
    }
    return h;
}

std::string head_of(const std::vector<uint8_t>& bytes) {
    return to_hex(bytes.data(), std::min<size_t>(bytes.size(), 32));
}

Pdu make(PduType type, PduChoice choice, uint32_t id = kMessageId, uint64_t ts = kTimestamp) {
    Pdu pdu(type);
    pdu.message_id = id;
    pdu.timestamp = ts;
    pdu.choice = std::move(choice);
    return pdu;
}

std::vector<uint8_t> payload(size_t n) {
    std::vector<uint8_t> v(n);
    uint32_t s = 0x9E3779B9u;
    for (auto& b : v) {
        s = s * 1103515245u + 12345u;
        b = static_cast<uint8_t>(s >> 16);
    }
    return v;
}

std::vector<uint32_t> ids(size_t n, uint32_t first) {
    static const uint32_t pick[] = {1, 255, 256, 65535};
    std::vector<uint32_t> v;
    for (size_t i = 0; i < n; ++i) {
        v.push_back(i == 0 ? first : pick[i % 4]);
    }
    return v;
}

using Case = std::pair<std::string, Pdu>;

std::vector<Case> cases() {
    std::vector<Case> v;
    auto add = [&v](const std::string& name, Pdu pdu) { v.emplace_back(name, std::move(pdu)); };
    auto num = [](uint64_t n) { return std::to_string(n); };

    for (uint32_t id : {0u, 1u, 255u, 256u, 65535u, 65536u, 4294967295u}) {
        add("message-id/" + num(id), make(PduType::RELEASE_MESSAGE, ReleaseMessage{7}, id));
        MessageAck ack;
        ack.request_id = id;
        ack.response_code = ResponseCode::POSITIVE;
        add("ack-request-id/" + num(id), make(PduType::MESSAGE_ACK, ack));
        DAppReport report;
        report.dapp_identifier = 1;
        report.ran_function_identifier = 1;
        report.report_data = {1};
        report.sequence_id = id;
        add("report-sequence-id/" + num(id), make(PduType::DAPP_REPORT, report));
        XAppControlAction xapp;
        xapp.dapp_identifier = 1;
        xapp.ran_function_identifier = 1;
        xapp.xapp_control_data = {1};
        xapp.sequence_id = id;
        add("xapp-sequence-id/" + num(id), make(PduType::XAPP_CONTROL_ACTION, xapp));
        DAppControlAction control;
        control.dapp_identifier = 1;
        control.ran_function_identifier = 1;
        control.control_identifier = 1;
        control.action_data = {1};
        control.sequence_id = id;
        add("control-sequence-id/" + num(id), make(PduType::DAPP_CONTROL_ACTION, control));
    }

    for (uint64_t ts : {0ull, 1ull, 127ull, 128ull, 255ull, 256ull, 4294967296ull, (1ull << 63) - 1,
                        1ull << 63, ~0ull}) {
        add("timestamp/" + num(ts), make(PduType::RELEASE_MESSAGE, ReleaseMessage{7}, kMessageId, ts));
    }

    for (uint32_t id : {0u, 1u, 255u, 256u, 65535u, 65536u}) {
        SubscriptionRequest request;
        request.dapp_identifier = id;
        request.ran_function_identifier = id;
        request.telemetry_identifier_list = {id};
        request.control_identifier_list = {id};
        add("subscription-request-ids/" + num(id), make(PduType::SUBSCRIPTION_REQUEST, request));
        SubscriptionDelete del;
        del.dapp_identifier = id;
        del.subscription_id = id;
        add("subscription-delete-ids/" + num(id), make(PduType::SUBSCRIPTION_DELETE, del));
        IndicationMessage indication;
        indication.dapp_identifier = id;
        indication.ran_function_identifier = id;
        indication.protocol_data = {9};
        add("indication-ids/" + num(id), make(PduType::INDICATION_MESSAGE, indication));
    }

    for (size_t n : {size_t(0), size_t(1), size_t(256), size_t(257)}) {
        SubscriptionRequest request;
        request.dapp_identifier = 7;
        request.ran_function_identifier = 1;
        request.telemetry_identifier_list = ids(n, 65535);
        request.control_identifier_list = ids(n, 256);
        add("subscription-request-list/" + num(n), make(PduType::SUBSCRIPTION_REQUEST, request));
        SubscriptionResponse response;
        response.request_id = 1;
        response.dapp_identifier = 7;
        response.response_code = ResponseCode::POSITIVE;
        response.subscription_id = 65535;
        response.telemetry_identifier_list = ids(n, 1);
        response.control_identifier_list = ids(n, 65535);
        response.ran_function_identifier = 65535;
        response.periodicity = 60000000;
        add("subscription-response-list/" + num(n), make(PduType::SUBSCRIPTION_RESPONSE, response));
    }
    for (int telemetry = 0; telemetry < 2; ++telemetry) {
        for (int control = 0; control < 2; ++control) {
            SubscriptionResponse response;
            response.request_id = 1;
            response.dapp_identifier = 7;
            response.response_code = ResponseCode::POSITIVE;
            if (telemetry) response.telemetry_identifier_list = std::vector<uint32_t>{};
            if (control) response.control_identifier_list = std::vector<uint32_t>{3};
            add("subscription-response-optional-lists/t" + num(telemetry) + "c" + num(control),
                make(PduType::SUBSCRIPTION_RESPONSE, response));
        }
    }
    for (int64_t time : {-1, 0, 86400, 86401}) {
        for (int64_t period : {-1, 0, 60000000, 60000001}) {
            SubscriptionRequest request;
            request.dapp_identifier = 7;
            request.ran_function_identifier = 1;
            if (time >= 0) request.subscription_time = static_cast<uint32_t>(time);
            if (period >= 0) request.periodicity = static_cast<uint32_t>(period);
            add("subscription-request-optional/time" + std::to_string(time) + "-period" + std::to_string(period),
                make(PduType::SUBSCRIPTION_REQUEST, request));
        }
    }

    for (size_t n : {size_t(0), size_t(1), size_t(127), size_t(128), size_t(255), size_t(256), size_t(16383),
                     size_t(16384), size_t(32767), size_t(32768), size_t(32769)}) {
        IndicationMessage indication;
        indication.dapp_identifier = 1;
        indication.ran_function_identifier = 1;
        indication.protocol_data = payload(n);
        add("indication-size/" + num(n), make(PduType::INDICATION_MESSAGE, indication));
        DAppReport report;
        report.dapp_identifier = 1;
        report.ran_function_identifier = 1;
        report.report_data = payload(n);
        report.sequence_id = 5;
        add("report-size/" + num(n), make(PduType::DAPP_REPORT, report));
        DAppControlAction control;
        control.dapp_identifier = 1;
        control.ran_function_identifier = 1;
        control.control_identifier = 1;
        control.action_data = payload(n);
        add("control-size/" + num(n), make(PduType::DAPP_CONTROL_ACTION, control));
        XAppControlAction xapp;
        xapp.dapp_identifier = 1;
        xapp.ran_function_identifier = 1;
        xapp.xapp_control_data = payload(n);
        xapp.sequence_id = 5;
        add("xapp-size/" + num(n), make(PduType::XAPP_CONTROL_ACTION, xapp));
    }

    for (size_t n : {size_t(0), size_t(1), size_t(32), size_t(33), size_t(64), size_t(65)}) {
        SetupRequest request;
        request.e3ap_protocol_version = std::string(std::min<size_t>(n, 40), 'v');
        request.dapp_name = std::string(n, 'n');
        request.dapp_version = std::string(std::min<size_t>(n, 40), '1');
        request.vendor = std::string(n, 'w');
        add("setup-request-length/" + num(n), make(PduType::SETUP_REQUEST, request));
    }

    {
        SetupResponse response;
        response.request_id = 1;
        response.response_code = ResponseCode::POSITIVE;
        response.dapp_identifier = 65535;
        response.ran_identifier = "r";
        add("setup-response/no-functions", make(PduType::SETUP_RESPONSE, response));
        response.e3ap_protocol_version = "1.0.0";
        add("setup-response/with-version", make(PduType::SETUP_RESPONSE, response));
    }
    for (size_t functions : {size_t(1), size_t(64), size_t(65)}) {
        for (size_t listed : {size_t(0), size_t(256)}) {
            for (size_t data : {size_t(1), size_t(2000)}) {
                SetupResponse response;
                response.request_id = 1;
                response.response_code = ResponseCode::POSITIVE;
                response.dapp_identifier = 3;
                response.ran_identifier = std::string(64, 'R');
                for (size_t f = 0; f < functions; ++f) {
                    RanFunctionDef def;
                    def.ran_function_identifier = f == 0 ? 1u : (f % 2 ? 65535u : 256u);
                    def.telemetry_identifier_list = ids(listed, 1);
                    def.control_identifier_list = ids(listed, 65535);
                    def.ran_function_data = payload(data);
                    response.ran_function_list.push_back(def);
                }
                add("setup-response-functions/" + num(functions) + "-ids" + num(listed) + "-data" + num(data),
                    make(PduType::SETUP_RESPONSE, response));
            }
        }
    }

    for (int cause : {0, 7, 14, 15}) {
        for (size_t detail : {size_t(0), size_t(512), size_t(513)}) {
            const std::string text(detail, 'd');
            const std::string tag = num(static_cast<uint64_t>(cause)) + "-detail" + num(detail);
            MessageAck ack;
            ack.request_id = 9;
            ack.response_code = ResponseCode::NEGATIVE;
            ack.cause = static_cast<Cause>(cause);
            ack.detail = text;
            add("ack-cause/" + tag, make(PduType::MESSAGE_ACK, ack));
            SetupResponse setup;
            setup.request_id = 9;
            setup.response_code = ResponseCode::NEGATIVE;
            setup.ran_identifier = "x";
            setup.cause = static_cast<Cause>(cause);
            setup.detail = text;
            add("setup-response-cause/" + tag, make(PduType::SETUP_RESPONSE, setup));
            SubscriptionResponse response;
            response.request_id = 9;
            response.dapp_identifier = 1;
            response.response_code = ResponseCode::NEGATIVE;
            response.cause = static_cast<Cause>(cause);
            response.detail = text;
            add("subscription-response-cause/" + tag, make(PduType::SUBSCRIPTION_RESPONSE, response));
        }
    }
    {
        MessageAck ack;
        ack.request_id = 9;
        ack.response_code = ResponseCode::NEGATIVE;
        ack.cause = Cause::INVALID_REQUEST;
        ack.detail = std::string(510, 'a') + "\xC3\xA9";
        add("ack-detail/utf8-at-limit", make(PduType::MESSAGE_ACK, ack));
        ack.detail = std::string(511, 'a') + "\xC3\xA9";
        add("ack-detail/utf8-over-limit", make(PduType::MESSAGE_ACK, ack));
    }
    return v;
}

const Pin kPins[] = {
    {"message-id/0", false, 0, 0u, ""},
    {"ack-request-id/0", false, 0, 0u, ""},
    {"report-sequence-id/0", false, 0, 0u, ""},
    {"xapp-sequence-id/0", false, 0, 0u, ""},
    {"control-sequence-id/0", true, 21, 0xa58cf95bu, "4029081861684accc3cd1530000000000000000001"},
    {"message-id/1", true, 14, 0xbb106899u, "4000081861684accc3cd15480006"},
    {"ack-request-id/1", true, 14, 0xd98788f8u, "4029081861684accc3cd15500000"},
    {"report-sequence-id/1", true, 21, 0xe14f0513u, "4029081861684accc3cd1538000000000000000001"},
    {"xapp-sequence-id/1", true, 21, 0x60f0554bu, "4029081861684accc3cd1540000000000000000001"},
    {"control-sequence-id/1", true, 23, 0xf38fd2fdu, "4029081861684accc3cd15320000000000000000000001"},
    {"message-id/255", true, 14, 0x1393650bu, "40fe081861684accc3cd15480006"},
    {"ack-request-id/255", true, 14, 0xeee76f36u, "4029081861684accc3cd1550fe00"},
    {"report-sequence-id/255", true, 21, 0x40f686f1u, "4029081861684accc3cd15380000000000fe000001"},
    {"xapp-sequence-id/255", true, 21, 0xd97b50b9u, "4029081861684accc3cd15400000000000fe000001"},
    {"control-sequence-id/255", true, 23, 0xf9a2df07u, "4029081861684accc3cd153200000000000000fe000001"},
    {"message-id/256", true, 14, 0xcd7ed264u, "40ff081861684accc3cd15480006"},
    {"ack-request-id/256", true, 14, 0x14e9e99fu, "4029081861684accc3cd1550ff00"},
    {"report-sequence-id/256", true, 21, 0x8f22c2dcu, "4029081861684accc3cd15380000000000ff000001"},
    {"xapp-sequence-id/256", true, 21, 0x27a78ca4u, "4029081861684accc3cd15400000000000ff000001"},
    {"control-sequence-id/256", true, 23, 0x47cf1af2u, "4029081861684accc3cd153200000000000000ff000001"},
    {"message-id/65535", true, 15, 0x6c2e2c1au, "50fffe081861684accc3cd15480006"},
    {"ack-request-id/65535", true, 15, 0x00a82bf2u, "4029081861684accc3cd1551fffe00"},
    {"report-sequence-id/65535", true, 22, 0x8e4eecf0u, "4029081861684accc3cd15380000000040fffe000001"},
    {"xapp-sequence-id/65535", true, 22, 0xaae88898u, "4029081861684accc3cd15400000000040fffe000001"},
    {"control-sequence-id/65535", true, 24, 0x973861ceu, "4029081861684accc3cd153200000000000040fffe000001"},
    {"message-id/65536", true, 15, 0xc216e589u, "50ffff081861684accc3cd15480006"},
    {"ack-request-id/65536", true, 15, 0x26aaa65bu, "4029081861684accc3cd1551ffff00"},
    {"report-sequence-id/65536", true, 22, 0x4022b105u, "4029081861684accc3cd15380000000040ffff000001"},
    {"xapp-sequence-id/65536", true, 22, 0x5cbc4cadu, "4029081861684accc3cd15400000000040ffff000001"},
    {"control-sequence-id/65536", true, 24, 0x490c25e3u, "4029081861684accc3cd153200000000000040ffff000001"},
    {"message-id/4294967295", true, 17, 0x76c0c93cu, "70fffffffe081861684accc3cd15480006"},
    {"ack-request-id/4294967295", true, 17, 0x9fc919beu, "4029081861684accc3cd1553fffffffe00"},
    {"report-sequence-id/4294967295", true, 24, 0xcd903572u, "4029081861684accc3cd153800000000c0fffffffe000001"},
    {"xapp-sequence-id/4294967295", true, 24, 0x19f8b55au, "4029081861684accc3cd154000000000c0fffffffe000001"},
    {"control-sequence-id/4294967295", true, 26, 0xbfae0980u, "4029081861684accc3cd1532000000000000c0fffffffe000001"},
    {"timestamp/0", true, 5, 0xf8ecead4u, "0029480006"},
    {"timestamp/1", true, 7, 0x282529d2u, "40290101480006"},
    {"timestamp/127", true, 7, 0x5edc42a4u, "4029017f480006"},
    {"timestamp/128", true, 8, 0x2bf25c4au, "4029020080480006"},
    {"timestamp/255", true, 8, 0x58b48171u, "40290200ff480006"},
    {"timestamp/256", true, 8, 0x0da79d5du, "4029020100480006"},
    {"timestamp/4294967296", true, 11, 0x82318086u, "4029050100000000480006"},
    {"timestamp/9223372036854775807", true, 14, 0xc7ea9a78u, "4029087fffffffffffffff480006"},
    {"timestamp/9223372036854775808", true, 14, 0x0b9d3120u, "4029088000000000000000480006"},
    {"timestamp/18446744073709551615", true, 7, 0x48bcba24u, "402901ff480006"},
    {"subscription-request-ids/0", false, 0, 0u, ""},
    {"subscription-delete-ids/0", false, 0, 0u, ""},
    {"indication-ids/0", false, 0, 0u, ""},
    {"subscription-request-ids/1", true, 24, 0xac3fbda8u, "4029081861684accc3cd1510000000000001000000010000"},
    {"subscription-delete-ids/1", true, 16, 0xd2c12ee0u, "4029081861684accc3cd151800000000"},
    {"indication-ids/1", true, 19, 0x628977bbu, "4029081861684accc3cd152800000000000009"},
    {"subscription-request-ids/255", true, 24, 0x0d1b0888u, "4029081861684accc3cd151000fe00fe000100fe000100fe"},
    {"subscription-delete-ids/255", true, 16, 0x7d46c650u, "4029081861684accc3cd151800fe00fe"},
    {"indication-ids/255", true, 19, 0xca5f3b8bu, "4029081861684accc3cd152800fe00fe000009"},
    {"subscription-request-ids/256", true, 24, 0x6e2e9e90u, "4029081861684accc3cd151000ff00ff000100ff000100ff"},
    {"subscription-delete-ids/256", true, 16, 0xeb2c3098u, "4029081861684accc3cd151800ff00ff"},
    {"indication-ids/256", true, 19, 0x9b13bf73u, "4029081861684accc3cd152800ff00ff000009"},
    {"subscription-request-ids/65535", true, 24, 0x442374c8u, "4029081861684accc3cd1510fffefffe0001fffe0001fffe"},
    {"subscription-delete-ids/65535", true, 16, 0xb3abdd34u, "4029081861684accc3cd1518fffefffe"},
    {"indication-ids/65535", true, 19, 0x93ce6257u, "4029081861684accc3cd1528fffefffe000009"},
    {"subscription-request-ids/65536", false, 0, 0u, ""},
    {"subscription-delete-ids/65536", false, 0, 0u, ""},
    {"indication-ids/65536", false, 0, 0u, ""},
    {"subscription-request-list/0", true, 20, 0xdc23725au, "4029081861684accc3cd15100006000000000000"},
    {"subscription-response-list/0", true, 34, 0xb214bc4du, "4029081861684accc3cd15278000000600fffe00000000066002fffe05c00393"},
    {"subscription-request-list/1", true, 24, 0x9f411ebau, "4029081861684accc3cd1510000600000001fffe000100ff"},
    {"subscription-response-list/1", true, 38, 0x2c96fc36u, "4029081861684accc3cd15278000000600fffe000100000001fffe066002fffe"},
    {"subscription-request-list/256", true, 1044, 0xbe3e774eu, "4029081861684accc3cd1510000600000100fffe00fe00fffffe000000fe00ff"},
    {"subscription-response-list/256", true, 1058, 0xbe76ea06u, "4029081861684accc3cd15278000000600fffe0100000000fe00fffffe000000"},
    {"subscription-request-list/257", false, 0, 0u, ""},
    {"subscription-response-list/257", false, 0, 0u, ""},
    {"subscription-response-optional-lists/t0c0", true, 17, 0xa77be94eu, "4029081861684accc3cd15200000000600"},
    {"subscription-response-optional-lists/t0c1", true, 21, 0xf853d9d3u, "4029081861684accc3cd1520800000060000010002"},
    {"subscription-response-optional-lists/t1c0", true, 19, 0x82ff3c1fu, "4029081861684accc3cd152100000006000000"},
    {"subscription-response-optional-lists/t1c1", true, 23, 0xf2b3196au, "4029081861684accc3cd15218000000600000000010002"},
    {"subscription-request-optional/time-1-period-1", true, 20, 0xdc23725au, "4029081861684accc3cd15100006000000000000"},
    {"subscription-request-optional/time-1-period0", true, 22, 0x566dbc95u, "4029081861684accc3cd151100060000000000000000"},
    {"subscription-request-optional/time-1-period60000000", true, 25, 0x1b05f22au, "4029081861684accc3cd15110006000000000000c003938700"},
    {"subscription-request-optional/time-1-period60000001", false, 0, 0u, ""},
    {"subscription-request-optional/time0-period-1", true, 22, 0xb7fbaa74u, "4029081861684accc3cd151200060000000000000000"},
    {"subscription-request-optional/time0-period0", true, 24, 0x2970ceb7u, "4029081861684accc3cd1513000600000000000000000000"},
    {"subscription-request-optional/time0-period60000000", true, 27, 0xa44602bcu, "4029081861684accc3cd151300060000000000000000c003938700"},
    {"subscription-request-optional/time0-period60000001", false, 0, 0u, ""},
    {"subscription-request-optional/time86400-period-1", true, 24, 0x82256a46u, "4029081861684accc3cd1512000600000000000080015180"},
    {"subscription-request-optional/time86400-period0", true, 26, 0x3ee44dadu, "4029081861684accc3cd15130006000000000000800151800000"},
    {"subscription-request-optional/time86400-period60000000", true, 29, 0x0e6f0fb2u, "4029081861684accc3cd1513000600000000000080015180c003938700"},
    {"subscription-request-optional/time86400-period60000001", false, 0, 0u, ""},
    {"subscription-request-optional/time86401-period-1", false, 0, 0u, ""},
    {"subscription-request-optional/time86401-period0", false, 0, 0u, ""},
    {"subscription-request-optional/time86401-period60000000", false, 0, 0u, ""},
    {"subscription-request-optional/time86401-period60000001", false, 0, 0u, ""},
    {"indication-size/0", false, 0, 0u, ""},
    {"report-size/0", false, 0, 0u, ""},
    {"control-size/0", false, 0, 0u, ""},
    {"xapp-size/0", false, 0, 0u, ""},
    {"indication-size/1", true, 19, 0x248aa921u, "4029081861684accc3cd1528000000000000cb"},
    {"report-size/1", true, 21, 0xdbff9325u, "4029081861684accc3cd15380000000000040000cb"},
    {"control-size/1", true, 21, 0x5f8e1e29u, "4029081861684accc3cd15300000000000000000cb"},
    {"xapp-size/1", true, 21, 0x6ba0fc8du, "4029081861684accc3cd15400000000000040000cb"},
    {"indication-size/127", true, 145, 0xfae0746eu, "4029081861684accc3cd152800000000007ecbe8f6cc776e8601f77875f89492"},
    {"report-size/127", true, 147, 0x869df102u, "4029081861684accc3cd1538000000000004007ecbe8f6cc776e8601f77875f8"},
    {"control-size/127", true, 147, 0xa8941d56u, "4029081861684accc3cd1530000000000000007ecbe8f6cc776e8601f77875f8"},
    {"xapp-size/127", true, 147, 0xedb7362au, "4029081861684accc3cd1540000000000004007ecbe8f6cc776e8601f77875f8"},
    {"indication-size/128", true, 146, 0x1c2d0085u, "4029081861684accc3cd152800000000007fcbe8f6cc776e8601f77875f89492"},
    {"report-size/128", true, 148, 0xf26906f1u, "4029081861684accc3cd1538000000000004007fcbe8f6cc776e8601f77875f8"},
    {"control-size/128", true, 148, 0x5cbb0ebdu, "4029081861684accc3cd1530000000000000007fcbe8f6cc776e8601f77875f8"},
    {"xapp-size/128", true, 148, 0x45902989u, "4029081861684accc3cd1540000000000004007fcbe8f6cc776e8601f77875f8"},
    {"indication-size/255", true, 273, 0x063ff1dcu, "4029081861684accc3cd15280000000000fecbe8f6cc776e8601f77875f89492"},
    {"report-size/255", true, 275, 0xe3252050u, "4029081861684accc3cd153800000000000400fecbe8f6cc776e8601f77875f8"},
    {"control-size/255", true, 275, 0x858fccd4u, "4029081861684accc3cd153000000000000000fecbe8f6cc776e8601f77875f8"},
    {"xapp-size/255", true, 275, 0x47abaa68u, "4029081861684accc3cd154000000000000400fecbe8f6cc776e8601f77875f8"},
    {"indication-size/256", true, 274, 0x5c92f3c3u, "4029081861684accc3cd15280000000000ffcbe8f6cc776e8601f77875f89492"},
    {"report-size/256", true, 276, 0xe3349c4fu, "4029081861684accc3cd153800000000000400ffcbe8f6cc776e8601f77875f8"},
    {"control-size/256", true, 276, 0xaf8fedfbu, "4029081861684accc3cd153000000000000000ffcbe8f6cc776e8601f77875f8"},
    {"xapp-size/256", true, 276, 0xd639d8c7u, "4029081861684accc3cd154000000000000400ffcbe8f6cc776e8601f77875f8"},
    {"indication-size/16383", true, 16401, 0xe28a29a5u, "4029081861684accc3cd1528000000003ffecbe8f6cc776e8601f77875f89492"},
    {"report-size/16383", true, 16403, 0x32e646b1u, "4029081861684accc3cd15380000000000043ffecbe8f6cc776e8601f77875f8"},
    {"control-size/16383", true, 16403, 0x6dc314fdu, "4029081861684accc3cd15300000000000003ffecbe8f6cc776e8601f77875f8"},
    {"xapp-size/16383", true, 16403, 0x7806f009u, "4029081861684accc3cd15400000000000043ffecbe8f6cc776e8601f77875f8"},
    {"indication-size/16384", true, 16402, 0x304f0b3bu, "4029081861684accc3cd1528000000003fffcbe8f6cc776e8601f77875f89492"},
    {"report-size/16384", true, 16404, 0xef2a03c7u, "4029081861684accc3cd15380000000000043fffcbe8f6cc776e8601f77875f8"},
    {"control-size/16384", true, 16404, 0xb744e003u, "4029081861684accc3cd15300000000000003fffcbe8f6cc776e8601f77875f8"},
    {"xapp-size/16384", true, 16404, 0x8306baafu, "4029081861684accc3cd15400000000000043fffcbe8f6cc776e8601f77875f8"},
    {"indication-size/32767", true, 32785, 0x881580e6u, "4029081861684accc3cd1528000000007ffecbe8f6cc776e8601f77875f89492"},
    {"report-size/32767", true, 32787, 0xe5434b0au, "4029081861684accc3cd15380000000000047ffecbe8f6cc776e8601f77875f8"},
    {"control-size/32767", true, 32787, 0x6bbb1beeu, "4029081861684accc3cd15300000000000007ffecbe8f6cc776e8601f77875f8"},
    {"xapp-size/32767", true, 32787, 0xef2e8c52u, "4029081861684accc3cd15400000000000047ffecbe8f6cc776e8601f77875f8"},
    {"indication-size/32768", true, 32786, 0xb98e166fu, "4029081861684accc3cd1528000000007fffcbe8f6cc776e8601f77875f89492"},
    {"report-size/32768", true, 32788, 0xd27ce88bu, "4029081861684accc3cd15380000000000047fffcbe8f6cc776e8601f77875f8"},
    {"control-size/32768", true, 32788, 0xa73b40a7u, "4029081861684accc3cd15300000000000007fffcbe8f6cc776e8601f77875f8"},
    {"xapp-size/32768", true, 32788, 0xdaf16e03u, "4029081861684accc3cd15400000000000047fffcbe8f6cc776e8601f77875f8"},
    {"indication-size/32769", false, 0, 0u, ""},
    {"report-size/32769", false, 0, 0u, ""},
    {"control-size/32769", false, 0, 0u, ""},
    {"xapp-size/32769", false, 0, 0u, ""},
    {"setup-request-length/0", false, 0, 0u, ""},
    {"setup-request-length/1", true, 20, 0x256bb61au, "4029081861684accc3cd15000076006e00310077"},
    {"setup-request-length/32", true, 144, 0x458b3e19u, "4029081861684accc3cd1503e076767676767676767676767676767676767676"},
    {"setup-request-length/33", false, 0, 0u, ""},
    {"setup-request-length/64", false, 0, 0u, ""},
    {"setup-request-length/65", false, 0, 0u, ""},
    {"setup-response/no-functions", true, 18, 0x4ef597b4u, "4029081861684accc3cd150900000072fffe"},
    {"setup-response/with-version", true, 24, 0xc1fe8431u, "4029081861684accc3cd150b0000007220312e302e30fffe"},
    {"setup-response-functions/1-ids0-data1", true, 90, 0x1cdbfb85u, "4029081861684accc3cd150980007e5252525252525252525252525252525252"},
    {"setup-response-functions/1-ids0-data2000", true, 2090, 0x460e531du, "4029081861684accc3cd150980007e5252525252525252525252525252525252"},
    {"setup-response-functions/1-ids256-data1", true, 1114, 0x58fcb6c0u, "4029081861684accc3cd150980007e5252525252525252525252525252525252"},
    {"setup-response-functions/1-ids256-data2000", true, 3114, 0xd81761b4u, "4029081861684accc3cd150980007e5252525252525252525252525252525252"},
    {"setup-response-functions/64-ids0-data1", true, 657, 0x803180aau, "4029081861684accc3cd150980007e5252525252525252525252525252525252"},
    {"setup-response-functions/64-ids0-data2000", false, 0, 0u, ""},
    {"setup-response-functions/64-ids256-data1", false, 0, 0u, ""},
    {"setup-response-functions/64-ids256-data2000", false, 0, 0u, ""},
    {"setup-response-functions/65-ids0-data1", false, 0, 0u, ""},
    {"setup-response-functions/65-ids0-data2000", false, 0, 0u, ""},
    {"setup-response-functions/65-ids256-data1", false, 0, 0u, ""},
    {"setup-response-functions/65-ids256-data2000", false, 0, 0u, ""},
    {"ack-cause/0-detail0", true, 17, 0x787f994cu, "4029081861684accc3cd15540881800100"},
    {"setup-response-cause/0-detail0", true, 20, 0xfeefb516u, "4029081861684accc3cd150c0008807803000100"},
    {"subscription-response-cause/0-detail0", true, 20, 0xc40ac206u, "4029081861684accc3cd15240008000083800100"},
    {"ack-cause/0-detail512", true, 533, 0x4ce7672au, "4029081861684accc3cd15540881c00100820202006464646464646464646464"},
    {"setup-response-cause/0-detail512", true, 536, 0xba409118u, "4029081861684accc3cd150c0008807803800100820202006464646464646464"},
    {"subscription-response-cause/0-detail512", true, 536, 0x74dc51c8u, "4029081861684accc3cd15240008000083c00100820202006464646464646464"},
    {"ack-cause/0-detail513", true, 533, 0x4ce7672au, "4029081861684accc3cd15540881c00100820202006464646464646464646464"},
    {"setup-response-cause/0-detail513", true, 536, 0xba409118u, "4029081861684accc3cd150c0008807803800100820202006464646464646464"},
    {"subscription-response-cause/0-detail513", true, 536, 0x74dc51c8u, "4029081861684accc3cd15240008000083c00100820202006464646464646464"},
    {"ack-cause/7-detail0", true, 17, 0xb07ff174u, "4029081861684accc3cd15540881800138"},
    {"setup-response-cause/7-detail0", true, 20, 0x16efdadeu, "4029081861684accc3cd150c0008807803000138"},
    {"subscription-response-cause/7-detail0", true, 20, 0xbc0ab56eu, "4029081861684accc3cd15240008000083800138"},
    {"ack-cause/7-detail512", true, 533, 0xeb4f99d2u, "4029081861684accc3cd15540881c00138820202006464646464646464646464"},
    {"setup-response-cause/7-detail512", true, 536, 0xc2249c60u, "4029081861684accc3cd150c0008807803800138820202006464646464646464"},
    {"subscription-response-cause/7-detail512", true, 536, 0xdd5042b0u, "4029081861684accc3cd15240008000083c00138820202006464646464646464"},
    {"ack-cause/7-detail513", true, 533, 0xeb4f99d2u, "4029081861684accc3cd15540881c00138820202006464646464646464646464"},
    {"setup-response-cause/7-detail513", true, 536, 0xc2249c60u, "4029081861684accc3cd150c0008807803800138820202006464646464646464"},
    {"subscription-response-cause/7-detail513", true, 536, 0xdd5042b0u, "4029081861684accc3cd15240008000083c00138820202006464646464646464"},
    {"ack-cause/14-detail0", true, 17, 0xe880499cu, "4029081861684accc3cd15540881800170"},
    {"setup-response-cause/14-detail0", true, 20, 0xceef6986u, "4029081861684accc3cd150c0008807803000170"},
    {"subscription-response-cause/14-detail0", true, 20, 0xf40b0d96u, "4029081861684accc3cd15240008000083800170"},
    {"ack-cause/14-detail512", true, 533, 0x72393bfau, "4029081861684accc3cd15540881c00170820202006464646464646464646464"},
    {"setup-response-cause/14-detail512", true, 536, 0x7d3fad88u, "4029081861684accc3cd150c0008807803800170820202006464646464646464"},
    {"subscription-response-cause/14-detail512", true, 536, 0xc82464d8u, "4029081861684accc3cd15240008000083c00170820202006464646464646464"},
    {"ack-cause/14-detail513", true, 533, 0x72393bfau, "4029081861684accc3cd15540881c00170820202006464646464646464646464"},
    {"setup-response-cause/14-detail513", true, 536, 0x7d3fad88u, "4029081861684accc3cd150c0008807803800170820202006464646464646464"},
    {"subscription-response-cause/14-detail513", true, 536, 0xc82464d8u, "4029081861684accc3cd15240008000083c00170820202006464646464646464"},
    {"ack-cause/15-detail0", false, 0, 0u, ""},
    {"setup-response-cause/15-detail0", false, 0, 0u, ""},
    {"subscription-response-cause/15-detail0", false, 0, 0u, ""},
    {"ack-cause/15-detail512", false, 0, 0u, ""},
    {"setup-response-cause/15-detail512", false, 0, 0u, ""},
    {"subscription-response-cause/15-detail512", false, 0, 0u, ""},
    {"ack-cause/15-detail513", false, 0, 0u, ""},
    {"setup-response-cause/15-detail513", false, 0, 0u, ""},
    {"subscription-response-cause/15-detail513", false, 0, 0u, ""},
    {"ack-detail/utf8-at-limit", true, 533, 0x2205c9acu, "4029081861684accc3cd15540881c00138820202006161616161616161616161"},
    {"ack-detail/utf8-over-limit", true, 532, 0x405a2236u, "4029081861684accc3cd15540881c00138820101ff6161616161616161616161"},
};

}  // namespace

TEST(Asn1Boundary_encodeProducesThePinnedBytes) {
    auto encoder = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    const auto all = cases();
    ASSERT_EQ(all.size(), sizeof(kPins) / sizeof(kPins[0]));

    std::string drift;
    for (size_t i = 0; i < all.size(); ++i) {
        const Pin& pin = kPins[i];
        ASSERT_STREQ(all[i].first, std::string(pin.name));
        auto encoded = encoder->encode(all[i].second);
        if (!encoded.has_value()) {
            if (pin.encodes) drift += std::string("\n  ") + pin.name + ": refused, pinned as encoding";
            continue;
        }
        if (!pin.encodes) {
            drift += std::string("\n  ") + pin.name + ": encodes, pinned as refused";
            continue;
        }
        if (encoded->buffer.size() != pin.size || fnv1a(encoded->buffer) != pin.checksum ||
            head_of(encoded->buffer) != pin.head) {
            drift += std::string("\n  ") + pin.name + ": " + std::to_string(encoded->buffer.size()) + " bytes, " +
                     head_of(encoded->buffer);
        }
    }
    ASSERT_STREQ(drift, "");
}

TEST(Asn1Boundary_pinnedPdusDecodeAndReencodeIdentically) {
    auto encoder = create_encoder(EncodingFormat::ASN1);
    ASSERT_TRUE(encoder != nullptr);

    std::string drift;
    for (const auto& c : cases()) {
        auto encoded = encoder->encode(c.second);
        if (!encoded.has_value()) continue;
        auto decoded = encoder->decode(encoded->buffer.data(), encoded->buffer.size());
        if (!decoded.has_value() || decoded->type != c.second.type || decoded->message_id != c.second.message_id ||
            decoded->timestamp != c.second.timestamp) {
            drift += "\n  " + c.first + ": does not decode back";
            continue;
        }
        auto again = encoder->encode(*decoded);
        if (!again.has_value() || again->buffer != encoded->buffer) drift += "\n  " + c.first + ": re-encodes differently";
    }
    ASSERT_STREQ(drift, "");
}

int main() {
    return RUN_ALL_TESTS();
}
