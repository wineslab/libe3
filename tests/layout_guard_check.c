/*
 * A unit that includes libe3's layout guard. Built twice by cmake/libe3Tests.cmake: with the
 * asn_config.h that matches libe3's runtime, which must compile, and with one that does not,
 * which must be refused (test_asn1_layout_guard_refuses_mismatch).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/asn1_layout_check.h"

int libe3_layout_guard_unit(void) {
    return LIBE3_ASN1_JER;
}
