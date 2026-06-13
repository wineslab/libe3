/**
 * @file test_multi_agent_registry.cpp
 * @brief Multi-instance correctness of the (now per-interface) SM registry.
 *
 * Historically SmRegistry was a process-wide singleton, which broke any
 * process hosting more than one E3 agent:
 *
 *   - registering an SM with the same RAN function id on a second agent
 *     failed with SM_ALREADY_REGISTERED even though the agents are
 *     completely independent peers on different endpoints, and
 *
 *   - stopping/destroying one RAN-role agent cleared the shared registry,
 *     silently wiping the OTHER agent's Service Models.
 *
 * Properties verified with two real RAN-role E3Agents in one process (and
 * a real dApp-role agent as the client):
 *
 *   (1) Both agents can register an SM with the SAME ran_function_id.
 *
 *   (2) Each agent advertises its own registration independently.
 *
 *   (3) After one agent is stopped and destroyed, the surviving agent is
 *       still running, still has its SM registered, and still serves a
 *       full setup handshake advertising that RAN function to a dApp.
 *
 * The test uses whichever encoding the build provides (JSON preferred) so
 * it runs on JSON-only, ASN.1-only, and dual-encoder builds.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/sm_interface.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
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
    oss << "ipc:///tmp/dapps/multireg_test_" << getpid() << "_"
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

/// Minimal ServiceModel so register_sm() succeeds.
class RegistryTestSM : public ServiceModel {
public:
    explicit RegistryTestSM(uint32_t ran_function_id) : id_(ran_function_id) {}
    std::string name() const override { return "RegistryTestSM"; }
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

struct RanEndpoints {
    std::string setup;
    std::string sub;
    std::string pub;
};

E3Config make_ran_config(const RanEndpoints& ep, const char* ran_id,
                         EncodingFormat encoding) {
    E3Config cfg;
    cfg.role             = E3Role::RAN;
    cfg.link_layer       = E3LinkLayer::ZMQ;
    cfg.transport_layer  = E3TransportLayer::IPC;
    cfg.setup_endpoint   = ep.setup;
    cfg.subscriber_endpoint = ep.sub;
    cfg.publisher_endpoint  = ep.pub;
    cfg.encoding         = encoding;
    cfg.log_level        = 0;
    cfg.ran_identifier   = ran_id;
    return cfg;
}

}  // namespace

TEST(MultiAgent_sameRanFunctionId_independentRegistries) {
    constexpr uint32_t RAN_FUNC_ID = 1;
    const EncodingFormat encoding = pick_encoding();

    const RanEndpoints ep1{unique_ipc("setup1"), unique_ipc("in1"), unique_ipc("out1")};
    const RanEndpoints ep2{unique_ipc("setup2"), unique_ipc("in2"), unique_ipc("out2")};

    // Agent 1 hosts SM with ran_function_id 1.
    auto agent1 = std::make_unique<E3Agent>(make_ran_config(ep1, "multireg-ran-1", encoding));
    ASSERT_EQ(error_to_int(agent1->register_sm(std::make_unique<RegistryTestSM>(RAN_FUNC_ID))),
              error_to_int(ErrorCode::SUCCESS));
    ASSERT_EQ(error_to_int(agent1->start()), error_to_int(ErrorCode::SUCCESS));

    // Agent 2 hosts an SM with the SAME ran_function_id. With a process-wide
    // registry this registration failed with SM_ALREADY_REGISTERED.
    E3Agent agent2(make_ran_config(ep2, "multireg-ran-2", encoding));
    ASSERT_EQ(error_to_int(agent2.register_sm(std::make_unique<RegistryTestSM>(RAN_FUNC_ID))),
              error_to_int(ErrorCode::SUCCESS));
    ASSERT_EQ(error_to_int(agent2.start()), error_to_int(ErrorCode::SUCCESS));

    // Both agents advertise their own registration.
    auto funcs1 = agent1->get_available_ran_functions();
    ASSERT_EQ(funcs1.size(), 1u);
    ASSERT_EQ(funcs1[0], RAN_FUNC_ID);
    auto funcs2 = agent2.get_available_ran_functions();
    ASSERT_EQ(funcs2.size(), 1u);
    ASSERT_EQ(funcs2[0], RAN_FUNC_ID);

    // Stop and destroy agent 1. With a process-wide registry this cleared
    // agent 2's SM as collateral damage.
    agent1->stop();
    agent1.reset();

    ASSERT_TRUE(agent2.is_running());
    auto funcs2_after = agent2.get_available_ran_functions();
    ASSERT_EQ(funcs2_after.size(), 1u);
    ASSERT_EQ(funcs2_after[0], RAN_FUNC_ID);

    // Agent 2 must still SERVE: a real dApp-role agent performs the full
    // setup handshake against it and must be offered RAN function 1.
    E3Config dapp_cfg;
    dapp_cfg.role             = E3Role::DAPP;
    dapp_cfg.link_layer       = E3LinkLayer::ZMQ;
    dapp_cfg.transport_layer  = E3TransportLayer::IPC;
    dapp_cfg.setup_endpoint   = ep2.setup;
    dapp_cfg.subscriber_endpoint = ep2.sub;
    dapp_cfg.publisher_endpoint  = ep2.pub;
    dapp_cfg.encoding         = encoding;
    dapp_cfg.log_level        = 0;
    dapp_cfg.dapp_name        = "multireg-dapp";
    dapp_cfg.dapp_version     = "0.0.1";
    dapp_cfg.vendor           = "test-vendor";

    std::mutex resp_mu;
    std::vector<RanFunctionDef> offered;
    std::atomic<bool> got_positive{false};

    E3Agent dapp(std::move(dapp_cfg));
    dapp.set_setup_response_handler([&](const SetupResponse& resp) {
        std::lock_guard<std::mutex> lk(resp_mu);
        offered = resp.ran_function_list;
        if (resp.response_code == ResponseCode::POSITIVE) {
            got_positive.store(true);
        }
    });
    ASSERT_EQ(error_to_int(dapp.start()), error_to_int(ErrorCode::SUCCESS));
    ASSERT_EQ(error_to_int(dapp.wait_for_setup(10s)),
              error_to_int(ErrorCode::SUCCESS));
    ASSERT_TRUE(got_positive.load());
    {
        std::lock_guard<std::mutex> lk(resp_mu);
        bool found = false;
        for (const auto& rf : offered) {
            if (rf.ran_function_identifier == RAN_FUNC_ID) {
                found = true;
            }
        }
        ASSERT_TRUE(found);
    }

    dapp.stop();
    agent2.stop();
}

// ---------------------------------------------------------------------------

int main() {
    return RUN_ALL_TESTS();
}
