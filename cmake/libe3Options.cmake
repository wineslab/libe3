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
# SCTP is an optional POSIX transport. OFF by default because it is the only
# dependency that needs a kernel module plus a distro -dev package (libsctp-dev /
# lksctp-tools-devel) for a single system header, while deployments typically run
# the E3 link over IPC or TCP and never reach for it. The E3TransportLayer::SCTP
# enumerator exists either way -- it is part of the ABI and of the wire-facing
# value that the C API and the Python bindings mirror -- but requesting it from a
# build with this OFF fails at socket creation with a clear message rather than
# silently doing something else.
option(LIBE3_ENABLE_SCTP "Enable the SCTP POSIX transport (needs libsctp-dev)" OFF)
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
