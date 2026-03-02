# libe3 Dependencies
#
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
