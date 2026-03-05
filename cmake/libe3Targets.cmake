# libe3 Library Targets
#
# SPDX-License-Identifier: Apache-2.0

# ============================================================================
# Static Library
# ============================================================================

add_library(libe3 STATIC ${LIBE3_SOURCES})
add_library(libe3::libe3 ALIAS libe3)

target_include_directories(libe3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libe3
    PUBLIC
        Threads::Threads
        tl::expected
        
    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

# Conditionally link JSON and ASN.1 libraries and expose compile-time flags
if(LIBE3_ENABLE_JSON)
    target_link_libraries(libe3 PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_JSON)
endif()

if(LIBE3_ENABLE_ASN1)
    target_link_libraries(libe3 PUBLIC asn1_e3ap)
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_ASN1)
endif()

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3 PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=0)
endif()

set_target_properties(libe3 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${LIBE3_PUBLIC_HEADERS}"
)

# ============================================================================
# Shared Library
# ============================================================================

add_library(libe3_shared SHARED ${LIBE3_SOURCES})
add_library(libe3::shared ALIAS libe3_shared)

target_include_directories(libe3_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libe3_shared
    PUBLIC
        Threads::Threads
        tl::expected
    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

# Conditionally link JSON and ASN.1 libraries and expose compile-time flags
if(LIBE3_ENABLE_JSON)
    target_link_libraries(libe3_shared PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_JSON)
endif()

if(LIBE3_ENABLE_ASN1)
    target_link_libraries(libe3_shared PUBLIC asn1_e3ap)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_ASN1)
endif()

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3_shared PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=0)
endif()

set_target_properties(libe3_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME libe3
)
