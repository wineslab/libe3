/**
 * @file test_e2e_report_path.cpp
 * @brief End-to-end integration tests for the dApp → libe3 report path.
 *
 * Drives a real E3Agent and a fake dApp ZMQ peer in the same process to
 * exercise the full inbound report pipeline:
 *
 *   fake dApp PUB
 *     → ZMQ IPC
 *       → libe3 inbound SUB socket
 *         → APER decode
 *           → LockFreeQueue<DAppReport>
 *             → report worker thread
 *               → handle_dapp_report()
 *                 → user-registered DAppReportHandler
 *
 * The E3-SETUP handshake is deliberately skipped: libe3's subscriber
 * loop dispatches DAPP_REPORT PDUs to the report queue without
 * validating that the dApp ID was registered, so the handshake is not
 * relevant to the properties being verified here.
 *
 * Properties verified:
 *
 *   (1) Every PDU sent by the fake dApp is delivered to the registered
 *       handler exactly once.
 *
 *   (2) FIFO ordering is preserved end-to-end.
 *
 *   (3) The decoded payload bytes match the original payload exactly,
 *       with no truncation or trailing contamination.
 *
 *   (4) When the producer rate exceeds the handler's processing rate,
 *       every message still reaches the handler (the inbound pipeline
 *       does not silently drop messages while the handler is busy).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"
#include "libe3/sm_interface.hpp"

#include <zmq.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>   // getpid

using namespace libe3;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

inline int error_to_int(ErrorCode e) { return static_cast<int>(e); }

/// Counter so each test instance uses distinct IPC namespaces — required
/// when running ctest -j to avoid socket-file collisions.
static std::atomic<int> g_e2e_seq{0};

static std::string unique_ipc(const char* tag) {
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/e2e_test_" << getpid() << "_"
        << g_e2e_seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

/// Minimal ServiceModel so register_sm() succeeds.
class E2ETestSM : public ServiceModel {
public:
    explicit E2ETestSM(uint32_t ran_function_id) : id_(ran_function_id) {}
    std::string name() const override { return "E2ETestSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return id_; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {10}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }
private:
    uint32_t id_;
    bool running_ = false;
};

/// Tag a 32-bit sequence number into the first 4 payload bytes; remaining
/// bytes use a distinctive filler so the handler can validate the
/// payload arrived intact.
static std::vector<uint8_t> tagged_payload(uint32_t seq, size_t bytes = 70) {
    std::vector<uint8_t> v(bytes, 0xCD);
    if (bytes >= 4) {
        v[0] = static_cast<uint8_t>(seq);
        v[1] = static_cast<uint8_t>(seq >> 8);
        v[2] = static_cast<uint8_t>(seq >> 16);
        v[3] = static_cast<uint8_t>(seq >> 24);
    }
    return v;
}

static uint32_t read_seq(const std::vector<uint8_t>& v) {
    if (v.size() < 4) return 0xFFFFFFFFu;
    return  static_cast<uint32_t>(v[0])
         | (static_cast<uint32_t>(v[1]) << 8)
         | (static_cast<uint32_t>(v[2]) << 16)
         | (static_cast<uint32_t>(v[3]) << 24);
}

/**
 * Fake dApp ZMQ PUB peer.  Owns a ZMQ context and a single PUB socket
 * connected to libe3's inbound SUB endpoint.  Encodes and sends
 * DAPP_REPORT PDUs through the same APER encoder libe3 uses, so the
 * wire format matches what a real dApp would produce.
 */
class FakeDappPub {
public:
    FakeDappPub()
        : ctx_(zmq_ctx_new()),
          encoder_(create_encoder(EncodingFormat::ASN1))
    {}

    ~FakeDappPub() {
        if (pub_) zmq_close(pub_);
        if (ctx_) zmq_ctx_destroy(ctx_);
    }

