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
# Optional [LAT] latency-profiling log points (see include/libe3/latency.hpp).
# Off by default: compiled out entirely, so normal builds are unaffected and
# enabling it does not require raising the logger to DEBUG.
option(LIBE3_ENABLE_LATENCY "Emit [LAT] latency-profiling log points" OFF)
