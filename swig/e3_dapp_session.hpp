/**
 * @file e3_dapp_session.hpp
 * @brief Python-facing dApp session over libe3 — batched, low-latency seam.
 *
 * @section why Why this class exists
 * libe3's core @ref libe3::E3Agent drives the dApp role with `std::function`
 * handlers (@ref libe3::E3Agent::set_indication_handler et al.). SWIG cannot
 * bridge those callbacks to Python across libe3's inbound worker threads
 * without either running Python under the GIL *on the hot thread* (directors —
 * deadlock-prone, serialises dispatch) or paying a GIL round-trip per message.
 * Both are unacceptable at the E3AP target rate (real-time, sub-millisecond
 * cadence, very high message throughput).
 *
 * @ref libe3::py::DAppSession is the answer: it owns an @ref libe3::E3Agent in
 * DAPP role, registers the C++ handlers itself, and funnels every inbound
 * message into a **bounded lock-free ring** (@ref libe3::LockFreeQueue). Python
 * drains that ring in **batches** via @ref poll_events, so one GIL acquire and
 * one Python↔C++ boundary crossing are amortised across many messages. The
 * SWIG wrapper releases the GIL around the blocking calls (see swig/libe3.i,
 * `%module(threads="1")`), so libe3's C++ threads never stall on Python.
 *
 * The E3SM (service-model) payloads stay opaque here (`std::vector<uint8_t>`);
 * dApp-library encodes/decodes them in Python. This class is E3AP only, mirroring
 * the OAI split where libe3 owns E3AP and the SM owns encode/decode.
 *
 * @section lifecycle Lifecycle
 * ctor (registers handlers) → @ref start → @ref wait_for_setup →
 * (read setup via @ref dapp_id / setup accessors) → @ref subscribe →
 * loop: @ref poll_events + @ref send_control / @ref send_report →
 * @ref release → @ref stop.
 *
 * @section rejected Rejected alternatives (auditable)
 * - SWIG directors: run the Python callback on libe3's inbound thread under the
 *   GIL — serialises dispatch, deadlock-prone if Python re-enters libe3.
 * - per-message poll: one GIL round-trip + one boundary crossing per message
 *   caps throughput; batching is strictly better with no added latency (the
 *   ring still wakes on the first event).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_PY_E3_DAPP_SESSION_HPP
#define LIBE3_PY_E3_DAPP_SESSION_HPP

#include <libe3/types.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace libe3 {
namespace py {

/**
 * @brief Discriminator for @ref E3Event.
 */
enum E3EventKind {
    E3_EVENT_NONE = 0,                  ///< placeholder (never enqueued)
    E3_EVENT_INDICATION = 1,            ///< RAN → dApp IndicationMessage
    E3_EVENT_XAPP_CONTROL = 2,          ///< xApp → dApp XAppControlAction
    E3_EVENT_SUBSCRIPTION_RESPONSE = 3, ///< RAN → dApp SubscriptionResponse
    E3_EVENT_SETUP_RESPONSE = 4,        ///< RAN → dApp SetupResponse
    E3_EVENT_MESSAGE_ACK = 5            ///< RAN → dApp MessageAck
};

/**
 * @brief One inbound E3AP event, POD-flat for a clean SWIG crossing.
 *
 * `payload` carries the opaque E3SM bytes for indications
 * (protocol_data) and xApp controls (xapp_control_data); it is empty for
 * subscription/setup responses and acks. Fields not relevant to a given
 * `kind` are left zero.
 */
struct E3Event {
    int kind{E3_EVENT_NONE};        ///< one of @ref E3EventKind
    uint32_t dapp_id{0};            ///< dApp identifier the message targets
    uint32_t ran_function_id{0};    ///< RAN function (indication / xApp control)
    uint32_t subscription_id{0};    ///< subscription id (subscription response)
    uint32_t request_id{0};         ///< request id (subscription response / ack)
    int response_code{-1};          ///< 0=positive, 1=negative, -1=n/a
    std::vector<uint8_t> payload;   ///< opaque E3SM bytes (indication / xApp control)
    // NOTE: in Python (libe3py) the payload is read via ev.get_payload(), which
    // returns native `bytes`. SWIG member getters return a wrapped vector, so the
    // binding exposes the payload as a by-value method instead (see swig/libe3.i).
};

