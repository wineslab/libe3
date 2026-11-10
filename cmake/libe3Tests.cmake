# libe3 Unit Tests
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_BUILD_TESTS)
    return()
endif()

enable_testing()

# Simple test framework header
add_library(libe3_test_framework INTERFACE)
target_include_directories(libe3_test_framework INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

file(GLOB LIBE3_TEST_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "tests/*.cpp")

# Tests that hardcode the ASN.1 wire format: test_asn1_size exercises the
# APER encoder directly, and test_e2e_report_path configures its agent and
# fake-dApp peer with EncodingFormat::ASN1. On builds with
# LIBE3_ENABLE_ASN1=OFF the encoder factory cannot create an ASN.1 encoder,
# so these tests can only fail; exclude them instead of weakening them.
set(LIBE3_ASN1_ONLY_TESTS
    asn1_boundary
    asn1_golden
    asn1_host_interposer
    asn1_size
    e2e_report_path
)

# Tests that parse JSON with nlohmann: skipped when LIBE3_ENABLE_JSON=OFF.
set(LIBE3_JSON_ONLY_TESTS
    interop_aerial
    json_encoder
)

# Tests that drive a raw ZMQ peer against the agent: they include <zmq.h> and
# link libzmq directly, so with LIBE3_ENABLE_ZMQ=OFF there is nothing for them
# to link against. Same treatment as the ASN.1-only list above.
set(LIBE3_ZMQ_ONLY_TESTS
    dapp_setup_retry_zmq
    e2e_report_path
    role_aware_connector_zmq
    setup_bad_request
)

# E2SM-DAPP codec tests (tests/test_e2sm_dapp_*.cpp). They link libe3::e2sm_dapp
# alone, which is the point: the codec must work without libe3, so only the
# link-coexist test also links libe3. Each of encode/decode/roundtrip/c_api is
# built twice, against the static and against the shared codec library. Skipped
# when the codec is not built (LIBE3_ENABLE_E2SM_DAPP=OFF, or LIBE3_ENABLE_ASN1=OFF).
function(libe3_add_e2sm_dapp_test target src)
    cmake_parse_arguments(ARG "" "CODEC" "EXTRA_LIBS" ${ARGN})
    add_executable(${target} "${src}")
    target_link_libraries(${target}
        PRIVATE
            ${ARG_CODEC}
            ${ARG_EXTRA_LIBS}
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

foreach(test_src IN LISTS LIBE3_TEST_SOURCES)
    # Derive a target name from the source file name: tests/test_foo.cpp -> test_foo
    get_filename_component(test_name ${test_src} NAME_WE)
    # If the filename already starts with 'test_', strip it to avoid 'test_test_...' targets
    string(REGEX REPLACE "^test_" "" simple_name ${test_name})
    set(target_name "test_${simple_name}")
    # Skip tests that require optional components when those components are disabled
    if(NOT LIBE3_ENABLE_JSON AND simple_name IN_LIST LIBE3_JSON_ONLY_TESTS)
        message(STATUS "Skipping test_${simple_name}: JSON support disabled")
        continue()
    endif()
    if(NOT LIBE3_ENABLE_PROTOBUF AND simple_name STREQUAL "protobuf_encoder")
        message(STATUS "Skipping test_protobuf_encoder: Protobuf support disabled")
        continue()
    endif()
    if(NOT LIBE3_ENABLE_ASN1 AND simple_name IN_LIST LIBE3_ASN1_ONLY_TESTS)
        message(STATUS "Skipping test_${simple_name}: ASN.1 support disabled")
        continue()
    endif()
    if(NOT LIBE3_ENABLE_ZMQ AND simple_name IN_LIST LIBE3_ZMQ_ONLY_TESTS)
        message(STATUS "Skipping test_${simple_name}: ZeroMQ support disabled")
        continue()
    endif()
    if(simple_name MATCHES "^e2sm_dapp_")
        if(NOT LIBE3_ENABLE_E2SM_DAPP)
            message(STATUS "Skipping test_${simple_name}: E2SM-DAPP codec not built")
            continue()
        endif()
        set(_e2sm_src "${CMAKE_CURRENT_SOURCE_DIR}/${test_src}")
        if(simple_name STREQUAL "e2sm_dapp_link_coexist")
            # Both libraries in one executable, each carrying its own asn1c
            # runtime: static libe3 + static codec, and shared libe3 + static codec.
            set(_zmq_libs "")
            if(LIBE3_ENABLE_ZMQ AND TARGET PkgConfig::ZMQ)
                set(_zmq_libs PkgConfig::ZMQ)
            endif()
            libe3_add_e2sm_dapp_test(${target_name} "${_e2sm_src}"
                CODEC libe3::e2sm_dapp EXTRA_LIBS libe3::libe3 ${_zmq_libs})
            libe3_add_e2sm_dapp_test(${target_name}_shared_libe3 "${_e2sm_src}"
                CODEC libe3::e2sm_dapp EXTRA_LIBS libe3::shared ${_zmq_libs})
        else()
            libe3_add_e2sm_dapp_test(${target_name} "${_e2sm_src}" CODEC libe3::e2sm_dapp)
            libe3_add_e2sm_dapp_test(${target_name}_shared "${_e2sm_src}" CODEC libe3::e2sm_dapp_shared)
        endif()
        continue()
    endif()
    # A host that exports asn1c functions of its own and links libe3's shared library.
    if(simple_name STREQUAL "asn1_host_interposer")
        if(NOT (UNIX AND NOT APPLE))
            message(STATUS "Skipping test_asn1_host_interposer: needs ELF symbol interposition")
            continue()
        endif()
        add_executable(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/${test_src}")
        target_link_libraries(${target_name}
            PRIVATE
                libe3::shared
                libe3_test_framework
                libe3_warnings
                libe3_sanitizers
                ${CMAKE_DL_LIBS}
        )
        set_target_properties(${target_name} PROPERTIES ENABLE_EXPORTS ON)
        add_test(NAME ${target_name} COMMAND ${target_name})
        continue()
    endif()
    # test_latrec.cpp exercises latrec.h's TLS convenience layer
    # (latrec_tls_open_as/latrec_tstamp/latrec_seq_next/latrec_ctx*) directly,
    # which are true no-op inline stubs -- not the real symbols -- unless the
    # library was built with LIBE3_ENABLE_LATREC=ON (see include/libe3/latrec.h).
    if(NOT LIBE3_ENABLE_LATREC AND simple_name STREQUAL "latrec")
        message(STATUS "Skipping test_latrec: latrec disabled (LIBE3_ENABLE_LATREC=OFF)")
        continue()
    endif()

    add_executable(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/${test_src}")
    target_link_libraries(${target_name}
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )

    if(LIBE3_ENABLE_ZMQ AND TARGET PkgConfig::ZMQ)
        target_link_libraries(${target_name} PRIVATE PkgConfig::ZMQ)
    endif()

    # Where the vendored peer messages live (tests/fixtures/README.md).
    if(simple_name STREQUAL "interop_aerial")
        target_compile_definitions(${target_name} PRIVATE
            LIBE3_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    endif()

    # The grammars it reads.
    if(simple_name STREQUAL "grammar_no_utf8string")
        target_compile_definitions(${target_name} PRIVATE
            LIBE3_GRAMMAR_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()

# No shared library exports a symbol of the asn1c runtime (cmake/libe3CheckAsn1Exports.cmake reads
# the dynamic symbol table, so it needs an ELF platform).
if(LIBE3_ENABLE_ASN1 AND UNIX AND NOT APPLE)
    set(_libe3_exports "$<TARGET_FILE:libe3_shared>")
    if(LIBE3_ENABLE_E2SM_DAPP)
        string(APPEND _libe3_exports "|$<TARGET_FILE:libe3_e2sm_dapp_shared>")
    endif()
    add_test(NAME test_asn1_exports
        COMMAND ${CMAKE_COMMAND} "-DLIBRARIES=${_libe3_exports}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3CheckAsn1Exports.cmake")
endif()

# libe3/asn1_layout_check.h refuses a unit generated with the other JER setting. The matching unit
# is part of every build. The mismatching one is a unit that says it was generated with
# -no-gen-JER (the macro asn1c writes into asn_config.h, here given on the command line) against a
# runtime that has JER; the test builds it and passes when the compiler stops at the #error. It is
# registered for a libe3 built with JSON, whose runtime has JER.
if(LIBE3_ENABLE_ASN1 AND UNIX)
    add_library(libe3_layout_guard_match OBJECT tests/layout_guard_check.c)
    target_link_libraries(libe3_layout_guard_match PRIVATE asn1_e3ap)
    target_include_directories(libe3_layout_guard_match PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
    if(LIBE3_ASN1_JER)
        add_library(libe3_layout_guard_mismatch OBJECT EXCLUDE_FROM_ALL tests/layout_guard_check.c)
        target_link_libraries(libe3_layout_guard_mismatch PRIVATE asn1_e3ap)
        target_include_directories(libe3_layout_guard_mismatch PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
        target_compile_definitions(libe3_layout_guard_mismatch PRIVATE ASN_DISABLE_JER_SUPPORT=1)
        add_test(NAME test_asn1_layout_guard_refuses_mismatch
            COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target libe3_layout_guard_mismatch)
        set_tests_properties(test_asn1_layout_guard_refuses_mismatch PROPERTIES
            PASS_REGULAR_EXPRESSION "libe3's asn1c runtime has JER on")
    endif()
endif()

# libe3_check_asn1c_version() against stand-in compilers that print the banner of the pin, of the
# next release, of OCUDU's commit and of the commit libe3 pinned before. The pin and the release are
# accepted without a word, OCUDU's with a warning, the old one refused.
if(LIBE3_ENABLE_ASN1 AND UNIX)
    set(_asn1c_banner_pin "ASN.1 Compiler, v1.5.3-124-ge4ee5e48-2026-11-02 (/tmp/asn1c) Copyright (c) 2003-2017 Lev Walkin")
    set(_asn1c_banner_release "ASN.1 Compiler, v1.5.4 (/tmp/asn1c) Copyright (c) 2003-2017 Lev Walkin")
    set(_asn1c_banner_ocudu "ASN.1 Compiler, v1.5.1-44-g844f9caa-2026-07-24 (/tmp/asn1c) Copyright (c) 2003-2017 Lev Walkin")
    set(_asn1c_banner_old "ASN.1 Compiler, v0.9.29 Copyright (c) 2003-2017 Lev Walkin")
    foreach(_case pin release ocudu old)
        add_test(NAME test_asn1c_version_${_case}
            COMMAND ${CMAKE_COMMAND}
                "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/asn1c_version_${_case}"
                "-DBANNER=${_asn1c_banner_${_case}}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3CheckAsn1cVersion.cmake")
    endforeach()
    set_tests_properties(test_asn1c_version_pin test_asn1c_version_release PROPERTIES
        FAIL_REGULAR_EXPRESSION "CMake Warning|CMake Error|older than")
    set_tests_properties(test_asn1c_version_ocudu PROPERTIES
        PASS_REGULAR_EXPRESSION "older than[ \n]+1.5.3-124")
    set_tests_properties(test_asn1c_version_old PROPERTIES
        PASS_REGULAR_EXPRESSION "older than[ \n]+1.5.1-44")
endif()

# Integration tests in tests/integration/ are opt-in via
# LIBE3_BUILD_INTEGRATION_TESTS=ON. They require ASN.1 encoding and the
# example_simple_agent/example_simple_dapp executables for cross-process
# scenarios. Each test gets the "integration" CTest label so callers can
# `ctest -L integration` (or skip them with `ctest -LE integration`).
#
# These specifically exercise latrec's TLS convenience layer (or, for
# simple_sm_modes, the shipped reference SM's own stamps), which are no-ops
# without LIBE3_ENABLE_LATREC=ON: skip them rather than let them build and
# fail (or pass vacuously) against an empty capture.
set(LIBE3_LATREC_ONLY_TESTS
    bench_full_loop_latency
    bench_latrec_load
    latrec_drops
    latrec_stages
    simple_sm_modes
)

if(LIBE3_BUILD_INTEGRATION_TESTS AND LIBE3_ENABLE_ASN1)
    file(GLOB LIBE3_INTEGRATION_TEST_SOURCES
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "tests/integration/*.cpp")
    foreach(test_src IN LISTS LIBE3_INTEGRATION_TEST_SOURCES)
        get_filename_component(test_name ${test_src} NAME_WE)
        string(REGEX REPLACE "^test_" "" simple_name ${test_name})
        if(NOT LIBE3_ENABLE_LATREC AND simple_name IN_LIST LIBE3_LATREC_ONLY_TESTS)
            message(STATUS "Skipping test_${simple_name}: latrec disabled (LIBE3_ENABLE_LATREC=OFF)")
            continue()
        endif()
        set(target_name "test_${simple_name}")
        add_executable(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/${test_src}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/sm_simple/e3sm_simple_wrapper.cpp")
        target_link_libraries(${target_name}
            PRIVATE
                libe3::libe3
                libe3_test_framework
                libe3_warnings
                libe3_sanitizers
                asn1_e3ap
        )
        target_include_directories(${target_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples)
        if(simple_name STREQUAL "bench_encoding_size")
            target_link_libraries(${target_name} PRIVATE benchmark::benchmark)
        endif()
        if(simple_name STREQUAL "bench_encoding_wire" AND TARGET libe3::e2sm_dapp)
            target_link_libraries(${target_name} PRIVATE libe3::e2sm_dapp)
            target_compile_definitions(${target_name} PRIVATE LIBE3_BENCH_E2SM_DAPP)
        endif()
        set(_test_args "")
        if(simple_name STREQUAL "bench_encoding_wire")
            set(_test_args --values)
        endif()
        add_test(NAME ${target_name} COMMAND ${target_name} ${_test_args})
        set_tests_properties(${target_name} PROPERTIES LABELS "integration")
        # A test that needs the sctp kernel module exits with 77 when the host has none, which
        # CTest reports as skipped; LIBE3_REQUIRE_SCTP makes it fail there instead.
        if(simple_name MATCHES "sctp")
            set_tests_properties(${target_name} PROPERTIES SKIP_RETURN_CODE 77)
        endif()
    endforeach()
endif()

# --- latrec format round trip -------------------------------------------------
# The ring format and the stage catalog are mirrored by four writers and read by
# tools/latrec_reader.py. latrec_fixture writes a ring with the real writer and
# the Python test reads it back field by field, so neither the layout nor the
# catalog can change on one side only.
add_executable(latrec_fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/latrec_fixture.c")
target_link_libraries(latrec_fixture PRIVATE libe3::libe3)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_latrec_reader
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latrec_reader.py"
                     $<TARGET_FILE:latrec_fixture>
                     "${CMAKE_CURRENT_SOURCE_DIR}/include/libe3/latrec.h")
else()
    message(STATUS "Skipping test_latrec_reader: no Python 3 interpreter")
endif()

# --- latrec2csv against injected ground truth ---------------------------------
# Synthetic rings whose hops are known in advance, so the CSVs are checked
# against the delays that were written rather than against themselves. Needs
# numpy, which the converter uses and the reader does not.
if(Python3_Interpreter_FOUND)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import numpy"
                    RESULT_VARIABLE LIBE3_NO_NUMPY
                    OUTPUT_QUIET ERROR_QUIET)
    if(LIBE3_NO_NUMPY EQUAL 0)
        add_test(NAME test_latrec2csv
                 COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latrec2csv.py"
                         "${CMAKE_CURRENT_SOURCE_DIR}/tools/latrec2csv.py")
    else()
        message(STATUS "Skipping test_latrec2csv: numpy not available")
    endif()
endif()
