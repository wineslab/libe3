/**
 * @file test_latrec_drops.cpp
 * @brief Exercises the L9_DROP paths, which normal traffic never reaches.
 *
 * Every loss point in libe3 stamps L9_DROP with a reason, and a healthy run
 * takes none of those branches, so those stamps are otherwise never executed.
 * The reachable ones are provoked here:
 *
 *   queue-full    reports are flooded until the bounded outbound queue rejects
 *                 a push
 *   report-queue  the report handler is made slow so the bounded report queue
 *                 fills while reports keep arriving
 *
 * Not covered here: encode is unreachable through the public API, since the
 * encoders do not enforce the ASN.1 SIZE constraints and an oversized report
 * still encodes; decode and no-handler require a peer emitting frames libe3
 * does not produce; send requires the transport to fail mid-call.
 * session-queue belongs to the Python binding and has its own test; filtered
 * requires a second dApp identity to be rejected by handle_indication's own
 * dApp-id filter. shutdown may appear incidentally (not provoked): stop()
 * shuts response_queue_/report_queue_ down before joining a registered SM's
 * own producer thread, so a straggling push from that window is expected.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/e3_agent.hpp>
#include <libe3/latrec.h>

#include "sm_simple/simple_service_model.hpp"
#include "latrec_ring_reader.hpp"
#include "test_framework.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace libe3;
using namespace latrec_test;

namespace {

std::vector<latrec_rec> g_recs;

size_t drops_with_reason(uint64_t reason) {
    size_t n = 0;
    for (const auto& r : g_recs)
        if (stage_of(r) == LATREC_DROP && r.aux2 == reason) n++;
    return n;
}

std::string mkdir_tmp(const char* prefix) {
    std::string tmpl = std::string("/tmp/") + prefix + "_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    const char* d = mkdtemp(buf.data());
    return d ? d : "/tmp";
}

}  // namespace

// ---------------------------------------------------------------------------

TEST(a_full_outbound_queue_is_recorded_as_a_queue_push_drop) {
    const size_t n = drops_with_reason(LATREC_DROP_QUEUE_PUSH);
    std::printf("      queue-full drops: %zu\n", n);
    ASSERT_GT(n, 0u);
}

TEST(a_full_report_queue_is_recorded_as_a_report_queue_drop) {
    const size_t n = drops_with_reason(LATREC_DROP_REPORT_QUEUE);
    std::printf("      report-queue drops: %zu\n", n);
    ASSERT_GT(n, 0u);
}

TEST(a_queue_push_drop_ends_its_outbound_message) {
    // A PDU rejected by a full outbound queue never reaches the publisher and
    // so has no L1, L2 or L3. Drops are matched by reason rather than by
    // presence: a report-queue drop coexists with a completed inbound dispatch
    // on the same seq.
    std::map<uint64_t, std::map<uint8_t, size_t>> stages_by_seq;
    std::map<uint64_t, uint64_t> reason_by_seq;
    for (const auto& r : g_recs) {
        stages_by_seq[seq_of(r)][stage_of(r)]++;
        if (stage_of(r) == LATREC_DROP) reason_by_seq[seq_of(r)] = r.aux2;
    }
    size_t checked = 0;
    for (const auto& [seq, stages] : stages_by_seq) {
        const auto it = reason_by_seq.find(seq);
        if (it == reason_by_seq.end() || it->second != LATREC_DROP_QUEUE_PUSH) continue;
        ASSERT_EQ(stages.count(LATREC_DEQUEUE), 0u);
        ASSERT_EQ(stages.count(LATREC_ENCODE_E3AP_DONE), 0u);
        ASSERT_EQ(stages.count(LATREC_SEND_DONE), 0u);
        checked++;
    }
    std::printf("      %zu queue-full dropped messages checked\n", checked);
    ASSERT_GT(checked, 0u);
}

TEST(every_drop_carries_a_known_reason) {
    // A reason of 0, or past the end of the enum, indicates a stamp written
    // with the wrong argument.
    size_t total = 0;
    std::map<uint64_t, size_t> seen;
    for (const auto& r : g_recs) {
        if (stage_of(r) != LATREC_DROP) continue;
        total++;
        seen[r.aux2]++;
        ASSERT_GE(r.aux2, static_cast<uint64_t>(LATREC_DROP_QUEUE_PUSH));
        ASSERT_LE(r.aux2, static_cast<uint64_t>(LATREC_DROP_SHUTDOWN));
    }
    for (const auto& [reason, n] : seen)
        std::printf("      reason %llu: %zu\n", static_cast<unsigned long long>(reason), n);
    ASSERT_GT(total, 0u);
}

// ---------------------------------------------------------------------------

int main() {
    const std::string trace_dir = mkdir_tmp("latrec_drops");
    latrec_set_output_dir(trace_dir.c_str());
    setenv("LATREC_ENTRIES_LOG2", "16", 1);
    const std::string ipc = mkdir_tmp("latrec_drops_ipc");

    E3Config ran_cfg;
    ran_cfg.role = E3Role::RAN;
    ran_cfg.ran_identifier = "drops-ran";
    ran_cfg.link_layer = E3LinkLayer::ZMQ;
    ran_cfg.transport_layer = E3TransportLayer::IPC;
    ran_cfg.encoding = EncodingFormat::ASN1;
    ran_cfg.log_level = 0;
    ran_cfg.setup_endpoint      = "ipc://" + ipc + "/setup";
    ran_cfg.subscriber_endpoint = "ipc://" + ipc + "/dapp_socket";
    ran_cfg.publisher_endpoint  = "ipc://" + ipc + "/e3_socket";
    auto dapp_cfg = ran_cfg;
    dapp_cfg.role = E3Role::DAPP;
    dapp_cfg.dapp_name = "DropsDApp";

    E3Agent ran(ran_cfg);
    auto sm = std::make_unique<libe3_examples::SimpleServiceModel>(
        /*period_us=*/0 /* quiet */, EncodingFormat::ASN1,
        libe3_examples::SimpleServiceModel::PacingMode::PingPong);
    if (ran.register_sm(std::move(sm)) != ErrorCode::SUCCESS ||
        ran.start() != ErrorCode::SUCCESS) {
        std::fprintf(stderr, "RAN start failed\n");
        return 1;
    }

    // A slow report handler backs the bounded report queue up while the
    // driver below keeps pushing, which fills it at its configured capacity.
    ran.set_dapp_report_handler([](const DAppReport&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    });

    E3Agent dapp(dapp_cfg);
    if (dapp.start() != ErrorCode::SUCCESS) {
        std::fprintf(stderr, "dApp start failed\n");
        return 1;
    }
    dapp.wait_for_setup(std::chrono::seconds(5));
    dapp.subscribe(/*ran_function_id=*/1, {}, {});
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // --- report-queue drops: flood reports past a deliberately slow handler --
    std::atomic<bool> stop{false};
    std::thread flood([&] {
        latrec_tls_open_as("drops.flood");
        while (!stop.load()) dapp.send_report(1, std::vector<uint8_t>(32, 0xAB));
    });
    std::this_thread::sleep_for(std::chrono::seconds(3));
    stop.store(true);
    flood.join();

    dapp.stop();
    ran.stop();

    g_recs = read_ring_dir(trace_dir);
    std::printf("captured %zu records\n\n", g_recs.size());
    const int rc = RUN_ALL_TESTS();

    const std::string rm = "rm -rf '" + trace_dir + "' '" + ipc + "'";
    if (system(rm.c_str()) != 0) { /* best effort */ }
    latrec_set_output_dir(nullptr);
    unsetenv("LATREC_ENTRIES_LOG2");
    return rc;
}
