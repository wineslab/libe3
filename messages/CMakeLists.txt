# E3AP Messages Library
#
# SPDX-License-Identifier: Apache-2.0
#
# Handles ASN.1 code generation and the asn1_e3ap library

set(E3AP_VERSION 1 0 0)
string(REPLACE ";" "." E3AP_RELEASE "${E3AP_VERSION}")

if(E3AP_RELEASE VERSION_EQUAL "1.0.0")
    include(asn1/V1/e3ap-1.0.0.cmake)
else()
    message(FATAL_ERROR "Unknown E3AP_RELEASE ${E3AP_RELEASE}")
endif()

message(STATUS "Selected E3AP_VERSION: ${E3AP_RELEASE}")

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
