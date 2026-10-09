# The libe3py SWIG module, shared by the in-tree build (libe3SWIG.cmake) and the
# pip build against an installed libe3 (python/CMakeLists.txt), so the two
# cannot drift apart.
#
#   libe3_add_python_module(<target> SOURCE_DIR <repo root> OUTPUT_DIR <dir> LINK <libe3 target> [INCLUDE <dir>...])
#
# Produces <OUTPUT_DIR>/_libe3py.so and <OUTPUT_DIR>/libe3py.py. The module links
# libe3 statically, so it needs no liblibe3.so at run time.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

find_package(SWIG 4.0 REQUIRED COMPONENTS python)
if(CMAKE_VERSION VERSION_LESS 3.18)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
endif()
include(UseSWIG)

function(libe3_add_python_module target)
    cmake_parse_arguments(ARG "" "SOURCE_DIR;OUTPUT_DIR;LINK" "INCLUDE" ${ARGN})
    set(_i "${ARG_SOURCE_DIR}/swig/libe3.i")
    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")
    set_property(SOURCE "${_i}" PROPERTY CPLUSPLUS ON)
    set(_swig_inc "${ARG_SOURCE_DIR}/swig" ${ARG_INCLUDE})
    set_property(SOURCE "${_i}" PROPERTY INCLUDE_DIRECTORIES ${_swig_inc})
    set(_swig_flags "")
    foreach(_d IN LISTS _swig_inc)
        list(APPEND _swig_flags "-I${_d}")
    endforeach()
    # latrec.h refuses to compile without CLOCK_MONOTONIC declared (see its own
    # #error), which SWIG's preprocessor never sees for real: it does not walk
    # glibc's actual <time.h>/<bits/time.h> chain the way the real compiler does
    # afterwards on the generated wrapper. The numeric value is never evaluated
    # by SWIG, only its definedness, so a placeholder satisfies the check.
    list(APPEND _swig_flags "-DCLOCK_MONOTONIC=1")
    set_property(SOURCE "${_i}" PROPERTY SWIG_FLAGS ${_swig_flags})

    swig_add_library(${target}
        TYPE SHARED
        LANGUAGE python
        SOURCES "${_i}" "${ARG_SOURCE_DIR}/swig/e3_dapp_session.cpp"
        OUTPUT_DIR "${ARG_OUTPUT_DIR}")
    set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIR}")
    target_include_directories(${target} PRIVATE "${ARG_SOURCE_DIR}/swig" ${ARG_INCLUDE})
    if(TARGET Python3::Module)
        target_link_libraries(${target} PRIVATE ${ARG_LINK} Python3::Module)
    else()
        target_include_directories(${target} PRIVATE ${Python3_INCLUDE_DIRS})
        target_link_libraries(${target} PRIVATE ${ARG_LINK} ${Python3_LIBRARIES})
    endif()
endfunction()
