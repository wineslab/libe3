# libe3 Build Options
#
# SPDX-License-Identifier: Apache-2.0

option(LIBE3_BUILD_TESTS "Build unit tests" ON)
option(LIBE3_BUILD_EXAMPLES "Build examples" ON)
option(LIBE3_ENABLE_ZMQ "Enable ZeroMQ transport" ON)
option(LIBE3_ENABLE_ASN1 "Enable ASN.1 encoding support" ON)
option(LIBE3_ENABLE_JSON "Enable JSON encoding support" OFF)
option(LIBE3_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LIBE3_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LIBE3_BUILD_DOCS "Build documentation" OFF)
