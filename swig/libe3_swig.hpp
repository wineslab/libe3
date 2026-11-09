/* SWIG-friendly re-declarations of the libe3 public API.
 *
 * SWIG 4.1 has trouble parsing some modern C++ constructs in the real
 * headers (e.g. brace-initialised default arguments, std::function
 * typedefs, std::variant). This file mirrors the slice of the API we
 * want to expose to Python with SWIG-compatible declarations only.
 *
 * The actual implementations live in libe3 — this header exists purely
 * so SWIG can parse simple declarations without needing C++-aware
 * type recovery.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SWIG_HPP
#define LIBE3_SWIG_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

#include "libe3/types.hpp"

namespace libe3 {

// Forward-declared; real class lives in e3_agent.hpp. SWIG sees only
// these signatures.
class E3Agent {
public:
    /// Build the agent. @param config Role, links, encoding and limits, as for libe3::E3Agent
    E3Agent(E3Config config);
    ~E3Agent();

    /// Prepare the agent. @return ErrorCode::SUCCESS on success
    ErrorCode init();
    /// Start the agent. @return ErrorCode::SUCCESS on success
    ErrorCode start();
    /// Stop the agent and join its threads.
    void stop();
    /// @return The state of the agent
    AgentState state() const noexcept;
    /// @return True while the agent runs
    bool is_running() const noexcept;

    // Manual RAN-side operations (kept narrow — SM registration is not
    // wrapped; the Python consumer keeps its own SM encoders).
    /// Send an indication to one dApp (RAN role).
    /// @param dapp_id dApp the indication is for
    /// @param ran_function_id RAN function it comes from
    /// @param data opaque Service Model payload
    /// @return ErrorCode::SUCCESS once queued
    ErrorCode send_indication(uint32_t dapp_id,
                              uint32_t ran_function_id,
                              const std::vector<uint8_t>& data);

    /// Send a message acknowledgment. DEPRECATED TO BE REMOVED.
    /// @param request_id request being acknowledged
    /// @param response_code POSITIVE or NEGATIVE
    /// @return ErrorCode::SUCCESS once queued
    ErrorCode send_message_ack(uint32_t request_id, ResponseCode response_code);

    // dApp-side verbs.
    /// Delete a subscription (dApp role). @param ran_function_id RAN function it was for
    /// @return ErrorCode::SUCCESS once queued
    ErrorCode unsubscribe(uint32_t ran_function_id);
    // sequence_id correlates a message with the procedure it belongs to:
    // mandatory on a report, which starts one, and left 0 on a control the dApp
    // decided on its own. out_message_id and request_ack are not mirrored -- an
    // out-pointer is not worth wrapping, and DAppSession carries both.
    /// Send a dApp control (dApp role).
    /// @param ran_function_id RAN function the control is for
    /// @param control_id control identifier granted by the RAN
    /// @param action_data opaque Service Model payload
    /// @param sequence_id correlation id of the xApp procedure this control re-issues, or 0
    /// @return ErrorCode::SUCCESS once queued
    ErrorCode send_control(uint32_t ran_function_id,
                           uint32_t control_id,
                           std::vector<uint8_t> action_data,
                           uint32_t sequence_id = 0);
    /// Send a dApp report (dApp role).
    /// @param ran_function_id RAN function the report is for
    /// @param report_data opaque Service Model payload
    /// @param sequence_id correlation id the dApp assigns to the detection, not 0
    /// @return ErrorCode::SUCCESS once queued
    ErrorCode send_report(uint32_t ran_function_id,
                          std::vector<uint8_t> report_data,
                          uint32_t sequence_id);
    /// Release this dApp (dApp role). @return ErrorCode::SUCCESS once queued
    ErrorCode release();
    /// @return The RAN functions this dApp holds a subscription to
    std::vector<uint32_t> subscribed_ran_functions() const;
    /// @return The subscription identifiers the RAN granted and that have not ended
    std::vector<uint32_t> active_subscription_ids() const;

    /// @return The configuration the agent was built with
    const E3Config& config() const noexcept;
    /// @return The dApps registered with this RAN
    size_t dapp_count() const;
    /// @return The subscriptions this RAN holds
    size_t subscription_count() const;
};

}  // namespace libe3

#endif  // LIBE3_SWIG_HPP
