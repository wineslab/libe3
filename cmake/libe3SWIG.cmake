# SWIG Python bindings for libe3 — opt-in via LIBE3_ENABLE_SWIG.
#
# Produces a Python extension module (_libe3py.so) plus the SWIG-generated
# libe3py.py shim. Designed as the seam that dApp-library's e3interface/ layer
# consumes in place of its pure-Python ZMQ + asn1tools implementation. Besides
# the minimal E3Agent view it wraps the batched dApp session
# (swig/e3_dapp_session.{hpp,cpp}) — the low-latency DAppSession + E3Event.
#
# When LIBE3_ENABLE_SWIG is on and the library is installed (build_libe3
# --install --enable-swig), the module (_libe3py.so + libe3py.py) is installed
# into the active interpreter's site-packages (Python3_SITEARCH) so consumers
# can simply `import libe3py`. Activate the target venv before installing so the
# module lands in the environment that will import it.
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
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/swig/libe3.i"
        "${CMAKE_CURRENT_SOURCE_DIR}/swig/e3_dapp_session.cpp"
    OUTPUT_DIR "${LIBE3_SWIG_OUTPUT_DIR}"
)

set_target_properties(libe3py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${LIBE3_SWIG_OUTPUT_DIR}"
)

target_include_directories(libe3py PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/swig
    ${Python3_INCLUDE_DIRS}
)

target_link_libraries(libe3py PRIVATE libe3::libe3 ${Python3_LIBRARIES})

# Install the Python module into the active interpreter's site-packages so
# `import libe3py` works after `build_libe3 --install --enable-swig`. The .so
# target name from UseSWIG is `libe3py` (file `_libe3py.so`); the generated
# libe3py.py shim lives in LIBE3_SWIG_OUTPUT_DIR after the build.
#
# LIBE3_PYTHON_INSTALL_DIR defaults to the interpreter's site-packages, which is
# an ABSOLUTE path — so CMAKE_INSTALL_PREFIX / --prefix does NOT relocate the
# Python module (see swig/README.md). Override this var to stage it elsewhere
# (CI does this to test the install without touching system site-packages).
set(LIBE3_PYTHON_INSTALL_DIR "${Python3_SITEARCH}" CACHE PATH
    "Install directory for the libe3py Python module")
install(TARGETS libe3py
    LIBRARY DESTINATION "${LIBE3_PYTHON_INSTALL_DIR}"
)
install(FILES "${LIBE3_SWIG_OUTPUT_DIR}/libe3py.py"
    DESTINATION "${LIBE3_PYTHON_INSTALL_DIR}"
)

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
