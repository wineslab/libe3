/**
 * @file link_under_test.hpp
 * @brief Which socket link a test drives, so one test body covers the POSIX and the io_uring link.
 *
 * The POSIX and the io_uring link speak the same wire, so the tests of the one-endpoint wire
 * run against both, and against a RAN of one with a dApp of the other. A test binary takes
 *
 *     --link posix|uring        both ends
 *     --ran-link posix|uring    the RAN end
 *     --dapp-link posix|uring   the dApp end
 *
 * and asks link_for(role) which connector to build. Without arguments both ends are POSIX.
 *
 * io_uring can be missing where the build has it: a container under Docker's default seccomp
 * profile, a kernel with kernel.io_uring_disabled set, or a kernel older than 5.11. A binary
 * asked to run uring there says so and exits 77, which CTest reports as skipped, unless
 * LIBE3_REQUIRE_URING is set: then it fails, so that a machine meant to run io_uring cannot
 * pass by skipping. A single test that needs io_uring calls uring_runs_here() instead.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_TEST_LINK_UNDER_TEST_HPP
#define LIBE3_TEST_LINK_UNDER_TEST_HPP

#include "libe3/e3_connector.hpp"
#include "libe3/types.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace libe3_test {

struct LinksUnderTest {
    libe3::E3LinkLayer ran{libe3::E3LinkLayer::POSIX};
    libe3::E3LinkLayer dapp{libe3::E3LinkLayer::POSIX};
};

inline LinksUnderTest& links_under_test() {
    static LinksUnderTest links;
    return links;
}

inline libe3::E3LinkLayer ran_link() { return links_under_test().ran; }
inline libe3::E3LinkLayer dapp_link() { return links_under_test().dapp; }

inline libe3::E3LinkLayer link_for(libe3::E3Role role) {
    return role == libe3::E3Role::RAN ? ran_link() : dapp_link();
}

/**
 * @brief Whether a TCP port, and an SCTP port where the kernel has SCTP, can be bound right now.
 *
 * This is the question listen() asks. A bind is refused by any socket that holds the port without
 * SO_REUSEADDR, and that includes the client sockets of earlier connections still in TIME_WAIT:
 * the kernel hands out source ports from 32768 up, so a test that fixes its port in that range
 * meets one every so often, once enough connections have come and gone.
 *
 * @param port The port
 * @return true when none of the protocols refuses it
 */
