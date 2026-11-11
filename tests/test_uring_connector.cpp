/**
 * @file test_uring_connector.cpp
 * @brief What only the io_uring link does: its ring options, batched sends, and how it stops.
 *
 * The wire it shares with the POSIX link is tested by running the POSIX tests with --link uring
 * (test_role_aware_connector_posix and the others listed in cmake/libe3Tests.cmake). Here the
 * connector is driven directly. Properties verified:
 *   - every ring variant (plain, registered files, registered buffers, a kernel submission
 *     thread, busy polling, and all of them) carries a setup and both directions on IPC, TCP and
 *     SCTP, and the variant asked for is the one in effect where the kernel allows it;
 *   - a broadcast to more peers than the ring has entries reaches every peer, once and in order;
 *   - a peer that stops reading does not delay what the others receive: they have each PDU
 *     before the send to the stalled one gives up, and the stalled one is then closed;
 *   - a peer that resets its connection is dropped alone and reported closed once;
 *   - thousands of PDUs of every size up to the limit arrive whole and in order, both ways;
 *   - a PDU of the limit passes and one byte more is refused before it is sent;
 *   - receive() waits about its half second and returns 0; shutdown() wakes a receive() that
 *     is waiting, a send after it is CANCELLED, and dispose() can be called twice.
 *
 * Needs a kernel that lets this process use io_uring; otherwise exits 77 (skipped) unless
 * LIBE3_REQUIRE_URING is set.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "link_under_test.hpp"
#include "libe3/e3_connector.hpp"
#include "libe3/types.hpp"
#include "connector/uring_connector.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

using namespace libe3;
using namespace std::chrono_literals;
using clk = std::chrono::steady_clock;

namespace {

constexpr size_t LIMIT = MAX_PROTOCOL_DATA_SIZE * 2;

std::string make_tmpdir() {
    char tmpl[] = "/tmp/libe3_uring_XXXXXX";
    char* d = mkdtemp(tmpl);
    if (!d) throw std::runtime_error("mkdtemp failed");
    chmod(d, 0777);
    return std::string(d);
}

uint16_t unique_port() { return libe3_test::free_port(); }

bool sctp_here() {
#ifdef LIBE3_ENABLE_SCTP
    int probe = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (probe < 0) return false;
    close(probe);
    return true;
#else
    return false;
#endif
}

/// Sets the ring options for the connectors made while it lives; the connector reads them once.
class RingOptions {
public:
    explicit RingOptions(std::map<std::string, std::string> values) : values_(std::move(values)) {
        for (const auto& [name, value] : values_) setenv(name.c_str(), value.c_str(), 1);
    }
    ~RingOptions() {
        for (const auto& [name, value] : values_) unsetenv(name.c_str());
    }

private:
    std::map<std::string, std::string> values_;
};

struct Endpoint {
    E3TransportLayer transport;
    std::string uri;
    uint16_t port;
};

Endpoint endpoint_for(E3TransportLayer transport) {
    if (transport == E3TransportLayer::IPC) return {transport, "ipc://" + make_tmpdir() + "/e3", 0};
    return {transport, "", unique_port()};
}

std::unique_ptr<E3Connector> make(const Endpoint& where, E3Role role) {
    return create_connector(E3LinkLayer::URING, where.transport, where.uri, "", "", where.port, 0, 0, 1, role);
}

std::vector<uint8_t> bytes(const std::string& text) { return {text.begin(), text.end()}; }
std::string text(const std::vector<uint8_t>& data) { return {data.begin(), data.end()}; }

std::vector<uint8_t> receive_pdu(E3Connector& ran, int tries = 10) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        if (ran.receive(buf) > 0) return buf;
    }
    return {};
}

std::vector<uint8_t> dapp_receive(E3Connector& dapp, int tries = 4) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        const int n = dapp.receive(buf);
        if (n > 0) return buf;
        if (n < 0) return {};
    }
    return {};
}

std::vector<uint8_t> setup_request(E3Connector& ran, int tries = 10) {
    for (int i = 0; i < tries; ++i) {
        std::vector<uint8_t> buf;
        if (ran.recv_setup_request(buf) > 0) return buf;
    }
    return {};
}

std::unique_ptr<E3Connector> registered_dapp(E3Connector& ran, const Endpoint& where, uint64_t* token = nullptr) {
    auto dapp = make(where, E3Role::DAPP);
    if (!dapp || dapp->setup_initial_connection_client() != ErrorCode::SUCCESS) return nullptr;
    if (dapp->send_setup_request_client(bytes("hello")) != ErrorCode::SUCCESS) return nullptr;
    if (setup_request(ran).empty()) return nullptr;
    if (token) *token = ran.last_setup_peer_token();
    if (ran.send_response(bytes("OK")) != ErrorCode::SUCCESS) return nullptr;
    std::vector<uint8_t> response;
    int n = 0;
    for (int i = 0; i < 10 && n == 0; ++i) n = dapp->recv_setup_response_client(response);
    return n == 2 ? std::move(dapp) : nullptr;
}

std::vector<uint8_t> patterned(size_t size, uint32_t seed) {
    std::vector<uint8_t> out(size);
    for (size_t i = 0; i < size; ++i) out[i] = static_cast<uint8_t>((i * 131 + seed * 7 + (i >> 8)) & 0xFF);
    return out;
}

/// Setup, then each direction, then a broadcast to two dApps, on one transport.
void exchange_on(E3TransportLayer transport) {
    const Endpoint where = endpoint_for(transport);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran != nullptr);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    uint64_t first_token = 0;
    auto first = registered_dapp(*ran, where, &first_token);
    auto second = registered_dapp(*ran, where);
    ASSERT_TRUE(first != nullptr);
    ASSERT_TRUE(second != nullptr);

    ASSERT_TRUE(first->send(bytes("from-first")) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("from-first"));
    ASSERT_EQ(ran->last_peer_token(), first_token);
    ASSERT_TRUE(ran->send_to(first_token, bytes("to-first"), PduClass::INDICATION) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*first)), std::string("to-first"));
    ASSERT_TRUE(ran->send(bytes("to-all")) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*first)), std::string("to-all"));
    ASSERT_EQ(text(dapp_receive(*second)), std::string("to-all"));

    second->dispose();
    first->dispose();
    ran->dispose();
}

void variant(const std::map<std::string, std::string>& options,
             const std::function<void(const UringE3Connector::Settings&)>& expect) {
    RingOptions set(options);
    auto probe = make({E3TransportLayer::TCP, "", unique_port()}, E3Role::DAPP);
    ASSERT_TRUE(probe != nullptr);
    auto* uring = dynamic_cast<UringE3Connector*>(probe.get());
    ASSERT_TRUE(uring != nullptr);
    std::cout << "      in effect: " << uring->settings().describe() << "\n";
    expect(uring->settings());
    probe.reset();

    exchange_on(E3TransportLayer::IPC);
    exchange_on(E3TransportLayer::TCP);
    if (sctp_here()) {
        exchange_on(E3TransportLayer::SCTP);
    } else {
        std::cout << "      SCTP NOT RUN: no sctp kernel module or built without it\n";
    }
}

int raw_tcp(uint16_t port, int rcvbuf = 0) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (rcvbuf > 0) setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
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

bool send_framed(int fd, const std::string& payload) {
    const uint32_t prefix = htonl(static_cast<uint32_t>(payload.size()));
    std::vector<uint8_t> out(sizeof(prefix) + payload.size());
    std::memcpy(out.data(), &prefix, sizeof(prefix));
    std::memcpy(out.data() + sizeof(prefix), payload.data(), payload.size());
    return ::send(fd, out.data(), out.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(out.size());
}

}  // namespace

TEST(uring_plain_ring_carries_every_transport) {
    variant({{"LIBE3_URING_FIXED_FILES", "0"}, {"LIBE3_URING_FIXED_BUFFERS", "0"}},
            [](const UringE3Connector::Settings& s) {
                ASSERT_FALSE(s.fixed_files);
                ASSERT_FALSE(s.fixed_buffers);
                ASSERT_FALSE(s.sqpoll);
                ASSERT_EQ(s.busy_poll_us, 0u);
            });
}

TEST(uring_registered_files_carry_every_transport) {
    variant({{"LIBE3_URING_FIXED_FILES", "1"}, {"LIBE3_URING_FIXED_BUFFERS", "0"}},
            [](const UringE3Connector::Settings& s) { ASSERT_TRUE(s.fixed_files); });
}

TEST(uring_registered_buffers_carry_every_transport) {
    variant({{"LIBE3_URING_FIXED_FILES", "0"}, {"LIBE3_URING_FIXED_BUFFERS", "1"}},
            [](const UringE3Connector::Settings& s) { ASSERT_TRUE(s.fixed_buffers); });
}

TEST(uring_kernel_submission_thread_carries_every_transport) {
    variant({{"LIBE3_URING_SQPOLL", "1"}},
            [](const UringE3Connector::Settings& s) { ASSERT_TRUE(s.sqpoll); });
}

TEST(uring_busy_polling_carries_every_transport) {
    variant({{"LIBE3_URING_BUSY_POLL_US", "200"}},
            [](const UringE3Connector::Settings& s) { ASSERT_EQ(s.busy_poll_us, 200u); });
}

TEST(uring_everything_at_once_carries_every_transport) {
    variant({{"LIBE3_URING_FIXED_FILES", "1"}, {"LIBE3_URING_FIXED_BUFFERS", "1"},
             {"LIBE3_URING_SQPOLL", "1"}, {"LIBE3_URING_BUSY_POLL_US", "1000"}},
            [](const UringE3Connector::Settings& s) {
                ASSERT_TRUE(s.fixed_files);
                ASSERT_TRUE(s.fixed_buffers);
                ASSERT_TRUE(s.sqpoll);
                ASSERT_EQ(s.busy_poll_us, 1000u);
            });
}

TEST(uring_a_ring_option_that_makes_no_sense_is_refused_with_the_default_kept) {
    RingOptions set({{"LIBE3_URING_DEPTH", "banana"}, {"LIBE3_URING_BUSY_POLL_US", "-3"}});
    auto conn = make({E3TransportLayer::TCP, "", unique_port()}, E3Role::DAPP);
    ASSERT_TRUE(conn != nullptr);
    const auto& s = dynamic_cast<UringE3Connector*>(conn.get())->settings();
    ASSERT_EQ(s.depth, UringE3Connector::Settings{}.depth);
    ASSERT_EQ(s.busy_poll_us, 0u);
}

TEST(uring_broadcast_to_more_peers_than_ring_entries_reaches_each_once_in_order) {
    RingOptions set({{"LIBE3_URING_DEPTH", "4"}});
    const Endpoint where = endpoint_for(E3TransportLayer::TCP);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    std::vector<std::unique_ptr<E3Connector>> dapps;
    for (int i = 0; i < 11; ++i) {
        auto dapp = registered_dapp(*ran, where);
        ASSERT_TRUE(dapp != nullptr);
        dapps.push_back(std::move(dapp));
    }
    for (int round = 0; round < 3; ++round) {
        ASSERT_TRUE(ran->send(bytes("round-" + std::to_string(round))) == ErrorCode::SUCCESS);
    }
    for (auto& dapp : dapps) {
        for (int round = 0; round < 3; ++round) {
            ASSERT_EQ(text(dapp_receive(*dapp)), "round-" + std::to_string(round));
        }
        ASSERT_TRUE(dapp_receive(*dapp, 1).empty());
    }
    for (auto& dapp : dapps) dapp->dispose();
    ran->dispose();
}

TEST(uring_a_stalled_peer_does_not_delay_the_others) {
    const Endpoint where = endpoint_for(E3TransportLayer::TCP);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    // First in the accept order, so a sender that went peer by peer would wait for it first.
    int stalled = raw_tcp(where.port, 2048);
    ASSERT_TRUE(stalled >= 0);
    ASSERT_TRUE(send_framed(stalled, "SETUP"));
    ASSERT_EQ(text(setup_request(*ran)), std::string("SETUP"));
    const uint64_t stalled_token = ran->last_setup_peer_token();
    ASSERT_TRUE(ran->send_response(bytes("OK")) == ErrorCode::SUCCESS);

    uint64_t good_token = 0;
    auto good = registered_dapp(*ran, where, &good_token);
    ASSERT_TRUE(good != nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> received{0};
    std::vector<clk::time_point> arrivals;
    std::mutex arrivals_mu;
    std::thread reader([&] {
        while (!stop.load()) {
            std::vector<uint8_t> buf;
            if (good->receive(buf) > 0) {
                std::lock_guard<std::mutex> lk(arrivals_mu);
                arrivals.push_back(clk::now());
                received.fetch_add(1);
            }
        }
    });

    // Broadcast until a send has to give up on the stalled peer.
    const std::vector<uint8_t> chunk(16384, 0x5A);
    clk::time_point slow_start{}, slow_end{};
    int sent = 0;
    for (; sent < 4000; ++sent) {
        const auto t0 = clk::now();
        const ErrorCode rc = ran->send(chunk);
        const auto t1 = clk::now();
        ASSERT_TRUE(rc == ErrorCode::SUCCESS);  // the good peer took it
        if (t1 - t0 > 300ms) {
            slow_start = t0;
            slow_end = t1;
            ++sent;
            break;
        }
    }
    ASSERT_TRUE(slow_end != clk::time_point{});
    for (int i = 0; i < 100 && received.load() < sent; ++i) std::this_thread::sleep_for(10ms);
    stop.store(true);
    reader.join();
    ASSERT_EQ(received.load(), sent);
    {
        std::lock_guard<std::mutex> lk(arrivals_mu);
        // The PDU of the send that waited for the stalled peer reached the good one long before that send returned.
        ASSERT_TRUE(arrivals.back() - slow_start < 100ms);
        ASSERT_TRUE(slow_end - arrivals.back() > 200ms);
    }

    // The stalled peer is closed, and only it.
    std::vector<uint64_t> closed;
    for (int i = 0; i < 10 && closed.empty(); ++i) {
        std::vector<uint8_t> buf;
        ran->receive(buf);
        closed = ran->take_closed_peers();
    }
    ASSERT_EQ(closed.size(), size_t{1});
    ASSERT_EQ(closed[0], stalled_token);
    ASSERT_TRUE(ran->send_to(good_token, bytes("STILL-HERE"), PduClass::PROCEDURE) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(dapp_receive(*good)), std::string("STILL-HERE"));

    close(stalled);
    good->dispose();
    ran->dispose();
}

TEST(uring_a_peer_that_resets_is_dropped_alone_and_reported_once) {
    const Endpoint where = endpoint_for(E3TransportLayer::TCP);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    uint64_t good_token = 0;
    auto good = registered_dapp(*ran, where, &good_token);
    ASSERT_TRUE(good != nullptr);

    int rude = raw_tcp(where.port);
    ASSERT_TRUE(rude >= 0);
    ASSERT_TRUE(send_framed(rude, "SETUP"));
    ASSERT_EQ(text(setup_request(*ran)), std::string("SETUP"));
    const uint64_t rude_token = ran->last_setup_peer_token();
    struct linger hard{1, 0};
    setsockopt(rude, SOL_SOCKET, SO_LINGER, &hard, sizeof(hard));
    close(rude);  // a reset, not a clean close

    std::vector<uint64_t> closed;
    for (int i = 0; i < 10 && closed.empty(); ++i) {
        std::vector<uint8_t> buf;
        ran->receive(buf);
        closed = ran->take_closed_peers();
    }
    ASSERT_EQ(closed.size(), size_t{1});
    ASSERT_EQ(closed[0], rude_token);
    ASSERT_TRUE(ran->take_closed_peers().empty());

    ASSERT_TRUE(good->send(bytes("STILL-SERVED")) == ErrorCode::SUCCESS);
    ASSERT_EQ(text(receive_pdu(*ran)), std::string("STILL-SERVED"));
    ASSERT_EQ(ran->last_peer_token(), good_token);
    good->dispose();
    ran->dispose();
}

TEST(uring_thousands_of_pdus_of_every_size_arrive_whole_and_in_order) {
    for (auto transport : {E3TransportLayer::TCP, E3TransportLayer::IPC, E3TransportLayer::SCTP}) {
        if (transport == E3TransportLayer::SCTP && !sctp_here()) continue;
        const Endpoint where = endpoint_for(transport);
        auto ran = make(where, E3Role::RAN);
        ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
        uint64_t token = 0;
        auto dapp = registered_dapp(*ran, where, &token);
        ASSERT_TRUE(dapp != nullptr);

        const std::vector<size_t> sizes = {1, 3, 4, 5, 63, 64, 100, 1000, 4095, 4096, 16384, 32768, LIMIT - 1, LIMIT};
        constexpr size_t COUNT = 2000;
        std::atomic<int> errors{0};
        std::thread to_ran([&] {
            for (size_t i = 0; i < COUNT; ++i) {
                if (dapp->send(patterned(sizes[i % sizes.size()], static_cast<uint32_t>(i))) != ErrorCode::SUCCESS) errors.fetch_add(1);
            }
        });
        for (size_t i = 0; i < COUNT; ++i) {
            const auto got = receive_pdu(*ran);
            if (got != patterned(sizes[i % sizes.size()], static_cast<uint32_t>(i))) {
                errors.fetch_add(1);
                break;
            }
        }
        to_ran.join();
        ASSERT_EQ(errors.load(), 0);

        std::thread to_dapp([&] {
            for (size_t i = 0; i < COUNT; ++i) {
                if (ran->send_to(token, patterned(sizes[i % sizes.size()], static_cast<uint32_t>(i + 7)), PduClass::INDICATION) !=
                    ErrorCode::SUCCESS) {
                    errors.fetch_add(1);
                }
            }
        });
        for (size_t i = 0; i < COUNT; ++i) {
            const auto got = dapp_receive(*dapp, 20);
            if (got != patterned(sizes[i % sizes.size()], static_cast<uint32_t>(i + 7))) {
                errors.fetch_add(1);
                break;
            }
        }
        to_dapp.join();
        ASSERT_EQ(errors.load(), 0);
        dapp->dispose();
        ran->dispose();
    }
}

TEST(uring_a_pdu_of_the_limit_passes_and_one_byte_more_is_refused) {
    const Endpoint where = endpoint_for(E3TransportLayer::IPC);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    auto dapp = registered_dapp(*ran, where);
    ASSERT_TRUE(dapp != nullptr);
    ASSERT_TRUE(dapp->send(std::vector<uint8_t>(LIMIT, 'x')) == ErrorCode::SUCCESS);
    ASSERT_EQ(receive_pdu(*ran).size(), LIMIT);
    ASSERT_TRUE(dapp->send(std::vector<uint8_t>(LIMIT + 1, 'x')) == ErrorCode::INVALID_PARAM);
    ASSERT_TRUE(dapp->send({}) == ErrorCode::INVALID_PARAM);
    ASSERT_TRUE(ran->send(std::vector<uint8_t>(LIMIT + 1, 'x')) == ErrorCode::INVALID_PARAM);
    dapp->dispose();
    ran->dispose();
}

TEST(uring_receive_waits_about_half_a_second_when_nothing_comes) {
    const Endpoint where = endpoint_for(E3TransportLayer::TCP);
    auto ran = make(where, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    auto dapp = registered_dapp(*ran, where);
    ASSERT_TRUE(dapp != nullptr);
    for (E3Connector* side : {ran.get(), dapp.get()}) {
        std::vector<uint8_t> buf;
        const auto t0 = clk::now();
        ASSERT_EQ(side->receive(buf), 0);
        const auto waited = clk::now() - t0;
        ASSERT_TRUE(waited > 300ms);
        ASSERT_TRUE(waited < 1500ms);
    }
    dapp->dispose();
    ran->dispose();
}

TEST(uring_shutdown_wakes_a_waiting_receive_and_cancels_later_sends) {
    for (E3Role role : {E3Role::RAN, E3Role::DAPP}) {
        const Endpoint where = endpoint_for(E3TransportLayer::IPC);
        auto ran = make(where, E3Role::RAN);
        ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
        auto dapp = registered_dapp(*ran, where);
        ASSERT_TRUE(dapp != nullptr);
        E3Connector& side = role == E3Role::RAN ? *ran : *dapp;

        clk::time_point returned_at;
        std::thread waiter([&] {
            std::vector<uint8_t> buf;
            side.receive(buf);  // waits up to half a second
            returned_at = clk::now();
        });
        std::this_thread::sleep_for(100ms);
        const auto t0 = clk::now();
        side.shutdown();
        side.shutdown();  // twice is harmless
        waiter.join();
        ASSERT_TRUE(returned_at >= t0);
        ASSERT_TRUE(returned_at - t0 < 150ms);
        ASSERT_TRUE(side.send(bytes("late")) == ErrorCode::CANCELLED);
        side.dispose();
        side.dispose();
        ran->dispose();
        dapp->dispose();
    }
}

int main(int argc, char** argv) {
    if (const int rc = libe3_test::select_links(argc, argv); rc >= 0) return rc;
    if (!libe3_test::uring_available()) {
        if (libe3_test::uring_required()) {
            std::cerr << "FAIL: io_uring is required (LIBE3_REQUIRE_URING) and cannot be used here\n";
            return 1;
        }
        std::cout << "URING NOT RUN: this build or this kernel cannot create an io_uring ring\n";
        return 77;
    }
    return RUN_ALL_TESTS();
}
