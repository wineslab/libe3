# Where libe3 finds its asn1c, which asn1c it accepts, and how it runs it
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

# The value of `local <name>="..."` in build_libe3, the one place the pin, the versions and the
# prefix are written. Empty when there is no build_libe3 (an sdist) or no such line.
function(_libe3_asn1c_setting name out)
    set(value "")
    if(EXISTS "${PROJECT_SOURCE_DIR}/build_libe3")
        file(STRINGS "${PROJECT_SOURCE_DIR}/build_libe3" line REGEX "^ *local ${name}=\"" LIMIT_COUNT 1)
        if(line MATCHES "${name}=\"([^\"]+)\"")
            set(value "${CMAKE_MATCH_1}")
        endif()
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

# Sets the cache variable ASN1C_EXEC unless the caller gave it: libe3's own prefix first, then
# PATH, then /opt/asn1c, where other projects install theirs.
function(libe3_find_asn1c)
    if(ASN1C_EXEC)
        return()
    endif()
    _libe3_asn1c_setting(asn1c_prefix prefix)
    if(NOT prefix)
        set(prefix "/opt/libe3/asn1c")
    endif()
    find_program(ASN1C_EXEC asn1c PATHS "${prefix}/bin" NO_DEFAULT_PATH)
    find_program(ASN1C_EXEC asn1c)
    find_program(ASN1C_EXEC asn1c PATHS /opt/asn1c/bin NO_DEFAULT_PATH)
endfunction()

# `1.5.3-124` or `v1.5.3-124-ge4ee5e48-2026-11-02` or `v1.5.4` to `1.5.3.124`, `1.5.4.0`; empty if
# it is not a version. The number after the dash counts commits after the tag, and v0.9.29 is what
# the asn1c of apt and brew, and the commit libe3 pinned before, print.
function(_libe3_asn1c_version text out)
    if(text MATCHES "v?([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9]+))?")
        set(extra 0)
        if(CMAKE_MATCH_5)
            set(extra ${CMAKE_MATCH_5})
        endif()
        set(${out} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}.${extra}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

# Refuses an asn1c older than the minimum, warns about one older than the version libe3 is tested with.
function(libe3_check_asn1c_version)
    execute_process(COMMAND "${ASN1C_EXEC}" -version
        OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
    string(REGEX MATCH "ASN\\.1 Compiler, v[0-9][^ \n]*" banner "${out}${err}")
    _libe3_asn1c_version("${banner}" found)
    _libe3_asn1c_setting(asn1c_min_version min_text)
    _libe3_asn1c_setting(asn1c_pin_version pin_text)
    _libe3_asn1c_version("${min_text}" min)
    _libe3_asn1c_version("${pin_text}" pin)
    string(REGEX REPLACE "^ASN\\.1 Compiler, v" "" shown "${banner}")
    if(NOT found)
        message(FATAL_ERROR "Cannot read the version of ${ASN1C_EXEC} (`-version` printed: ${out}${err}). "
            "Run ./build_libe3 -I, or pass -DASN1C_EXEC=<asn1c ${min_text} or newer>.")
    endif()
    if(min AND found VERSION_LESS min)
        message(FATAL_ERROR "asn1c ${shown} (${ASN1C_EXEC}) is older than ${min_text}, the oldest libe3 builds with. "
            "The asn1c of apt and brew (0.9.29) and the commit libe3 pinned before (940dd5fa, which also prints "
            "v0.9.29) write a runtime whose layout differs from the one libe3 now builds with. "
            "Run ./build_libe3 -I, which installs ${pin_text} under /opt/libe3/asn1c, "
            "or pass -DASN1C_EXEC=<asn1c ${min_text} or newer>.")
    endif()
    if(pin AND found VERSION_LESS pin)
        message(WARNING "asn1c ${shown} is older than ${pin_text}, the version libe3 is tested with; "
            "it is accepted down to ${min_text}.")
    endif()
    message(STATUS "asn1c found: ${ASN1C_EXEC} (${shown})")
    set(LIBE3_ASN1C_VERSION "${shown}" PARENT_SCOPE)
endfunction()

# Runs asn1c now, at configure time, on one grammar, and puts what it wrote into DESTINATION, touching
# only the files whose content changed so that a reconfigure does not rebuild the library. The files an
# asn1c writes depend on its version (the E3AP grammar needed 194 listed files with 940dd5fa; e4ee5e4
# writes 265), so no list of them is kept. SHARE_WITH names an earlier run into the same directory: the
# files that run wrote are left out, since the runtime files of two grammars differ in their numbering.
# Sets <name>_SOURCES and <name>_HEADERS to the paths in DESTINATION. LIBE3_ASN1C_JER_FLAG is
# -no-gen-JER or -gen-JER.
function(libe3_asn1c_generate name)
    cmake_parse_arguments(ARG "" "GRAMMAR;DESTINATION;SHARE_WITH" "" ${ARGN})
    set(staging "${CMAKE_BINARY_DIR}/asn1c-staging/${name}")
    file(REMOVE_RECURSE "${staging}")
    file(MAKE_DIRECTORY "${staging}" "${ARG_DESTINATION}")
    execute_process(
        COMMAND "${ASN1C_EXEC}" -pdu=all -gen-APER -gen-UPER ${LIBE3_ASN1C_JER_FLAG} -no-gen-BER -no-gen-OER
            -no-gen-CBOR -fno-include-deps -fcompound-names -findirect-choice -no-gen-example
            -D "${staging}" "${ARG_GRAMMAR}"
        WORKING_DIRECTORY "${staging}"
        RESULT_VARIABLE status OUTPUT_QUIET ERROR_VARIABLE log)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "asn1c failed on ${ARG_GRAMMAR}:\n${log}")
    endif()
    file(GLOB written RELATIVE "${staging}" "${staging}/*.c" "${staging}/*.h")
    set_property(GLOBAL PROPERTY LIBE3_ASN1C_FILES_${name} "${written}")
    if(ARG_SHARE_WITH)
        get_property(shared GLOBAL PROPERTY LIBE3_ASN1C_FILES_${ARG_SHARE_WITH})
        list(REMOVE_ITEM written ${shared})
    endif()
    set(from "")
    set(sources "")
    set(headers "")
    foreach(file IN LISTS written)
        list(APPEND from "${staging}/${file}")
        if(file MATCHES "\\.c$")
            list(APPEND sources "${ARG_DESTINATION}/${file}")
        else()
            list(APPEND headers "${ARG_DESTINATION}/${file}")
        endif()
    endforeach()
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${from} "${ARG_DESTINATION}"
        RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Could not copy the asn1c output of ${ARG_GRAMMAR} into ${ARG_DESTINATION}")
    endif()
    list(LENGTH written count)
    message(STATUS "asn1c: generated ${count} files for ${name} (${LIBE3_ASN1C_VERSION})")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ARG_GRAMMAR}")
    set(${name}_SOURCES "${sources}" PARENT_SCOPE)
    set(${name}_HEADERS "${headers}" PARENT_SCOPE)
endfunction()

# Removes the .c and .h files in a directory that no asn1c run of this configure wrote, left by an
# asn1c of another version, so that they are not installed.
function(libe3_asn1c_prune directory)
    file(GLOB present "${directory}/*.c" "${directory}/*.h")
    foreach(file IN LISTS present)
        if(NOT file IN_LIST ARGN)
            file(REMOVE "${file}")
        endif()
    endforeach()
endfunction()