inline bool port_is_free(uint16_t port) {
    for (const int protocol : {IPPROTO_TCP, IPPROTO_SCTP}) {
        const int fd = ::socket(AF_INET, SOCK_STREAM, protocol);
        if (fd < 0) {
            continue;  // the protocol is not there, so nothing of it holds the port
        }
        const int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);
        const bool free = ::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
        ::close(fd);
        if (!free) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Reserve @p count consecutive ports for listeners, free now and kept from every other process.
 *
 * Ports come from 20000 to 31999, below the range of source ports. Being free when asked is not
 * enough, because a test asks for its port some time before it listens, and another test that
 * asks in between is told the same port is free. So each port is also claimed with a lock on a file
 * in /tmp/libe3-test-ports, held until the process exits: no two of these tests ever take the same
 * port, whatever order they ask and listen in. The search starts at a place that depends on the
 * process, so that tests CTest runs side by side seldom look at the same ports at all.
 *
 * @param count How many consecutive ports (3 for the triple of a ZeroMQ configuration)
 * @return The first of them
 */
inline uint16_t free_ports(unsigned count = 1) {
    static std::atomic<unsigned> counter{0};
    static std::vector<int> held;  // the lock files, open until the process ends
    const unsigned span = 12000;
    const unsigned start = (static_cast<unsigned>(::getpid()) % 750) * 16;
    mkdir("/tmp/libe3-test-ports", 0777);
    chmod("/tmp/libe3-test-ports", 0777);
    for (unsigned attempt = 0; attempt < span; ++attempt) {
        const unsigned first = (start + counter.fetch_add(1)) % (span - count);
        std::vector<int> taken;
        bool all = true;
        for (unsigned i = 0; i < count && all; ++i) {
            const uint16_t port = static_cast<uint16_t>(20000 + first + i);
            const std::string name = "/tmp/libe3-test-ports/" + std::to_string(port);
            const int lock = ::open(name.c_str(), O_CREAT | O_RDWR, 0666);
            if (lock < 0 || ::flock(lock, LOCK_EX | LOCK_NB) != 0 || !port_is_free(port)) {
                if (lock >= 0) ::close(lock);
                all = false;
            } else {
                taken.push_back(lock);
            }
        }
        if (all) {
            held.insert(held.end(), taken.begin(), taken.end());
            return static_cast<uint16_t>(20000 + first);
        }
        for (const int lock : taken) ::close(lock);
    }
    throw std::runtime_error("no free port to listen on");
}

/** @brief One port: see free_ports(). @return The port */
inline uint16_t free_port() { return free_ports(1); }

inline bool uring_required() {
    const char* require = std::getenv("LIBE3_REQUIRE_URING");
    return require != nullptr && *require != '\0' && *require != '0';
}

/** @brief Whether this build and this kernel can create an io_uring connector. */
inline bool uring_available() {
    static const bool available = [] {
        auto probe = libe3::create_connector(libe3::E3LinkLayer::URING, libe3::E3TransportLayer::TCP,
                                             "", "", "", 1, 0, 0, 1, libe3::E3Role::DAPP);
        return probe != nullptr;
    }();
    return available;
}

/**
 * @brief For one test that needs io_uring: true when it can run here.
 *
 * Otherwise prints why and returns false, so the test returns early; with
 * LIBE3_REQUIRE_URING set it throws instead, which fails the test.
 */
inline bool uring_runs_here() {
    if (uring_available()) {
        return true;
    }
    if (uring_required()) {
        throw std::runtime_error("io_uring is required (LIBE3_REQUIRE_URING) and cannot be used here");
    }
    std::cout << "URING NOT RUN: this build or this kernel cannot create an io_uring ring "
                 "(built without liburing, seccomp, kernel.io_uring_disabled, or an old kernel)\n";
    return false;
}

inline libe3::E3LinkLayer parse_link_or_throw(const char* text) {
    if (std::strcmp(text, "posix") == 0) return libe3::E3LinkLayer::POSIX;
    if (std::strcmp(text, "uring") == 0) return libe3::E3LinkLayer::URING;
    throw std::invalid_argument(std::string("unknown link '") + text + "', expected posix or uring");
}

/**
 * @brief Read the links from the command line.
 *
 * @return -1 to go on and run the tests, or the exit code main() should return:
 *         77 when io_uring was asked for and cannot run here (1 with LIBE3_REQUIRE_URING),
 *         2 on a bad argument
 */
inline int select_links(int argc, char** argv) {
    auto& links = links_under_test();
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (i + 1 >= argc) throw std::invalid_argument("missing value after " + arg);
            if (arg == "--link") {
                links.ran = links.dapp = parse_link_or_throw(argv[++i]);
            } else if (arg == "--ran-link") {
                links.ran = parse_link_or_throw(argv[++i]);
            } else if (arg == "--dapp-link") {
                links.dapp = parse_link_or_throw(argv[++i]);
            } else {
                throw std::invalid_argument("unknown argument " + arg);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
    std::cout << "Links under test: RAN " << libe3::link_layer_to_string(links.ran) << ", dApp "
              << libe3::link_layer_to_string(links.dapp) << "\n";
    const bool wants_uring = links.ran == libe3::E3LinkLayer::URING || links.dapp == libe3::E3LinkLayer::URING;
    if (wants_uring && !uring_available()) {
        if (uring_required()) {
            std::cerr << "FAIL: io_uring is required (LIBE3_REQUIRE_URING) and cannot be used here\n";
            return 1;
        }
        std::cout << "URING NOT RUN: this build or this kernel cannot create an io_uring ring "
                     "(built without liburing, seccomp, kernel.io_uring_disabled, or an old kernel)\n";
        return 77;
    }
    return -1;
}

}  // namespace libe3_test

#endif  // LIBE3_TEST_LINK_UNDER_TEST_HPP
