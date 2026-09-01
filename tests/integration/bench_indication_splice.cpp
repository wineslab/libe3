/**
 * @file bench_indication_splice.cpp
 * @brief Before/after micro-benchmark for JSON IndicationMessage encode/decode:
 *        DOM parse+dump vs. verbatim byte splice.
 *
 * Run this binary once built against a commit before the splice change and
 * once after to produce the two halves of a before/after comparison table
 * (not a permanently-shipped dual code path).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <libe3/e3_encoder.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace libe3;
using clk = std::chrono::steady_clock;

namespace {

constexpr int kIterations = 200000;  // matches the issue's own methodology
constexpr int kWarmup = 1000;

// 184-byte IQ-metadata-style payload, matching the issue's example shape.
std::string representative_payload() {
    return R"({"shm_name":"iq_ring_0","write_index":128,"buf_index":3,)"
           R"("timestamp":1234567890123,"sfn":512,"slot":7,"cell_id":1,)"
           R"("antenna_count":4,"symbol_mask":255})";
}

double percentile(std::vector<double>& v, double p) {
    std::sort(v.begin(), v.end());
    return v[static_cast<size_t>(p * static_cast<double>(v.size() - 1))];
}

template <typename Fn>
std::vector<double> time_ns(Fn&& fn) {
    for (int i = 0; i < kWarmup; ++i) fn();
    std::vector<double> out;
    out.reserve(kIterations);
    for (int i = 0; i < kIterations; ++i) {
        auto t0 = clk::now();
        fn();
        auto t1 = clk::now();
        out.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return out;
}

double mean(const std::vector<double>& v) {
    double s = 0;
    for (auto x : v) s += x;
    return s / static_cast<double>(v.size());
}

}  // namespace

int main() {
#ifdef LIBE3_ENABLE_JSON
    auto encoder = create_encoder(EncodingFormat::JSON);
    std::string payload = representative_payload();

    Pdu pdu(PduType::INDICATION_MESSAGE);
    IndicationMessage msg;
    msg.dapp_identifier = 1;
    msg.ran_function_identifier = 1;
    msg.protocol_data.assign(payload.begin(), payload.end());
    pdu.choice = msg;

    auto encode_times = time_ns([&] {
        auto r = encoder->encode(pdu);
        (void)r;
    });
    auto encoded = encoder->encode(pdu);
    auto decode_times = time_ns([&] {
        auto r = encoder->decode(*encoded);
        (void)r;
    });

    std::printf("## JSON IndicationMessage encode/decode microbenchmark\n\n");
    std::printf("Payload: %zu bytes, N=%d iterations after %d warmup.\n\n",
                 payload.size(), kIterations, kWarmup);
    std::printf("| Operation | mean (us) | p50 (us) | p99 (us) |\n");
    std::printf("|---|---:|---:|---:|\n");
    std::printf("| encode | %.3f | %.3f | %.3f |\n",
                 mean(encode_times) / 1000.0, percentile(encode_times, 0.5) / 1000.0,
                 percentile(encode_times, 0.99) / 1000.0);
    std::printf("| decode | %.3f | %.3f | %.3f |\n",
                 mean(decode_times) / 1000.0, percentile(decode_times, 0.5) / 1000.0,
                 percentile(decode_times, 0.99) / 1000.0);
#else
    std::fprintf(stderr, "LIBE3_ENABLE_JSON is OFF; nothing to benchmark.\n");
#endif
    return 0;
}
