# libe3 Version Configuration
#
# SPDX-License-Identifier: Apache-2.0
#
# Read version from VERSION file (inspired by Python's hatchling approach)

# ============================================================================
# Read Version from File
# ============================================================================

set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

if(NOT EXISTS "${VERSION_FILE}")
    message(FATAL_ERROR "VERSION file not found at ${VERSION_FILE}")
endif()

file(READ "${VERSION_FILE}" VERSION_CONTENT)
string(STRIP "${VERSION_CONTENT}" LIBE3_VERSION)

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${LIBE3_VERSION}")
if(NOT CMAKE_MATCH_0)
    message(FATAL_ERROR "Invalid version format in VERSION file: ${LIBE3_VERSION}")
endif()

set(LIBE3_VERSION_MAJOR ${CMAKE_MATCH_1})
set(LIBE3_VERSION_MINOR ${CMAKE_MATCH_2})
set(LIBE3_VERSION_PATCH ${CMAKE_MATCH_3})

message(STATUS "libe3 version from VERSION file: ${LIBE3_VERSION}")

# ============================================================================
# Configure Version Header
# ============================================================================

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3_version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/libe3/version.hpp"
    @ONLY
)
