# libe3 Documentation support
#
# Adds a `docs` target when LIBE3_BUILD_DOCS is ON and Doxygen is available.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED LIBE3_BUILD_DOCS)
    set(LIBE3_BUILD_DOCS OFF)
endif()

# EXTRACT_ALL = YES hides Doxygen's "is not documented" checks, so the strict
# mode turns it off and asks for every @param and @return. Off for the published
# documentation, which keeps listing undocumented symbols.
option(LIBE3_DOCS_STRICT "Doxygen: warn on every undocumented member, parameter and return value" OFF)
if(LIBE3_DOCS_STRICT)
    set(DOXYGEN_STRICT_OVERRIDES "EXTRACT_ALL             = NO\nWARN_NO_PARAMDOC       = YES\nWARN_IF_INCOMPLETE_DOC = YES")
else()
    set(DOXYGEN_STRICT_OVERRIDES "")
endif()

if(LIBE3_BUILD_DOCS)
    find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)

    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

        # Variables substituted into Doxyfile.in
        set(DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
        set(PROJECT_VERSION "${LIBE3_VERSION}")
        set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
        set(DOXYGEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set(DOXYGEN_INPUT_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/swig ${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTING.md")
        set(DOXYGEN_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        set(DOXYGEN_WARN_LOGFILE "${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen_warnings.txt")

        # Dot / Graphviz support
        if(DOXYGEN_DOT_FOUND)
            set(DOXYGEN_HAVE_DOT "YES")
            get_filename_component(DOXYGEN_DOT_PATH "${DOXYGEN_DOT_EXECUTABLE}" DIRECTORY)
        else()
            set(DOXYGEN_HAVE_DOT "NO")
            set(DOXYGEN_DOT_PATH "")
        endif()

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )

        message(STATUS "Doxygen found; 'docs' target available (HTML → ${DOXYGEN_OUTPUT_DIR}/html)")
    else()
        message(WARNING "Doxygen not found; cannot build documentation. Install Doxygen or disable LIBE3_BUILD_DOCS.")
    endif()
endif()
