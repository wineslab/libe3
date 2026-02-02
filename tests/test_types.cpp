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
    ASSERT_EQ(static_cast<int>(PduType::INDICATION_MESSAGE), 7);
}

TEST(ErrorCode_values) {
    ASSERT_EQ(ErrorCode::SUCCESS, ErrorCode::SUCCESS);
    ASSERT_NE(ErrorCode::SUCCESS, ErrorCode::GENERIC_ERROR);
}

TEST(ErrorCode_to_string) {
    ASSERT_STREQ(error_code_to_string(ErrorCode::SUCCESS), "Success");
    ASSERT_STREQ(error_code_to_string(ErrorCode::TIMEOUT), "Timeout");
    ASSERT_STREQ(error_code_to_string(ErrorCode::NOT_FOUND), "Not found");
}

TEST(AgentState_values) {
    ASSERT_NE(AgentState::UNINITIALIZED, AgentState::RUNNING);
    ASSERT_NE(AgentState::RUNNING, AgentState::STOPPED);
}

TEST(AgentState_to_string) {
    ASSERT_STREQ(agent_state_to_string(AgentState::UNINITIALIZED), "Uninitialized");
    ASSERT_STREQ(agent_state_to_string(AgentState::RUNNING), "Running");
    ASSERT_STREQ(agent_state_to_string(AgentState::STOPPED), "Stopped");
}

TEST(Pdu_construction) {
    Pdu pdu(PduType::SETUP_REQUEST);
    ASSERT_EQ(pdu.type, PduType::SETUP_REQUEST);
    ASSERT_GT(pdu.timestamp, 0u);
    ASSERT_EQ(pdu.message_id, 0u);
}

TEST(SetupRequest_fields) {
    SetupRequest req;
    req.ran_identifier = "test-ran-001";
    req.protocol_version = LIBE3_PROTOCOL_VERSION;
    req.ran_functions.push_back({100, "SM1", "1.0"});
    req.ran_functions.push_back({200, "SM2", "2.0"});
    
    ASSERT_STREQ(req.ran_identifier.c_str(), "test-ran-001");
    ASSERT_EQ(req.protocol_version, LIBE3_PROTOCOL_VERSION);
    ASSERT_EQ(req.ran_functions.size(), 2u);
    ASSERT_EQ(req.ran_functions[0].ran_function_id, 100u);
    ASSERT_STREQ(req.ran_functions[1].sm_name.c_str(), "SM2");
}

TEST(SetupResponse_fields) {
    SetupResponse resp;
    resp.result = SetupResult::SUCCESS;
    resp.accepted_ran_functions.push_back(100);
    resp.accepted_ran_functions.push_back(200);
    resp.rejected_ran_functions.clear();
    
    ASSERT_EQ(resp.result, SetupResult::SUCCESS);
    ASSERT_EQ(resp.accepted_ran_functions.size(), 2u);
    ASSERT_TRUE(resp.rejected_ran_functions.empty());
}

TEST(SubscriptionRequest_fields) {
    SubscriptionRequest req;
    req.dapp_identifier = 42;
    req.ran_functions_to_subscribe.push_back(100);
    req.ran_functions_to_subscribe.push_back(200);
    req.ran_functions_to_unsubscribe.push_back(300);
    
    ASSERT_EQ(req.dapp_identifier, 42u);
    ASSERT_EQ(req.ran_functions_to_subscribe.size(), 2u);
    ASSERT_EQ(req.ran_functions_to_unsubscribe.size(), 1u);
}

TEST(ControlAction_fields) {
    ControlAction action;
    action.dapp_identifier = 123;
    action.ran_function_identifier = 456;
    action.action_data = {0x01, 0x02, 0x03, 0x04};
    
    ASSERT_EQ(action.dapp_identifier, 123u);
    ASSERT_EQ(action.ran_function_identifier, 456u);
    ASSERT_EQ(action.action_data.size(), 4u);
    ASSERT_EQ(action.action_data[0], 0x01);
}

TEST(IndicationMessage_fields) {
    IndicationMessage msg;
    msg.dapp_identifier = 99;
    msg.protocol_data = {0xDE, 0xAD, 0xBE, 0xEF};
    
    ASSERT_EQ(msg.dapp_identifier, 99u);
    ASSERT_EQ(msg.protocol_data.size(), 4u);
}

TEST(Pdu_with_SetupRequest) {
    Pdu pdu(PduType::SETUP_REQUEST);
    SetupRequest req;
    req.ran_identifier = "my-ran";
    pdu.choice = req;
    
    ASSERT_TRUE(std::holds_alternative<SetupRequest>(pdu.choice));
    auto& stored = std::get<SetupRequest>(pdu.choice);
    ASSERT_STREQ(stored.ran_identifier.c_str(), "my-ran");
}

TEST(Pdu_with_SubscriptionResponse) {
    Pdu pdu(PduType::SUBSCRIPTION_RESPONSE);
    SubscriptionResponse resp;
    resp.dapp_identifier = 77;
    resp.accepted_ran_functions.push_back(111);
    resp.rejected_ran_functions.push_back(222);
    pdu.choice = resp;
    
    ASSERT_TRUE(std::holds_alternative<SubscriptionResponse>(pdu.choice));
    auto& stored = std::get<SubscriptionResponse>(pdu.choice);
    ASSERT_EQ(stored.dapp_identifier, 77u);
}

TEST(E3Config_defaults) {
    E3Config config;
    config.ran_identifier = "test";
    
    ASSERT_EQ(config.link_layer, E3LinkLayer::POSIX);
    ASSERT_EQ(config.transport_layer, E3TransportLayer::IPC);
    ASSERT_EQ(config.encoding, EncodingFormat::ASN1);
    ASSERT_FALSE(config.simulation_mode);
}

TEST(E3Config_zmq) {
    E3Config config;
    config.ran_identifier = "zmq-ran";
    config.link_layer = E3LinkLayer::ZMQ;
    config.transport_layer = E3TransportLayer::TCP;
    config.setup_endpoint = "tcp://localhost:5555";
    config.subscriber_endpoint = "tcp://localhost:5556";
    config.publisher_endpoint = "tcp://localhost:5557";
    
    ASSERT_EQ(config.link_layer, E3LinkLayer::ZMQ);
    ASSERT_EQ(config.transport_layer, E3TransportLayer::TCP);
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
    ack.original_message_id = 12345;
    ack.result = ErrorCode::SUCCESS;
    ack.message = "OK";
    
    ASSERT_EQ(ack.original_message_id, 12345u);
    ASSERT_EQ(ack.result, ErrorCode::SUCCESS);
    ASSERT_STREQ(ack.message.c_str(), "OK");
}

int main() {
    return RUN_ALL_TESTS();
}
