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
# The latrec stage-timing recorder (include/libe3/latrec.h) is a benchmarking
# tool, not a production feature: libE3's job is serving a real-time
# protocol, not measuring itself. Off by default so a normal build carries
# none of it -- not even a near-zero-cost runtime branch. When ON, every
# latrec_* call site compiles to the real mechanism, still gated at runtime
# by LATREC_DIR (unset = capture off without rebuilding). See docs/latrec.md.
option(LIBE3_ENABLE_LATREC "Compile in the latrec stage-timing recorder (benchmarking only)" OFF)
