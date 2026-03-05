## Example-only ASN.1 generation for the Simple SM
# This file is intended to be included by messages/CMakeLists.txt when
# `LIBE3_BUILD_EXAMPLES` is enabled. It generates the ASN.1 C sources into
# the messages binary directory and exports the generated file lists to the
# parent scope.

if(NOT LIBE3_BUILD_EXAMPLES)
    return()
endif()

if(NOT ASN1C_EXEC)
    find_program(ASN1C_EXEC asn1c HINTS /opt/asn1c/bin)
endif()
if(NOT ASN1C_EXEC)
    message(FATAL_ERROR "asn1c not found; required to generate e3sm_simple ASN.1 sources")
endif()

# Grammar file is next to this CMakeLists
set(E3SM_SIMPLE_GRAMMAR "${CMAKE_CURRENT_LIST_DIR}/e3sm_simple.asn")

# Output directory: use the messages binary dir (the including CMake sets this as CMAKE_CURRENT_BINARY_DIR)
set(E3SM_SIMPLE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(E3SM_SIMPLE_GENERATED_SRCS
    ${E3SM_SIMPLE_OUT_DIR}/Simple-Indication.c
    ${E3SM_SIMPLE_OUT_DIR}/Simple-Control.c
    ${E3SM_SIMPLE_OUT_DIR}/Simple-ConfigControl.c
    ${E3SM_SIMPLE_OUT_DIR}/Simple-DAppReport.c
    ${E3SM_SIMPLE_OUT_DIR}/Simple-RanFunctionData.c
)

set(E3SM_SIMPLE_GENERATED_HDRS
    ${E3SM_SIMPLE_OUT_DIR}/Simple-Indication.h
    ${E3SM_SIMPLE_OUT_DIR}/Simple-Control.h
    ${E3SM_SIMPLE_OUT_DIR}/Simple-ConfigControl.h
    ${E3SM_SIMPLE_OUT_DIR}/Simple-DAppReport.h
    ${E3SM_SIMPLE_OUT_DIR}/Simple-RanFunctionData.h
)

add_custom_command(OUTPUT ${E3SM_SIMPLE_GENERATED_SRCS} ${E3SM_SIMPLE_GENERATED_HDRS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${E3SM_SIMPLE_OUT_DIR}
    COMMAND ${ASN1C_EXEC} -pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example -D ${E3SM_SIMPLE_OUT_DIR} ${E3SM_SIMPLE_GRAMMAR}
    DEPENDS ${E3SM_SIMPLE_GRAMMAR}
    COMMENT "Generating Simple ASN.1 source files from ${E3SM_SIMPLE_GRAMMAR}"
)

# Export lists to the parent scope so messages/CMakeLists can append them
set(E3SM_SIMPLE_GENERATED_SRCS ${E3SM_SIMPLE_GENERATED_SRCS} PARENT_SCOPE)
set(E3SM_SIMPLE_GENERATED_HDRS ${E3SM_SIMPLE_GENERATED_HDRS} PARENT_SCOPE)
