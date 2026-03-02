/**
 * @file test_types.cpp
 * @brief Unit tests for libe3 types and PDU handling
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/types.hpp"
#include <cstring>

using namespace libe3;

TEST(PduType_values) {
    // Verify PDU type enumeration values
    ASSERT_EQ(static_cast<int>(PduType::SETUP_REQUEST), 0);
    ASSERT_EQ(static_cast<int>(PduType::SETUP_RESPONSE), 1);
    ASSERT_EQ(static_cast<int>(PduType::SUBSCRIPTION_REQUEST), 2);
    ASSERT_EQ(static_cast<int>(PduType::SUBSCRIPTION_DELETE), 3);
    ASSERT_EQ(static_cast<int>(PduType::SUBSCRIPTION_RESPONSE), 4);
    ASSERT_EQ(static_cast<int>(PduType::INDICATION_MESSAGE), 5);
}

TEST(ErrorCode_to_string) {
    ASSERT_STREQ(error_code_to_string(ErrorCode::SUCCESS), "Success");
    ASSERT_STREQ(error_code_to_string(ErrorCode::TIMEOUT), "Timeout");
    ASSERT_STREQ(error_code_to_string(ErrorCode::NOT_FOUND), "Not found");
}

TEST(AgentState_to_string) {
    ASSERT_STREQ(agent_state_to_string(AgentState::UNINITIALIZED), "Uninitialized");
    ASSERT_STREQ(agent_state_to_string(AgentState::RUNNING), "Running");
    ASSERT_STREQ(agent_state_to_string(AgentState::STOPPED), "Stopped");
}

TEST(Pdu_construction) {
    Pdu pdu(PduType::SETUP_REQUEST);
    ASSERT_TRUE(pdu.type == PduType::SETUP_REQUEST);
    ASSERT_GT(pdu.timestamp, 0u);
    ASSERT_EQ(pdu.message_id, 0u);
}

TEST(SetupRequest_fields) {
    SetupRequest req;
    req.e3ap_protocol_version = "1.0.0";
    req.dapp_name = "TestDApp";
    req.dapp_version = "1.0.0";
    req.vendor = "TestVendor";
    
    ASSERT_STREQ(req.e3ap_protocol_version.c_str(), "1.0.0");
    ASSERT_STREQ(req.dapp_name.c_str(), "TestDApp");
    ASSERT_STREQ(req.dapp_version.c_str(), "1.0.0");
    ASSERT_STREQ(req.vendor.c_str(), "TestVendor");
}

TEST(SetupResponse_fields) {
    SetupResponse resp;
    resp.request_id = 100;
    resp.response_code = ResponseCode::POSITIVE;
    resp.e3ap_protocol_version = "1.0.0";
    resp.dapp_identifier = 42;
    resp.ran_identifier = "test-ran";
    
    ASSERT_EQ(resp.request_id, 100u);
    ASSERT_TRUE(resp.response_code == ResponseCode::POSITIVE);
    ASSERT_TRUE(resp.e3ap_protocol_version.has_value());
    ASSERT_TRUE(resp.dapp_identifier.has_value());
    ASSERT_STREQ(resp.ran_identifier.c_str(), "test-ran");
}

TEST(SubscriptionRequest_fields) {
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_function_identifier = 100;
    req.telemetry_identifier_list = {1, 2, 3};
    req.control_identifier_list = {10, 20};
    req.subscription_time = 3600;
    
    ASSERT_EQ(req.dapp_identifier, 42u);
    ASSERT_EQ(req.ran_function_identifier, 100u);
    ASSERT_EQ(req.telemetry_identifier_list.size(), 3u);
    ASSERT_EQ(req.control_identifier_list.size(), 2u);
    ASSERT_TRUE(req.subscription_time.has_value());
}

TEST(SubscriptionDelete_fields) {
    SubscriptionDelete del;
    del.dapp_identifier = 42;
    del.subscription_id = 77;
    
    ASSERT_EQ(del.dapp_identifier, 42u);
    ASSERT_EQ(del.subscription_id, 77u);
}

TEST(DAppControlAction_fields) {
    DAppControlAction action;
    action.dapp_identifier = 123;
    action.ran_function_identifier = 456;
    action.control_identifier = 789;
    action.action_data = {0x01, 0x02, 0x03, 0x04};
    
    ASSERT_EQ(action.dapp_identifier, 123u);
    ASSERT_EQ(action.ran_function_identifier, 456u);
    ASSERT_EQ(action.control_identifier, 789u);
    ASSERT_EQ(action.action_data.size(), 4u);
    ASSERT_EQ(action.action_data[0], 0x01);
}

TEST(IndicationMessage_fields) {
    IndicationMessage msg;
    msg.dapp_identifier = 99;
    msg.ran_function_identifier = 55;
    msg.protocol_data = {0xDE, 0xAD, 0xBE, 0xEF};
    
    ASSERT_EQ(msg.dapp_identifier, 99u);
    ASSERT_EQ(msg.ran_function_identifier, 55u);
    ASSERT_EQ(msg.protocol_data.size(), 4u);
}

TEST(Pdu_with_SetupRequest) {
    Pdu pdu(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.dapp_name = "my-dapp";
    pdu.choice = req;
    
    ASSERT_TRUE(std::holds_alternative<SetupRequest>(pdu.choice));
    auto& stored = std::get<SetupRequest>(pdu.choice);
    ASSERT_STREQ(stored.dapp_name.c_str(), "my-dapp");
}

TEST(Pdu_with_SubscriptionResponse) {
    Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.request_id = 77;
    resp.response_code = ResponseCode::POSITIVE;
    pdu.choice = resp;
    
    ASSERT_TRUE(std::holds_alternative<SubscriptionResponse>(pdu.choice));
    auto& stored = std::get<SubscriptionResponse>(pdu.choice);
    ASSERT_EQ(stored.request_id, 77u);
}

TEST(E3Config_defaults) {
    E3Config config;
    config.ran_identifier = "test";
    
    ASSERT_TRUE(config.link_layer == E3LinkLayer::ZMQ);
    ASSERT_TRUE(config.transport_layer == E3TransportLayer::IPC);
    // Encoding default follows build-time selection
#if defined(LIBE3_ENABLE_JSON)
    ASSERT_TRUE(config.encoding == EncodingFormat::JSON);
#elif defined(LIBE3_ENABLE_ASN1)
    ASSERT_TRUE(config.encoding == EncodingFormat::ASN1);
#else
    ASSERT_TRUE(config.encoding == EncodingFormat::ASN1);
#endif
    ASSERT_EQ(config.setup_port, 9990u);
    ASSERT_EQ(config.subscriber_port, 9999u);
    ASSERT_EQ(config.publisher_port, 9991u);
}

TEST(E3Config_zmq) {
    E3Config config;
    config.ran_identifier = "zmq-ran";
    config.link_layer = E3LinkLayer::ZMQ;
    config.transport_layer = E3TransportLayer::TCP;
    config.setup_port = 5555;
    config.subscriber_port = 5556;
    config.publisher_port = 5557;
    config.setup_endpoint = "tcp://localhost:5555";
    config.subscriber_endpoint = "tcp://localhost:5556";
    config.publisher_endpoint = "tcp://localhost:5557";
    
    ASSERT_TRUE(config.link_layer == E3LinkLayer::ZMQ);
    ASSERT_TRUE(config.transport_layer == E3TransportLayer::TCP);
    ASSERT_EQ(config.setup_port, 5555u);
    ASSERT_EQ(config.subscriber_port, 5556u);
    ASSERT_EQ(config.publisher_port, 5557u);
    ASSERT_STREQ(config.setup_endpoint.c_str(), "tcp://localhost:5555");
}

TEST(E3LinkLayer_to_string) {
    ASSERT_STREQ(link_layer_to_string(E3LinkLayer::ZMQ), "zmq");
    ASSERT_STREQ(link_layer_to_string(E3LinkLayer::POSIX), "posix");
}

TEST(E3TransportLayer_to_string) {
    ASSERT_STREQ(transport_layer_to_string(E3TransportLayer::SCTP), "sctp");
    ASSERT_STREQ(transport_layer_to_string(E3TransportLayer::TCP), "tcp");
    ASSERT_STREQ(transport_layer_to_string(E3TransportLayer::IPC), "ipc");
}

TEST(RanFunctionDefinition_fields) {
    RanFunctionDefinition def;
    def.ran_function_id = 42;
    def.sm_name = "MyServiceModel";
    def.sm_version = "1.2.3";
    
    ASSERT_EQ(def.ran_function_id, 42u);
    ASSERT_STREQ(def.sm_name.c_str(), "MyServiceModel");
    ASSERT_STREQ(def.sm_version.c_str(), "1.2.3");
}

TEST(DAppReport_fields) {
    DAppReport report;
    report.dapp_identifier = 555;
    report.ran_function_identifier = 666;
    report.report_data = {0x11, 0x22, 0x33};
    
    ASSERT_EQ(report.dapp_identifier, 555u);
    ASSERT_EQ(report.ran_function_identifier, 666u);
    ASSERT_EQ(report.report_data.size(), 3u);
}

TEST(MessageAck_fields) {
    MessageAck ack;
    ack.request_id = 12345;
    ack.response_code = ResponseCode::POSITIVE;
    
    ASSERT_EQ(ack.request_id, 12345u);
    ASSERT_TRUE(ack.response_code == ResponseCode::POSITIVE);
}

int main() {
    return RUN_ALL_TESTS();
}
