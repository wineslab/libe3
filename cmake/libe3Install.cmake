# libe3 Installation
#
# SPDX-License-Identifier: Apache-2.0

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install libraries
install(TARGETS libe3 libe3_shared asn1_e3ap libe3_warnings libe3_sanitizers
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

# Install ASN.1 generated headers
install(DIRECTORY ${ASN1_GENERATED_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libe3/asn1
    FILES_MATCHING PATTERN "*.h"
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
