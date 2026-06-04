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
    E3Agent(E3Config config);
    ~E3Agent();

    ErrorCode init();
    ErrorCode start();
    void stop();
    AgentState state() const noexcept;
    bool is_running() const noexcept;

    // Manual RAN-side operations (kept narrow — SM registration is not
    // wrapped; spear-dApp keeps its Python SM encoders).
    ErrorCode send_indication(uint32_t dapp_id,
                              uint32_t ran_function_id,
                              const std::vector<uint8_t>& data);
    ErrorCode send_message_ack(uint32_t request_id, ResponseCode response_code);

    // dApp-side verbs.
    ErrorCode unsubscribe(uint32_t ran_function_id);
    ErrorCode send_control(uint32_t ran_function_id,
                           uint32_t control_id,
                           std::vector<uint8_t> action_data);
    ErrorCode send_report(uint32_t ran_function_id,
                          std::vector<uint8_t> report_data);
    ErrorCode release();
    std::vector<uint32_t> subscribed_ran_functions() const;
    std::vector<uint32_t> active_subscription_ids() const;

    const E3Config& config() const noexcept;
    size_t dapp_count() const;
    size_t subscription_count() const;
};

}  // namespace libe3

#endif  // LIBE3_SWIG_HPP
