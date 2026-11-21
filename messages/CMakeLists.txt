# E3AP Messages Library
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Generates the E3AP message code for the enabled encodings:
#   - ASN.1  -> asn1_e3ap (C, via asn1c)
#   - Protobuf -> pb_e3ap (C++, via protoc)
# and, with LIBE3_ENABLE_E2SM_DAPP, the E2SM-DAPP codec sources:
#   - ASN.1  -> asn1_e2sm_dapp (C objects, via asn1c, with a runtime copy of its own)

set(E3AP_VERSION 1 0 0)
string(REPLACE ";" "." E3AP_RELEASE "${E3AP_VERSION}")
message(STATUS "Selected E3AP_VERSION: ${E3AP_RELEASE}")

# ============================================================================
# ASN.1 message library (asn1_e3ap)
# ============================================================================
if(LIBE3_ENABLE_ASN1)
    if(E3AP_RELEASE VERSION_EQUAL "1.0.0")
        include(asn1/V1/e3ap-1.0.0.cmake)
    else()
        message(FATAL_ERROR "Unknown E3AP_RELEASE ${E3AP_RELEASE}")
    endif()

    # Find asn1c compiler
    libe3_find_asn1c()
    if(NOT ASN1C_EXEC)
        message(FATAL_ERROR "asn1c not found! ASN.1 encoding is required.\n"
            "Install it with: ./build_libe3 -I\n"
            "Or provide path: cmake -DASN1C_EXEC=/path/to/asn1c")
    endif()
    libe3_check_asn1c_version()

    # The runtime carries the JSON encoding rules (JER) whenever libe3 carries JSON, so that a service
    # model that links libe3::asn1_e3ap gets its JSON codec from its grammar as well as its APER codec.
    # The E2SM-DAPP codec below follows the same setting.
    # The layout of the type descriptors depends on it: see libe3/asn1_layout_check.h.
    if(LIBE3_ENABLE_JSON)
        set(LIBE3_ASN1C_JER_FLAG -gen-JER)
        set(LIBE3_ASN1_JER 1)
    else()
        set(LIBE3_ASN1C_JER_FLAG -no-gen-JER)
        set(LIBE3_ASN1_JER 0)
    endif()
    set(LIBE3_ASN1_JER "${LIBE3_ASN1_JER}" PARENT_SCOPE)
    message(STATUS "asn1c: JER ${LIBE3_ASN1C_JER_FLAG} (follows LIBE3_ENABLE_JSON)")

    # asn1c writes the C code now, at configure time: which files it writes depends on its version.
    libe3_asn1c_generate(e3ap GRAMMAR ${CMAKE_CURRENT_SOURCE_DIR}/${E3AP_GRAMMAR}
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    set(e3ap_source ${e3ap_SOURCES})
    set(e3ap_headers ${e3ap_HEADERS})

    # Optionally generate sources for the simple service model (e3sm_simple.asn)
    if(LIBE3_BUILD_EXAMPLES)
        # Include example-only CMake snippet which generates the simple ASN grammar
        include(${CMAKE_CURRENT_SOURCE_DIR}/../examples/sm_simple/CMakeLists.txt)

        # If the examples CMake exported generated lists, append them into the
        # asn1 sources so they are compiled only when examples are enabled.
        if(DEFINED E3SM_SIMPLE_GENERATED_SRCS)
            list(APPEND e3ap_source ${E3SM_SIMPLE_GENERATED_SRCS})
        endif()
        if(DEFINED E3SM_SIMPLE_GENERATED_HDRS)
            list(APPEND e3ap_headers ${E3SM_SIMPLE_GENERATED_HDRS})
        endif()
        # Both runs write the runtime files; they are the same files.
        list(REMOVE_DUPLICATES e3ap_source)
        list(REMOVE_DUPLICATES e3ap_headers)
    endif()
    libe3_asn1c_prune(${CMAKE_CURRENT_BINARY_DIR} ${e3ap_source} ${e3ap_headers})

    # Create ASN.1 library
    # The runtime is private to whatever links it: libe3's shared library must not export it, or
    # a host with an asn1c runtime of its own (OAI links -rdynamic) would be called instead of it.
    add_library(asn1_e3ap ${e3ap_source})
    set_target_properties(asn1_e3ap PROPERTIES
        LINKER_LANGUAGE C
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
    target_include_directories(asn1_e3ap
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libe3/asn1>
    )
    target_compile_options(asn1_e3ap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

    # For the service models that link this library: a unit generated with the other JER setting
    # is refused at compile time (the file is in the install).
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../include/libe3/asn1_layout_check.h.in"
        "${PROJECT_BINARY_DIR}/include/libe3/asn1_layout_check.h" @ONLY)

    # Export the generated directory for parent to use
    set(ASN1_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

    message(STATUS "ASN.1 encoding support enabled")
endif()

# ============================================================================
# E2SM-DAPP ASN.1 objects (asn1_e2sm_dapp)
# ============================================================================
# A second, independent asn1c run: its own output directory, its own copy of the
# asn1c runtime, and nothing shared with asn1_e3ap, so that the E2SM-DAPP codec
# (libe3::e2sm_dapp) can be used without the E3 agent, ZeroMQ or the E3AP grammar.
# An OBJECT library, so the codec libraries embed the objects and no separate
# asn1_e2sm_dapp archive has to be installed. The runtime symbols are hidden: a
# consumer that links its own asn1c (flexric does) must not see or clash with ours.
if(LIBE3_ENABLE_E2SM_DAPP)
    # Outside this directory's own binary dir: libe3Install.cmake installs every
    # generated E3AP header found there, and these are private.
    set(E2SM_DAPP_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated/e2sm_dapp")
    include(asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.cmake)

    # Same JER setting as asn1_e3ap: when libe3.a and the static codec are linked into one executable
    # their runtime symbols merge, which works only while the two runtimes have one layout.
    libe3_asn1c_generate(e2sm_dapp GRAMMAR ${CMAKE_CURRENT_SOURCE_DIR}/${E2SM_DAPP_GRAMMAR}
        DESTINATION ${E2SM_DAPP_GENERATED_DIR})
    set(e2sm_dapp_source ${e2sm_dapp_SOURCES})
    libe3_asn1c_prune(${E2SM_DAPP_GENERATED_DIR} ${e2sm_dapp_SOURCES} ${e2sm_dapp_HEADERS})

    add_library(asn1_e2sm_dapp OBJECT ${e2sm_dapp_source})
    set_target_properties(asn1_e2sm_dapp PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
    target_include_directories(asn1_e2sm_dapp PRIVATE ${E2SM_DAPP_GENERATED_DIR})
    target_compile_options(asn1_e2sm_dapp PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

    # Export the generated directory for parent to use
    set(E2SM_DAPP_GENERATED_DIR "${E2SM_DAPP_GENERATED_DIR}" PARENT_SCOPE)

    message(STATUS "E2SM-DAPP codec enabled")
endif()

# ============================================================================
# Protocol Buffers message library (pb_e3ap)
# ============================================================================
if(LIBE3_ENABLE_PROTOBUF)
    set(PROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")
    file(MAKE_DIRECTORY "${PROTO_OUT_DIR}")

    # E3AP protobuf grammar
    set(E3AP_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/V1")
    set(E3AP_PROTO "${E3AP_PROTO_DIR}/e3ap-1.0.0.proto")
    set(e3ap_pb_src "${PROTO_OUT_DIR}/e3ap-1.0.0.pb.cc")
    set(e3ap_pb_hdr "${PROTO_OUT_DIR}/e3ap-1.0.0.pb.h")

    add_custom_command(OUTPUT ${e3ap_pb_src} ${e3ap_pb_hdr}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT_DIR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --cpp_out=${PROTO_OUT_DIR}
            --proto_path=${E3AP_PROTO_DIR}
            ${E3AP_PROTO}
        DEPENDS ${E3AP_PROTO}
        COMMENT "Generating E3AP protobuf source files from ${E3AP_PROTO}"
    )

    set(pb_e3ap_sources ${e3ap_pb_src})

    # Optionally generate the simple service model proto (e3sm_simple.proto)
    if(LIBE3_BUILD_EXAMPLES)
        set(E3SM_SIMPLE_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../examples/sm_simple")
        set(E3SM_SIMPLE_PROTO "${E3SM_SIMPLE_PROTO_DIR}/e3sm_simple.proto")
        set(e3sm_pb_src "${PROTO_OUT_DIR}/e3sm_simple.pb.cc")
        set(e3sm_pb_hdr "${PROTO_OUT_DIR}/e3sm_simple.pb.h")

        add_custom_command(OUTPUT ${e3sm_pb_src} ${e3sm_pb_hdr}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT_DIR}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                --cpp_out=${PROTO_OUT_DIR}
                --proto_path=${E3SM_SIMPLE_PROTO_DIR}
                ${E3SM_SIMPLE_PROTO}
            DEPENDS ${E3SM_SIMPLE_PROTO}
            COMMENT "Generating Simple SM protobuf source files from ${E3SM_SIMPLE_PROTO}"
        )
        list(APPEND pb_e3ap_sources ${e3sm_pb_src})
    endif()

    # Create Protobuf library. Generated protobuf code is treated as a system
    # include so it does not trip the project's strict warning set.
    add_library(pb_e3ap ${pb_e3ap_sources})
    set_target_properties(pb_e3ap PROPERTIES LINKER_LANGUAGE CXX POSITION_INDEPENDENT_CODE ON)
    target_include_directories(pb_e3ap SYSTEM
        PUBLIC
            $<BUILD_INTERFACE:${PROTO_OUT_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libe3/proto>
    )
    target_link_libraries(pb_e3ap PUBLIC protobuf::libprotobuf)
    target_compile_options(pb_e3ap PRIVATE -w)

    # Export the generated directory for parent to use
    set(PROTOBUF_GENERATED_DIR "${PROTO_OUT_DIR}" PARENT_SCOPE)

    message(STATUS "Protocol Buffers encoding support enabled")
endif()
