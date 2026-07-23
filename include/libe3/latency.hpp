/**
 * @file latency.hpp
 * @brief Optional latency-profiling log points for the E3 critical path.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compile-gated by LIBE3_LATENCY (enable with the CMake option
 * -DLIBE3_ENABLE_LATENCY=ON): off by default, so normal builds pay zero cost
 * and default log verbosity is untouched. When enabled, each site emits ONE
 * INFO line in a shared machine-parseable format so an offline collector can
 * grep and correlate interleaved gNB / E3 agent / dApp logs:
 *
 *   [LAT] stage=<name> t_ns=<monotonic ns> anchor_ns=<producer_ts|0> [k=v ...]
 *
 * t_ns comes from steady_clock, which on Linux/glibc is backed by
 * CLOCK_MONOTONIC with the same epoch as clock_gettime(CLOCK_MONOTONIC), so it
 * subtracts directly against the gNB (C) and aerial timestamps on the same
 * node. anchor_ns is the carried producer timestamp (0 on the control return
 * leg, where the collector pairs by consecutive line).
 *
 * Gating is the compile switch, NOT the log level: a profiling run does not
 * have to raise the logger to DEBUG (which would flood it with unrelated
 * debug output).
 */
#ifndef LIBE3_LATENCY_HPP
#define LIBE3_LATENCY_HPP

#include <chrono>
#include <cstdint>

#include "libe3/logger.hpp"

namespace libe3 {

/** Monotonic now in ns (== CLOCK_MONOTONIC on glibc). */
inline uint64_t lat_mono_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/** Wall-clock now in ns (== CLOCK_REALTIME on glibc). */
inline uint64_t lat_real_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

#ifndef LIBE3_LATENCY
/** Sink that swallows the trailing "<< k << v" chain when profiling is off. */
struct NullLatSink {
    template <typename T>
    NullLatSink& operator<<(const T&) {
        return *this;
    }
};
#endif

}  // namespace libe3

#ifdef LIBE3_LATENCY

// Returns the log stream so callers may append " k=v" pairs, e.g.
//   E3_LAT(LOG_TAG, "ind_wire_tx", 0) << " type=" << t;
#define E3_LAT(component, stage, anchor)                               \
    E3_LOG_INFO(component) << "[LAT] stage=" stage                     \
                           << " t_ns=" << ::libe3::lat_mono_ns()       \
                           << " anchor_ns=" << (anchor)

// Per-process run-start line: CLOCK_MONOTONIC <-> CLOCK_REALTIME for future
// cross-node (xApp) alignment. No-op for same-node Phase 1 correlation.
#define E3_LAT_CLOCK_OFFSET(component)                                 \
    E3_LOG_INFO(component) << "[LAT] clock_offset"                     \
                           << " mono_ns=" << ::libe3::lat_mono_ns()    \
                           << " real_ns=" << ::libe3::lat_real_ns()

#else /* !LIBE3_LATENCY */

#define E3_LAT(component, stage, anchor) \
    while (false) ::libe3::NullLatSink {}
#define E3_LAT_CLOCK_OFFSET(component) \
    while (false) ::libe3::NullLatSink {}

#endif /* LIBE3_LATENCY */

#endif  // LIBE3_LATENCY_HPP
