/**
 * @file test_role_pair_sctp.cpp
 * @brief The POSIX link over SCTP, end to end, with the payload protocol id and the stream of
 *        every message read off the association.
 *
 * A proxy sits between the dApp and the RAN. It is an SCTP peer of both, so it sees what the
 * kernel tells a receiver: for each message the stream and the payload protocol id. Properties
 * verified:
 *   - one association per dApp carries setup, subscription, indications, a control and its
 *     answer, and the release;
 *   - every message carries payload protocol id 0x00E3A001;
 *   - indications go from the RAN on stream 1, every other message on stream 0, and what a
 *     dApp sends goes on stream 0;
 *   - with two dApps on one RAN, each association carries only the indications of its own dApp;
 *   - the RAN accepts payload protocol id 0 on stream 0, closes an association whose messages
 *     carry another id, requests two streams, and sets the socket buffers it was told to.
 *
 * Needs the sctp kernel module. Without it the test says so and exits with the code CTest
 * reads as "skipped" (77), unless LIBE3_REQUIRE_SCTP is set: then it fails, so that a machine
 * that is meant to run SCTP cannot pass by skipping.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include <libe3/libe3.hpp>
#include <libe3/e3_connector.hpp>
#include <libe3/e3_encoder.hpp>
#include "sm_simple/e3sm_simple_wrapper.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef LIBE3_ENABLE_SCTP
#include <netinet/sctp.h>
#endif

using namespace libe3;
using namespace std::chrono_literals;

#ifdef LIBE3_ENABLE_SCTP

namespace {

constexpr uint32_t PPID = 0x00E3A001;

uint16_t unique_port() {
    static std::atomic<int> next{0};
    return static_cast<uint16_t>(31000 + (getpid() % 2000) * 8 + (next.fetch_add(1) % 8));
}

bool wait_until(const std::function<bool()>& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

int sctp_socket_with(int streams, int rcvinfo) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (fd < 0) return -1;
    struct sctp_initmsg init{};
    init.sinit_num_ostreams = static_cast<uint16_t>(streams);
    init.sinit_max_instreams = static_cast<uint16_t>(streams);
    setsockopt(fd, IPPROTO_SCTP, SCTP_INITMSG, &init, sizeof(init));
    if (rcvinfo) setsockopt(fd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &rcvinfo, sizeof(rcvinfo));
    return fd;
}

sockaddr_in loopback(uint16_t port) {
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return addr;
}

int connect_to(uint16_t port, int streams) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        int fd = sctp_socket_with(streams, 1);
        auto addr = loopback(port);
        if (fd >= 0 && connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) return fd;
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(20ms);
    }
    return -1;
}

bool send_with(int fd, const std::vector<uint8_t>& data, uint16_t stream, uint32_t ppid_host_order) {
    alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(struct sctp_sndinfo))]{};
    struct iovec iov{const_cast<uint8_t*>(data.data()), data.size()};
    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = IPPROTO_SCTP;
    c->cmsg_type = SCTP_SNDINFO;
    c->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));
    struct sctp_sndinfo info{};
    info.snd_sid = stream;
    info.snd_ppid = htonl(ppid_host_order);
    std::memcpy(CMSG_DATA(c), &info, sizeof(info));
    return ::sendmsg(fd, &msg, MSG_NOSIGNAL) == static_cast<ssize_t>(data.size());
}

/// One message as the receiver is told about it.
struct Record {
    int association;     ///< 1, 2, ... in the order the dApps connected
    char from;           ///< 'D' from the dApp, 'R' from the RAN
    uint16_t stream;
    uint32_t ppid;       ///< host order
    PduType type;
    uint32_t dapp;       ///< the dApp an indication or a subscription response is for, else 0
};

/// An SCTP peer of both ends that forwards every message unchanged and records what it was told.
class Tap {
public:
    Tap(uint16_t listen_port, uint16_t upstream_port)
        : listen_port_(listen_port), upstream_port_(upstream_port), encoder_(create_encoder(EncodingFormat::ASN1)) {}
    ~Tap() { stop(); }

    bool start() {
        listener_ = sctp_socket_with(2, 0);
        if (listener_ < 0) return false;
        int one = 1;
        setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        auto addr = loopback(listen_port_);
        if (bind(listener_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) return false;
        if (listen(listener_, 8) != 0) return false;
        thread_ = std::thread([this] { run(); });
        return true;
    }

    void stop() {
        stop_.store(true);
        if (thread_.joinable()) thread_.join();
        if (listener_ >= 0) { close(listener_); listener_ = -1; }
    }

    std::vector<Record> records() {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_;
    }
    int associations() const { return associations_.load(); }

private:
    struct Pair { int dapp_fd; int ran_fd; int index; };

    void run() {
        std::vector<Pair> pairs;
        while (!stop_.load()) {
            std::vector<struct pollfd> pfds{{listener_, POLLIN, 0}};
            for (const auto& p : pairs) {
                pfds.push_back({p.dapp_fd, POLLIN, 0});
                pfds.push_back({p.ran_fd, POLLIN, 0});
            }
            if (poll(pfds.data(), pfds.size(), 50) <= 0) continue;
            if (pfds[0].revents & POLLIN) {
                int dapp_fd = accept(listener_, nullptr, nullptr);
                if (dapp_fd >= 0) {
                    int one = 1;
                    setsockopt(dapp_fd, IPPROTO_SCTP, SCTP_RECVRCVINFO, &one, sizeof(one));
                    int ran_fd = connect_to(upstream_port_, 2);
                    if (ran_fd >= 0) {
                        pairs.push_back({dapp_fd, ran_fd, associations_.fetch_add(1) + 1});
                    } else {
                        close(dapp_fd);
                    }
                }
            }
            for (size_t i = 0; i < pairs.size();) {
                const Pair p = pairs[i];
                bool alive = true;
                if (pfds.size() > 2 * i + 2) {
                    if (pfds[2 * i + 1].revents & (POLLIN | POLLHUP | POLLERR)) alive = forward(p.dapp_fd, p.ran_fd, 'D', p.index);
                    if (alive && (pfds[2 * i + 2].revents & (POLLIN | POLLHUP | POLLERR))) alive = forward(p.ran_fd, p.dapp_fd, 'R', p.index);
                }
                if (!alive) {
                    close(p.dapp_fd);
                    close(p.ran_fd);
                    pairs.erase(pairs.begin() + static_cast<long>(i));
                    pfds.erase(pfds.begin() + static_cast<long>(2 * i + 1), pfds.begin() + static_cast<long>(2 * i + 3));
                } else {
                    ++i;
                }
            }
        }
        for (const auto& p : pairs) { close(p.dapp_fd); close(p.ran_fd); }
    }

    bool forward(int from, int to, char direction, int index) {
        std::vector<uint8_t> data(70000);
        alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(struct sctp_rcvinfo))];
        struct iovec iov{data.data(), data.size()};
        struct msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        const ssize_t n = ::recvmsg(from, &msg, 0);
        if (n <= 0) return n < 0 && (errno == EAGAIN || errno == EINTR);
        if (msg.msg_flags & MSG_NOTIFICATION) return true;
        data.resize(static_cast<size_t>(n));

        struct sctp_rcvinfo info{};
        for (struct cmsghdr* c = CMSG_FIRSTHDR(&msg); c != nullptr; c = CMSG_NXTHDR(&msg, c)) {
            if (c->cmsg_level == IPPROTO_SCTP && c->cmsg_type == SCTP_RCVINFO) {
                std::memcpy(&info, CMSG_DATA(c), sizeof(info));
            }
        }
        Record record{index, direction, info.rcv_sid, ntohl(info.rcv_ppid), PduType::SETUP_REQUEST, 0};
        if (auto pdu = encoder_->decode(data.data(), data.size())) {
            record.type = pdu->type;
            if (const auto* m = std::get_if<IndicationMessage>(&pdu->choice)) record.dapp = m->dapp_identifier;
            if (const auto* m = std::get_if<SubscriptionResponse>(&pdu->choice)) record.dapp = m->dapp_identifier;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            records_.push_back(record);
        }
        // Forward as it came: same stream, same payload protocol id.
        alignas(struct cmsghdr) char out_control[CMSG_SPACE(sizeof(struct sctp_sndinfo))]{};
        struct iovec out_iov{data.data(), data.size()};
        struct msghdr out{};
        out.msg_iov = &out_iov;
        out.msg_iovlen = 1;
        out.msg_control = out_control;
        out.msg_controllen = sizeof(out_control);
        struct cmsghdr* c = CMSG_FIRSTHDR(&out);
        c->cmsg_level = IPPROTO_SCTP;
        c->cmsg_type = SCTP_SNDINFO;
        c->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));
        struct sctp_sndinfo si{};
        si.snd_sid = info.rcv_sid;
        si.snd_ppid = info.rcv_ppid;
        std::memcpy(CMSG_DATA(c), &si, sizeof(si));
        return ::sendmsg(to, &out, MSG_NOSIGNAL) == static_cast<ssize_t>(data.size());
    }

    uint16_t listen_port_;
    uint16_t upstream_port_;
    std::unique_ptr<E3Encoder> encoder_;
    int listener_{-1};
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> associations_{0};
    std::mutex mutex_;
    std::vector<Record> records_;
};

class SctpSM : public ServiceModel {
public:
    std::string name() const override { return "SCTP-TEST"; }
    uint32_t version() const override { return 1; }
    uint32_t ran_function_id() const override { return 1; }
    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }
    ErrorCode init() override { return ErrorCode::SUCCESS; }
    void destroy() override { stop(); }
    bool is_running() const override { return running_; }
    std::vector<uint8_t> ran_function_data() const override {
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data("SCTPSM", out)) return out;
        return {0x01};
    }
    ErrorCode start() override {
        if (running_) return ErrorCode::SUCCESS;
        running_ = true;
        worker_ = std::thread([this]() {
            uint32_t seq = 0;
            while (running_) {
                std::this_thread::sleep_for(100ms);
                auto subs = get_subscribers();
                if (subs.empty()) continue;
                libe3_examples::SimpleIndication si{seq, 0};
                std::vector<uint8_t> enc;
                if (!libe3_examples::encode_simple_indication(si, enc)) continue;
                for (auto did : subs) (void)emit_outbound(make_indication_pdu(did, 1, enc));
                ++seq;
            }
        });
        return ErrorCode::SUCCESS;
    }
    void stop() override {
        if (!running_) return;
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }
    ErrorCode handle_control_action(uint32_t request_message_id, const DAppControlAction& a) override {
        (void)respond_to_control(a.dapp_identifier, request_message_id, ControlResult{});
        return ErrorCode::SUCCESS;
    }

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
};

E3Config ran_config(uint16_t port) {
    E3Config c;
    c.role = E3Role::RAN;
    c.ran_identifier = "sctp-test-ran";
    c.link_layer = E3LinkLayer::POSIX;
    c.transport_layer = E3TransportLayer::SCTP;
    c.encoding = EncodingFormat::ASN1;
    c.log_level = 0;
    c.setup_port = port;
    return c;
}

/// Everything a dApp does, counted.
struct DAppLog {
    std::mutex mu;
    std::condition_variable cv;
    int indications{0};
    int control_responses{0};
    bool granted{false};
};

std::unique_ptr<E3Agent> make_dapp(E3Config config, const char* name, DAppLog& log) {
    config.role = E3Role::DAPP;
    config.dapp_name = name;
    auto dapp = std::make_unique<E3Agent>(config);
    dapp->set_indication_handler([&log](const IndicationMessage&) {
        std::lock_guard<std::mutex> lk(log.mu);
        ++log.indications;
        log.cv.notify_all();
    });
    dapp->set_subscription_response_handler([&log](const SubscriptionResponse& r) {
        std::lock_guard<std::mutex> lk(log.mu);
        if (r.response_code == ResponseCode::POSITIVE) log.granted = true;
        log.cv.notify_all();
    });
    dapp->set_control_response_handler([&log](const ControlResponse&) {
        std::lock_guard<std::mutex> lk(log.mu);
        ++log.control_responses;
        log.cv.notify_all();
    });
    return dapp;
}

bool wait_for(DAppLog& log, const std::function<bool()>& pred) {
    std::unique_lock<std::mutex> lk(log.mu);
    return log.cv.wait_for(lk, 10s, pred);
}

/// The sndbuf and rcvbuf the kernel reports for the association whose local port is @p port, or {0, 0}.
std::pair<long, long> association_buffers(uint16_t port) {
    std::ifstream in("/proc/net/sctp/assocs");
    std::string line;
    std::getline(in, line);  // header
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::vector<std::string> tokens;
        for (std::string w; words >> w;) tokens.push_back(w);
        // ASSOC SOCK STY SST ST HBKT ASSOC-ID TX_QUEUE RX_QUEUE UID INODE LPORT RPORT ... sndbuf rcvbuf
        if (tokens.size() < 16) continue;
        if (tokens[11] == std::to_string(port)) {
            return {std::stol(tokens[tokens.size() - 2]), std::stol(tokens[tokens.size() - 1])};
        }
    }
    return {0, 0};
}

long wmem_max() {
    std::ifstream in("/proc/sys/net/core/wmem_max");
    long v = 0;
    in >> v;
    return v;
}

long rmem_max() {
    std::ifstream in("/proc/sys/net/core/rmem_max");
    long v = 0;
    in >> v;
    return v;
}

}  // namespace

TEST(sctp_pair_one_association_with_the_ppid_and_streams_of_the_link) {
    const uint16_t ran_port = unique_port();
    const uint16_t tap_port = unique_port();

    E3Agent ran(ran_config(ran_port));
    ASSERT_TRUE(ran.register_sm(std::make_unique<SctpSM>()) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.start() == ErrorCode::SUCCESS);

    Tap tap(tap_port, ran_port);
    ASSERT_TRUE(tap.start());

    auto dapp_cfg = ran_config(tap_port);
    DAppLog log;
    auto dapp = make_dapp(dapp_cfg, "SctpDApp", log);
    ASSERT_TRUE(dapp->start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->wait_for_setup(10000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(dapp->dapp_id().has_value());

    ASSERT_TRUE(dapp->subscribe(1, {1}, {1}) == ErrorCode::SUCCESS);
    ASSERT_TRUE(wait_for(log, [&] { return log.granted && log.indications >= 3; }));

    uint32_t request_id = 0;
    ASSERT_TRUE(dapp->send_control(1, 1, {0x01}, 0, &request_id, /*request_ack=*/true) == ErrorCode::SUCCESS);
    ASSERT_TRUE(wait_for(log, [&] { return log.control_responses >= 1; }));

    // The socket buffers the RAN set before the association existed, as the kernel reports them.
    // It clamps the request to net.core.[wr]mem_max and doubles it.
    const auto buffers = association_buffers(ran_port);
    if (buffers.first > 0) {
        const long wanted = static_cast<long>(E3Config{}.socket_buffer_bytes);
        ASSERT_EQ(buffers.first, 2 * std::min(wanted, wmem_max()));
        ASSERT_EQ(buffers.second, 2 * std::min(wanted, rmem_max()));
    } else {
        std::cout << "NOTE: /proc/net/sctp/assocs does not show the association; buffers not checked\n";
    }

    ASSERT_TRUE(dapp->release() == ErrorCode::SUCCESS);
    ASSERT_TRUE(wait_until([&] { return ran.dapp_count() == 0u; }, 10000ms));
    dapp->stop();
    ran.stop();
    tap.stop();

    // One association for the one dApp.
    ASSERT_EQ(tap.associations(), 1);

    std::set<PduType> from_dapp, from_ran;
    int indications = 0;
    for (const auto& r : tap.records()) {
        // Every message carries the payload protocol id of the link.
        ASSERT_EQ(r.ppid, PPID);
        if (r.from == 'D') {
            from_dapp.insert(r.type);
            ASSERT_EQ(static_cast<int>(r.stream), 0);  // a dApp sends everything on stream 0
        } else {
            from_ran.insert(r.type);
            ASSERT_EQ(static_cast<int>(r.stream), r.type == PduType::INDICATION_MESSAGE ? 1 : 0);
            if (r.type == PduType::INDICATION_MESSAGE) ++indications;
        }
    }
    ASSERT_TRUE(indications >= 3);
    for (PduType t : {PduType::SETUP_REQUEST, PduType::SUBSCRIPTION_REQUEST, PduType::DAPP_CONTROL_ACTION,
                      PduType::RELEASE_MESSAGE}) {
        ASSERT_TRUE(from_dapp.count(t) == 1);
    }
    for (PduType t : {PduType::SETUP_RESPONSE, PduType::SUBSCRIPTION_RESPONSE, PduType::INDICATION_MESSAGE,
                      PduType::CONTROL_RESPONSE}) {
        ASSERT_TRUE(from_ran.count(t) == 1);
    }
}

