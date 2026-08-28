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
        # tl::expected is header-only and IS part of the public interface
        # (e3_encoder.hpp exposes tl::expected<T, ErrorCode>), but it arrives via
        # FetchContent, so it is not an imported target a consumer can resolve:
        # naming it in the installed interface made every
        # find_package(libe3 CONFIG) fail on a missing tl::expected. Scope the
        # target to the build tree and install the header instead (see
        # libe3Install.cmake), so <tl/expected.hpp> still resolves through
        # ${includedir} on both the CMake and the pkg-config route.
        $<BUILD_INTERFACE:tl::expected>

    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

# Conditionally link JSON and ASN.1 libraries and expose compile-time flags
if(LIBE3_ENABLE_JSON)
    # nlohmann/json is header-only and used only inside the implementation
    # (src/encoder/json_encoder.*), never in a public header, and it is fully
    # compiled into libe3.a. Scope it to the BUILD interface: for a STATIC
    # library even a PRIVATE dependency is re-exported as $<LINK_ONLY:...> (so
    # consumers would be required to link it), which pulls the un-exported
    # nlohmann_json target into install(EXPORT) and breaks it. $<BUILD_INTERFACE:>
    # keeps the include dirs available while building libe3 but drops the target
    # from the installed/exported interface, where consumers do not need it. The
    # LIBE3_ENABLE_JSON define stays PUBLIC: it only selects a default enum in the
    # public types.hpp and carries no nlohmann dependency.
    target_link_libraries(libe3 PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_JSON)
endif()

if(LIBE3_ENABLE_ASN1)
    target_link_libraries(libe3 PUBLIC asn1_e3ap)
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_ASN1)
endif()

if(LIBE3_ENABLE_PROTOBUF)
    target_link_libraries(libe3 PUBLIC pb_e3ap protobuf::libprotobuf)
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_PROTOBUF)
endif()

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3 PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3 PUBLIC LIBE3_HAS_ZMQ=0)
endif()

if(LIBE3_ENABLE_SCTP)
    # PUBLIC so it reaches libe3.pc: a consumer can then tell whether requesting
    # E3TransportLayer::SCTP will work, instead of finding out at socket().
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_SCTP)
endif()

if(LIBE3_ENABLE_LATREC)
    # PUBLIC: any consumer building against this target (OAI, flexric) must
    # see the same define, or its own latrec_* calls silently resolve to the
    # no-op stubs while linking against a libe3 that actually has the ring
    # registry compiled in, or vice versa.
    target_compile_definitions(libe3 PUBLIC LIBE3_ENABLE_LATREC)
    if(LATREC_DEFAULT_DIR)
        # PRIVATE, unlike the flag above. latrec_open_in() applies this fallback
        # from src/core/latrec.c -- libe3's own translation unit -- so every
        # caller of latrec_tls_open_as() inherits it without needing the macro
        # itself. Exporting it would put a quoted string macro in libe3.pc,
        # where pkg-config's tokenizer strips the quotes and the bare path then
        # fails to lex ("expected expression before '/'"). Escaping as \\" keeps
        # CMake consumers working but breaks `cc $(pkg-config --cflags libe3)`
        # with an unterminated string, so there is no form that is safe for
        # both. It is also a build-tree path, meaningless once installed.
        target_compile_definitions(libe3
            PRIVATE LATREC_DEFAULT_DIR=\"${LATREC_DEFAULT_DIR}\")
    endif()
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
        # Build-interface only; see the static libe3 target above.
        $<BUILD_INTERFACE:tl::expected>
    PRIVATE
        libe3_warnings
        libe3_sanitizers
)

# Conditionally link JSON and ASN.1 libraries and expose compile-time flags
if(LIBE3_ENABLE_JSON)
    # Header-only, implementation-only dependency (see the static libe3 target
    # above); $<BUILD_INTERFACE:> keeps it out of the exported interface so
    # install(EXPORT) does not require the un-exported nlohmann_json target.
    target_link_libraries(libe3_shared PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_JSON)
endif()

if(LIBE3_ENABLE_ASN1)
    target_link_libraries(libe3_shared PUBLIC asn1_e3ap)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_ASN1)
endif()

if(LIBE3_ENABLE_PROTOBUF)
    target_link_libraries(libe3_shared PUBLIC pb_e3ap protobuf::libprotobuf)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_PROTOBUF)
endif()

if(LIBE3_ENABLE_ZMQ)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=1)
    target_link_libraries(libe3_shared PRIVATE PkgConfig::ZMQ)
else()
    target_compile_definitions(libe3_shared PUBLIC LIBE3_HAS_ZMQ=0)
endif()

if(LIBE3_ENABLE_SCTP)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_SCTP)
endif()

if(LIBE3_ENABLE_LATREC)
    target_compile_definitions(libe3_shared PUBLIC LIBE3_ENABLE_LATREC)
    if(LATREC_DEFAULT_DIR)
        # PRIVATE for the same reason as the static target above.
        target_compile_definitions(libe3_shared
            PRIVATE LATREC_DEFAULT_DIR=\"${LATREC_DEFAULT_DIR}\")
    endif()
endif()

set_target_properties(libe3_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME libe3
)
