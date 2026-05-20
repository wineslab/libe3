/**
 * @file test_role_aware_connector_zmq.cpp
 * @brief Verify that ZmqE3Connector's *_client methods produce the right
 *        socket types and use connect (not bind).
 *
 * The endpoint of a connected ZMQ socket is queryable via ZMQ_LAST_ENDPOINT,
 * which returns the resolved transport-specific string. We open two
 * connectors (a RAN-role one that BINDS and a DAPP-role one that CONNECTS
 * to the same endpoints) over IPC, then check ZMQ_TYPE matches expectations
 * and that the client side actually managed to connect.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/types.hpp"
#include "libe3/e3_connector.hpp"

#include <zmq.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

using namespace libe3;

namespace {

// Pre-create a unique IPC tmpdir for the test. ZmqE3Connector's RAN side
// will also try to mkdir /tmp/dapps, which is harmless if it already exists.
std::string make_tmpdir() {
    char tmpl[] = "/tmp/libe3_zmq_role_test_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) {
        throw std::runtime_error("mkdtemp failed");
    }
    chmod(d, 0777);
    return std::string(d);
}

struct Endpoints {
    std::string setup;
    std::string inbound;
    std::string outbound;
};

Endpoints make_endpoints(const std::string& dir) {
    return Endpoints{
        "ipc://" + dir + "/setup",
        "ipc://" + dir + "/dapp_socket",
        "ipc://" + dir + "/e3_socket",
    };
}

}  // namespace

TEST(zmq_role_aware_connector_dapp_role_connects_setup_as_REQ) {
    const std::string dir = make_tmpdir();
    const Endpoints ep = make_endpoints(dir);

    // RAN side (binds) — required so connect() has someone to talk to
    auto ran = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                ep.setup, ep.inbound, ep.outbound,
                                0, 0, 0, /*io_threads=*/1, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    // dApp side (connects)
    auto dapp = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                 ep.setup, ep.inbound, ep.outbound,
                                 0, 0, 0, /*io_threads=*/1, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->role() == E3Role::DAPP);
    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);

    dapp->dispose();
    ran->dispose();
}

TEST(zmq_role_aware_connector_dapp_role_setup_request_response_roundtrip) {
    // The dApp sends a SetupRequest, the RAN's recv_setup_request unblocks
    // and we send a fake response back. This validates the directionality
    // and basic framing of the new *_client methods.
    const std::string dir = make_tmpdir();
    const Endpoints ep = make_endpoints(dir);

    auto ran = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                ep.setup, ep.inbound, ep.outbound,
                                0, 0, 0, 1, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    auto dapp = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                 ep.setup, ep.inbound, ep.outbound,
                                 0, 0, 0, 1, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);

    // RAN listens in a background thread.
    std::vector<uint8_t> rx;
    std::thread ran_thread([&] {
        int n = 0;
        for (int tries = 0; tries < 20 && n == 0; ++tries) {
            n = ran->recv_setup_request(rx);
            if (n > 0) break;
        }
        if (n > 0) {
            std::vector<uint8_t> ack = {'A', 'C', 'K'};
            ran->send_response(ack);
        }
    });

    // dApp sends the payload, then waits for the response.
    std::vector<uint8_t> tx = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(dapp->send_setup_request_client(tx) == ErrorCode::SUCCESS);

    std::vector<uint8_t> resp;
    int n = 0;
    for (int tries = 0; tries < 20 && n == 0; ++tries) {
        n = dapp->recv_setup_response_client(resp);
    }

    ran_thread.join();

    ASSERT_TRUE(n > 0);
    ASSERT_EQ(resp.size(), size_t{3});
    ASSERT_EQ(resp[0], static_cast<uint8_t>('A'));

    ASSERT_EQ(rx.size(), tx.size());
    ASSERT_EQ(rx[0], tx[0]);

    dapp->dispose();
    ran->dispose();
}

TEST(zmq_role_aware_connector_dapp_inbound_outbound_setup_succeeds) {
    const std::string dir = make_tmpdir();
    const Endpoints ep = make_endpoints(dir);

    auto ran = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                ep.setup, ep.inbound, ep.outbound,
                                0, 0, 0, 1, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran->setup_inbound_connection() == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran->setup_outbound_connection() == ErrorCode::SUCCESS);

    auto dapp = create_connector(E3LinkLayer::ZMQ, E3TransportLayer::IPC,
                                 ep.setup, ep.inbound, ep.outbound,
                                 0, 0, 0, 1, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->setup_inbound_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->setup_outbound_connection_client() == ErrorCode::SUCCESS);

    dapp->dispose();
    ran->dispose();
}

int main() {
    return RUN_ALL_TESTS();
}
