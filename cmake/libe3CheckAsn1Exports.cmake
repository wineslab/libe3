# Fails when a shared library exports a symbol of the asn1c runtime or of a generated type.
# Run by CTest as a script: cmake -DLIBRARIES=<lib>|<lib> -P libe3CheckAsn1Exports.cmake
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

find_program(NM_EXECUTABLE nm REQUIRED)
string(REPLACE "|" ";" libraries "${LIBRARIES}")

set(runtime "ANY_|BIT_STRING_|BOOLEAN_|CHOICE_|ENUMERATED_|INTEGER_|NULL_|NativeEnumerated_|NativeInteger_")
string(APPEND runtime "|OBJECT_IDENTIFIER_|OCTET_STRING_|OPEN_TYPE_|REAL_|RELATIVE_OID_|SEQUENCE_|SET_")
string(APPEND runtime "|asn_|asn1|aper_|uper_|per_|ber_|der_|xer_|jer_|oer_|cer_|cbor_|constr_|generic_")

set(failed FALSE)
foreach(library IN LISTS libraries)
    execute_process(COMMAND ${NM_EXECUTABLE} -D --defined-only "${library}"
        OUTPUT_VARIABLE symbols RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "nm -D failed on ${library}")
    endif()
    string(REPLACE "\n" ";" lines "${symbols}")
    set(leaked "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^[0-9a-fA-F]+ [A-Za-z] ((${runtime})[A-Za-z0-9_]*)$")
            list(APPEND leaked "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(LENGTH leaked count)
    if(count GREATER 0)
        list(SUBLIST leaked 0 10 first)
        list(JOIN first ", " shown)
        message(SEND_ERROR "${library} exports ${count} asn1c symbols, for example: ${shown}")
        set(failed TRUE)
    endif()
endforeach()
if(failed)
    message(FATAL_ERROR "asn1c symbols are exported")
endif()
