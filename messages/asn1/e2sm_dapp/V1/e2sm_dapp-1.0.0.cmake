# E2SM-DAPP ASN.1 grammar
#
# The grammar is a byte-identical copy of flexric's; do not edit it here. The C files asn1c
# generates from it, with the same flags as the E3AP grammar, are not listed: they depend on the
# asn1c version, and libe3_asn1c_generate() (cmake/libe3Asn1c.cmake) takes what asn1c wrote.
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

set(E2SM_DAPP_GRAMMAR asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn)
