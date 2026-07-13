/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * latrec.h — lock-free per-thread latency stamp recorder for the
 * RAN -> libe3 -> dApp pipeline. Header-only, C99 (usable from C and C++).
 *
 * Design (agreed measurement plan):
 *   - One ring PER WRITER THREAD: single producer, no atomics, no locks.
 *   - File-backed mmap, pre-faulted at open: a stamp is one vDSO clock read
 *     plus four stores (~30 ns). No syscalls, no formatting, no branches
 *     beyond one predicted `enabled` check. Kernel writeback persists the
 *     data in the background; records survive a crash (t_ns != 0 = valid).
 *   - 32-byte records: seq:48 | cpu:8 | stage:8, t_ns, aux, aux2.
 *     seq is assigned ONCE where IQs leave the RAN and joins every stage
 *     offline. aux/aux2 carry stage-specific payloads (see the stage table).
 *   - Disabled unless the LATREC_DIR environment variable is set; when unset
 *     every latrec_* call is a cheap no-op, so instrumented builds are safe
 *     to run in production.
 *
 * File format: 4 KiB header (latrec_hdr) + entries * 32 B records.
 * Offline tooling: radio_code/libe3_tests/analysis/ (latrec2csv + report).
 */
#ifndef LATREC_H
#define LATREC_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1   /* sched_getcpu (no-op if the TU included sched.h already) */
#endif

#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__)
/* glibc exports the symbol unconditionally; only the declaration is gated
 * behind _GNU_SOURCE, which a TU may have locked in before including us. */
extern int sched_getcpu(void);
#endif

/* ---- stage catalog (stable IDs; never renumber, only append) -------------- */
enum {
    /* OAI producer — PHY thread (e3_ran_buffers.c) */
    LATREC_P0_PUSH_ENTRY   = 0x10, /* aux = sfn<<16|slot<<8|n_valid, aux2 = valid_mask */
    LATREC_P1_MASK_DONE    = 0x11,
    LATREC_P2_COPY_DONE    = 0x12, /* aux = bytes copied */
    LATREC_P3_INFO_BUILT   = 0x13, /* aux = timestamp_ns (the in-band origin stamp) */
    LATREC_P4_PUBLISHED    = 0x14, /* aux = buffer_idx<<8|write_idx (ring position) */

    /* OAI L1-KPM SM worker — emit thread (l1_kpm_sm.c) */
    LATREC_W0_WAKE         = 0x20,
    LATREC_W1_SLOT_SELECT  = 0x21, /* aux = seq gap since last emit (ring skips) */
    LATREC_W2_META_BUILT   = 0x22,
    LATREC_W3_ENCODE_DONE  = 0x23, /* aux = encoded bytes */
    LATREC_W4_SENT_TO_E3   = 0x24, /* handoff into libe3 (same thread => L0 follows) */

    /* libe3 — enqueue on caller thread, publisher thread after (e3_interface) */
    LATREC_L0_ENQUEUE      = 0x30, /* seq = libe3 enqueue counter; aux = queue depth */
    LATREC_L1_DEQUEUE      = 0x31,
    LATREC_L2_ENCODE_DONE  = 0x32, /* aux = wire bytes */
    LATREC_L3_ZMQ_SENT     = 0x33,
    LATREC_L9_DROP         = 0x39, /* aux2 = drop reason */

    /* dApp — ingest thread (e3_manager subscription loop + handler) */
    LATREC_D0_RECV         = 0x40, /* aux = drain-burst position (backlog proxy) */
    LATREC_D1_PARSED       = 0x41,
    LATREC_D2_DISPATCHED   = 0x42,
    LATREC_D3_HANDLER_IN   = 0x43,
    LATREC_D4_RX_ACCOUNTED = 0x44, /* aux = age_us at arrival */
    LATREC_D5_ADMITTED     = 0x45, /* aux2 = outcome (see LATREC_OUT_*) */
    LATREC_D6_COMPUTED     = 0x46, /* aux = n_valid symbols converted */
    LATREC_D7_DETECTED     = 0x47,
    LATREC_D8_SM_SENT      = 0x48,
    LATREC_D9_SNAPPED      = 0x49, /* aux2 = 1 handoff ok, 0 try_lock miss */

    /* dApp — publisher thread */
    LATREC_V0_SNAP_TAKEN   = 0x50, /* aux = frames skipped since last consume */
    LATREC_V1_QUANTIZED    = 0x51,
    LATREC_V2_PUBLISHED    = 0x52,

