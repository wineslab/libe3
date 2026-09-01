# libe3 Dependencies
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

# ============================================================================
# Required Dependencies
# ============================================================================

# Required: pthreads
find_package(Threads REQUIRED)

# Required: nlohmann/json for JSON encoding
if(LIBE3_ENABLE_JSON)
    find_package(nlohmann_json 3.11 QUIET)
    if(NOT nlohmann_json_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(nlohmann_json)
        message(STATUS "nlohmann/json: Fetched from GitHub")
    else()
        message(STATUS "nlohmann/json: Found installed version")
    endif()
endif()

# Required: Protocol Buffers (libprotobuf runtime + protoc compiler) for protobuf encoding
if(LIBE3_ENABLE_PROTOBUF)
    find_package(Protobuf REQUIRED)
    message(STATUS "Protobuf found: ${Protobuf_VERSION} (protoc: ${Protobuf_PROTOC_EXECUTABLE})")
endif()

# Required: tl::expected for C++17 std::expected-like functionality
include(FetchContent)
FetchContent_Declare(
    tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.1.0
    GIT_SHALLOW TRUE
)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tl_expected)
message(STATUS "tl::expected: Fetched from GitHub")

# ============================================================================
# Optional Dependencies
# ============================================================================

# Optional: SCTP. Header-only from libe3's point of view -- the connector uses
# IPPROTO_SCTP and SCTP_NODELAY from <netinet/sctp.h> and calls no libsctp
# function, so there is nothing to link. Checked here so -DLIBE3_ENABLE_SCTP=ON
# without the -dev package fails at configure time with the package name, rather
# than partway through the build with a bare "netinet/sctp.h: No such file".
if(LIBE3_ENABLE_SCTP)
    include(CheckIncludeFile)
    check_include_file("netinet/sctp.h" LIBE3_HAVE_SCTP_H)
    if(NOT LIBE3_HAVE_SCTP_H)
        message(FATAL_ERROR
            "LIBE3_ENABLE_SCTP=ON but <netinet/sctp.h> was not found. Install the "
            "SCTP headers (Debian/Ubuntu: libsctp-dev, Fedora/RHEL: "
            "lksctp-tools-devel, Arch: lksctp-tools), or configure with "
            "-DLIBE3_ENABLE_SCTP=OFF (the default).")
    endif()
endif()

# Optional: Google Benchmark for the integration micro-benchmarks
# (dev-only; never linked into the shipped library)
if(LIBE3_BUILD_INTEGRATION_TESTS)
    find_package(benchmark 1.8 QUIET)
    if(NOT benchmark_FOUND)
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(googlebenchmark)
        message(STATUS "Google Benchmark: Fetched from GitHub")
    else()
        message(STATUS "Google Benchmark: Found installed version")
    endif()
endif()

# Optional: ZeroMQ
if(LIBE3_ENABLE_ZMQ)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZMQ IMPORTED_TARGET libzmq)
    endif()
    
    if(NOT ZMQ_FOUND)
        find_library(ZMQ_LIBRARIES NAMES zmq)
        find_path(ZMQ_INCLUDE_DIRS NAMES zmq.h)
        if(ZMQ_LIBRARIES AND ZMQ_INCLUDE_DIRS)
            set(ZMQ_FOUND TRUE)
            add_library(PkgConfig::ZMQ INTERFACE IMPORTED)
            target_link_libraries(PkgConfig::ZMQ INTERFACE ${ZMQ_LIBRARIES})
            target_include_directories(PkgConfig::ZMQ INTERFACE ${ZMQ_INCLUDE_DIRS})
        endif()
    endif()
    
    if(ZMQ_FOUND)
        message(STATUS "ZeroMQ found: ${ZMQ_LIBRARIES}")
    else()
        message(WARNING "ZeroMQ not found, ZMQ transport disabled")
        set(LIBE3_ENABLE_ZMQ OFF PARENT_SCOPE)
    endif()
endif()
