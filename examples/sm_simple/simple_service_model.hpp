/**
 * @file simple_service_model.hpp
 * @brief Shared Simple Service Model used by the example agent and the
 *        full-loop latency benchmark.
 *
 * This is the reference Service Model shipped with libe3. It is header-only
 * and placed under examples/ so both the example RAN agent
 * (example_simple_agent) and the integration latency benchmark
 * (test_bench_full_loop_latency) instantiate the *same* SM: the latency the
 * benchmark reports is therefore the latency of the SM users actually run.
 *
 * Pacing modes (@ref PacingMode):
 *   - FixedRate: emit one indication every period_us microseconds, paced by a
 *     compensated monotonic deadline. period_us == 0 floods at max rate (the
 *     pacer sleep is skipped), which is how the throughput sweep locates the
 *     saturation ceiling.
 *   - PingPong: closed-loop, one indication in flight at a time; the next
 *     indication is emitted only after the dApp's control action round-trips.
 *     Used by the latency benchmark to measure a clean, unpipelined round trip.
 *
 * RAN-side phase boundaries (collect / encode / send, and the
 * control-handler entry/exit) are stamped directly with latrec's LATREC_EX*
 * stage block (include/libe3/latrec.h), keyed on this SM's own indication
 * counter. Every stamp resolves to a no-op unless the library was built with
 * -DLIBE3_ENABLE_LATREC=ON, so this costs nothing in a normal build.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <libe3/libe3.hpp>
#include <libe3/latrec.h>
// Sibling header in this directory; the plain form resolves both when this
// header is reached via the examples/ include path (integration tests) and via
// the examples/sm_simple/ include path (example_simple_agent).
#include "e3sm_simple_wrapper.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace libe3_examples {

class SimpleServiceModel : public libe3::ServiceModel {
public:
    static constexpr uint32_t RAN_FUNCTION_ID = 1;

    /// How the indication worker is paced.
    enum class PacingMode {
        FixedRate,  ///< emit every period_us (period_us == 0 => flood/max rate)
        PingPong,   ///< one in flight; next emit gated on the control round trip
    };

    // period_us: microseconds between indication emissions (FixedRate). Sub-ms
    // values stress the conflate/queueing balance; the per-send log is silenced
    // below 1 ms so stdout I/O does not dominate the measurement. mode selects
    // the pacing model; it defaults to FixedRate so the historical two-argument
    // construction stays source-compatible. pingpong_fallback_ms is the PingPong
    // liveness-fallback timeout (see worker_loop); the 50 ms default keeps the
    // shipped SM snappy, and tests can widen it so a slow-runner wakeup delay
    // stays attributable to the control ack rather than the fallback timer.
    explicit SimpleServiceModel(uint64_t period_us = 2'000'000,
                                libe3::EncodingFormat encoding = libe3::EncodingFormat::ASN1,
                                PacingMode mode = PacingMode::FixedRate,
                                uint64_t pingpong_fallback_ms = 50)
        : period_us_(period_us), quiet_(period_us < 1000), encoding_(encoding),
          mode_(mode), pingpong_fallback_ms_(pingpong_fallback_ms) {}

    ~SimpleServiceModel() override { stop(); }

    std::string name() const override { return "SIMPLE"; }
    uint32_t version() const override { return 1; }

    uint32_t ran_function_id() const override {
        return RAN_FUNCTION_ID;
    }

    std::vector<uint32_t> telemetry_ids() const override {
        return {1};
    }

    std::vector<uint32_t> control_ids() const override {
        return {1};
    }

    libe3::ErrorCode init() override { return libe3::ErrorCode::SUCCESS; }

    void destroy() override {
        stop();
    }

    libe3::ErrorCode start() override {
        if (running_) {
            return libe3::ErrorCode::SUCCESS;
        }
        running_ = true;
        worker_ = std::thread(&SimpleServiceModel::worker_loop, this);
        return libe3::ErrorCode::SUCCESS;
    }

    std::vector<uint8_t> ran_function_data() const override {
        const std::string name = "SIMPLE";
        std::vector<uint8_t> out;
        if (libe3_examples::encode_ran_function_data(name, out, encoding_)) {
            return out;
        }
        return {};
    }

    void stop() override {
        if (!running_) {
            return;
        }
        running_ = false;
        // Wake a PingPong worker blocked on the emission gate so it can observe
        // !running_ and exit; FixedRate wakes on its own from sleep_until.
        if (mode_ == PacingMode::PingPong) {
            {
                std::lock_guard<std::mutex> lk(emit_mu_);
                may_emit_ = true;
            }
            emit_cv_.notify_all();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        if (!quiet_) {
            std::cout << "[SIMPLE] total indication cycles emitted: " << seq_ << "\n";
        }
    }

    bool is_running() const override { return running_; }

    libe3::ErrorCode handle_control_action(
        uint32_t request_message_id,
        const libe3::DAppControlAction& action
    ) override {
        const uint32_t seq = inflight_seq_.load(std::memory_order_relaxed);
        latrec_tstamp(seq, LATREC_EX3_CTRL_RECV, 0, 0);

        int sampling = 0;
        bool decode_ok = libe3_examples::decode_simple_control(action.action_data, sampling, encoding_);

        latrec_tstamp(seq, LATREC_EX4_CTRL_DONE, 0, 0);

        if (!quiet_) {
            if (decode_ok) {
                std::cout << "[SIMPLE] Control action " << action.control_identifier
                          << ": samplingThreshold=" << sampling << "\n";
            } else {
                std::cout << "[SIMPLE] Control action " << action.control_identifier
                          << ": failed to decode (" << action.action_data.size() << " bytes)\n";
            }
        }

        libe3::Pdu ack_pdu = make_message_ack_pdu(
            request_message_id,
            decode_ok ? libe3::ResponseCode::POSITIVE : libe3::ResponseCode::NEGATIVE
        );
        libe3::ErrorCode rc = emit_outbound(std::move(ack_pdu));

        // Closed-loop pacing: releasing the next emission only after the control
        // action lands keeps exactly one indication in flight.
        if (mode_ == PacingMode::PingPong) {
            allow_next_emit();
        }
        return rc;
    }

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
    uint32_t seq_{0};                        ///< worker-owned; read in stop() after join
    std::atomic<uint32_t> inflight_seq_{0};  ///< last emitted seq, for the control-phase hook
    uint64_t period_us_{2'000'000};
    bool quiet_{false};
    libe3::EncodingFormat encoding_{libe3::EncodingFormat::ASN1};
    PacingMode mode_{PacingMode::FixedRate};
    uint64_t pingpong_fallback_ms_{50};  ///< PingPong liveness-fallback timeout

    // PingPong emission gate.
    std::mutex emit_mu_;
    std::condition_variable emit_cv_;
    bool may_emit_{true};  // starts true so the first indication bootstraps

    // PingPong only: release the next indication emission.
    void allow_next_emit() {
        std::lock_guard<std::mutex> lk(emit_mu_);
        may_emit_ = true;
        emit_cv_.notify_one();
    }

    void worker_loop() {
        // This thread is spawned by the SM itself, not by E3Interface, so
        // nothing else opens its latrec ring; the EX0..EX2 stamps below (and
        // the latrec_ctx_set() bridge into LE0) are no-ops until this runs.
        latrec_tls_open_as("ex.worker");

        // Compensated pacing (FixedRate): sleep to an absolute monotonic
        // deadline that advances by exactly one period per cycle, so the offered
        // rate is 1/period regardless of how long the cycle's own work (encode +
        // emit) takes. A plain sleep_for(period) makes each cycle cost
        // period + work, which capped the offered rate well below the
        // configured one (~6.7 kHz instead of 10 kHz at a 100 us period).
        // If the loop falls behind (the next deadline is already in the past,
        // i.e. one period or more behind, e.g. after a scheduling stall), the
        // deadline is re-anchored instead of bursting to catch up, so the
        // instantaneous rate never exceeds the configured one. period_us_ == 0
        // skips the sleep entirely and floods at max rate.
        auto next = std::chrono::steady_clock::now();
        while (running_) {
            if (mode_ == PacingMode::PingPong) {
                // Wait until the previous control action releases the next emit.
                // The pingpong_fallback_ms_ timeout (50 ms by default) is a
                // liveness fallback: it re-drives the loop so the first
                // indication still bootstraps when the worker wakes before any
                // dApp has subscribed, and so a lost control action cannot wedge
                // the loop forever.
                std::unique_lock<std::mutex> lk(emit_mu_);
                emit_cv_.wait_for(lk, std::chrono::milliseconds(pingpong_fallback_ms_),
                                  [this]() { return !running_ || may_emit_; });
                if (!running_) {
                    break;
                }
                may_emit_ = false;
            } else if (period_us_ > 0) {
                next += std::chrono::microseconds(period_us_);
                const auto now = std::chrono::steady_clock::now();
                if (next > now) {
                    std::this_thread::sleep_until(next);
                } else {
                    next = now;  // behind schedule: re-anchor, don't burst
                }
            }
            if (!running_) {
                break;
            }

            auto subscribers = get_subscribers();
            if (subscribers.empty()) {
                continue;
            }

            const uint32_t seq = seq_;
            inflight_seq_.store(seq, std::memory_order_relaxed);
            latrec_tstamp(seq, LATREC_EX0_COLLECT_BEGIN, 0, 0);

            libe3_examples::SimpleIndication si;
            si.data1 = seq;  // monotonic; data1 range was widened in the SM grammar
            // Wall-clock send time in milliseconds, masked into the 31-bit
            // ASN.1 range. The dApp uses this to measure per-indication age
            // (transport + queueing delay) in the E2E tests: this lives only
            // in the example, the libe3 core does no timing. Note this clock
            // read sits inside the collect phase (the full-loop latency
            // benchmark bills it to phase 1) and populates the timestamp field,
            // so the encoded indication is a few bytes larger than a
            // zero-timestamp one; the benchmark therefore measures the SM that
            // actually ships, and its numbers are not comparable to runs that
            // used the earlier stripped-down inline copy.
            const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            si.timestamp = static_cast<uint32_t>(now_ms & 0x7FFFFFFF);

            latrec_tstamp(seq, LATREC_EX1_ENCODE_BEGIN, 0, 0);
            std::vector<uint8_t> encoded;
            if (!libe3_examples::encode_simple_indication(si, encoded, encoding_)) {
                std::cerr << "Failed to encode Simple-Indication\n";
                continue;
            }

            latrec_tstamp(seq, LATREC_EX2_SEND_INDICATION, 0, 0);
            // Bridges this SM's own seq to libe3's Pdu::enqueue_seq: LE0's aux
            // captures whatever latrec_ctx_set() last carried, which is what
            // lets a reader join the EX0..EX2 chain above to the L0.. outbound
            // leg below (see LATREC_LE0_EMIT_ENTER's doc comment).
            latrec_ctx_set(seq);
            for (uint32_t dapp_id : subscribers) {
                libe3::Pdu pdu = make_indication_pdu(dapp_id, RAN_FUNCTION_ID, encoded);
                auto rc = emit_outbound(std::move(pdu));
                if (rc == libe3::ErrorCode::SUCCESS) {
                    if (!quiet_) {
                        std::cout << "  -> Sent indication #" << seq_
                                  << " to dApp " << dapp_id
                                  << " (" << encoded.size() << " bytes)\n";
                    }
                } else if (!quiet_) {
                    // Quiet also suppresses failure logging: a PingPong bench
                    // (quiet) can hit transient NOT_INITIALIZED emits during
                    // startup, before the outbound path is wired, which the
                    // warmup absorbs; keep stdout/stderr clean for the CI table.
                    std::cerr << "  -> Failed to send indication to dApp "
                              << dapp_id << ": "
                              << libe3::error_code_to_string(rc) << "\n";
                }
            }
            ++seq_;
        }
    }
};

} // namespace libe3_examples
