/**
 * @file libe3.hpp
 * @brief libe3 umbrella header - includes all public headers
 *
 * Include this single header to get access to all libe3 functionality.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_LIBE3_HPP
#define LIBE3_LIBE3_HPP

// Version information (generated from VERSION file)
#include "version.hpp"

// Core types
#include "types.hpp"

// Logging utilities
#include "logger.hpp"

// Main facade
#include "e3_agent.hpp"

// Service Model interface for custom SMs
#include "sm_interface.hpp"

// Optional: Direct access to encoder/connector interfaces
// (usually not needed by library users)
#include "e3_encoder.hpp"
#include "e3_connector.hpp"

// Optional: Subscription manager access
// (usually accessed through E3Agent)
#include "subscription_manager.hpp"

// Optional: Response queue
// (usually accessed through E3Agent)
#include "response_queue.hpp"

namespace libe3 {

/**
 * @brief Get library version string
 */
inline const char* version() noexcept {
    return LIBE3_VERSION_STRING;
}

/**
 * @brief Get library version as integers
 */
inline void version(int& major, int& minor, int& patch) noexcept {
    major = LIBE3_VERSION_MAJOR;
    minor = LIBE3_VERSION_MINOR;
    patch = LIBE3_VERSION_PATCH;
}

} // namespace libe3

#endif // LIBE3_LIBE3_HPP
