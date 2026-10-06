/**
 * @file test_interop_aerial.cpp
 * @brief Decodes the example messages NVIDIA publishes for its E3 agent.
 *
 * tests/fixtures/aerial/e3_message_examples.json is Aerial's own file
 * (aerial-sample-apps 99b10eb, dapps v1.1.0). Every message in it must decode
 * to the fields Aerial describes, and re-encoding must agree with Aerial on
 * every key libe3 also writes. The places where libe3 and Aerial differ are
 * asserted one by one, so a change on either side shows up as a failure here
 * instead of a surprise on a live link.
 *
 * Aerial publishes no dAppReport or xAppControlAction example.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace libe3;
using json = nlohmann::json;

namespace {

json load_examples() {
    const std::string path = std::string(LIBE3_FIXTURE_DIR) + "/aerial/e3_message_examples.json";
    std::ifstream in(path);
    ASSERT_TRUE(in.good());
    return json::parse(in);
}

json bytes_to_json(const std::vector<uint8_t>& bytes) {
    return json::parse(std::string(bytes.begin(), bytes.end()));
}

std::vector<uint32_t> ids(const json& array) {
    return array.get<std::vector<uint32_t>>();
}

Pdu decode_message(const json& message) {
    auto encoder = create_encoder(EncodingFormat::JSON);
    ASSERT_TRUE(encoder != nullptr);
    const std::string wire = message.dump();
    auto decoded = encoder->decode(reinterpret_cast<const uint8_t*>(wire.data()), wire.size());
    ASSERT_TRUE(decoded.has_value());
    return *decoded;
}

json reencode(const Pdu& pdu) {
    auto encoder = create_encoder(EncodingFormat::JSON);
    auto encoded = encoder->encode(pdu);
    ASSERT_TRUE(encoded.has_value());
    return json::parse(std::string(encoded->buffer.begin(), encoded->buffer.end()));
}

template <typename T>
const T& as(const Pdu& pdu) {
    const T* payload = std::get_if<T>(&pdu.choice);
    ASSERT_TRUE(payload != nullptr);
    return *payload;
}

using Check = std::function<void(const Pdu&, const json&)>;

// What each example must decode to. The key is the example's name in the file.
const std::map<std::string, std::pair<PduType, Check>>& expectations() {
    static const std::map<std::string, std::pair<PduType, Check>> table = {
        {"setupRequest", {PduType::SETUP_REQUEST, [](const Pdu& p, const json&) {
            const auto& m = as<SetupRequest>(p);
            ASSERT_TRUE(m.e3ap_protocol_version == "1.0.0");
            ASSERT_TRUE(m.dapp_name == "PRB Power");
            ASSERT_TRUE(m.dapp_version == "1.0.0");
            ASSERT_TRUE(m.vendor == "NVIDIA");
        }}},
        {"setupResponse_positive", {PduType::SETUP_RESPONSE, [](const Pdu& p, const json& j) {
            const auto& m = as<SetupResponse>(p);
            ASSERT_EQ(m.request_id, 1u);
            ASSERT_TRUE(m.response_code == ResponseCode::POSITIVE);
            ASSERT_TRUE(m.dapp_identifier.has_value());
            ASSERT_EQ(*m.dapp_identifier, 1u);
            ASSERT_TRUE(m.ran_identifier == "NVIDIA_L1");
            ASSERT_EQ(m.ran_function_list.size(), 1u);
            const auto& fn = m.ran_function_list[0];
            ASSERT_EQ(fn.ran_function_identifier, 2u);
            ASSERT_TRUE(fn.telemetry_identifier_list == ids(j["ranFunctionList"][0]["telemetryIdentifierList"]));
            ASSERT_TRUE(fn.control_identifier_list.empty());
            // Aerial describes its streams with a JSON array, not an object.
            ASSERT_TRUE(bytes_to_json(fn.ran_function_data) == j["ranFunctionList"][0]["ranFunctionData"]);
            ASSERT_TRUE(bytes_to_json(fn.ran_function_data).is_array());
        }}},
        {"setupResponse_negative", {PduType::SETUP_RESPONSE, [](const Pdu& p, const json&) {
            const auto& m = as<SetupResponse>(p);
            ASSERT_EQ(m.request_id, 1u);
            ASSERT_TRUE(m.response_code == ResponseCode::NEGATIVE);
            ASSERT_FALSE(m.dapp_identifier.has_value());
            ASSERT_TRUE(m.ran_function_list.empty());
            // Aerial leaves ranIdentifier out of a negative reply.
            ASSERT_TRUE(m.ran_identifier.empty());
        }}},
        {"subscriptionRequest_pusch", {PduType::SUBSCRIPTION_REQUEST, [](const Pdu& p, const json&) {
            const auto& m = as<SubscriptionRequest>(p);
            ASSERT_EQ(m.dapp_identifier, 1u);
            ASSERT_EQ(m.ran_function_identifier, 2u);
            ASSERT_TRUE((m.telemetry_identifier_list == std::vector<uint32_t>{1, 4, 5, 6}));
            ASSERT_TRUE(m.control_identifier_list.empty());
            ASSERT_TRUE(m.periodicity.has_value());
            ASSERT_EQ(*m.periodicity, 100000u);
            ASSERT_TRUE(m.subscription_time.has_value());
            ASSERT_EQ(*m.subscription_time, 0u);
        }}},
        {"subscriptionRequest_srs", {PduType::SUBSCRIPTION_REQUEST, [](const Pdu& p, const json& j) {
            const auto& m = as<SubscriptionRequest>(p);
            ASSERT_TRUE(m.telemetry_identifier_list == ids(j["telemetryIdentifierList"]));
            ASSERT_EQ(m.telemetry_identifier_list.size(), 16u);
            ASSERT_TRUE(m.periodicity.has_value());
            ASSERT_EQ(*m.periodicity, 0u);
        }}},
        {"subscriptionResponse_positive", {PduType::SUBSCRIPTION_RESPONSE, [](const Pdu& p, const json&) {
            const auto& m = as<SubscriptionResponse>(p);
            ASSERT_EQ(m.request_id, 2u);
            ASSERT_EQ(m.dapp_identifier, 1u);
            ASSERT_TRUE(m.response_code == ResponseCode::POSITIVE);
            ASSERT_TRUE(m.subscription_id.has_value());
            ASSERT_EQ(*m.subscription_id, 1u);
            ASSERT_TRUE(m.ran_function_identifier.has_value());
            ASSERT_EQ(*m.ran_function_identifier, 2u);
            // Aerial names the granted lists ...GrantedList; libe3 reads them as aliases.
            ASSERT_TRUE(m.telemetry_identifier_list.has_value());
            ASSERT_TRUE((*m.telemetry_identifier_list == std::vector<uint32_t>{1, 4, 5, 6}));
            ASSERT_TRUE(m.control_identifier_list.has_value());
            ASSERT_TRUE(m.control_identifier_list->empty());
            ASSERT_TRUE(m.periodicity.has_value());
            ASSERT_EQ(*m.periodicity, 100000u);
        }}},
        {"subscriptionResponse_negative", {PduType::SUBSCRIPTION_RESPONSE, [](const Pdu& p, const json&) {
            const auto& m = as<SubscriptionResponse>(p);
            ASSERT_EQ(m.request_id, 2u);
            ASSERT_TRUE(m.response_code == ResponseCode::NEGATIVE);
            ASSERT_FALSE(m.subscription_id.has_value());
            ASSERT_FALSE(m.telemetry_identifier_list.has_value());
        }}},
        {"indicationMessage_single_ue", {PduType::INDICATION_MESSAGE, nullptr}},
        {"indicationMessage_multi_ue", {PduType::INDICATION_MESSAGE, nullptr}},
        {"indicationMessage_srs_single_ue", {PduType::INDICATION_MESSAGE, nullptr}},
        {"indicationMessage_srs_multi_ue", {PduType::INDICATION_MESSAGE, nullptr}},
        {"subscriptionDelete", {PduType::SUBSCRIPTION_DELETE, [](const Pdu& p, const json&) {
            const auto& m = as<SubscriptionDelete>(p);
            ASSERT_EQ(m.dapp_identifier, 1u);
            ASSERT_EQ(m.subscription_id, 1u);
        }}},
        {"subscriptionDelete_response", {PduType::SUBSCRIPTION_RESPONSE, [](const Pdu& p, const json&) {
            const auto& m = as<SubscriptionResponse>(p);
            ASSERT_EQ(m.request_id, 3u);
            ASSERT_TRUE(m.response_code == ResponseCode::POSITIVE);
            ASSERT_TRUE(m.subscription_id.has_value());
            ASSERT_FALSE(m.telemetry_identifier_list.has_value());
            ASSERT_FALSE(m.control_identifier_list.has_value());
        }}},
        // Aerial has no controls, so controlIdentifier is 0 here, which the ASN.1
        // grammar (1..65535) would refuse. This test only decodes.
        {"dAppControlAction", {PduType::DAPP_CONTROL_ACTION, [](const Pdu& p, const json& j) {
            const auto& m = as<DAppControlAction>(p);
            ASSERT_EQ(m.dapp_identifier, 1u);
            ASSERT_EQ(m.ran_function_identifier, 2u);
            ASSERT_EQ(m.control_identifier, 0u);
            ASSERT_EQ(m.sequence_id, 0u);
            ASSERT_TRUE(bytes_to_json(m.action_data) == j["actionData"]);
        }}},
        {"messageAck", {PduType::MESSAGE_ACK, [](const Pdu& p, const json&) {
            const auto& m = as<MessageAck>(p);
            ASSERT_EQ(m.request_id, 4u);
            ASSERT_TRUE(m.response_code == ResponseCode::NEGATIVE);
        }}},
        {"releaseMessage_from_dapp", {PduType::RELEASE_MESSAGE, [](const Pdu& p, const json&) {
            ASSERT_EQ(as<ReleaseMessage>(p).dapp_identifier, 1u);
        }}},
        {"releaseMessage_from_agent", {PduType::RELEASE_MESSAGE, [](const Pdu& p, const json&) {
            ASSERT_EQ(as<ReleaseMessage>(p).dapp_identifier, 1u);
        }}},
    };
    return table;
}

}  // namespace

TEST(InteropAerial_everyExampleIsCovered) {
    const json doc = load_examples();
    const json& examples = doc["e3ap_message_examples"];

    std::set<std::string> in_file;
    for (const auto& [name, example] : examples.items()) in_file.insert(name);
    std::set<std::string> in_table;
    for (const auto& [name, expected] : expectations()) in_table.insert(name);

    // A new or removed example in the vendored file has to be decided here.
    ASSERT_TRUE(in_file == in_table);
    ASSERT_EQ(in_file.size(), 17u);
}

TEST(InteropAerial_examplesDecodeToTheirFields) {
    const json doc = load_examples();
    for (const auto& [name, expected] : expectations()) {
        const json& message = doc["e3ap_message_examples"][name]["message"];
        const Pdu pdu = decode_message(message);

        ASSERT_TRUE(pdu.type == expected.first);
        ASSERT_EQ(pdu.message_id, message["id"].get<uint32_t>());
        // Aerial's schema has no timestamp.
        ASSERT_EQ(pdu.timestamp, 0ull);

        if (expected.second) {
            expected.second(pdu, message);
        } else {
            // The four indication examples: nested protocolData comes through verbatim.
            const auto& m = as<IndicationMessage>(pdu);
            ASSERT_EQ(m.dapp_identifier, 1u);
            ASSERT_EQ(m.ran_function_identifier, 2u);
            ASSERT_TRUE(bytes_to_json(m.protocol_data) == message["protocolData"]);
        }
    }
}

TEST(InteropAerial_reencodingAgreesOnEveryKeyBothWrite) {
    const json doc = load_examples();
    std::string disagreements;
    auto expect = [&disagreements](bool ok, const std::string& name, const std::string& key,
                                   const char* what) {
        if (!ok) disagreements += "\n  " + name + "." + key + ": " + what;
    };

    for (const auto& [name, expected] : expectations()) {
        const json& message = doc["e3ap_message_examples"][name]["message"];
        const std::string type = message["type"];
        const json out = reencode(decode_message(message));

        // libe3 always writes a timestamp, which Aerial's schema lacks.
        expect(out.contains("timestamp"), name, "timestamp", "libe3 does not write it");
        expect(!message.contains("timestamp"), name, "timestamp", "Aerial now sends it");

        for (const auto& [key, value] : message.items()) {
            if (key == "telemetryGrantedList" || key == "controlGrantedList") {
                // Aerial's names for the granted lists; libe3 writes the others.
                const std::string libe3_key = key == "telemetryGrantedList"
                                                  ? "telemetryIdentifierList"
                                                  : "controlIdentifierList";
                expect(!out.contains(key), name, key, "libe3 now writes Aerial's name");
                expect(out.contains(libe3_key) && out[libe3_key] == value, name, libe3_key,
                       "granted list differs");
            } else if (key == "message") {
                // Free text on a negative reply: libe3 has no field for it.
                expect(!out.contains(key), name, key, "libe3 now writes it");
            } else if ((type == "indicationMessage" && key == "subscriptionId") ||
                       (type == "messageAck" && key == "dAppIdentifier")) {
                // Aerial tags every indication with its subscription and every ack
                // with the dApp: libe3 has no field for either.
                expect(!out.contains(key), name, key, "libe3 now writes it");
            } else {
                expect(out.contains(key) && out[key] == value, name, key, "libe3 writes another value or none");
            }
        }
    }
    ASSERT_STREQ(disagreements, "");
}

TEST(InteropAerial_reencodedMessagesDecodeAgain) {
    const json doc = load_examples();
    for (const auto& [name, expected] : expectations()) {
        const json& message = doc["e3ap_message_examples"][name]["message"];
        const json once = reencode(decode_message(message));
        const json twice = reencode(decode_message(once));
        ASSERT_TRUE(once == twice);
    }
}

int main() {
    return RUN_ALL_TESTS();
}
