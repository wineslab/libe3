# SWIG Python bindings for libe3 — opt-in via LIBE3_ENABLE_SWIG.
#
# Produces a Python extension module (_libe3py.so) plus the SWIG-generated
# libe3py.py shim. Designed as the seam that dApp-library's e3interface/ layer
# consumes in place of its pure-Python ZMQ + asn1tools implementation. Besides
# the minimal E3Agent view it wraps the batched dApp session
# (swig/e3_dapp_session.{hpp,cpp}) — the low-latency DAppSession + E3Event.
#
# This in-tree build is for development and the swig tests (build/swig). Users
# install the binding with pip (`pip install libe3py`, or `pip install .` from
# this checkout), which builds the same module against the installed libe3; see
# python/CMakeLists.txt. `cmake --install` installs the module only when
# LIBE3_PYTHON_INSTALL_DIR is set, so a library install never writes into a
# Python's site-packages behind the user's back.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_ENABLE_SWIG)
    return()
endif()

include(libe3SwigModule)

set(LIBE3_SWIG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/swig")
libe3_add_python_module(libe3py
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_DIR "${LIBE3_SWIG_OUTPUT_DIR}"
    LINK libe3::libe3
    INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(LIBE3_PYTHON_INSTALL_DIR "" CACHE PATH
    "Install the libe3py module here with the library (empty: do not; users install it with pip)")
if(LIBE3_PYTHON_INSTALL_DIR)
    install(TARGETS libe3py LIBRARY DESTINATION "${LIBE3_PYTHON_INSTALL_DIR}")
    install(FILES "${LIBE3_SWIG_OUTPUT_DIR}/libe3py.py" DESTINATION "${LIBE3_PYTHON_INSTALL_DIR}")
    message(STATUS "libe3py will be installed with the library into ${LIBE3_PYTHON_INSTALL_DIR}")
else()
    message(STATUS "libe3py is built in ${LIBE3_SWIG_OUTPUT_DIR} and not installed; install it with pip (pip install .)")
endif()

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

# The latrec TLS stamping API, bound alongside the rest of libe3py: a Python
# caller records its own stages through the same rings libe3 writes to. No RAN
# peer needed (unlike tests/test_swig_latrec.py's session-ring coverage), so
# it runs in the same lightweight tier as test_swig_smoke rather than needing
# the integration harness's opt-in.
if(LIBE3_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_swig_latrec_stamp.py")
    add_test(
        NAME test_swig_latrec_stamp
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_swig_latrec_stamp.py"
    )
    set_tests_properties(test_swig_latrec_stamp PROPERTIES
        ENVIRONMENT "PYTHONPATH=${LIBE3_SWIG_OUTPUT_DIR}"
        LABELS "swig"
    )
endif()
