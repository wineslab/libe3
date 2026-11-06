/**
 * @file test_dapp_bound_to_connection_posix.cpp
 * @brief A POSIX RAN acts on a message that names a dApp only if it came on that dApp's connection.
 *
 * A real RAN E3Agent and a real dApp E3Agent over POSIX IPC, and a second,
 * bare connection that plays another dApp. Properties verified:
 *   (1) a release that names the dApp but comes on another connection is
 *       dropped: the dApp keeps its registration, its subscription and its
 *       RAN function, and is told nothing;
 *   (2) the same goes for a subscription delete that names it;
 *   (3) the dApp's own messages still work afterwards: it can subscribe again;
 *   (4) a message from the dApp's own connection can still release it.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/libe3.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/e3_connector.hpp"
#include "libe3/e3_encoder.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

constexpr uint32_t FIRST_RF = 100;
constexpr uint32_t SECOND_RF = 101;

std::string unique_ipc(const char* tag) {
    static std::atomic<int> seq{0};
    std::ostringstream oss;
    oss << "ipc:///tmp/dapps/bound_test_" << getpid() << "_" << seq.fetch_add(1) << "_" << tag;
    return oss.str();
}

EncodingFormat pick_encoding() {
#if defined(LIBE3_ENABLE_JSON)
    return EncodingFormat::JSON;
#else
    return EncodingFormat::ASN1;
#endif
}

class CountingSM : public ServiceModel {
public:
    explicit CountingSM(uint32_t rf) : rf_(rf) {}
    std::string name() const override { return "CountingSM"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return rf_; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {}; }
    // The JSON encoder carries ranFunctionData as a JSON value, so it must be one.
    std::vector<uint8_t> ran_function_data() const override { return {'{', '}'}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { running_ = false; }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; ++stops; }
    bool is_running() const override { return running_; }
    ErrorCode handle_control_action(uint32_t, const DAppControlAction&) override {
        return ErrorCode::SUCCESS;
    }

    std::atomic<int> stops{0};

private:
    uint32_t rf_;
    std::atomic<bool> running_{false};
};

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

struct Endpoints {
    std::string setup = unique_ipc("setup");
    std::string sub = unique_ipc("sub");
    std::string pub = unique_ipc("pub");
};

E3Config make_config(E3Role role, const Endpoints& ep) {
    E3Config c;
    c.role                = role;
    c.link_layer          = E3LinkLayer::POSIX;
    c.transport_layer     = E3TransportLayer::IPC;
    c.setup_endpoint      = ep.setup;
    c.subscriber_endpoint = ep.sub;
    c.publisher_endpoint  = ep.pub;
    c.encoding            = pick_encoding();
    c.log_level           = 0;
    c.ran_identifier      = "bound-test";
    c.dapp_name           = "BoundDApp";
    c.dapp_version        = "1.0.0";
    c.dapp_idle_timeout_s = 0;
    return c;
}

/// A second connection to the RAN's inbound side that sends what it is told to.
struct OtherConnection {
    explicit OtherConnection(const Endpoints& ep)
        : encoder(create_encoder(pick_encoding())),
          connector(create_connector(E3LinkLayer::POSIX, E3TransportLayer::IPC, ep.setup, ep.sub,
                                     ep.pub, 0, 0, 0, 1, E3Role::DAPP)) {}

    bool open() { return connector && connector->setup_outbound_connection_client() == ErrorCode::SUCCESS; }

    bool send(Pdu pdu) {
        pdu.message_id = ++next_message_id;
        auto encoded = encoder->encode(pdu);
        return encoded && connector->send(encoded->buffer) == ErrorCode::SUCCESS;
    }

    std::unique_ptr<E3Encoder> encoder;
    std::unique_ptr<E3Connector> connector;
    uint32_t next_message_id{1000};
};

Pdu release_naming(uint32_t dapp_id) {
    Pdu pdu(PduType::RELEASE_MESSAGE);
    ReleaseMessage release;
    release.dapp_identifier = dapp_id;
    pdu.choice = release;
    return pdu;
}

Pdu delete_naming(uint32_t dapp_id, uint32_t subscription_id) {
    Pdu pdu(PduType::SUBSCRIPTION_DELETE);
    SubscriptionDelete del;
    del.dapp_identifier = dapp_id;
    del.subscription_id = subscription_id;
    pdu.choice = del;
    return pdu;
}

}  // namespace

TEST(DAppBound_posix_aMessageFromAnotherConnectionIsDropped) {
    mkdir("/tmp/dapps", 0777);  // EEXIST is fine
    const Endpoints ep;
    E3Agent ran(make_config(E3Role::RAN, ep));
    auto first_sm = std::make_unique<CountingSM>(FIRST_RF);
    auto second_sm = std::make_unique<CountingSM>(SECOND_RF);
    CountingSM* first = first_sm.get();
    CountingSM* second = second_sm.get();
    ran.register_sm(std::move(first_sm));
    ran.register_sm(std::move(second_sm));
    ASSERT_EQ(static_cast<int>(ran.start()), static_cast<int>(ErrorCode::SUCCESS));
    std::this_thread::sleep_for(100ms);

    E3Agent dapp(make_config(E3Role::DAPP, ep));
    std::atomic<int> disconnects{0};
    dapp.set_disconnect_handler([&](DisconnectReason) { ++disconnects; });
    ASSERT_EQ(static_cast<int>(dapp.start()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_EQ(static_cast<int>(dapp.wait_for_setup(15000ms)), static_cast<int>(ErrorCode::SUCCESS));
    const uint32_t id = *dapp.dapp_id();

    // Its first message binds it to its own connection.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(FIRST_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 1u; }, 15000ms));
    ASSERT_TRUE(first->is_running());

    OtherConnection other(ep);
    ASSERT_TRUE(other.open());

    // (1) and (2) From the other connection: neither takes effect.
    // One message per read: the RAN reads this link as a stream and decodes what one read returns.
    ASSERT_TRUE(other.send(release_naming(id)));
    std::this_thread::sleep_for(300ms);
    ASSERT_TRUE(other.send(delete_naming(id, 1)));  // the first subscription the RAN granted
    std::this_thread::sleep_for(1500ms);
    ASSERT_EQ(ran.dapp_count(), 1u);
    ASSERT_EQ(ran.subscription_count(), 1u);
    ASSERT_EQ(ran.get_ran_function_subscribers(FIRST_RF).size(), 1u);
    ASSERT_TRUE(first->is_running());
    ASSERT_EQ(first->stops.load(), 0);
    ASSERT_EQ(disconnects.load(), 0);
    ASSERT_TRUE(dapp.dapp_id().has_value());

    // (3) The dApp's own connection is not affected.
    ASSERT_EQ(static_cast<int>(dapp.subscribe(SECOND_RF, {1}, {})), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.subscription_count() == 2u; }, 15000ms));
    ASSERT_TRUE(second->is_running());

    // (4) And it can still release itself.
    ASSERT_EQ(static_cast<int>(dapp.release()), static_cast<int>(ErrorCode::SUCCESS));
    ASSERT_TRUE(wait_until([&] { return ran.dapp_count() == 0u; }, 10000ms));
    ASSERT_TRUE(wait_until([&] { return !first->is_running() && !second->is_running(); }, 10000ms));

    dapp.stop();
    ran.stop();
}

int main() {
    return RUN_ALL_TESTS();
}