TEST(sctp_pair_two_dapps_each_association_carries_only_its_own_indications) {
    const uint16_t ran_port = unique_port();
    const uint16_t tap_port = unique_port();

    E3Agent ran(ran_config(ran_port));
    ASSERT_TRUE(ran.register_sm(std::make_unique<SctpSM>()) == ErrorCode::SUCCESS);
    ASSERT_TRUE(ran.start() == ErrorCode::SUCCESS);
    Tap tap(tap_port, ran_port);
    ASSERT_TRUE(tap.start());

    DAppLog first_log, second_log;
    auto first = make_dapp(ran_config(tap_port), "First", first_log);
    auto second = make_dapp(ran_config(tap_port), "Second", second_log);
    ASSERT_TRUE(first->start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(first->wait_for_setup(10000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(second->start() == ErrorCode::SUCCESS);
    ASSERT_TRUE(second->wait_for_setup(10000ms) == ErrorCode::SUCCESS);
    ASSERT_TRUE(first->subscribe(1, {1}, {}) == ErrorCode::SUCCESS);
    ASSERT_TRUE(second->subscribe(1, {1}, {}) == ErrorCode::SUCCESS);
    ASSERT_TRUE(wait_for(first_log, [&] { return first_log.indications >= 3; }));
    ASSERT_TRUE(wait_for(second_log, [&] { return second_log.indications >= 3; }));

    first->stop();
    second->stop();
    ran.stop();
    tap.stop();

    ASSERT_EQ(tap.associations(), 2);
    std::set<uint32_t> dapps_on[3];
    for (const auto& r : tap.records()) {
        if (r.from == 'R' && (r.type == PduType::INDICATION_MESSAGE || r.type == PduType::SUBSCRIPTION_RESPONSE)) {
            dapps_on[r.association].insert(r.dapp);
        }
    }
    ASSERT_EQ(dapps_on[1].size(), size_t{1});
    ASSERT_EQ(dapps_on[2].size(), size_t{1});
    ASSERT_TRUE(*dapps_on[1].begin() != *dapps_on[2].begin());
}

TEST(sctp_ran_accepts_ppid_zero_on_stream_zero_and_requests_two_streams) {
    const uint16_t port = unique_port();
    auto ran = create_connector(E3LinkLayer::POSIX, E3TransportLayer::SCTP, "", "", "", port, 0, 0, 1, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    // A peer that asks for ten streams gets two: the RAN's listener allows two.
    int peer = connect_to(port, 10);
    ASSERT_TRUE(peer >= 0);
    struct sctp_status status{};
    socklen_t length = sizeof(status);
    ASSERT_EQ(getsockopt(peer, IPPROTO_SCTP, SCTP_STATUS, &status, &length), 0);
    ASSERT_EQ(static_cast<int>(status.sstat_outstrms), 2);

    // PPID 0 on stream 0, as a peer that stamps nothing sends it, is served.
    ASSERT_TRUE(send_with(peer, {'S', 'E', 'T', 'U', 'P'}, 0, 0));
    std::vector<uint8_t> rx;
    int n = 0;
    for (int i = 0; i < 10 && n <= 0; ++i) n = ran->recv_setup_request(rx);
    ASSERT_EQ(n, 5);

    // So is the id of the link.
    ASSERT_TRUE(send_with(peer, {'O', 'N', 'E'}, 0, PPID));
    n = 0;
    for (int i = 0; i < 10 && n <= 0; ++i) {
        std::vector<uint8_t> buf;
        n = ran->receive(buf);
        if (n > 0) ASSERT_EQ(std::string(buf.begin(), buf.end()), std::string("ONE"));
    }
    ASSERT_EQ(n, 3);

    close(peer);
    ran->dispose();
}

TEST(sctp_ran_closes_an_association_whose_messages_carry_another_ppid) {
    const uint16_t port = unique_port();
    auto ran = create_connector(E3LinkLayer::POSIX, E3TransportLayer::SCTP, "", "", "", port, 0, 0, 1, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);

    int peer = connect_to(port, 2);
    ASSERT_TRUE(peer >= 0);
    ASSERT_TRUE(send_with(peer, {'S', 'E', 'T', 'U', 'P'}, 0, 0x1234));
    std::vector<uint8_t> rx;
    ASSERT_TRUE(ran->recv_setup_request(rx) <= 0);  // not served

    std::vector<uint64_t> closed;
    for (int i = 0; i < 10 && closed.empty(); ++i) {
        std::vector<uint8_t> buf;
        ran->receive(buf);
        closed = ran->take_closed_peers();
    }
    ASSERT_EQ(closed.size(), size_t{1});

    // The peer finds the association gone.
    struct timeval timeout{2, 0};
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    char byte;
    ASSERT_TRUE(::recv(peer, &byte, 1, 0) <= 0);
    close(peer);
    ran->dispose();
}

TEST(sctp_ran_takes_a_pdu_of_sixty_thousand_bytes_whole) {
    const uint16_t port = unique_port();
    auto ran = create_connector(E3LinkLayer::POSIX, E3TransportLayer::SCTP, "", "", "", port, 0, 0, 1, E3Role::RAN);
    ASSERT_TRUE(ran->setup_initial_connection() == ErrorCode::SUCCESS);
    int peer = connect_to(port, 2);
    ASSERT_TRUE(peer >= 0);
    ASSERT_TRUE(send_with(peer, {'S'}, 0, PPID));
    std::vector<uint8_t> rx;
    for (int i = 0; i < 10 && rx.empty(); ++i) ran->recv_setup_request(rx);
    ASSERT_EQ(rx.size(), size_t{1});

    std::vector<uint8_t> big(60000);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i * 7);
    ASSERT_TRUE(send_with(peer, big, 0, PPID));
    std::vector<uint8_t> got;
    for (int i = 0; i < 10 && got.empty(); ++i) ran->receive(got);
    ASSERT_TRUE(got == big);

    close(peer);
    ran->dispose();
}

int main() {
    // The kernel module may be missing even when the headers are there.
    int probe = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (probe < 0) {
        const char* require = std::getenv("LIBE3_REQUIRE_SCTP");
        if (require != nullptr && *require != '\0' && *require != '0') {
            std::cerr << "FAIL: SCTP is required (LIBE3_REQUIRE_SCTP) and this host cannot open an SCTP socket\n";
            return 1;
        }
        std::cout << "SCTP NOT RUN: this host cannot open an SCTP socket (is the sctp kernel module loaded?)\n";
        return 77;
    }
    close(probe);
    return RUN_ALL_TESTS();
}

#else  // LIBE3_ENABLE_SCTP

int main() {
    const char* require = std::getenv("LIBE3_REQUIRE_SCTP");
    if (require != nullptr && *require != '\0' && *require != '0') {
        std::cerr << "FAIL: SCTP is required (LIBE3_REQUIRE_SCTP) and libe3 was built without it\n";
        return 1;
    }
    std::cout << "SCTP NOT RUN: libe3 was built without LIBE3_ENABLE_SCTP\n";
    return 77;
}

#endif
