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

    add_executable(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/${test_src}")
    target_link_libraries(${target_name}
        PRIVATE
            libe3::libe3
            libe3_test_framework
            libe3_warnings
            libe3_sanitizers
    )
    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()
