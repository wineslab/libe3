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
if(LIBE3_BUILD_INTEGRATION_TESTS AND LIBE3_ENABLE_ASN1)
    file(GLOB LIBE3_INTEGRATION_TEST_SOURCES
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "tests/integration/*.cpp")
    foreach(test_src IN LISTS LIBE3_INTEGRATION_TEST_SOURCES)
        get_filename_component(test_name ${test_src} NAME_WE)
        string(REGEX REPLACE "^test_" "" simple_name ${test_name})
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
