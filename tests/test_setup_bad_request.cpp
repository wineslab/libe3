/**
 * @file test_setup_bad_request.cpp
 * @brief Regression test: a malformed setup request must not wedge the
 *        RAN's setup REP socket.
 *
 * The RAN side of the E3 setup channel is a ZMQ REP socket. The REP state
 * machine requires exactly one reply per received request before the socket
 * can receive again. If the agent bails out of the setup loop without
 * replying (e.g. the incoming bytes do not decode — wrong encoding dialect,
 * or plain garbage), the socket is stuck in the send state and every later
 * setup request from any dApp stalls until the agent restarts.
 *
 * Properties verified, using a raw ZMQ REQ peer against a real E3Agent:
 *
 *   (1) Sending undecodable bytes on the setup channel produces an explicit
 *       rejection: a decodable SetupResponse with ResponseCode::NEGATIVE
 *       (the original request id is unrecoverable from garbage bytes, so
 *       the agent substitutes a fresh one — E3-MessageID excludes 0),
 *       i.e. the REQ/REP exchange always completes.
 *
 *   (2) After the garbage exchange, a well-formed SetupRequest on the very
 *       same channel still receives a positive SetupResponse — the setup
 *       channel survived the malformed message.
 *
 * The test uses whichever encoding the build provides (JSON preferred) so
 * it runs on JSON-only, ASN.1-only, and dual-encoder builds.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <zmq.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>   // getpid

using namespace libe3;
using namespace std::chrono_literals;

namespace {

inline int error_to_int(ErrorCode e) { return static_cast<int>(e); }

/// Counter so each test instance uses distinct IPC namespaces — required
/// when running ctest -j to avoid socket-file collisions.
std::atomic<int> g_seq{0};

std::string unique_ipc(const char* tag) {
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/badsetup_test_" << getpid() << "_"
        << g_seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

/// Pick an encoding that is compiled into this build (JSON preferred so the
/// test also runs on JSON-only builds).
EncodingFormat pick_encoding() {
#if defined(LIBE3_ENABLE_JSON)
    return EncodingFormat::JSON;
#else
    return EncodingFormat::ASN1;
#endif
}

}  // namespace

TEST(SetupChannel_garbageRequest_repliesAndChannelSurvives) {
    const std::string setup_ep = unique_ipc("setup");
    const std::string sub_ep   = unique_ipc("inbound");
    const std::string pub_ep   = unique_ipc("outbound");
    const EncodingFormat encoding = pick_encoding();

    E3Config cfg;
    cfg.role             = E3Role::RAN;
    cfg.link_layer       = E3LinkLayer::ZMQ;
    cfg.transport_layer  = E3TransportLayer::IPC;
    cfg.setup_endpoint   = setup_ep;
    cfg.subscriber_endpoint = sub_ep;
    cfg.publisher_endpoint  = pub_ep;
    cfg.encoding         = encoding;
    cfg.log_level        = 0;
    cfg.ran_identifier   = "badsetup-test";

    E3Agent agent(std::move(cfg));
    ASSERT_EQ(error_to_int(agent.start()), error_to_int(ErrorCode::SUCCESS));
    ASSERT_TRUE(agent.is_running());

    // The REP socket is bound synchronously inside start(); a short settle
    // keeps the IPC connect race-free.
    std::this_thread::sleep_for(100ms);

    void* ctx = zmq_ctx_new();
    ASSERT_TRUE(ctx != nullptr);
    void* req = zmq_socket(ctx, ZMQ_REQ);
    ASSERT_TRUE(req != nullptr);
    int recv_timeout = 5000;  // a wedged REP socket shows up as a timeout
    zmq_setsockopt(req, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    int linger = 0;
    zmq_setsockopt(req, ZMQ_LINGER, &linger, sizeof(linger));
    ASSERT_EQ(zmq_connect(req, setup_ep.c_str()), 0);

    auto encoder = create_encoder(encoding);
    ASSERT_TRUE(encoder != nullptr);

    // (1) Garbage on the setup channel: an explicit rejection — a decodable
    //     negative SetupResponse — must arrive. Without the fix the agent
    //     never replies and this recv times out with -1/EAGAIN.
    const char garbage[] = "definitely-not-an-e3ap-setup-request";
    ASSERT_GE(zmq_send(req, garbage, sizeof(garbage) - 1, 0), 0);

    uint8_t buf[4096];
    int n = zmq_recv(req, buf, sizeof(buf), 0);
    ASSERT_GT(n, 0);
    auto rejection = encoder->decode(buf, static_cast<size_t>(n));
    ASSERT_TRUE(rejection.has_value());
    ASSERT_EQ(static_cast<int>(rejection->type),
              static_cast<int>(PduType::SETUP_RESPONSE));
    auto* neg = std::get_if<SetupResponse>(&rejection->choice);
    ASSERT_TRUE(neg != nullptr);
    ASSERT_EQ(static_cast<int>(neg->response_code),
              static_cast<int>(ResponseCode::NEGATIVE));
    ASSERT_GE(neg->request_id, 1u);   // id unrecoverable → substituted, never 0
    ASSERT_TRUE(neg->ran_function_list.empty());
    ASSERT_TRUE(!neg->dapp_identifier.has_value());

    // (2) The same channel must still serve a well-formed SetupRequest.
    auto setup = encoder->encode_setup_request(
        42, "1.0.0", "badsetup-dapp", "0.0.1", "test-vendor");
    ASSERT_TRUE(setup.has_value());
    ASSERT_GE(zmq_send(req, setup->buffer.data(), setup->buffer.size(), 0), 0);

    n = zmq_recv(req, buf, sizeof(buf), 0);
    ASSERT_GT(n, 0);
    auto decoded = encoder->decode(buf, static_cast<size_t>(n));
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(static_cast<int>(decoded->type),
              static_cast<int>(PduType::SETUP_RESPONSE));
    auto* resp = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(resp != nullptr);
    ASSERT_EQ(static_cast<int>(resp->response_code),
              static_cast<int>(ResponseCode::POSITIVE));
    ASSERT_EQ(resp->request_id, 42u);
    ASSERT_TRUE(resp->dapp_identifier.has_value());

    zmq_close(req);
    zmq_ctx_destroy(ctx);
    agent.stop();
}

#if defined(LIBE3_ENABLE_JSON)
/**
 * A well-formed SetupRequest whose message id lies outside E3-MessageID's
 * 1..1000 must still receive a decodable positive SetupResponse: the agent
 * substitutes an in-range id instead of failing the response encode.
 *
 * JSON-only: the JSON codec is the one that lets an out-of-range id through
 * to the handler (the APER encoder rejects it on the sender side, but the
 * APER decoder does not range-check either, so the substitution protects
 * both encodings).
 */
