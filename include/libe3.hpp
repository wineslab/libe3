/**
 * @file libe3.hpp
 * @brief Main header for libe3 - E3AP Protocol Library
 *
 * Include this header to get access to all public libe3 APIs.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_HPP
#define LIBE3_HPP

// Version information (generated from VERSION file)
#include "libe3/version.hpp"

// Core types
#include "libe3/types.hpp"

// Main facade (the only class RAN vendors need to use)
#include "libe3/e3_agent.hpp"

// Service Model interface (for implementing custom SMs)
#include "libe3/sm_interface.hpp"

// Logging (optional - for custom log integration)
#include "libe3/logger.hpp"

namespace libe3 {

/**
 * @brief Get libe3 version string
 */
inline const char* version() noexcept {
    return LIBE3_VERSION_STRING;
}

/**
 * @brief Get libe3 version as components
 */
inline void version(int& major, int& minor, int& patch) noexcept {
    major = LIBE3_VERSION_MAJOR;
    minor = LIBE3_VERSION_MINOR;
    patch = LIBE3_VERSION_PATCH;
}

} // namespace libe3

#endif // LIBE3_HPP
