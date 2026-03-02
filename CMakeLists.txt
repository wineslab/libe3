# libe3 - Vendor-Neutral E3AP C++ Library
#
# SPDX-License-Identifier: Apache-2.0
#
# Modern CMake (3.16+) configuration for building libe3

cmake_minimum_required(VERSION 3.16)

# Read version from VERSION file before project() call
set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if(NOT EXISTS "${VERSION_FILE}")
    message(FATAL_ERROR "VERSION file not found at ${VERSION_FILE}")
endif()
file(READ "${VERSION_FILE}" LIBE3_VERSION)
string(STRIP "${LIBE3_VERSION}" LIBE3_VERSION)

project(libe3
    VERSION ${LIBE3_VERSION}
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
include(libe3Version)

# ============================================================================
# Mutual exclusion between ASN.1 and JSON encoders
# ============================================================================
if(LIBE3_ENABLE_ASN1 AND LIBE3_ENABLE_JSON)
    message(FATAL_ERROR "Both LIBE3_ENABLE_ASN1 and LIBE3_ENABLE_JSON are enabled. Please enable only one encoding: ASN.1 or JSON.")
endif()

# ============================================================================
# ASN.1 Messages Library
# ============================================================================
if(LIBE3_ENABLE_ASN1)
    add_subdirectory(messages)
else()
    message(STATUS "ASN.1 encoding support disabled by configuration")
endif()

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
include(libe3Docs)

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