TEST(SetupChannel_outOfRangeRequestId_substitutedAndAnswered) {
    const std::string setup_ep = unique_ipc("setup_oor");
    const std::string sub_ep   = unique_ipc("inbound_oor");
    const std::string pub_ep   = unique_ipc("outbound_oor");

    E3Config cfg;
    cfg.role             = E3Role::RAN;
    cfg.link_layer       = E3LinkLayer::ZMQ;
    cfg.transport_layer  = E3TransportLayer::IPC;
    cfg.setup_endpoint   = setup_ep;
    cfg.subscriber_endpoint = sub_ep;
    cfg.publisher_endpoint  = pub_ep;
    cfg.encoding         = EncodingFormat::JSON;
    cfg.log_level        = 0;
    cfg.ran_identifier   = "badsetup-test";

    E3Agent agent(std::move(cfg));
    ASSERT_EQ(error_to_int(agent.start()), error_to_int(ErrorCode::SUCCESS));
    ASSERT_TRUE(agent.is_running());

    std::this_thread::sleep_for(100ms);

    void* ctx = zmq_ctx_new();
    ASSERT_TRUE(ctx != nullptr);
    void* req = zmq_socket(ctx, ZMQ_REQ);
    ASSERT_TRUE(req != nullptr);
    int recv_timeout = 5000;
    zmq_setsockopt(req, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    int linger = 0;
    zmq_setsockopt(req, ZMQ_LINGER, &linger, sizeof(linger));
    ASSERT_EQ(zmq_connect(req, setup_ep.c_str()), 0);

    auto encoder = create_encoder(EncodingFormat::JSON);
    ASSERT_TRUE(encoder != nullptr);

    auto setup = encoder->encode_setup_request(
        4096, "1.0.0", "badsetup-dapp", "0.0.1", "test-vendor");
    ASSERT_TRUE(setup.has_value());
    ASSERT_GE(zmq_send(req, setup->buffer.data(), setup->buffer.size(), 0), 0);

    uint8_t buf[4096];
    int n = zmq_recv(req, buf, sizeof(buf), 0);
    ASSERT_GT(n, 0);
    auto decoded = encoder->decode(buf, static_cast<size_t>(n));
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(static_cast<int>(decoded->type),
              static_cast<int>(PduType::SETUP_RESPONSE));
    auto* resp = std::get_if<SetupResponse>(&decoded->choice);
    ASSERT_TRUE(resp != nullptr);
    ASSERT_EQ(static_cast<int>(resp->response_code),
              static_cast<int>(ResponseCode::POSITIVE));
    ASSERT_TRUE(resp->dapp_identifier.has_value());
    // The unencodable id was substituted with one inside E3-MessageID.
    ASSERT_GE(resp->request_id, 1u);
    ASSERT_LE(resp->request_id, 1000u);

    zmq_close(req);
    zmq_ctx_destroy(ctx);
    agent.stop();
}
#endif  // LIBE3_ENABLE_JSON

// ---------------------------------------------------------------------------

int main() {
    return RUN_ALL_TESTS();
}
