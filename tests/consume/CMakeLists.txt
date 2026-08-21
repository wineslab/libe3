# Out-of-tree consumer check for the installed libe3 package.
#
# Deliberately a standalone project: libe3's own tests build in-tree, which is
# exactly why two packaging bugs (a .pc with no feature macros, and an exported
# target referring to an uninstalled tl::expected) went unnoticed. This is built
# against an install prefix, the way a real consumer sees us.
#
# Two routes, selected by LIBE3_CONSUME_MODE:
#   cmake      -- find_package(libe3 CONFIG REQUIRED), the target route
#   pkgconfig  -- pkg_check_modules(IMPORTED_TARGET libe3), the flags route
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(libe3_consume_check LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBE3_CONSUME_MODE "cmake" CACHE STRING "How to consume libe3: cmake or pkgconfig")

add_executable(consume main.cpp)

if(LIBE3_CONSUME_MODE STREQUAL "cmake")
    find_package(libe3 CONFIG REQUIRED)
    target_link_libraries(consume PRIVATE libe3::libe3)
elseif(LIBE3_CONSUME_MODE STREQUAL "pkgconfig")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBE3 REQUIRED IMPORTED_TARGET libe3)
    target_link_libraries(consume PRIVATE PkgConfig::LIBE3)
    # The point of the exercise: the feature macros must arrive from the .pc,
    # not from the consumer guessing. main.cpp asserts on them at compile time.
    message(STATUS "libe3 Cflags from pkg-config: ${LIBE3_CFLAGS_OTHER}")
else()
    message(FATAL_ERROR "LIBE3_CONSUME_MODE must be 'cmake' or 'pkgconfig'")
endif()
