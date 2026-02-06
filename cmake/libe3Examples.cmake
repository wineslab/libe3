# libe3 Examples
#
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_BUILD_EXAMPLES)
    return()
endif()

# Example: Simple E3 Agent
add_executable(example_simple_agent examples/simple_agent.cpp)
target_link_libraries(example_simple_agent PRIVATE libe3::libe3)

