/**
 * @file latrec_fixture.c
 * @brief Writes a ring with known contents, for the reader round-trip test.
 *
 * Uses the real writer (libe3/latrec.h), so what the reader is checked against
 * is the actual on-disk format rather than a second description of it.
 *
 * Usage: latrec_fixture <dir> <role>   (records are printed to stdout as
 * "seq stage aux aux2" lines so the reader's view can be compared field by
 * field, without the test having to hardcode the same table twice.)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "libe3/latrec.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <dir> <role>\n", argv[0]);
        return 2;
    }
    setenv("LATREC_DIR", argv[1], 1);

    /* A small ring: the point is the format, not the capacity. */
    if (setenv("LATREC_ENTRIES_LOG2", "12", 1) != 0) return 1;
    if (latrec_tls_open_as(argv[2]) == NULL) return 1;
    if (!latrec_tls->enabled) {
        fprintf(stderr, "ring did not open (LATREC_DIR=%s)\n", argv[1]);
        return 1;
    }

    /* One record per stage family, so the reader is exercised across the whole
     * id range rather than one block, plus the field packing at its limits:
     * seq is 48 bits and aux/aux2 are full 64-bit. */
    const struct { uint64_t seq; uint8_t stage; uint64_t aux, aux2; } recs[] = {
        {1,                     LATREC_P0_PUSH_ENTRY,  0x1234,             0},
        {2,                     LATREC_W4_SENT_TO_E3,  7,                  3},
        {3,                     LATREC_LE0_EMIT_ENTER, 42,                 1},
        {4,                     LATREC_L3_SEND_DONE,   184,                0},
        {5,                     LATREC_D5_ADMITTED,    99,   LATREC_OUT_SHED_LAG},
        {6,                     LATREC_M7_SLOT_EXIT,   0x00B7000B,         4},
        {7,                     LATREC_B6_LIVE_ON_AIR, 0x02330001,         2},
        {8,                     LATREC_X3_CTRL_SCTP_SENT, 512,             0},
        {0x0000FFFFFFFFFFFFull, LATREC_L9_DROP, 0xFFFFFFFFFFFFFFFFull,
                                                LATREC_DROP_SEND},
    };
    const size_t n = sizeof(recs) / sizeof(recs[0]);
    for (size_t i = 0; i < n; i++) {
        latrec_tstamp(recs[i].seq, recs[i].stage, recs[i].aux, recs[i].aux2);
        printf("%llu %u %llu %llu\n",
               (unsigned long long)recs[i].seq, (unsigned)recs[i].stage,
               (unsigned long long)recs[i].aux, (unsigned long long)recs[i].aux2);
    }
    fflush(stdout);
    /* Flushed by the atexit hook, so rec_count lands in the header. */
    return 0;
}
