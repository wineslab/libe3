/**
 * @file bench_latrec.cpp
 * @brief Cost of a latrec stamp, as a regression gate.
 *
 * Reports the cost of the enabled and disabled stamp paths and fails if
 * either exceeds a ceiling. The ceilings are wide enough to absorb the
 * variance of a shared runner, so they detect a change in kind -- a syscall,
 * a lock or an allocation added to the hot path -- rather than drift.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/latrec.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

/* Ceilings, in ns per stamp. A vDSO clock read is ~28 ns on x86-64 and the
 * cold cacheline adds the rest, placing the enabled path near 30-85 ns
 * depending on access pattern. */
constexpr double kMaxEnabledNs = 400.0;
constexpr double kMaxDisabledNs = 10.0;

double bench(const char* label, latrec_t* ring, int iters) {
    // Warm the ring: the first stamp on a fresh cacheline is excluded.
    for (int i = 0; i < 1000; i++) latrec_stamp(ring, 1, LATREC_L0_ENQUEUE, 0, 0);
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++) {
        latrec_stamp(ring, static_cast<uint64_t>(i), LATREC_L0_ENQUEUE,
                     static_cast<uint64_t>(i), 0);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count())
        / iters;
    std::printf("  %-42s %8.2f ns/stamp\n", label, ns);
    return ns;
}

}  // namespace

int main() {
    constexpr int kIters = 2000000;
    bool ok = true;

    std::printf("latrec stamp cost (%d iterations each)\n", kIters);

    // Disabled: what a production build with LATREC_DIR unset pays.
    unsetenv("LATREC_DIR");
    latrec_t off;
    latrec_open(&off, "bench.disabled", 16);
    const double disabled_ns = bench("disabled (LATREC_DIR unset)", &off, kIters);

    // Enabled: a ring large enough that most stamps touch a cold line, which
    // is the realistic case for a thread stamping a few times per slot.
    char tmpl[] = "/tmp/latrec_bench_XXXXXX";
    const char* dir = mkdtemp(tmpl);
    if (!dir) {
        std::fprintf(stderr, "ERROR: could not create a temp LATREC_DIR\n");
        return 1;
    }
    setenv("LATREC_DIR", dir, 1);
    latrec_t on;
    if (latrec_open(&on, "bench.enabled", 22) != 1) {
        std::fprintf(stderr, "ERROR: latrec_open failed with LATREC_DIR set\n");
        return 1;
    }
    std::printf("  (clock self-test: %u ns/call)\n",
                reinterpret_cast<const latrec_hdr*>(on.map)->clock_ns_per_call);
    const double enabled_ns = bench("enabled (128 MiB ring)", &on, kIters);
    latrec_close(&on);

    const std::string rm = std::string("rm -rf '") + dir + "'";
    if (system(rm.c_str()) != 0) { /* best effort */ }
    unsetenv("LATREC_DIR");

    if (enabled_ns > kMaxEnabledNs) {
        std::fprintf(stderr, "ERROR: enabled stamp %.2f ns exceeds %.2f ns ceiling\n",
                     enabled_ns, kMaxEnabledNs);
        ok = false;
    }
    if (disabled_ns > kMaxDisabledNs) {
        std::fprintf(stderr, "ERROR: disabled stamp %.2f ns exceeds %.2f ns ceiling"
                             " -- the LATREC_DIR gate is no longer free\n",
                     disabled_ns, kMaxDisabledNs);
        ok = false;
    }
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
