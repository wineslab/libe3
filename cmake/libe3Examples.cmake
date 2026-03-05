# libe3 Examples
#
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_BUILD_EXAMPLES)
    return()
endif()

# Example: Simple E3 Agent
if(LIBE3_ENABLE_ASN1)
    add_executable(example_simple_agent
        examples/simple_agent.cpp
        examples/sm_simple/e3sm_simple_wrapper.cpp
    )

    # Link example with main lib and ASN.1 helper library (generated at messages)
    target_link_libraries(example_simple_agent PRIVATE libe3::libe3 asn1_e3ap)
    target_include_directories(example_simple_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples)
    target_include_directories(example_simple_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples/sm_simple)
else()
    message(STATUS "Skipping simple example: ASN.1 support disabled")
endif()

