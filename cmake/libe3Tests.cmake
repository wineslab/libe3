# libe3 Unit Tests
#
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
    asn1_size
    e2e_report_path
)

# Tests that drive a raw ZMQ peer against the agent: they include <zmq.h> and
# link libzmq directly, so with LIBE3_ENABLE_ZMQ=OFF there is nothing for them
# to link against. Same treatment as the ASN.1-only list above.
set(LIBE3_ZMQ_ONLY_TESTS
    e2e_report_path
    role_aware_connector_zmq
    setup_bad_request
)

foreach(test_src IN LISTS LIBE3_TEST_SOURCES)
    # Derive a target name from the source file name: tests/test_foo.cpp -> test_foo
    get_filename_component(test_name ${test_src} NAME_WE)
    # If the filename already starts with 'test_', strip it to avoid 'test_test_...' targets
    string(REGEX REPLACE "^test_" "" simple_name ${test_name})
    set(target_name "test_${simple_name}")
    # Skip tests that require optional components when those components are disabled
    if(NOT LIBE3_ENABLE_JSON AND simple_name STREQUAL "json_encoder")
        message(STATUS "Skipping test_json_encoder: JSON support disabled")
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
    
    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()

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
        add_test(NAME ${target_name} COMMAND ${target_name})
        set_tests_properties(${target_name} PROPERTIES LABELS "integration")
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
