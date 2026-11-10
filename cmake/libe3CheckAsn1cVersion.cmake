# Runs libe3_check_asn1c_version() against a stand-in asn1c that prints BANNER for `-version`.
# Run by CTest as a script:
#   cmake -DSOURCE_DIR=<libe3> -DWORK_DIR=<dir> -DBANNER=<text> -P libe3CheckAsn1cVersion.cmake
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

set(PROJECT_SOURCE_DIR "${SOURCE_DIR}")
include("${SOURCE_DIR}/cmake/libe3Asn1c.cmake")

file(MAKE_DIRECTORY "${WORK_DIR}")
set(ASN1C_EXEC "${WORK_DIR}/asn1c")
file(WRITE "${ASN1C_EXEC}" "#!/bin/sh\nprintf '%s\\n' '${BANNER}'\n")
execute_process(COMMAND chmod +x "${ASN1C_EXEC}")

libe3_check_asn1c_version()
