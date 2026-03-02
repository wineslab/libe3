# libe3 Installation
#
# SPDX-License-Identifier: Apache-2.0

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# pkg-config (.pc) file
set(LIBE3_PC_LIB_NAME "libe3")

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
