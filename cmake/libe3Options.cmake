# libe3 Build Options
#
# SPDX-License-Identifier: Apache-2.0

option(LIBE3_BUILD_TESTS "Build unit tests" ON)
option(LIBE3_BUILD_EXAMPLES "Build examples" ON)
option(LIBE3_BUILD_INTEGRATION_TESTS
    "Build the integration test suite (multi-role end-to-end tests)" OFF)
option(LIBE3_ENABLE_SWIG
    "Build the SWIG Python bindings" OFF)
option(LIBE3_ENABLE_ZMQ "Enable ZeroMQ transport" ON)
option(LIBE3_ENABLE_ASN1 "Enable ASN.1 encoding support" ON)
option(LIBE3_ENABLE_JSON "Enable JSON encoding support" OFF)
option(LIBE3_ENABLE_PROTOBUF "Enable Protocol Buffers encoding support" OFF)
option(LIBE3_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LIBE3_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LIBE3_BUILD_DOCS "Build documentation" OFF)
# See docs/latrec.md.
option(LIBE3_ENABLE_LATREC "Compile in the latrec stage-timing recorder (benchmarking only)" OFF)

# Where latrec writes its rings when nothing calls latrec_set_output_dir().
# An enabled build records with no per-run opt-out, so a test or example that
# never names a directory would otherwise fill this one: when tests are being
# built, default it into the build tree, which goes away with the build tree.
# Deployments should set it explicitly. Empty leaves latrec.h's own fallback.
if(LIBE3_ENABLE_LATREC AND LIBE3_BUILD_TESTS AND NOT LATREC_DEFAULT_DIR)
    set(LATREC_DEFAULT_DIR "${CMAKE_BINARY_DIR}/latrec" CACHE STRING
        "Default directory for latrec rings")
endif()
