/**
 * @file test_role_aware_connector_posix.cpp
 * @brief Verify that PosixE3Connector's *_client methods establish
 *        connections via connect() (not bind/listen/accept).
 *
 * Covers IPC (AF_UNIX) and TCP (AF_INET) paths. SCTP is implemented in the
 * code but excluded from this test because ubuntu-latest CI runners lack
 * the sctp kernel module; the SCTP code path is exercised on hosts that
 * support it.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/types.hpp"
#include "libe3/e3_connector.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;

namespace {

std::string make_tmpdir() {
    char tmpl[] = "/tmp/libe3_posix_role_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

}  // namespace

TEST(posix_role_aware_connector_dapp_ipc_setup_request_response_roundtrip) {
    const std::string dir = make_tmpdir();
    const std::string setup    = "ipc://" + dir + "/setup";
    const std::string inbound  = "ipc://" + dir + "/dapp_socket";
    const std::string outbound = "ipc://" + dir + "/e3_socket";

    auto ran = create_connector(E3LinkLayer::POSIX, E3TransportLayer::IPC,
                                setup, inbound, outbound,
                                0, 0, 0, /*io_threads=*/1, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    auto dapp = create_connector(E3LinkLayer::POSIX, E3TransportLayer::IPC,
                                 setup, inbound, outbound,
                                 0, 0, 0, /*io_threads=*/1, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->role() == E3Role::DAPP);

    // Drive the recv from a side thread so the RAN's accept-then-recv
    // can complete after the dApp connects.
    std::vector<uint8_t> rx;
    std::thread ran_t([&]() {
        for (int i = 0; i < 20; ++i) {
            int n = ran->recv_setup_request(rx);
            if (n > 0) {
                std::vector<uint8_t> ack = {'O', 'K'};
                ran->send_response(ack);
                return;
            }
        }
    });

    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    std::vector<uint8_t> tx = {'p', 'i', 'n', 'g'};
    ASSERT_TRUE(dapp->send_setup_request_client(tx) == ErrorCode::SUCCESS);

    std::vector<uint8_t> resp;
    int n = 0;
    for (int i = 0; i < 20 && n == 0; ++i) {
        n = dapp->recv_setup_response_client(resp);
    }

    ran_t.join();
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(resp.size(), size_t{2});
    ASSERT_EQ(resp[0], static_cast<uint8_t>('O'));

    dapp->dispose();
    ran->dispose();
}

TEST(posix_role_aware_connector_dapp_tcp_setup_request_response_roundtrip) {
    // Pick a port unlikely to collide with system services.
    const uint16_t port = 23990;

    auto ran = create_connector(E3LinkLayer::POSIX, E3TransportLayer::TCP,
                                "", "", "",
                                port, port + 1, port + 2,
                                /*io_threads=*/1, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    auto dapp = create_connector(E3LinkLayer::POSIX, E3TransportLayer::TCP,
                                 "", "", "",
                                 port, port + 1, port + 2,
                                 /*io_threads=*/1, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);

    std::vector<uint8_t> rx;
    std::thread ran_t([&]() {
        for (int i = 0; i < 20; ++i) {
            int n = ran->recv_setup_request(rx);
            if (n > 0) {
                std::vector<uint8_t> ack = {'O', 'K'};
                ran->send_response(ack);
                return;
            }
        }
    });

    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    std::vector<uint8_t> tx = {'p', 'i', 'n', 'g'};
    ASSERT_TRUE(dapp->send_setup_request_client(tx) == ErrorCode::SUCCESS);

    std::vector<uint8_t> resp;
    int n = 0;
    for (int i = 0; i < 20 && n == 0; ++i) {
        n = dapp->recv_setup_response_client(resp);
    }

    ran_t.join();
    ASSERT_TRUE(n > 0);

    dapp->dispose();
    ran->dispose();
}

int main() {
    return RUN_ALL_TESTS();
}
