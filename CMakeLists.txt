# libe3 - Vendor-Neutral E3AP C++ Library
#
# SPDX-License-Identifier: Apache-2.0
#
# Modern CMake (3.16+) configuration for building libe3

cmake_minimum_required(VERSION 3.16)

project(libe3
    VERSION 0.1.0
    DESCRIPTION "Vendor-neutral E3AP C++ library for RAN integration"
    LANGUAGES CXX C
)

# ============================================================================
# CMake Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add cmake directory to module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# Build Configuration
# ============================================================================
include(libe3Options)
include(libe3Compiler)
include(libe3Dependencies)

# ============================================================================
# ASN.1 Messages Library
# ============================================================================
add_subdirectory(messages)

# ============================================================================
# Library
# ============================================================================
include(libe3Sources)
include(libe3Targets)

# ============================================================================
# Tests & Examples
# ============================================================================
include(libe3Tests)
include(libe3Examples)

# ============================================================================
# Installation
# ============================================================================
include(libe3Install)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "libe3 Configuration Summary")
message(STATUS "===========================")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build Tests:      ${LIBE3_BUILD_TESTS}")
message(STATUS "  Build Examples:   ${LIBE3_BUILD_EXAMPLES}")
message(STATUS "  ZeroMQ Support:   ${LIBE3_ENABLE_ZMQ}")
message(STATUS "  AddressSanitizer: ${LIBE3_ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer:  ${LIBE3_ENABLE_TSAN}")
message(STATUS "")
