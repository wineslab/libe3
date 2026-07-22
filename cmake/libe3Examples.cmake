# libe3 Examples
#
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_BUILD_EXAMPLES)
    return()
endif()

# Example: Simple E3 Agent (RAN side) and Simple dApp (dApp side)
if(LIBE3_ENABLE_ASN1)
    add_executable(example_simple_agent
        examples/simple_agent.cpp
        examples/sm_simple/e3sm_simple_wrapper.cpp
    )

    target_link_libraries(example_simple_agent PRIVATE libe3::libe3 asn1_e3ap libe3_warnings libe3_sanitizers)
    target_include_directories(example_simple_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples)
    target_include_directories(example_simple_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples/sm_simple)

    # Symmetric dApp-role counterpart of simple_agent. Pairs with the same
    # sm_simple wrapper and is wire-compatible with the Python
    # dApp-library/examples/simple_dapp.py.
    add_executable(example_simple_dapp
        examples/simple_dapp.cpp
        examples/sm_simple/e3sm_simple_wrapper.cpp
    )

    target_link_libraries(example_simple_dapp PRIVATE libe3::libe3 asn1_e3ap libe3_warnings libe3_sanitizers)
    target_include_directories(example_simple_dapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples)
    target_include_directories(example_simple_dapp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../examples/sm_simple)
else()
    message(STATUS "Skipping simple examples: ASN.1 support disabled")
endif()

