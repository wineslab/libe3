/**
 * @file latrec.c
 * @brief Per-thread ring storage and sequence allocator for latrec.
 *
 * latrec.h is usable on its own with an explicitly owned latrec_t. This file
 * backs the convenience API on top of it: one ring per calling thread, opened
 * on first stamp and named <prefix>.<tid>, plus the process-wide sequence
 * counter. Keeping the storage in one translation unit gives a single ring per
 * thread rather than one per includer.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/latrec.h"

#include <errno.h>   /* program_invocation_short_name */
#include <pthread.h>
#include <sys/syscall.h>

/* Capacity of a per-thread ring, as a power of two. The mapping is pre-faulted
 * at open, so the ring is resident and a stamp costs the same at any size; the
 * size sets the footprint and how long a run fits before wrapping. It is
 * file-backed, so that footprint is page cache and disk, not anonymous memory.
 *
 * Every thread that stamps in a LIBE3_ENABLE_LATREC build opens a ring, with
 * no per-run opt-out, so this default is what an ordinary run of an enabled
 * build costs per thread. It is deliberately modest for that reason: a long
 * capture raises it per role (see entries_log2() below and docs/latrec.md)
 * rather than every test and example paying for a capacity only a benchmark
 * needs. A ring that wrapped says so: rec_count keeps counting past the
 * capacity. */
#ifndef LATREC_TLS_ENTRIES_LOG2
#define LATREC_TLS_ENTRIES_LOG2 18 /* 256 K records = 8 MiB per thread */
#endif
#define LATREC_ENTRIES_LOG2_MIN 12
#define LATREC_ENTRIES_LOG2_MAX 28
#define LATREC_MAX_RINGS 64

/* Threads refer here until they open a ring of their own. It stays zeroed, so
 * enabled == 0 and a stamp from a thread that never called latrec_tls_open()
 * is discarded at the same check that disables tracing globally. */
static latrec_t           disabled_ring;

__thread latrec_t* latrec_tls = &disabled_ring;

static latrec_t*          open_rings[LATREC_MAX_RINGS];
static int                n_open_rings;
static pthread_mutex_t    rings_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t     atexit_once = PTHREAD_ONCE_INIT;

/* Threads do not close their rings, so rec_count and the closing clock pair
 * are refreshed at exit. Records are valid without this (t_ns != 0); it only
 * fills in the header counts. */
static void flush_open_rings(void)
{
    pthread_mutex_lock(&rings_mu);
    for (int i = 0; i < n_open_rings; i++) latrec_heartbeat(open_rings[i]);
    pthread_mutex_unlock(&rings_mu);
}

static void install_atexit(void) { atexit(flush_open_rings); }

/* LATREC_ENTRIES_LOG2 overrides the compiled-in capacity; out-of-range and
 * unparseable values fall back to it. */
static unsigned entries_log2_from(const char* name)
{
    const char* e = getenv(name);
    if (e && *e) {
        char* end = NULL;
        long v = strtol(e, &end, 10);
        if (end && !*end && v >= LATREC_ENTRIES_LOG2_MIN && v <= LATREC_ENTRIES_LOG2_MAX)
            return (unsigned)v;
        fprintf(stderr, "[latrec] ignoring %s=%s (expected %d..%d)\n",
                name, e, LATREC_ENTRIES_LOG2_MIN, LATREC_ENTRIES_LOG2_MAX);
    }
    return 0;
}

/* Ring capacity for a role: LATREC_ENTRIES_LOG2_<ROLE> if set, else
 * LATREC_ENTRIES_LOG2, else the compiled-in default. The per-role name is the
 * role uppercased with every non-alphanumeric character replaced by '_', so
 * role "oai.mac" reads LATREC_ENTRIES_LOG2_OAI_MAC. Roles differ by orders of
 * magnitude in stamp rate, so sizing them together either wastes memory or
 * wraps the busiest ring. */
static unsigned entries_log2(const char* role)
{
    if (role && *role) {
        char name[128];
        int n = snprintf(name, sizeof(name), "LATREC_ENTRIES_LOG2_%s", role);
        if (n > 0 && (size_t)n < sizeof(name)) {
            for (char* c = name + sizeof("LATREC_ENTRIES_LOG2_") - 1; *c; c++)
                *c = (*c >= 'a' && *c <= 'z') ? (char)(*c - 'a' + 'A')
                   : ((*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) ? *c : '_';
            const unsigned v = entries_log2_from(name);
            if (v)
                return v;
        }
    }
    const unsigned v = entries_log2_from("LATREC_ENTRIES_LOG2");
    return v ? v : LATREC_TLS_ENTRIES_LOG2;
}

/* Process-wide ring directory. Written once at start-up by whoever owns the
 * process, read by every thread that opens a ring; the mutex is only there to
 * make that publication well-defined, since ring opens are not on a hot path. */
static char output_dir[512];

void latrec_set_output_dir(const char* dir)
{
    pthread_mutex_lock(&rings_mu);
    if (dir && *dir) snprintf(output_dir, sizeof(output_dir), "%s", dir);
    else             output_dir[0] = '\0';
    pthread_mutex_unlock(&rings_mu);
}

latrec_t* latrec_tls_open_as(const char* role)
{
    if (latrec_tls != &disabled_ring) return latrec_tls;   /* already opened */

    /* Heap, not TLS: flush_open_rings() and the mapping are still live at
     * process exit, while a thread's TLS is released when it exits. Not
     * freed, like the mapping. */
    latrec_t* r = (latrec_t*)calloc(1, sizeof(*r));
    if (!r) return latrec_tls;                             /* stays disabled */

    if (!role || !*role) {
#ifdef __GLIBC__
        role = program_invocation_short_name;
#else
        role = "latrec";
#endif
    }
    char name[64];
    snprintf(name, sizeof(name), "%s.%ld", role, (long)syscall(SYS_gettid));

    char dir[sizeof(output_dir)];
    pthread_mutex_lock(&rings_mu);
    memcpy(dir, output_dir, sizeof(dir));
    pthread_mutex_unlock(&rings_mu);

    /* A failed open leaves a disabled ring, which every later latrec_stamp()
     * skips at the enabled check. */
    latrec_open_in(r, dir, name, entries_log2(role));
    latrec_tls = r;

    if (r->enabled) {
        pthread_once(&atexit_once, install_atexit);
        pthread_mutex_lock(&rings_mu);
        /* Beyond the cap a ring still records; it gets no exit flush. */
        if (n_open_rings < LATREC_MAX_RINGS) open_rings[n_open_rings++] = r;
        pthread_mutex_unlock(&rings_mu);
    }
    return latrec_tls;
}

latrec_t* latrec_tls_open(void) { return latrec_tls_open_as(NULL); }

uint64_t latrec_seq_next(void)
{
    static uint64_t counter;
    return __atomic_add_fetch(&counter, 1, __ATOMIC_RELAXED);
}
