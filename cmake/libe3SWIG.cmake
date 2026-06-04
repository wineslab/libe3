# SWIG Python bindings for libe3 — opt-in via LIBE3_ENABLE_SWIG.
#
# Produces a Python extension module (_libe3py.so) plus the SWIG-generated
# libe3py.py shim. Designed as the minimal seam that spear-dApp's
# e3interface/ layer can consume in place of its pure-Python ZMQ +
# asn1tools implementation.
#
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_ENABLE_SWIG)
    return()
endif()

find_package(SWIG 4.0 REQUIRED COMPONENTS python)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

include(UseSWIG)

set(LIBE3_SWIG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/swig")
file(MAKE_DIRECTORY "${LIBE3_SWIG_OUTPUT_DIR}")

set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/swig/libe3.i" PROPERTY CPLUSPLUS ON)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/swig/libe3.i" PROPERTY
    INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/swig")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/swig/libe3.i" PROPERTY
    SWIG_FLAGS
        "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
        "-I${CMAKE_CURRENT_SOURCE_DIR}/swig")

swig_add_library(libe3py
    TYPE SHARED
    LANGUAGE python
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/swig/libe3.i"
    OUTPUT_DIR "${LIBE3_SWIG_OUTPUT_DIR}"
)

set_target_properties(libe3py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${LIBE3_SWIG_OUTPUT_DIR}"
)

target_include_directories(libe3py PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(libe3py PRIVATE libe3::libe3 ${Python3_LIBRARIES})

# Register a CTest smoke test that imports the module and constructs a
# minimal E3Config + E3Agent. Only added when LIBE3_BUILD_TESTS is also on.
if(LIBE3_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_swig_smoke.py")
    add_test(
        NAME test_swig_smoke
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_swig_smoke.py"
    )
    set_tests_properties(test_swig_smoke PROPERTIES
        ENVIRONMENT "PYTHONPATH=${LIBE3_SWIG_OUTPUT_DIR}"
        LABELS "swig"
    )
endif()