/**
 * @brief dApp-role E3AP session with a batched, lock-free inbound ring.
 *
 * Thread model: two producers push into the ring — the setup-response handler
 * (libe3 setup thread) and the other handlers (libe3 inbound thread), which run
 * concurrently at startup; the Python drainer thread is the single consumer via
 * @ref poll_events. The ring is MPMC, so this is safe (do not "optimize" it to
 * SPSC). All outbound verbs are thread-safe (delegated to @ref libe3::E3Agent).
 */
class DAppSession {
public:
    /**
     * @brief Construct the session and register all inbound handlers.
     * @param config Agent config; `role` is forced to @ref E3Role::DAPP.
     * @param queue_capacity Inbound ring capacity (rounded up to a power of
     *        two). Sized for high-throughput headroom; overflow increments the
     *        drop counter (@ref dropped_events) rather than blocking libe3.
     */
    explicit DAppSession(libe3::E3Config config, std::size_t queue_capacity = 8192);
    ~DAppSession();

    DAppSession(const DAppSession&) = delete;
    DAppSession& operator=(const DAppSession&) = delete;

    /** @brief Start the agent, re-arming the inbound ring so a stop()->start()
     *  cycle works again. @return ErrorCode as int. */
    int start();
    /** @brief Block until the setup handshake completes. @return ErrorCode as int. */
    int wait_for_setup(int timeout_ms);
    /** @brief Send a ReleaseMessage (session keeps running until @ref stop). @return ErrorCode as int. */
    int release();
    /** @brief Stop the agent and shut down the ring so a blocked @ref poll_events
     *  returns an empty batch (i.e. empty-after-stop means "done"). Restartable
     *  via @ref start. */
    void stop();

    /** @brief dApp id assigned at setup, or -1 if not yet assigned. */
    long dapp_id() const;
    /** @brief RAN identifier from the SetupResponse, or "" if none. */
    std::string ran_identifier() const;

    /** @brief SetupResponse code: 0=positive, 1=negative, -1=not received. */
    int setup_response_code() const;
    /** @brief Number of RAN functions advertised in the SetupResponse. */
    std::size_t setup_ran_function_count() const;
    /** @brief RAN function id of advertised entry @p i (0 if out of range). */
    uint32_t setup_ran_function_id(std::size_t i) const;
    /** @brief Telemetry ids of advertised entry @p i. */
    std::vector<uint32_t> setup_ran_function_telemetry(std::size_t i) const;
    /** @brief Control ids of advertised entry @p i. */
    std::vector<uint32_t> setup_ran_function_control(std::size_t i) const;
    /** @brief Opaque ranFunctionData bytes of advertised entry @p i. */
    std::vector<uint8_t> setup_ran_function_data(std::size_t i) const;

    /**
     * @brief Subscribe to a RAN function (after a positive setup).
     * @param sub_time_ms subscription time, or -1 for unset.
     * @param periodicity delivery periodicity (µs), or -1 for unset.
     * @return ErrorCode as int.
     */
    int subscribe(uint32_t ran_function_id,
                  std::vector<uint32_t> telemetry_ids,
                  std::vector<uint32_t> control_ids,
                  int sub_time_ms,
                  int periodicity);
    /** @brief Delete a previously created subscription. @return ErrorCode as int. */
    int unsubscribe(uint32_t ran_function_id);

    /** @brief Send a dApp control action to the RAN. @return ErrorCode as int. */
    int send_control(uint32_t ran_function_id, uint32_t control_id,
                     std::vector<uint8_t> action_data);
    /** @brief Send a dApp report to the RAN. @return ErrorCode as int. */
    int send_report(uint32_t ran_function_id, std::vector<uint8_t> report_data);
    /** @brief Acknowledge a request. @param response_code 0=positive,1=negative. @return ErrorCode as int. */
    int send_message_ack(uint32_t request_id, int response_code);

    /**
     * @brief Drain up to @p max_batch inbound events, waiting up to
     *        @p timeout_ms for the first.
     *
     * Blocks on the ring for the first event (latency ≈ ring wakeup), then
     * sweeps up any further already-queued events without blocking.
     * @p timeout_ms <= 0 polls non-blocking (does not busy-wait). Returns an
     * empty vector on a quiet tick or after @ref stop. The SWIG wrapper releases
     * the GIL for the whole call so libe3's threads run freely while Python waits.
     */
    std::vector<E3Event> poll_events(std::size_t max_batch, int timeout_ms);

    /** @brief Total inbound events dropped due to ring overflow (backpressure). */
    unsigned long long dropped_events() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace py
} // namespace libe3

#endif // LIBE3_PY_E3_DAPP_SESSION_HPP