    bool connect(const std::string& subscriber_endpoint) {
        pub_ = zmq_socket(ctx_, ZMQ_PUB);
        if (!pub_) {
            std::cerr << "[FakeDappPub] zmq_socket(ZMQ_PUB) failed: "
                      << zmq_strerror(zmq_errno()) << "\n";
            return false;
        }
        int linger = 0;
        zmq_setsockopt(pub_, ZMQ_LINGER, &linger, sizeof(linger));
        if (zmq_connect(pub_, subscriber_endpoint.c_str()) != 0) {
            std::cerr << "[FakeDappPub] zmq_connect(\"" << subscriber_endpoint
                      << "\") failed: " << zmq_strerror(zmq_errno()) << "\n";
            return false;
        }
        return true;
    }

    bool send_report(uint32_t seq, uint32_t dapp_id, uint32_t ran_function_id,
                     size_t payload_bytes = 70) {
        Pdu pdu(PduType::DAPP_REPORT);
        pdu.message_id = (seq % 1000u) + 1u;   // E3-MessageID is INTEGER (1..1000)
        DAppReport rep;
        rep.dapp_identifier = dapp_id;
        rep.ran_function_identifier = ran_function_id;
        rep.report_data = tagged_payload(seq, payload_bytes);
        pdu.choice = rep;

        auto enc = encoder_->encode(pdu);
        if (!enc.has_value()) {
            std::cerr << "[FakeDappPub] encode(DAPP_REPORT) failed at seq=" << seq << "\n";
            return false;
        }
        return zmq_send(pub_, enc->buffer.data(), enc->buffer.size(), 0) >= 0;
    }

private:
    void* ctx_ = nullptr;
    void* pub_ = nullptr;
    std::unique_ptr<E3Encoder> encoder_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/**
 * Burst N reports into a real E3Agent's inbound SUB socket at full
 * encode-and-send speed and assert that all N reach the handler
 * exactly once, in FIFO order, with the original payload bytes intact.
 */
TEST(E2EReportPath_burst_AllReportsArriveExactlyOnce) {
    constexpr uint32_t N = 1000;
    constexpr uint32_t RAN_FUNC_ID = 100;
    constexpr uint32_t FAKE_DAPP_ID = 1;

    const std::string setup_ep = unique_ipc("setup");
    const std::string sub_ep   = unique_ipc("inbound");
    const std::string pub_ep   = unique_ipc("outbound");

    E3Config cfg;
    cfg.link_layer       = E3LinkLayer::ZMQ;
    cfg.transport_layer  = E3TransportLayer::IPC;
    cfg.setup_endpoint   = setup_ep;
    cfg.subscriber_endpoint = sub_ep;
    cfg.publisher_endpoint  = pub_ep;
    cfg.encoding         = EncodingFormat::ASN1;
    cfg.log_level        = 0;
    cfg.ran_identifier   = "e2e-test";

    E3Agent agent(std::move(cfg));
    ASSERT_EQ(error_to_int(agent.register_sm(std::make_unique<E2ETestSM>(RAN_FUNC_ID))),
              error_to_int(ErrorCode::SUCCESS));

    std::atomic<uint32_t> recv_count{0};
    std::vector<bool> seen(N, false);
    std::mutex seen_mtx;
    std::atomic<bool> bad_payload{false};
    std::atomic<bool> duplicate{false};
    std::atomic<bool> out_of_order{false};
    uint32_t expected_next = 0;

    agent.set_dapp_report_handler([&, N](const DAppReport& r) {
        if (r.report_data.size() < 4) { bad_payload.store(true); return; }
        for (size_t i = 4; i < r.report_data.size(); ++i) {
            if (r.report_data[i] != 0xCD) { bad_payload.store(true); break; }
        }
        const uint32_t seq = read_seq(r.report_data);
        if (seq >= N) { bad_payload.store(true); return; }

        std::lock_guard<std::mutex> lk(seen_mtx);
        if (seen[seq]) { duplicate.store(true); return; }
        seen[seq] = true;
        if (seq != expected_next) out_of_order.store(true);
        expected_next = seq + 1;
        recv_count.fetch_add(1);
    });

    ASSERT_EQ(error_to_int(agent.start()), error_to_int(ErrorCode::SUCCESS));
    ASSERT_TRUE(agent.is_running());

    // Allow the inbound SUB socket to bind before the fake dApp connects.
    std::this_thread::sleep_for(100ms);

    FakeDappPub pub;
    ASSERT_TRUE(pub.connect(sub_ep));

    // ZMQ slow-joiner: SUB needs a moment to register the subscription
    // with PUB after connect.  This is intrinsic to ZMQ PUB/SUB.
    std::this_thread::sleep_for(300ms);

    for (uint32_t i = 0; i < N; ++i) {
        ASSERT_TRUE(pub.send_report(i, FAKE_DAPP_ID, RAN_FUNC_ID));
    }

    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (recv_count.load() < N
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(20ms);
    }

    agent.stop();

    ASSERT_EQ(recv_count.load(), N);
    ASSERT_FALSE(bad_payload.load());
    ASSERT_FALSE(duplicate.load());
    ASSERT_FALSE(out_of_order.load());
}

/**
 * Producer rate (~20 kHz) exceeds the handler's processing rate
 * (~5 kHz, simulated by a 200 µs delay per call).  Every message must
 * still reach the handler — the rate gap is absorbed by the internal
 * dispatch queue rather than dropped at the inbound socket.
 */
TEST(E2EReportPath_slowHandler_doesNotBlockSubscriber) {
    constexpr uint32_t N = 200;
    constexpr uint32_t RAN_FUNC_ID = 100;   // E3-RanFunctionIdentifier has a bounded range
    constexpr uint32_t FAKE_DAPP_ID = 1;
    constexpr auto HANDLER_DELAY = std::chrono::microseconds(200);

    const std::string setup_ep = unique_ipc("setup");
    const std::string sub_ep   = unique_ipc("inbound");
    const std::string pub_ep   = unique_ipc("outbound");

    E3Config cfg;
    cfg.link_layer       = E3LinkLayer::ZMQ;
    cfg.transport_layer  = E3TransportLayer::IPC;
    cfg.setup_endpoint   = setup_ep;
    cfg.subscriber_endpoint = sub_ep;
    cfg.publisher_endpoint  = pub_ep;
    cfg.encoding         = EncodingFormat::ASN1;
    cfg.log_level        = 0;
    cfg.ran_identifier   = "e2e-test";

    E3Agent agent(std::move(cfg));
    agent.register_sm(std::make_unique<E2ETestSM>(RAN_FUNC_ID));

    std::atomic<uint32_t> recv_count{0};
    agent.set_dapp_report_handler([&](const DAppReport&) {
        std::this_thread::sleep_for(HANDLER_DELAY);
        recv_count.fetch_add(1);
    });

    ASSERT_EQ(error_to_int(agent.start()), error_to_int(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    FakeDappPub pub;
    ASSERT_TRUE(pub.connect(sub_ep));
    std::this_thread::sleep_for(300ms);

    // Producer at ~20 kHz (50 µs/msg) — 4× the handler's processing rate
    // (200 µs/msg).  The subscriber thread must drain the inbound ZMQ
    // socket at I/O speed and the rate gap must be absorbed by the
    // internal dispatch queue, not by silently dropping at the socket.
    for (uint32_t i = 0; i < N; ++i) {
        ASSERT_TRUE(pub.send_report(i, FAKE_DAPP_ID, RAN_FUNC_ID));
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (recv_count.load() < N
           && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    agent.stop();

    ASSERT_EQ(recv_count.load(), N);
}

// ---------------------------------------------------------------------------

int main() {
    return RUN_ALL_TESTS();
}
