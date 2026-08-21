/**
 * @file latrec_fixture.c
 * @brief Writes a ring with known contents, for the reader round-trip test.
 *
 * Uses the real writer (libe3/latrec.h), so what the reader is checked against
 * is the actual on-disk format rather than a second description of it. Uses
 * the low-level explicit-ring API (latrec_open_in/latrec_stamp/latrec_close)
 * rather than the TLS convenience layer (latrec_tls_open_as/latrec_tstamp),
 * since the latter are true no-op stubs unless the library was built with
 * LIBE3_ENABLE_LATREC=ON -- this fixture validates the on-disk format itself,
 * which does not depend on that flag, so it stays buildable and meaningful in
 * every configuration.
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
    /* A small ring: the point is the format, not the capacity. */
    latrec_t ring;
    const int opened = latrec_open_in(&ring, argv[1], argv[2], 12);
    if (opened <= 0) {
        fprintf(stderr, "ring did not open in %s: %d\n", argv[1], opened);
        return 1;
    }

    /* One record per stage family, so the reader is exercised across the whole
     * id range rather than one block, plus the field packing at its limits:
     * seq is 48 bits and aux/aux2 are full 64-bit. */
    const struct { uint64_t seq; uint8_t stage; uint64_t aux, aux2; } recs[] = {
        {1,                     LATREC_RECORD_BEGIN,      0x1234,          0},
        {2,                     LATREC_ENCODE_E3SM_DONE,  7,               3},
        {3,                     LATREC_EMIT_ENTER,        42,              1},
        {4,                     LATREC_SEND_DONE,         184,             0},
        {5,                     LATREC_ADMITTED,          99, LATREC_OUT_SHED_LAG},
        {6,                     LATREC_APPLY_CONTROL_DONE, 1,              0},
        {7,                     LATREC_LIVE_ON_AIR,       0x02330001,      2},
        {8,                     LATREC_ENCODE_E2AP_DONE,  512,             0},
        {0x0000FFFFFFFFFFFFull, LATREC_DROP, 0xFFFFFFFFFFFFFFFFull,
                                                LATREC_DROP_SEND},
    };
    const size_t n = sizeof(recs) / sizeof(recs[0]);
    for (size_t i = 0; i < n; i++) {
        latrec_stamp(&ring, recs[i].seq, recs[i].stage, recs[i].aux, recs[i].aux2);
        printf("%llu %u %llu %llu\n",
               (unsigned long long)recs[i].seq, (unsigned)recs[i].stage,
               (unsigned long long)recs[i].aux, (unsigned long long)recs[i].aux2);
    }
    fflush(stdout);
    latrec_close(&ring);   /* flushes rec_count and the closing clock pair */
    return 0;
}
