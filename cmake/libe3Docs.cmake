# libe3 Documentation support
#
# Adds a `docs` target when LIBE3_BUILD_DOCS is ON and Doxygen is available.

if(NOT DEFINED LIBE3_BUILD_DOCS)
    set(LIBE3_BUILD_DOCS OFF)
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
        set(DOXYGEN_INPUT_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src")
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
