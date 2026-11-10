# Where libe3 finds its asn1c
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

# Sets the cache variable ASN1C_EXEC unless the caller gave it: libe3's own prefix
# first (the line `local asn1c_prefix=` of build_libe3, the one place it is written),
# then PATH, then /opt/asn1c, where other projects install theirs.
function(libe3_find_asn1c)
    if(ASN1C_EXEC)
        return()
    endif()
    set(_prefix "/opt/libe3/asn1c")
    if(EXISTS "${PROJECT_SOURCE_DIR}/build_libe3")
        file(STRINGS "${PROJECT_SOURCE_DIR}/build_libe3" _line
            REGEX "^ *local asn1c_prefix=\"" LIMIT_COUNT 1)
        if(_line MATCHES "asn1c_prefix=\"([^\"]+)\"")
            set(_prefix "${CMAKE_MATCH_1}")
        endif()
    endif()
    find_program(ASN1C_EXEC asn1c PATHS "${_prefix}/bin" NO_DEFAULT_PATH)
    find_program(ASN1C_EXEC asn1c)
    find_program(ASN1C_EXEC asn1c PATHS /opt/asn1c/bin NO_DEFAULT_PATH)
endfunction()
