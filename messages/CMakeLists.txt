# E3AP Messages Library
#
# SPDX-License-Identifier: Apache-2.0
#
# Generates the E3AP message code for the enabled encodings:
#   - ASN.1  -> asn1_e3ap (C, via asn1c)
#   - Protobuf -> pb_e3ap (C++, via protoc)

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
    find_program(ASN1C_EXEC asn1c HINTS /opt/asn1c/bin)
    if(NOT ASN1C_EXEC)
        message(FATAL_ERROR "asn1c not found! ASN.1 encoding is required.\n"
            "Install with: sudo apt install asn1c\n"
            "Or provide path: cmake -DASN1C_EXEC=/path/to/asn1c")
    endif()
    message(STATUS "asn1c found: ${ASN1C_EXEC}")

    # Generate ASN.1 C code using custom_command (on-demand generation)
    add_custom_command(OUTPUT ${e3ap_source} ${e3ap_headers}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${ASN1C_EXEC} -pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example -D ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${E3AP_GRAMMAR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${E3AP_GRAMMAR}
        COMMENT "Generating E3AP ASN.1 source files from ${CMAKE_CURRENT_SOURCE_DIR}/${E3AP_GRAMMAR}"
    )

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
    endif()

    # Create ASN.1 library
    add_library(asn1_e3ap ${e3ap_source})
    set_target_properties(asn1_e3ap PROPERTIES LINKER_LANGUAGE C)
    target_include_directories(asn1_e3ap
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libe3/asn1>
    )
    target_compile_options(asn1_e3ap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

    # Export the generated directory for parent to use
    set(ASN1_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

    message(STATUS "ASN.1 encoding support enabled")
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
