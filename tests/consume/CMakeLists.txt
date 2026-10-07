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
# The E2SM-DAPP codec (libe3::e2sm_dapp) is checked when the install has it, in
# two ways: `consume` also packs and unpacks one message through it, next to the
# E3 agent (both runtimes in one executable), and `consume_e2sm_dapp` links the
# codec alone, from C++ and from C, which is how flexric uses it.
# LIBE3_CONSUME_E2SM_DAPP is AUTO (use it if installed), ON (require it:
# find_package(libe3 REQUIRED COMPONENTS e2sm_dapp)) or OFF.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(libe3_consume_check LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBE3_CONSUME_MODE "cmake" CACHE STRING "How to consume libe3: cmake or pkgconfig")

set(LIBE3_CONSUME_E2SM_DAPP "AUTO" CACHE STRING "Check libe3::e2sm_dapp: AUTO, ON or OFF")

add_executable(consume main.cpp)
set(CONSUME_E2SM_DAPP_TARGET "")

if(LIBE3_CONSUME_MODE STREQUAL "cmake")
    if(LIBE3_CONSUME_E2SM_DAPP STREQUAL "ON")
        find_package(libe3 CONFIG REQUIRED COMPONENTS e2sm_dapp)
    else()
        find_package(libe3 CONFIG REQUIRED OPTIONAL_COMPONENTS e2sm_dapp)
    endif()
    target_link_libraries(consume PRIVATE libe3::libe3)
    if(libe3_e2sm_dapp_FOUND AND NOT LIBE3_CONSUME_E2SM_DAPP STREQUAL "OFF")
        set(CONSUME_E2SM_DAPP_TARGET libe3::e2sm_dapp)
    endif()
elseif(LIBE3_CONSUME_MODE STREQUAL "pkgconfig")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBE3 REQUIRED IMPORTED_TARGET libe3)
    target_link_libraries(consume PRIVATE PkgConfig::LIBE3)
    if(NOT LIBE3_CONSUME_E2SM_DAPP STREQUAL "OFF")
        if(LIBE3_CONSUME_E2SM_DAPP STREQUAL "ON")
            pkg_check_modules(LIBE3_E2SM_DAPP REQUIRED IMPORTED_TARGET libe3_e2sm_dapp)
        else()
            pkg_check_modules(LIBE3_E2SM_DAPP IMPORTED_TARGET libe3_e2sm_dapp)
        endif()
        if(LIBE3_E2SM_DAPP_FOUND)
            set(CONSUME_E2SM_DAPP_TARGET PkgConfig::LIBE3_E2SM_DAPP)
        endif()
    endif()
    # The point of the exercise: the feature macros must arrive from the .pc,
    # not from the consumer guessing. main.cpp asserts on them at compile time.
    message(STATUS "libe3 Cflags from pkg-config: ${LIBE3_CFLAGS_OTHER}")
else()
    message(FATAL_ERROR "LIBE3_CONSUME_MODE must be 'cmake' or 'pkgconfig'")
endif()

if(CONSUME_E2SM_DAPP_TARGET)
    message(STATUS "libe3::e2sm_dapp found: also checking the E2SM-DAPP codec (${CONSUME_E2SM_DAPP_TARGET})")
    target_link_libraries(consume PRIVATE ${CONSUME_E2SM_DAPP_TARGET})
    target_compile_definitions(consume PRIVATE CONSUME_HAVE_E2SM_DAPP)

    # The codec alone, with no libe3 and no ZeroMQ: C++ and C.
    add_executable(consume_e2sm_dapp e2sm_dapp.cpp e2sm_dapp_c.c)
    target_link_libraries(consume_e2sm_dapp PRIVATE ${CONSUME_E2SM_DAPP_TARGET})

    # The same check against the shared codec library (CMake route only).
    if(TARGET libe3::e2sm_dapp_shared)
        add_executable(consume_e2sm_dapp_shared e2sm_dapp.cpp e2sm_dapp_c.c)
        target_link_libraries(consume_e2sm_dapp_shared PRIVATE libe3::e2sm_dapp_shared)
    endif()
else()
    message(STATUS "libe3::e2sm_dapp not checked (not installed, or LIBE3_CONSUME_E2SM_DAPP=OFF)")
endif()
