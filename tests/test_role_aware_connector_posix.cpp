/**
 * @file test_role_aware_connector_posix.cpp
 * @brief The POSIX connector serves a dApp on one endpoint, as one PDU per message.
 *
 * The connector is driven directly, with raw sockets where a test needs to see or
 * produce bytes. Covers IPC (a sequenced-packet socket) and TCP. SCTP is covered in
 * test_role_pair_sctp, which needs the sctp kernel module.
 *
 * The io_uring link speaks the same wire, so CTest runs this binary again with
 * --link uring, and with a RAN of one link and a dApp of the other (link_under_test.hpp).
 *
 * Properties verified:
 *   - setup request and response round trip on the one endpoint, IPC and TCP;
 *   - a final message sent just before a clean close is delivered, not lost;
 *   - TCP prefixes every PDU with its length in both directions, and two PDUs
 *     written together arrive as two;
 *   - IPC keeps the boundary of each PDU, and a PDU that does not fit is an error;
 *   - a peer that speaks the old layout (an unframed first message) is closed;
 *   - send_to reaches one peer only, send(0) all of them;
 *   - the peer list and the association cap close a connection at accept;
 *   - TCP works over IPv6 where the host has it.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "link_under_test.hpp"
#include "libe3/types.hpp"
#include "libe3/e3_connector.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
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

// One port per test: free when asked for, distinct across tests and across concurrent runs.
uint16_t unique_port() { return libe3_test::free_port(); }

std::unique_ptr<E3Connector> make(E3TransportLayer transport, const std::string& endpoint, uint16_t port,
                                  E3Role role, const PosixOptions& options = PosixOptions{}) {
    return create_connector(libe3_test::link_for(role), transport, endpoint, "", "", port, 0, 0, 1, role, options);
}

std::vector<uint8_t> bytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string text(const std::vector<uint8_t>& data) {
    return std::string(data.begin(), data.end());
}

std::vector<uint8_t> frame(const std::string& payload) {
    const uint32_t prefix = htonl(static_cast<uint32_t>(payload.size()));
    std::vector<uint8_t> out(sizeof(prefix) + payload.size());
    std::memcpy(out.data(), &prefix, sizeof(prefix));
    std::memcpy(out.data() + sizeof(prefix), payload.data(), payload.size());
    return out;
}

int raw_tcp(uint16_t port) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) return fd;
        close(fd);
        std::this_thread::sleep_for(20ms);
    }
    return -1;
}

int raw_ipc(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool send_raw(int fd, const std::vector<uint8_t>& data) {
    return ::send(fd, data.data(), data.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(data.size());
}

// What the RAN gets from `receive`, trying up to `tries` times (each waits up to half a second).
std::vector<uint8_t> receive_pdu(E3Connector& ran, int tries = 10) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        const int n = ran.receive(buf);
        if (n > 0) return buf;
    }
    return {};
}

std::vector<uint8_t> setup_request(E3Connector& ran, int tries = 10) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        const int n = ran.recv_setup_request(buf);
        if (n > 0) return buf;
    }
    return {};
}

// A dApp connector that has connected and had its setup request answered by `ran`.
std::unique_ptr<E3Connector> registered_dapp(E3Connector& ran, E3TransportLayer transport,
                                             const std::string& endpoint, uint16_t port,
                                             uint64_t* ran_token = nullptr) {
    auto dapp = make(transport, endpoint, port, E3Role::DAPP);
    if (dapp->setup_initial_connection_client() != ErrorCode::SUCCESS) return nullptr;
    if (dapp->send_setup_request_client(bytes("hello")) != ErrorCode::SUCCESS) return nullptr;
    if (setup_request(ran).empty()) return nullptr;
    if (ran_token) *ran_token = ran.last_setup_peer_token();
    if (ran.send_response(bytes("OK")) != ErrorCode::SUCCESS) return nullptr;
    std::vector<uint8_t> response;
    int n = 0;
    for (int i = 0; i < 10 && n == 0; ++i) n = dapp->recv_setup_response_client(response);
    return n == 2 ? std::move(dapp) : nullptr;
}

// A dApp's next PDU, trying up to `tries` times.
std::vector<uint8_t> dapp_receive(E3Connector& dapp, int tries = 4) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        const int n = dapp.receive(buf);
        if (n > 0) return buf;
        if (n < 0) return {};
    }
    return {};
}

void round_trip(E3TransportLayer transport, const std::string& endpoint, uint16_t port) {
    auto ran = make(transport, endpoint, port, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    // The inbound and outbound channels are the same connections: nothing more to open.
    ASSERT_TRUE(ran->setup_inbound_connection() == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran->setup_outbound_connection() == ErrorCode::SUCCESS);

    auto dapp = make(transport, endpoint, port, E3Role::DAPP);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->role() == E3Role::DAPP);

    std::vector<uint8_t> rx;
    std::thread ran_thread([&]() {
        for (int i = 0; i < 20; ++i) {
            if (ran->recv_setup_request(rx) > 0) {
                ran->send_response(bytes("OK"));
                return;
            }
        }
    });

    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->setup_inbound_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->setup_outbound_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->send_setup_request_client(bytes("ping")) == ErrorCode::SUCCESS);

    std::vector<uint8_t> resp;
    int n = 0;
    for (int i = 0; i < 20 && n == 0; ++i) {
        n = dapp->recv_setup_response_client(resp);
    }
    ran_thread.join();
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(text(resp), std::string("OK"));
    ASSERT_EQ(text(rx), std::string("ping"));

    // After setup, both directions use the same connection.
    ASSERT_TRUE(dapp->send(bytes("from-dapp")) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("from-dapp"));
    ASSERT_TRUE(ran->send(bytes("from-ran")) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*dapp)), std::string("from-ran"));

    dapp->dispose();
    ran->dispose();
}

}  // namespace

TEST(posix_one_endpoint_ipc_setup_request_response_roundtrip) {
    const std::string dir = make_tmpdir();
    round_trip(E3TransportLayer::IPC, "ipc://" + dir + "/e3", 0);
}

TEST(posix_one_endpoint_tcp_setup_request_response_roundtrip) {
    const uint16_t port = unique_port();
    round_trip(E3TransportLayer::TCP, "", port);
}

TEST(posix_ipc_default_endpoint_is_e3_not_the_zmq_setup_path) {
    // The ZeroMQ link shares E3Config's default endpoint string; the POSIX link answers on its own path.
    auto ran = make(E3TransportLayer::IPC, "ipc:///tmp/dapps/setup", 0, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    struct stat st{};
    ASSERT_EQ(stat("/tmp/dapps/e3", &st), 0);
    ASSERT_EQ(ran->setup_endpoint(), std::string("/tmp/dapps/e3"));
    ran->dispose();
    ASSERT_EQ(stat("/tmp/dapps/e3", &st), -1);  // the RAN removes the socket file it made
}

TEST(posix_ran_delivers_final_message_before_clean_close_ipc) {
    // Regression: on AF_UNIX a dApp that sends a final message (control, report,
    // release) and then closes shows up as POLLIN|POLLHUP on the same poll. The
    // RAN's receive() must deliver those bytes before dropping the peer.
    const std::string dir = make_tmpdir();
    auto ran = make(E3TransportLayer::IPC, "ipc://" + dir + "/e3", 0, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    int cli = raw_ipc(dir + "/e3");
    ASSERT_TRUE(cli >= 0);
    ASSERT_TRUE(send_raw(cli, bytes("SETUP")));
    ASSERT_TRUE(send_raw(cli, bytes("FINAL-CONTROL-MESSAGE")));
    close(cli);  // clean close right after the send

    ASSERT_EQ(text(setup_request(*ran)), std::string("SETUP"));
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("FINAL-CONTROL-MESSAGE"));

    // The closed connection is reported once its bytes were delivered.
    std::vector<uint64_t> closed;
    for (int i = 0; i < 10 && closed.empty(); ++i) {
        std::vector<uint8_t> buf;
        ran->receive(buf);
        closed = ran->take_closed_peers();
    }
    ASSERT_EQ(closed.size(), size_t{1});
    ran->dispose();
}

TEST(posix_tcp_prefixes_every_pdu_in_both_directions) {
    const uint16_t port = unique_port();

    // dApp to RAN: the connector writes a prefix before the PDU.
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(bind(listener, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(listen(listener, 1), 0);

    auto dapp = make(E3TransportLayer::TCP, "", port, E3Role::DAPP);
    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    int accepted = accept(listener, nullptr, nullptr);
    ASSERT_TRUE(accepted >= 0);
    ASSERT_TRUE(dapp->send_setup_request_client(bytes("ping")) == ErrorCode::SUCCESS);
    uint8_t got[8] = {};
    size_t have = 0;
    while (have < sizeof(got)) {
        const ssize_t n = ::recv(accepted, got + have, sizeof(got) - have, 0);
        ASSERT_TRUE(n > 0);
        have += static_cast<size_t>(n);
    }
    const uint8_t expected[8] = {0, 0, 0, 4, 'p', 'i', 'n', 'g'};
    ASSERT_TRUE(std::memcmp(got, expected, sizeof(got)) == 0);

    // RAN to dApp: a framed answer, read by the connector.
    ASSERT_TRUE(send_raw(accepted, frame("OK")));
    std::vector<uint8_t> response;
    int n = 0;
    for (int i = 0; i < 10 && n == 0; ++i) n = dapp->recv_setup_response_client(response);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(text(response), std::string("OK"));

    close(accepted);
    close(listener);
    dapp->dispose();
}

TEST(posix_tcp_two_pdus_written_together_arrive_as_two) {
    const uint16_t port = unique_port();
    auto ran = make(E3TransportLayer::TCP, "", port, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    int cli = raw_tcp(port);
    ASSERT_TRUE(cli >= 0);
    ASSERT_TRUE(send_raw(cli, frame("SETUP")));
    ASSERT_EQ(text(setup_request(*ran)), std::string("SETUP"));

    std::vector<uint8_t> both = frame("FIRST");
    const std::vector<uint8_t> second = frame("SECOND");
    both.insert(both.end(), second.begin(), second.end());
    ASSERT_TRUE(send_raw(cli, both));  // one write, two PDUs

    ASSERT_EQ(text(receive_pdu(*ran)), std::string("FIRST"));
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("SECOND"));

    // A PDU split over two writes arrives whole.
    const std::vector<uint8_t> third = frame("THIRD");
    ASSERT_TRUE(send_raw(cli, std::vector<uint8_t>(third.begin(), third.begin() + 6)));
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(send_raw(cli, std::vector<uint8_t>(third.begin() + 6, third.end())));
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("THIRD"));

    close(cli);
    ran->dispose();
}

TEST(posix_ipc_keeps_the_boundary_of_each_pdu_and_refuses_one_that_does_not_fit) {
    const std::string dir = make_tmpdir();
    auto ran = make(E3TransportLayer::IPC, "ipc://" + dir + "/e3", 0, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    int cli = raw_ipc(dir + "/e3");
    ASSERT_TRUE(cli >= 0);
    ASSERT_TRUE(send_raw(cli, bytes("SETUP")));
    ASSERT_EQ(text(setup_request(*ran)), std::string("SETUP"));

    // Written back to back, read one at a time.
    ASSERT_TRUE(send_raw(cli, bytes("A")));
    ASSERT_TRUE(send_raw(cli, bytes("BB")));
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("A"));
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("BB"));

    // One byte more than the limit is an error, not a silently cut message: the peer is dropped.
    ASSERT_TRUE(send_raw(cli, std::vector<uint8_t>(MAX_PROTOCOL_DATA_SIZE * 2 + 1, 'x')));
    std::vector<uint64_t> closed;
    for (int i = 0; i < 10 && closed.empty(); ++i) {
        std::vector<uint8_t> buf;
        ASSERT_TRUE(ran->receive(buf) <= 0);
        closed = ran->take_closed_peers();
    }
    ASSERT_EQ(closed.size(), size_t{1});

    close(cli);
    ran->dispose();
}

TEST(posix_tcp_peer_with_the_old_layout_is_closed_and_the_next_dapp_is_served) {
    const uint16_t port = unique_port();
    auto ran = make(E3TransportLayer::TCP, "", port, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    // An old dApp sends its setup request without a length prefix.
    int old_peer = raw_tcp(port);
    ASSERT_TRUE(old_peer >= 0);
    ASSERT_TRUE(send_raw(old_peer, bytes("an unframed setup request")));
    std::vector<uint8_t> none;
    ASSERT_TRUE(ran->recv_setup_request(none) <= 0);
    struct timeval timeout{2, 0};
    setsockopt(old_peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    char byte;
    ASSERT_EQ(::recv(old_peer, &byte, 1, 0), 0);  // the RAN closed it
    close(old_peer);

    auto dapp = registered_dapp(*ran, E3TransportLayer::TCP, "", port);
    ASSERT_TRUE(dapp != nullptr);
    dapp->dispose();
    ran->dispose();
}

TEST(posix_send_to_reaches_one_peer_and_zero_reaches_all) {
    const std::string dir = make_tmpdir();
    const std::string endpoint = "ipc://" + dir + "/e3";
    auto ran = make(E3TransportLayer::IPC, endpoint, 0, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    uint64_t first_token = 0, second_token = 0;
    auto first = registered_dapp(*ran, E3TransportLayer::IPC, endpoint, 0, &first_token);
    auto second = registered_dapp(*ran, E3TransportLayer::IPC, endpoint, 0, &second_token);
    ASSERT_TRUE(first != nullptr);
    ASSERT_TRUE(second != nullptr);
    ASSERT_TRUE(first_token != 0);
    ASSERT_TRUE(second_token != 0);
    ASSERT_TRUE(first_token != second_token);

    ASSERT_TRUE(ran->send_to(first_token, bytes("ONLY-FIRST"), PduClass::PROCEDURE) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*first)), std::string("ONLY-FIRST"));
    ASSERT_TRUE(dapp_receive(*second, 2).empty());

    ASSERT_TRUE(ran->send_to(second_token, bytes("ONLY-SECOND"), PduClass::INDICATION) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*second)), std::string("ONLY-SECOND"));
    ASSERT_TRUE(dapp_receive(*first, 2).empty());

    ASSERT_TRUE(ran->send_to(0, bytes("EVERYONE"), PduClass::PROCEDURE) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*first)), std::string("EVERYONE"));
    ASSERT_EQ(text(dapp_receive(*second)), std::string("EVERYONE"));

    // A peer that is gone is not a broadcast.
    first->dispose();
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> buf;
        ran->receive(buf);
        if (!ran->take_closed_peers().empty()) break;
    }
    ASSERT_TRUE(ran->send_to(first_token, bytes("GONE"), PduClass::PROCEDURE) != ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp_receive(*second, 2).empty());

    second->dispose();
    ran->dispose();
}

TEST(posix_tcp_peer_outside_the_allowed_list_is_closed_at_accept) {
    const uint16_t port = unique_port();
    PosixOptions options;
    options.allowed_peers = {"203.0.113.9"};  // not the loopback address a test connects from
    auto ran = make(E3TransportLayer::TCP, "", port, E3Role::RAN, options);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    auto dapp = make(E3TransportLayer::TCP, "", port, E3Role::DAPP);
    ASSERT_TRUE(dapp->setup_initial_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->send_setup_request_client(bytes("hello")) == ErrorCode::SUCCESS);
    std::vector<uint8_t> none;
    ASSERT_TRUE(ran->recv_setup_request(none) <= 0);  // accepts, then closes it
    std::vector<uint8_t> response;
    int n = 0;
    for (int i = 0; i < 10 && n == 0; ++i) n = dapp->recv_setup_response_client(response);
    ASSERT_TRUE(n < 0);  // the connection is closed
    dapp->dispose();
    ran->dispose();

    // The list that names it lets it in.
    const uint16_t port2 = unique_port();
    options.allowed_peers = {"127.0.0.1"};
    auto ran2 = make(E3TransportLayer::TCP, "", port2, E3Role::RAN, options);
    ASSERT_TRUE(ran2->setup_initial_connection() == ErrorCode::SUCCESS);
    auto dapp2 = registered_dapp(*ran2, E3TransportLayer::TCP, "", port2);
    ASSERT_TRUE(dapp2 != nullptr);
    dapp2->dispose();
    ran2->dispose();
}

TEST(posix_tcp_connection_beyond_the_cap_is_closed_and_the_first_stays) {
    const uint16_t port = unique_port();
    PosixOptions options;
    options.maximum_associations = 1;
    auto ran = make(E3TransportLayer::TCP, "", port, E3Role::RAN, options);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    uint64_t token = 0;
    auto first = registered_dapp(*ran, E3TransportLayer::TCP, "", port, &token);
    ASSERT_TRUE(first != nullptr);

    auto second = make(E3TransportLayer::TCP, "", port, E3Role::DAPP);
    ASSERT_TRUE(second->setup_initial_connection_client() == ErrorCode::SUCCESS);
    ASSERT_TRUE(second->send_setup_request_client(bytes("hello")) == ErrorCode::SUCCESS);
    std::vector<uint8_t> none;
    ASSERT_TRUE(ran->recv_setup_request(none) <= 0);  // accepts the second, finds the cap reached, closes it
    std::vector<uint8_t> response;
    int n = 0;
    for (int i = 0; i < 10 && n == 0; ++i) n = second->recv_setup_response_client(response);
    ASSERT_TRUE(n < 0);

    ASSERT_TRUE(ran->send_to(token, bytes("STILL-THERE"), PduClass::PROCEDURE) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*first)), std::string("STILL-THERE"));

    second->dispose();
    first->dispose();
    ran->dispose();
}

TEST(posix_tcp_over_ipv6) {
    // Needs an IPv6 loopback: skip on a host without one.
    int probe = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 loop{};
    loop.sin6_family = AF_INET6;
    loop.sin6_addr = in6addr_loopback;
    if (probe < 0 || bind(probe, reinterpret_cast<struct sockaddr*>(&loop), sizeof(loop)) != 0) {
        if (probe >= 0) close(probe);
        std::cout << "SKIP: no IPv6 loopback on this host\n";
        return;
    }
    close(probe);

    const uint16_t port = unique_port();
    auto ran = make(E3TransportLayer::TCP, "", port, E3Role::RAN);  // every address, both families
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    auto dapp = registered_dapp(*ran, E3TransportLayer::TCP, "tcp://[::1]:" + std::to_string(port), 0);
    ASSERT_TRUE(dapp != nullptr);
    dapp->dispose();

    // The same RAN also serves IPv4.
    auto v4 = registered_dapp(*ran, E3TransportLayer::TCP, "tcp://127.0.0.1:" + std::to_string(port), 0);
    ASSERT_TRUE(v4 != nullptr);
    v4->dispose();
    ran->dispose();
}

int main(int argc, char** argv) {
    if (const int rc = libe3_test::select_links(argc, argv); rc >= 0) return rc;
    return RUN_ALL_TESTS();
}
