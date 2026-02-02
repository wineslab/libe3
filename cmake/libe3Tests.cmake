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

# Test: Types and Encoding
add_executable(test_types tests/test_types.cpp)
target_link_libraries(test_types
    PRIVATE
        libe3::libe3
        libe3_test_framework
        libe3_warnings
        libe3_sanitizers
)
add_test(NAME test_types COMMAND test_types)

# Test: Subscription Manager
add_executable(test_subscription_manager tests/test_subscription_manager.cpp)
target_link_libraries(test_subscription_manager
    PRIVATE
        libe3::libe3
        libe3_test_framework
        libe3_warnings
        libe3_sanitizers
)
add_test(NAME test_subscription_manager COMMAND test_subscription_manager)

# Test: Response Queue
add_executable(test_response_queue tests/test_response_queue.cpp)
target_link_libraries(test_response_queue
    PRIVATE
        libe3::libe3
        libe3_test_framework
        libe3_warnings
        libe3_sanitizers
)
add_test(NAME test_response_queue COMMAND test_response_queue)

# Test: JSON Encoder
add_executable(test_json_encoder tests/test_json_encoder.cpp)
target_link_libraries(test_json_encoder
    PRIVATE
        libe3::libe3
        libe3_test_framework
        libe3_warnings
        libe3_sanitizers
)
add_test(NAME test_json_encoder COMMAND test_json_encoder)

# Test: E3 Agent
add_executable(test_e3_agent tests/test_e3_agent.cpp)
target_link_libraries(test_e3_agent
    PRIVATE
        libe3::libe3
        libe3_test_framework
        libe3_warnings
        libe3_sanitizers
)
add_test(NAME test_e3_agent COMMAND test_e3_agent)
