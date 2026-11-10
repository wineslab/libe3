## Example-only ASN.1 generation for the Simple SM
# This file is intended to be included by messages/CMakeLists.txt when
# `LIBE3_BUILD_EXAMPLES` is enabled. It generates the ASN.1 C sources into
# the messages binary directory and exports the generated file lists to the
# parent scope.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

if(NOT LIBE3_BUILD_EXAMPLES)
    return()
endif()

if(NOT ASN1C_EXEC)
    libe3_find_asn1c()
endif()
if(NOT ASN1C_EXEC)
    message(FATAL_ERROR "asn1c not found; required to generate e3sm_simple ASN.1 sources")
endif()

# Grammar file is next to this CMakeLists
set(E3SM_SIMPLE_GRAMMAR "${CMAKE_CURRENT_LIST_DIR}/e3sm_simple.asn")

# Output directory: use the messages binary dir (the including CMake sets this as CMAKE_CURRENT_BINARY_DIR)
libe3_asn1c_generate(e3sm_simple GRAMMAR ${E3SM_SIMPLE_GRAMMAR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
    SHARE_WITH e3ap)

# Export the lists, here and to the parent scope, so messages/CMakeLists can append them
set(E3SM_SIMPLE_GENERATED_SRCS ${e3sm_simple_SOURCES})
set(E3SM_SIMPLE_GENERATED_HDRS ${e3sm_simple_HEADERS})
set(E3SM_SIMPLE_GENERATED_SRCS ${E3SM_SIMPLE_GENERATED_SRCS} PARENT_SCOPE)
set(E3SM_SIMPLE_GENERATED_HDRS ${E3SM_SIMPLE_GENERATED_HDRS} PARENT_SCOPE)
