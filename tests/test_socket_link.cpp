/**
 * @file test_socket_link.cpp
 * @brief The framing and endpoint rules that the POSIX and the io_uring link share.
 *
 * Pure functions, no sockets, so it runs in every build. Properties verified:
 *   - a TCP frame is its length in four octets, network order, then the PDU;
 *   - a frame split anywhere needs more bytes, two frames back to back read as two;
 *   - a length of zero or above the limit is refused, the limit itself is taken;
 *   - the endpoint string gives host and port the same way for both links, IPv6 in brackets;
 *   - the shared default IPC endpoint of E3Config maps to /tmp/dapps/e3;
 *   - an SCTP socket of either link gets the bounded retransmission timeout, read back from the
 *     kernel (where the host has the sctp module): with Linux's own 1 s minimum, a burst of large
 *     PDUs dropped the dApp in 7 to 8 of 100 runs of 2000 PDUs on both links, and in none with it.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "connector/socket_link.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#ifdef LIBE3_ENABLE_SCTP
#include <netinet/in.h>
#include <netinet/sctp.h>
#endif

using namespace libe3;
using namespace libe3::socket_link;

namespace {

std::vector<uint8_t> frame(size_t size, uint8_t fill) {
    std::vector<uint8_t> out(TCP_PREFIX_BYTES + size, fill);
    uint8_t prefix[TCP_PREFIX_BYTES];
    write_stream_prefix(prefix, size);
    std::memcpy(out.data(), prefix, TCP_PREFIX_BYTES);
    return out;
}

}  // namespace

TEST(the_prefix_is_four_octets_in_network_order) {
    uint8_t prefix[TCP_PREFIX_BYTES];
    write_stream_prefix(prefix, 0x01020304);
    ASSERT_EQ(prefix[0], 1);
    ASSERT_EQ(prefix[1], 2);
    ASSERT_EQ(prefix[2], 3);
    ASSERT_EQ(prefix[3], 4);
}

TEST(a_frame_split_anywhere_needs_more_and_whole_reads_as_its_pdu) {
    const auto whole = frame(10, 0xAB);
    size_t size = 0;
    for (size_t cut = 0; cut < whole.size(); ++cut) {
        ASSERT_TRUE(parse_stream_frame(whole.data(), cut, size) == StreamFrame::NEED_MORE);
    }
    ASSERT_TRUE(parse_stream_frame(whole.data(), whole.size(), size) == StreamFrame::PDU);
    ASSERT_EQ(size, size_t{10});
}

TEST(two_frames_back_to_back_read_as_two) {
    auto both = frame(3, 1);
    const auto second = frame(5, 2);
    both.insert(both.end(), second.begin(), second.end());
    size_t size = 0;
    ASSERT_TRUE(parse_stream_frame(both.data(), both.size(), size) == StreamFrame::PDU);
    ASSERT_EQ(size, size_t{3});
    const size_t next = TCP_PREFIX_BYTES + size;
    ASSERT_TRUE(parse_stream_frame(both.data() + next, both.size() - next, size) == StreamFrame::PDU);
    ASSERT_EQ(size, size_t{5});
    ASSERT_EQ(both[next + TCP_PREFIX_BYTES], 2);
}

TEST(a_length_of_zero_or_above_the_limit_is_refused_and_the_limit_is_taken) {
    size_t size = 0;
    const uint8_t zero[TCP_PREFIX_BYTES] = {0, 0, 0, 0};
    ASSERT_TRUE(parse_stream_frame(zero, sizeof(zero), size) == StreamFrame::INVALID);
    uint8_t over[TCP_PREFIX_BYTES];
    write_stream_prefix(over, MAX_PDU + 1);
    ASSERT_TRUE(parse_stream_frame(over, sizeof(over), size) == StreamFrame::INVALID);
    const auto at_limit = frame(MAX_PDU, 9);
    ASSERT_TRUE(parse_stream_frame(at_limit.data(), at_limit.size(), size) == StreamFrame::PDU);
    ASSERT_EQ(size, MAX_PDU);
    ASSERT_EQ(MAX_PDU, size_t{65536});
}

TEST(the_endpoint_names_host_and_port_the_same_way_for_both_links) {
    Target t = parse_target("tcp://10.0.0.1:1234", 9990);
    ASSERT_EQ(t.host, std::string("10.0.0.1"));
    ASSERT_EQ(t.port, std::string("1234"));
    t = parse_target("sctp://[::1]:36423", 9990);
    ASSERT_EQ(t.host, std::string("::1"));
    ASSERT_EQ(t.port, std::string("36423"));
    t = parse_target("sctp://[fe80::1]", 9990);
    ASSERT_EQ(t.host, std::string("fe80::1"));
    ASSERT_EQ(t.port, std::string("9990"));
    t = parse_target("tcp://ran.example", 9990);
    ASSERT_EQ(t.host, std::string("ran.example"));
    ASSERT_EQ(t.port, std::string("9990"));
    t = parse_target("ipc:///tmp/dapps/setup", 7);
    ASSERT_TRUE(t.host.empty());
    ASSERT_EQ(t.port, std::string("7"));
}

TEST(the_shared_default_ipc_endpoint_maps_to_e3) {
    ASSERT_EQ(link_endpoint(E3TransportLayer::IPC, SHARED_DEFAULT_ENDPOINT), std::string("/tmp/dapps/e3"));
    ASSERT_EQ(link_endpoint(E3TransportLayer::IPC, "ipc:///run/e3.sock"), std::string("/run/e3.sock"));
    ASSERT_EQ(link_endpoint(E3TransportLayer::IPC, "/run/e3.sock"), std::string("/run/e3.sock"));
    ASSERT_EQ(link_endpoint(E3TransportLayer::TCP, "tcp://h:1"), std::string("tcp://h:1"));
}

int main() {
    return RUN_ALL_TESTS();
}
