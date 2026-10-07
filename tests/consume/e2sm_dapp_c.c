/*
 * Compile-and-link check for the installed E2SM-DAPP codec, C API, from a C
 * translation unit (the way flexric calls it).
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/e2sm_dapp_c.h>

#include <stdio.h>
#include <stdlib.h>

int consume_e2sm_dapp_c(void)
{
    libe3_e2sm_dapp_ctrl_hdr_t hdr = {0};
    hdr.format          = LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1;
    hdr.ran_function_id = 1;
    hdr.dapp_id         = 2;

    libe3_e2sm_dapp_bytes_t bytes = {0, 0};
    if (libe3_e2sm_dapp_encode_ctrl_hdr(&hdr, &bytes) != E3_SUCCESS || bytes.len != 4) {
        printf("C API: encode failed\n");
        return 1;
    }
    /* ch1_min of the golden vectors: 00 01 00 02. */
    if (bytes.data[0] != 0x00 || bytes.data[1] != 0x01 || bytes.data[2] != 0x00 || bytes.data[3] != 0x02) {
        printf("C API: unexpected encoding\n");
        return 1;
    }

    libe3_e2sm_dapp_ctrl_hdr_t back = {0};
    e3_error_t rc = libe3_e2sm_dapp_decode_ctrl_hdr(bytes.data, bytes.len, &back);
    free(bytes.data); /* plain free works on everything the API returns */
    if (rc != E3_SUCCESS || back.ran_function_id != 1 || back.dapp_id != 2 || back.has_timestamp) {
        printf("C API: decode failed\n");
        return 1;
    }
    libe3_e2sm_dapp_free_ctrl_hdr(&back);
    printf("e2sm_dapp C ok\n");
    return 0;
}