    /* slow-lane context records (1 Hz, own ring) */
    LATREC_C0_CONTEXT      = 0x60  /* aux = involuntary ctx switches, aux2 = cur freq */
};

/* D5 admission outcomes (aux2) */
enum {
    LATREC_OUT_PROCESSED = 0,
    LATREC_OUT_SHED_AGE  = 1,
    LATREC_OUT_SHED_LAG  = 2,
    LATREC_OUT_NO_RANGE  = 3
};

#define LATREC_MAGIC   0x31524C41u /* "ALR1" */
#define LATREC_HDR_LEN 4096

typedef struct {
    uint64_t sc;    /* seq:48 (low) | cpu:8 | stage:8 (high byte)              */
    uint64_t t_ns;  /* CLOCK_MONOTONIC; written LAST: nonzero == record valid  */
    uint64_t aux;
    uint64_t aux2;
} latrec_rec;

typedef struct {
    uint32_t magic, version;
    uint64_t entries;        /* ring capacity (power of two)                   */
    uint64_t rec_count;      /* final index, written at close                  */
    uint64_t t0_mono_ns;     /* paired clocks at open: mono<->wall mapping     */
    uint64_t t0_real_ns;
    char     name[64];
    uint8_t  pad[4096 - 4 * 2 - 8 * 4 - 64];
} latrec_hdr;

typedef struct {
    latrec_rec* recs;
    uint64_t    idx;
    uint64_t    mask;
    void*       map;
    size_t      map_len;
    int         enabled;
} latrec_t;

static inline uint64_t latrec_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Open (or no-op when LATREC_DIR is unset). name: "process.thread" — becomes
 * <LATREC_DIR>/<name>.latrec. entries_log2: ring capacity, e.g. 22 = 4 M
 * records = 128 MiB (≈ 40 min of a 26-stamp 1800/s pipeline thread). */
static inline int latrec_open(latrec_t* r, const char* name, unsigned entries_log2)
{
    memset(r, 0, sizeof(*r));
    const char* dir = getenv("LATREC_DIR");
    if (!dir || !*dir) return 0;                     /* tracing disabled */

    const uint64_t entries = 1ull << entries_log2;
    const size_t   len     = LATREC_HDR_LEN + (size_t)entries * sizeof(latrec_rec);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.latrec", dir, name);
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)len) != 0) { close(fd); return -1; }
    void* map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);                                       /* mapping keeps the file */
    if (map == MAP_FAILED) return -1;
    memset(map, 0, len);                             /* pre-fault EVERY page   */

    latrec_hdr* h = (latrec_hdr*)map;
    h->magic      = LATREC_MAGIC;
    h->version    = 1;
    h->entries    = entries;
    h->t0_mono_ns = latrec_now_ns();
    struct timespec rt;
    clock_gettime(CLOCK_REALTIME, &rt);
    h->t0_real_ns = (uint64_t)rt.tv_sec * 1000000000ull + (uint64_t)rt.tv_nsec;
    snprintf(h->name, sizeof(h->name), "%s", name);

    r->recs    = (latrec_rec*)((uint8_t*)map + LATREC_HDR_LEN);
    r->mask    = entries - 1;
    r->map     = map;
    r->map_len = len;
    r->enabled = 1;
    return 1;
}

/* THE hot-path call: one clock read + four stores. Single writer per ring. */
static inline void latrec_stamp(latrec_t* r, uint64_t seq, uint8_t stage,
                                uint64_t aux, uint64_t aux2)
{
    if (!r->enabled) return;
    latrec_rec* rec = &r->recs[r->idx++ & r->mask];
    rec->sc   = (seq & 0x0000FFFFFFFFFFFFull)
              | ((uint64_t)(unsigned)sched_getcpu() << 48)
              | ((uint64_t)stage << 56);
    rec->aux  = aux;
    rec->aux2 = aux2;
    rec->t_ns = latrec_now_ns();     /* last: marks the record valid */
}

static inline void latrec_close(latrec_t* r)
{
    if (!r->enabled) return;
    ((latrec_hdr*)r->map)->rec_count = r->idx;
    msync(r->map, r->map_len, MS_SYNC);
    munmap(r->map, r->map_len);
    r->enabled = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LATREC_H */
