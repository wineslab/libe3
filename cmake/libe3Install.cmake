# libe3 Installation
#
# SPDX-License-Identifier: Apache-2.0

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# pkg-config (.pc) file
set(LIBE3_PC_LIB_NAME "libe3")

# The public headers change shape with the feature macros, and those macros are
# PUBLIC on the target, so a CMake consumer inherits them automatically while a
# pkg-config consumer would have to guess. Read them off the target rather than
# re-listing the options here: a define added to libe3Targets.cmake then reaches
# the .pc without a second edit, and cannot drift out of sync with it.
get_target_property(LIBE3_PUBLIC_DEFS libe3 INTERFACE_COMPILE_DEFINITIONS)
set(LIBE3_PC_CFLAGS "")
if(LIBE3_PUBLIC_DEFS)
    foreach(_def IN LISTS LIBE3_PUBLIC_DEFS)
        if(_def MATCHES "\\$<")
            # A generator expression has no configure-time value, so it cannot be
            # written into a .pc. Fail loudly instead of silently dropping it.
            message(FATAL_ERROR
                "libe3: PUBLIC compile definition '${_def}' is a generator "
                "expression and cannot be exported through libe3.pc")
        endif()
        string(APPEND LIBE3_PC_CFLAGS " -D${_def}")
    endforeach()
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libe3.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libe3.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# Install libraries
set(LIBE3_INSTALL_TARGETS libe3 libe3_shared libe3_warnings libe3_sanitizers)

if(LIBE3_ENABLE_ASN1)
    list(APPEND LIBE3_INSTALL_TARGETS asn1_e3ap)
endif()

if(LIBE3_ENABLE_PROTOBUF)
    list(APPEND LIBE3_INSTALL_TARGETS pb_e3ap)
endif()

install(TARGETS ${LIBE3_INSTALL_TARGETS}
    EXPORT libe3Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3
)

# Install headers
install(DIRECTORY include/libe3
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# tl/expected.hpp travels with us: e3_encoder.hpp includes it and returns
# tl::expected<T, ErrorCode>, so a consumer of the installed headers needs it on
# the include path. It is fetched, not found, so there is nothing for the
# consumer to find -- hence vendoring the one header here rather than exporting
# a target they cannot resolve (see libe3Targets.cmake). Note this does place a
# third-party header under our own prefix, where it would shadow a system
# tl-expected installed to the same prefix; that is the cost of keeping
# tl::expected in public signatures.
install(FILES "${tl_expected_SOURCE_DIR}/include/tl/expected.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tl"
)

# The reader for the ring format latrec.h writes and the converter over it.
# PROGRAMS, not FILES: both are run from the command line, and latrec2csv.py
# locates the reader beside itself.
install(PROGRAMS ${LIBE3_TOOLS}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/libe3/tools
)

# Install generated version header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/libe3/version.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3
)

# Install ASN.1 generated headers
if(LIBE3_ENABLE_ASN1)
    install(DIRECTORY ${ASN1_GENERATED_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3/asn1
        FILES_MATCHING PATTERN "*.h"
    )
endif()

# Install Protobuf generated headers
if(LIBE3_ENABLE_PROTOBUF)
    install(DIRECTORY ${PROTOBUF_GENERATED_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3/proto
        FILES_MATCHING PATTERN "*.pb.h"
    )
endif()

# Install the Simple SM service-model definitions so Python (and other)
# consumers can reuse the exact grammar instead of copying it. dApp-library's
# examples/simple_dapp.py loads e3sm_simple.asn from here to stay wire-compatible
# with examples/simple_agent.cpp. Location is exported to the .pc file as
# `smdir` (see cmake/libe3.pc.in) and defaults to <datadir>/libe3/sm.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sm_simple/e3sm_simple.asn"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/sm_simple/e3sm_simple.proto"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/libe3/sm/sm_simple"
)

# Export targets
install(EXPORT libe3Targets
    FILE libe3Targets.cmake
    NAMESPACE libe3::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)

# Config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libe3Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libe3Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)

# Version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libe3ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libe3Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libe3ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libe3
)
