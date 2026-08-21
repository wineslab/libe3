/**
 * @file latrec.h
 * @brief Lock-free per-thread latency stamp recorder (header-only, C99).
 *
 * One mmap-backed ring per writer thread, single producer, no locks: a stamp
 * is one CLOCK_MONOTONIC read and four stores, with no syscall, allocation or
 * formatting. Records are 32 bytes (seq:48 | cpu:8 | stage:8, t_ns, aux,
 * aux2); seq is assigned once at the producer and joins the stages offline,
 * aux/aux2 carry the per-stage payloads listed in the stage catalog below.
 *
 * CLOCK_MONOTONIC shares an origin across processes, so RAN, dApp and xApp
 * stamps join without calibration.
 *
 * t_ns is stored last, with release ordering: a reader that acquire-loads a
 * nonzero t_ns also observes that record's sc, aux and aux2. That makes one
 * record self-consistent; it does not stop the slot being reused, so a reader
 * of a ring that has wrapped may pair one write's t_ns with the next write's
 * payload. A live reader must therefore keep up, or detect the wrap from
 * rec_count and the single descent in t_ns.
 *
 * Gated once, at compile time, by LIBE3_ENABLE_LATREC: off by default, so a
 * normal build carries none of this, not even a disabled-ring branch. With it
 * on, tracing runs -- there is no second per-run opt-in. See the bottom of
 * this file, and docs/latrec.md for where the rings are written.
 *
 * File layout: a 4 KiB latrec_hdr followed by `entries` 32-byte records. v1
 * field offsets are frozen and v2 fields appended inside the header pad, so a
 * v1 reader parses a v2 file. Readers detect a byte-swapped LATREC_MAGIC.
 * 64-bit hosts only: a 32-bit host can tear the t_ns store.
 *
 * Installed as <libe3/latrec.h>, so a component that links libe3 includes it
 * from there. Components that do not link libe3 mirror the layout instead, and
 * the stage catalog below carries their ids too, so it stays the one place a
 * new id is checked for collisions.
 *
 * @see \ref latrec_guide "docs/latrec.md" for the full guide: the clock
 *      model, ring naming/sizing, the stage catalog table, and the
 *      capture-to-CSV workflow.
 * @see \ref path_a_e3_loop "docs/path-a-e3-loop.md" and
 *      \ref path_b_e2_e3_loop "docs/path-b-e2-e3-loop.md" for the loop boxes
 *      the stage catalog names.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIBE3_LATREC_H
#define LIBE3_LATREC_H

/* clock_gettime() needs a feature-test macro under a strict -std=. Act only
 * when the TU selected none, and use _GNU_SOURCE: _POSIX_C_SOURCE would
 * suppress the _DEFAULT_SOURCE a -std=gnu* build otherwise gets, dropping the
 * BSD/SVID declarations from a TU that includes this header first. Effective
 * only before the first system header; the #error below catches the rest. */
#if !defined(_GNU_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_DEFAULT_SOURCE) && !defined(_XOPEN_SOURCE)
#define _GNU_SOURCE 1
#endif

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if !defined(CLOCK_MONOTONIC)
#error "latrec.h: CLOCK_MONOTONIC undeclared - build with a POSIX-enabled dialect (-std=gnu11 / -std=gnu++17) or define _POSIX_C_SOURCE >= 199309L before any system header."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- portability shims ---------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#define LATREC_STORE_RELEASE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define LATREC_LOAD_ACQUIRE(p)     __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define LATREC_PREFETCH_W(p)       __builtin_prefetch((p), 1, 0)
#define LATREC_UNLIKELY(x)         __builtin_expect(!!(x), 0)
#else
/* Fallback without the builtins: correct on TSO hosts only. */
#define LATREC_STORE_RELEASE(p, v) do { *(p) = (v); } while (0)
#define LATREC_LOAD_ACQUIRE(p)     (*(p))
#define LATREC_PREFETCH_W(p)       ((void)(p))
#define LATREC_UNLIKELY(x)         (x)
#endif

#if defined(__linux__)
#define LATREC_HAVE_GETCPU 1
/* glibc exports the symbol unconditionally; only the declaration is gated
 * behind _GNU_SOURCE, which a TU may have locked in before including us. The
 * C++ spelling has to carry noexcept to match glibc's __THROW, otherwise a
 * <sched.h> included after this header is a conflicting redeclaration. */
#if defined(__cplusplus)
extern int sched_getcpu(void) noexcept;
#else
extern int sched_getcpu(void);
#endif
#else
#define LATREC_HAVE_GETCPU 0
#endif

/* Read the cpu id in the stamp only where sched_getcpu() resolves in user
 * space (x86-64 glibc goes through rseq). Elsewhere it can be a getcpu
 * syscall, so the stamp uses the id cached by latrec_refresh_cpu(). */
#if LATREC_HAVE_GETCPU && (defined(__x86_64__) || defined(__i386__))
#define LATREC_CPUID_ON_STAMP 1
#else
#define LATREC_CPUID_ON_STAMP 0
#endif

#define LATREC_CAT_(a, b) a##b
#define LATREC_CAT(a, b)  LATREC_CAT_(a, b)
#if defined(__cplusplus)
#define LATREC_STATIC_ASSERT(c, m) static_assert(c, m)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LATREC_STATIC_ASSERT(c, m) _Static_assert(c, m)
#else
#define LATREC_STATIC_ASSERT(c, m) \
    typedef char LATREC_CAT(latrec_static_assert_, __LINE__)[(c) ? 1 : -1]
#endif

/* ---- stage catalog ------------------------------------------------------- */
/*
 * One identifier per *operation* on the E3AP / E3SM / E2SM-DAPP path -- not
 * one per (operation, component) pair. Several components implement the same
 * operation: an indication is encoded by whichever Service Model produced it,
 * and E3AP framing is the same libe3 call whether the RAN or the dApp made it.
 * They share the identifier. Which component wrote a given record comes from
 * its ring name (<role>.<tid>.latrec), and which concrete message it carried
 * from the aux payloads, so a component prefix on the stage id would only
 * duplicate what the reader already knows.
 *
 * Identifiers name the boxes of the two documented loops:
 *   docs/path-a-e3-loop.md   the E3-only loop (RAN <-> dApp), boxes A1..A20
 *   docs/path-b-e2-e3-loop.md  the E2-E3 loop (dApp <-> xApp), boxes B1..B15
 * Each comment below cites the box(es) it bounds. Boxes that hand off inside
 * one function on one thread share a single boundary stamp rather than
 * carrying a separate exit and entry.
 *
 * Stable IDs: from this baseline on, never renumber, only append.
 */
enum {
    /* --- Source side: recording and processing what a Service Model reports.
     * A1 (E3 data recording) and A2 (Processing). Stamped by whichever
     * component is the data source -- an OAI PHY thread, an OCUDU jbpf hook
     * feeding its E3 controller, a cuBB L1 data-lake reader, or the Simple SM
     * shipped in examples/sm_simple. seq = the source's own record counter;
     * aux = a source-defined slot/frame key (e.g. sfn<<16|slot). */
    LATREC_RECORD_BEGIN      = 0x10, /* A1 entry: raw input capture begins */
    LATREC_PROCESS_BEGIN     = 0x11, /* A1 exit / A2 entry: input in hand,
                                        SM-side processing begins */

    /* --- E3SM codec. A3 and A12 (encode), A9 and A18 (decode). The same
     * Service Model codec on either leg and in either role; aux2 = PduType,
     * which is what separates an indication from a control or a report.
     * ENCODE_E3SM_BEGIN doubles as A2's exit. */
    LATREC_ENCODE_E3SM_BEGIN = 0x18, /* A2 exit / A3 (or A12) entry */
    LATREC_ENCODE_E3SM_DONE  = 0x19, /* A3 (or A12) exit; aux = encoded bytes.
                                        Hands to EMIT_ENTER */
    LATREC_DECODE_E3SM_BEGIN = 0x1A, /* A9 (or A18) entry; aux = payload bytes */
    LATREC_DECODE_E3SM_DONE  = 0x1B, /* A9 (or A18) exit; aux = a decoded count
                                        the SM cares about (PRBs, slots, ...) */

    /* --- E3AP codec, queue and transport: libe3 itself. A4..A8 on the
     * forward leg, A13..A17 on the return leg, B10..B13 on the policy leg --
     * the same five outbound and three inbound stamps each time.
     * seq = Pdu::enqueue_seq outbound (allocated at EMIT_ENTER so this record
     * and the rest of the leg share one key), allocated at RECV inbound. */
    LATREC_EMIT_ENTER        = 0x20, /* A4 entry: the producer's thread crosses
                                        into libe3. aux = the producer's own
                                        trace seq (whatever it last passed to
                                        latrec_ctx_set(), 0 if none), which is
                                        what joins the producer's stages to
                                        this leg; aux2 = RAN function id.
                                        EMIT_ENTER->ENQUEUE is Pdu construction */
    LATREC_ENQUEUE           = 0x21, /* A4 first half exit / A5 entry;
                                        aux2 = PduType */
    LATREC_DEQUEUE           = 0x22, /* A5 exit / A4 second half entry, on the
                                        publisher thread; aux2 = PduType */
    LATREC_ENCODE_E3AP_DONE  = 0x23, /* A4 exit / A6 entry; aux = wire bytes */
    LATREC_SEND_DONE         = 0x24, /* A6 exit: the connector's send() returned;
                                        aux = message_id. NOT the wire: a ZMQ PUB
                                        send only copies into ZMQ's queue and the
                                        socket write happens later on ZMQ's own
                                        io thread, so ENCODE_E3AP_DONE->SEND_DONE
                                        excludes that hop for ZMQ but includes it
                                        for the POSIX connectors */
    LATREC_RECV              = 0x25, /* A7 / A8 entry; aux = wire bytes read */
    LATREC_DECODE_E3AP_DONE  = 0x26, /* A8 exit; aux = message_id,
                                        aux2 = PduType */
    LATREC_DELIVER_BEGIN     = 0x27, /* libe3's inbound filter passed; about to
                                        invoke the application. A9's entry seen
                                        from the library side. A message the
                                        dApp-id filter rejects gets neither this
                                        nor DELIVER_DONE: see
                                        LATREC_DROP_FILTERED */
    LATREC_DELIVER_DONE      = 0x28, /* the application handler returned;
                                        aux2 = PduType */
    LATREC_REPORT_QUEUED     = 0x29, /* A19b entry: report handed to the report
                                        worker */
    LATREC_REPORT_DONE       = 0x2A, /* A19b exit */
    LATREC_SESSION_QUEUED    = 0x2B, /* the batching seam a language binding
                                        drains, inside A9 / B13; aux = kind */
    LATREC_SESSION_POLLED    = 0x2C, /* handed to the binding; aux = position in
                                        batch, aux2 = batch size */

    LATREC_DROP              = 0x2F, /* a PDU went no further; aux2 = reason
                                        (LATREC_DROP_*). An outcome marker, not
                                        a box boundary */

    /* --- Bootstrap: the setup and subscription handshakes, and Service Model
     * lifecycle. These precede the measured loop rather than sitting on it, so
     * they are bracketed per handshake instead of per box. Either role stamps
     * the same pair -- the ring tells you which side you are looking at.
     * seq = the request's message id. */
    LATREC_SETUP_BEGIN       = 0x30, /* setup request sent (initiator) or
                                        reached the handler (responder) */
    LATREC_SETUP_DONE        = 0x31, /* setup response written, or the session
                                        is ready to carry traffic; aux2 = 1
                                        accepted, 0 rejected. A setup that never
                                        decoded into a request is rejected with
                                        no SETUP_BEGIN to pair with */
    LATREC_SUB_BEGIN         = 0x32, /* subscription request sent or received */
    LATREC_SUB_DONE          = 0x33, /* subscription response queued or
                                        confirmed; aux2 = 1 accepted,
                                        0 rejected; aux = granted id */
    LATREC_SM_START          = 0x34, /* first subscription: SM worker up */
    LATREC_SM_STOP           = 0x35, /* last unsubscribe: worker joined;
                                        aux2 = batches emitted */
    LATREC_SM_STATUS_BEGIN   = 0x36, /* peer connect/disconnect handler entered;
                                        aux = RAN function id */
    LATREC_SM_STATUS_DONE    = 0x37, /* handler returned, cadences recomputed
                                        and any RIC update sent */

    /* --- Receiving application, dApp side: A10 (Process), A11 (Create control
     * or report), and B15 (Apply policy). Bespoke application logic rather
     * than a library call, so these unify across dApp implementations (the
     * Python framework and the C++ e3_manager dApps) but never across roles.
     * seq = the key of the message being handled. */
    LATREC_PROCESS_DONE      = 0x40, /* A10 exit: the dApp reached a decision.
                                        A10's entry is DECODE_E3SM_DONE */
    LATREC_CREATE_OUTPUT     = 0x41, /* A11: control or report built and queued;
                                        aux = bytes. Hands to ENCODE_E3SM_BEGIN */
    LATREC_APPLY_POLICY_DONE = 0x42, /* B15 exit: an xApp policy applied in the
                                        dApp. B15's entry is DECODE_E3SM_DONE */
    LATREC_ADMITTED          = 0x43, /* the handler decided whether to do the
                                        work at all; aux2 = outcome
                                        (LATREC_OUT_*). An outcome marker */

    /* --- Receiving application, RAN side: A19a (Apply control). The MAC's own
     * slot-internal scheduler timing is not an E3 operation and is not in this
     * catalog. seq = the E3 message id of the control being applied. */
    LATREC_APPLY_CONTROL_DONE = 0x48, /* A19a: mask or policy installed in the
                                         MAC; aux = ok. Takes the scheduler
                                         lock, so this couples the control
                                         thread to the slot-deadline thread */
    LATREC_LIVE_ON_AIR        = 0x49, /* A19a terminal: the first scheduler tick
                                         that put this install (or its clear)
                                         on air. aux = sfn<<16|slot,
                                         aux2 = installs coalesced into this
                                         tick (>1 means the ones behind the
                                         keyed install rode along). Stamped on
                                         the MAC thread but keyed on the control
                                         that installed the mask. A clear with
                                         no control behind it carries seq 0 */

    /* --- Acknowledgment tail. Either direction: the receiver emits an ack,
     * the sender observes it. seq = the acknowledged message's id. */
    LATREC_ACK_SENT          = 0x4A, /* aux = response code, aux2 = reject
                                        reason, 0 when accepted */
    LATREC_ACK_RECV          = 0x4B,

    /* --- E2-E3 bridge: A20 (a dApp report going up toward E2) and B9 (an xApp
     * control coming down toward E3). One pair for both directions;
     * aux2 = 0 report-up, 1 control-down. seq = a bridge-local counter, since
     * the E2 envelope carries no id the bridge could propagate. */
    LATREC_BRIDGE_IN         = 0x50, /* message reached the bridge; aux = bytes */
    LATREC_BRIDGE_OUT        = 0x51, /* forwarded on; aux = peers or
                                        subscriptions it went to */

    /* --- E2AP codec and SCTP transport. B1 and B6 (encode and send), B2 and
     * B7 (decode). The same codec whether the xApp side or the E2 agent calls
     * it, on either flow; aux2 = PDU type. seq = the stamping process's own
     * counter: no id crosses the RIC, so the legs pair by time, not by seq. */
    LATREC_ENCODE_E2AP_BEGIN = 0x58, /* aux = payload bytes */
    LATREC_ENCODE_E2AP_DONE  = 0x59, /* SCTP send returned; aux = wire bytes */
    LATREC_DECODE_E2AP_BEGIN = 0x5A, /* aux = wire bytes received */
    LATREC_DECODE_E2AP_DONE  = 0x5B, /* aux2 = PDU type */

    /* --- E2SM (including E2SM-DAPP) codec, and the xApp's own decision. B3
     * and B8 (decode), B5 (encode), B4 (xApp processing). The codec pairs are
     * shared by every framework that speaks E2SM -- the E2 agent's own SM
     * handling and any xApp framework, Python or otherwise. XAPP_PROCESS_DONE
     * stays separate from the codecs: it is application logic, not a library
     * call. seq = the request id the stamping side keys on. */
    LATREC_DECODE_E2SM_BEGIN = 0x60, /* B3 / B8 entry; the E2AP layer hands the
                                        inner payload over; aux = bytes */
    LATREC_DECODE_E2SM_DONE  = 0x61, /* B3 / B8 exit; also B4's entry */
    LATREC_XAPP_PROCESS_DONE = 0x62, /* B4 exit: the xApp returned a decision.
                                        Also B5's entry */
    LATREC_ENCODE_E2SM_BEGIN = 0x63, /* B5 (or B1) entry, where no xApp
                                        decision precedes it */
    LATREC_ENCODE_E2SM_DONE  = 0x64, /* B5 / B1 exit; aux = bytes. Hands to
                                        ENCODE_E2AP_BEGIN */

    /* --- Off-path records. Not boxes: these answer "how loaded was the
     * process" and "why did this row end early", so the one-stamp-per-box
     * rule does not apply to them. */
    LATREC_WAIT_ENTER        = 0x70, /* a worker went back to its wait carrying
                                        the item it just handled. WAIT_ENTER ->
                                        the next row's RECORD_BEGIN is time
                                        blocked; stamped once per item, so a
                                        periodic wait that times out and loops
                                        does not restamp it */
    LATREC_SKIPPED           = 0x71, /* a fetched item produced no indication;
                                        aux2 = reason (LATREC_SKIP_*) */
    LATREC_CONTEXT           = 0x72  /* slow lane, ~1 Hz on its own ring:
                                        aux = involuntary context switches
                                        since the last tick, aux2 = current CPU
                                        frequency in kHz (0 if unavailable).
                                        seq = 0: no per-message key */
};

/* D5 admission outcomes (aux2) */
enum {
    LATREC_OUT_PROCESSED = 0,
    LATREC_OUT_SHED_AGE  = 1,
    LATREC_OUT_SHED_LAG  = 2,
    LATREC_OUT_NO_RANGE  = 3
};

/* LATREC_SKIPPED reasons (aux2). A telemetry SM drops a fetched publish for
 * reasons libe3 never sees, so they are numbered separately from
 * LATREC_DROP_*. The last two are reachable only on the sensing path. */
enum {
    LATREC_SKIP_NO_SUBSCRIBERS = 1, /* fetched, then the last dApp went away   */
    LATREC_SKIP_ENCODE         = 2, /* indication encode failed                */
    LATREC_SKIP_COALESCED      = 3, /* superseded by a newer publish inside the
                                     * same emission period (periodic mode)    */
    LATREC_SKIP_NO_SHM         = 4, /* /e3_l2_sensing unavailable, so the
                                     * indication has no ring entry to name    */
    LATREC_SKIP_RANGES_LOST    = 5  /* the publish's seqlock slot was recycled
                                     * by newer writes before it was read out  */
};

/* LATREC_DROP reasons (aux2) */
enum {
    LATREC_DROP_QUEUE_PUSH   = 1,
    LATREC_DROP_ENCODE       = 2,
    LATREC_DROP_SEND         = 3,
    LATREC_DROP_DECODE       = 4,
    LATREC_DROP_REPORT_QUEUE = 5,
    LATREC_DROP_NO_HANDLER   = 6,
    LATREC_DROP_SESSION_QUEUE = 7,
    LATREC_DROP_FILTERED     = 8,
    /* A push rejected because shutdown() had already been called on that
     * queue, distinct from LATREC_DROP_QUEUE_PUSH/REPORT_QUEUE (the ring was
     * actually full). E3Interface::stop() shuts response_queue_/report_queue_
     * down before joining a registered ServiceModel's own producer thread
     * (which it does not own directly -- that happens later, via
     * SmRegistry::clear()), so a handful of these during teardown is expected
     * and does not indicate undersized capacity. */
    LATREC_DROP_SHUTDOWN     = 9
};

#define LATREC_MAGIC   0x31524C41u /* "ALR1" */
#define LATREC_VERSION 2u
#define LATREC_HDR_LEN 4096

/* Measured timestamp cost above which latrec_open warns. A vDSO read is tens
 * of ns; a syscall fallback is several hundred. */
#define LATREC_CLOCK_SLOW_NS 100u

typedef struct {
    uint64_t sc;    /* seq:48 (low) | cpu:8 | stage:8 (high byte)              */
    uint64_t t_ns;  /* CLOCK_MONOTONIC; released last: nonzero == valid record */
    uint64_t aux;
    uint64_t aux2;
} latrec_rec;

/* v1 layout through name[]; v2 appends inside the pad. v1 field offsets are
 * frozen. */
typedef struct {
    uint32_t magic, version;
    uint64_t entries;             /* ring capacity (power of two)              */
    uint64_t rec_count;           /* final index; refreshed by latrec_heartbeat */
    uint64_t t0_mono_ns;          /* paired clocks at open: mono<->wall mapping */
    uint64_t t0_real_ns;
    char     name[64];
    /* --- v2 --- */
    uint32_t rec_size;            /* sizeof(latrec_rec), for reader validation  */
    uint32_t clock_ns_per_call;   /* measured timestamp cost (0 = not measured) */
    uint64_t t1_mono_ns;          /* paired clocks refreshed by heartbeat/close */
    uint64_t t1_real_ns;
    uint8_t  pad[LATREC_HDR_LEN - 4 * 4 - 8 * 6 - 64];
} latrec_hdr;

LATREC_STATIC_ASSERT(sizeof(latrec_rec) == 32, "latrec_rec must stay 32 bytes");
LATREC_STATIC_ASSERT(sizeof(latrec_hdr) == LATREC_HDR_LEN, "latrec_hdr must be 4 KiB");

typedef struct {
    latrec_rec* recs;
    uint64_t    idx;
    uint64_t    mask;
    void*       map;
    size_t      map_len;
    int         enabled;
    uint32_t    cpu;    /* cpu id used by the stamp when LATREC_CPUID_ON_STAMP
                         * is 0; set by latrec_refresh_cpu()                   */
    uint64_t    ctx_seq; /* seq for code too deep to be passed one; see
                          * latrec_ctx_set()                                   */
} latrec_t;

static inline uint64_t latrec_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline uint64_t latrec_real_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Re-read the cpu id into the ring. Call off the hot path: sched_getcpu() is
 * a syscall on targets where LATREC_CPUID_ON_STAMP is 0. */
static inline void latrec_refresh_cpu(latrec_t* r)
{
#if LATREC_HAVE_GETCPU
    const int cpu = sched_getcpu();
    if (cpu >= 0) r->cpu = (uint32_t)cpu;
#else
    (void)r;
#endif
}

/* Mean cost of one latrec_now_ns() over 1000 calls, about 30 us in total.
 * Called once per ring at open and recorded in the header. */
static inline uint32_t latrec_measure_clock_ns(void)
{
    enum { ITERS = 1000 };
    uint64_t sink = 0;
    const uint64_t t0 = latrec_now_ns();
    for (int i = 0; i < ITERS; i++) sink += latrec_now_ns();
    const uint64_t t1 = latrec_now_ns();
    if (sink == 0 || t1 <= t0) return 0;
    return (uint32_t)((t1 - t0) / (uint64_t)ITERS);
}

/* Where rings go when the caller does not name a directory. A build-time
 * setting, like the recorder itself: -DLATREC_DEFAULT_DIR=... at configure
 * time. Runtime placement is chosen with latrec_open_in(), or process-wide
 * with latrec_set_output_dir(). */
#ifndef LATREC_DEFAULT_DIR
#define LATREC_DEFAULT_DIR "/tmp/latrec"
#endif

/* Open a ring under `dir`. name ("process.thread") becomes
 * <dir>/<name>.latrec and must be unique per writer thread across every
 * process sharing the directory: the file is opened O_TRUNC, so a collision
 * discards the other writer's ring. entries_log2 sets the capacity (22 = 4 M
 * records = 128 MiB). `dir` is created if missing (one level only).
 * Returns 1 on success, -1 on error.
 *
 * There is no "tracing disabled" return: in a build with LIBE3_ENABLE_LATREC
 * the recorder records. Whether it exists at all is the build's decision. */
static inline int latrec_open_in(latrec_t* r, const char* dir, const char* name,
                                 unsigned entries_log2)
{
    memset(r, 0, sizeof(*r));
    if (!dir || !*dir) dir = LATREC_DEFAULT_DIR;
    mkdir(dir, 0755);                /* EEXIST is the normal case; ignore */

    const uint64_t entries = 1ull << entries_log2;
    const size_t   len     = LATREC_HDR_LEN + (size_t)entries * sizeof(latrec_rec);

    /* Built under a temporary name and renamed once the header is written:
     * rename() is atomic, so a <name>.latrec always carries a valid header. */
    char path[512], tmp[520];
    snprintf(path, sizeof(path), "%s/%s.latrec", dir, name);
    snprintf(tmp, sizeof(tmp), "%s.part", path);
    int fd = open(tmp, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)len) != 0) { close(fd); unlink(tmp); return -1; }
    void* map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);                                       /* mapping keeps the file */
    if (map == MAP_FAILED) { unlink(tmp); return -1; }
    memset(map, 0, len);                             /* pre-fault every page   */

    const uint32_t clock_ns = latrec_measure_clock_ns();

    latrec_hdr* h        = (latrec_hdr*)map;
    h->magic             = LATREC_MAGIC;
    h->version           = LATREC_VERSION;
    h->entries           = entries;
    h->rec_size          = (uint32_t)sizeof(latrec_rec);
    h->clock_ns_per_call = clock_ns;
    h->t0_mono_ns        = latrec_now_ns();
    h->t0_real_ns        = latrec_real_ns();
    h->t1_mono_ns        = h->t0_mono_ns;
    h->t1_real_ns        = h->t0_real_ns;
    snprintf(h->name, sizeof(h->name), "%s", name);
    if (rename(tmp, path) != 0) { munmap(map, len); unlink(tmp); return -1; }

    r->recs    = (latrec_rec*)((uint8_t*)map + LATREC_HDR_LEN);
    r->mask    = entries - 1;
    r->map     = map;
    r->map_len = len;
    r->enabled = 1;
    latrec_refresh_cpu(r);

    if (clock_ns > LATREC_CLOCK_SLOW_NS) {
        fprintf(stderr,
                "[latrec] WARNING %s: CLOCK_MONOTONIC costs %u ns/call; the vDSO "
                "is unavailable (check the clocksource)\n", name, clock_ns);
    }
    return 1;
}

/* Open a ring under the compiled-in default directory. */
static inline int latrec_open(latrec_t* r, const char* name, unsigned entries_log2)
{
    return latrec_open_in(r, LATREC_DEFAULT_DIR, name, entries_log2);
}

/* Stamp carrying a timestamp the caller already took, for the case where the
 * moment being recorded is not the moment the stamp can be issued: handing an
 * item to another thread is only known to have *succeeded* after the handoff,
 * by which time the consumer may already have run and stamped its own stage.
 * Reading the clock before the handoff and passing it here keeps the interval
 * meaningful; issuing a fresh stamp afterwards can order the producer's record
 * after the consumer's.
 *
 * t_ns must be a CLOCK_MONOTONIC reading from latrec_now_ns(). Zero is treated
 * as "not taken" and replaced with the current time, since zero is also how an
 * unwritten slot reads. */
static inline void latrec_stamp_at(latrec_t* r, uint64_t seq, uint8_t stage,
                                   uint64_t aux, uint64_t aux2, uint64_t t_ns)
{
    if (LATREC_UNLIKELY(!r->enabled)) return;
    const uint64_t i    = r->idx++;
    latrec_rec*    rec  = &r->recs[i & r->mask];
#if LATREC_CPUID_ON_STAMP
    const uint64_t cpu  = (uint64_t)(unsigned)sched_getcpu();
#else
    const uint64_t cpu  = (uint64_t)r->cpu;
#endif
    rec->sc   = (seq & 0x0000FFFFFFFFFFFFull)
              | ((cpu & 0xFFull) << 48)
              | ((uint64_t)stage << 56);
    rec->aux  = aux;
    rec->aux2 = aux2;
    /* Last store, released: makes the record observable as valid only once
     * sc/aux/aux2 are visible. */
    LATREC_STORE_RELEASE(&rec->t_ns, t_ns ? t_ns : latrec_now_ns());
    /* Take the next record's line for write while the caller runs. */
    LATREC_PREFETCH_W(&r->recs[(i + 1) & r->mask]);
}

/* Hot path: one clock read and four stores. Single writer per ring.
 *
 * The disabled-ring check is repeated here deliberately. latrec_now_ns() is an
 * argument, so it would be evaluated before the call even on a thread with no
 * ring, putting a clock read on a path whose whole point is to be free --
 * tests/bench_latrec.cpp holds that path to a 10 ns ceiling and catches it. */
static inline void latrec_stamp(latrec_t* r, uint64_t seq, uint8_t stage,
                                uint64_t aux, uint64_t aux2)
{
    if (LATREC_UNLIKELY(!r->enabled)) return;
    latrec_stamp_at(r, seq, stage, aux, aux2, latrec_now_ns());
}

/* Refresh rec_count and the closing clock pair without closing the ring. Call
 * periodically so a ring that is never closed still carries both. */
static inline void latrec_heartbeat(latrec_t* r)
{
    if (!r->enabled) return;
    latrec_hdr* h = (latrec_hdr*)r->map;
    h->rec_count  = r->idx;
    h->t1_mono_ns = latrec_now_ns();
    h->t1_real_ns = latrec_real_ns();
}

static inline void latrec_close(latrec_t* r)
{
    if (!r->enabled) return;
    latrec_heartbeat(r);
    msync(r->map, r->map_len, MS_SYNC);
    munmap(r->map, r->map_len);
    r->enabled = 0;
}

/* ---- per-thread rings (backed by latrec.c) -------------------------------- */

/* @see \ref latrec_guide "docs/latrec.md" for why this is a build-time
 * option, and for what a consumer of an installed libe3 has to match. */
#ifdef LIBE3_ENABLE_LATREC

/* Ring of the calling thread. Never NULL: it refers to a disabled ring until
 * the thread calls latrec_tls_open(), so a stamp before that is discarded. */
extern __thread latrec_t* latrec_tls;

/* Directory the per-thread rings are written to, for the whole process. Call
 * before the first latrec_tls_open*(); rings already open are unaffected.
 * NULL or empty restores LATREC_DEFAULT_DIR. This chooses *where*, never
 * *whether*: with LIBE3_ENABLE_LATREC on, the recorder records regardless.
 * Two processes that should not share rings (concurrent tests, one benchmark
 * per directory) each set their own. */
void latrec_set_output_dir(const char* dir);

/* Opens the calling thread's ring as <role>.<tid>, or <program>.<tid> when
 * role is NULL or empty; idempotent. Capacity comes from
 * LATREC_ENTRIES_LOG2_<ROLE> (role uppercased, non-alphanumerics as '_'),
 * else LATREC_ENTRIES_LOG2, else the compiled-in default. The open
 * pre-faults the whole mapping, which takes tens of ms; the stamp path does
 * not call it. */
latrec_t* latrec_tls_open_as(const char* role);
latrec_t* latrec_tls_open(void);

/* Next value of the process-wide sequence counter (starts at 1). */
uint64_t latrec_seq_next(void);

/* Stamp into the calling thread's ring, carrying a timestamp taken earlier.
 * See latrec_stamp_at for when that is the correct thing to do. */
static inline void latrec_tstamp_at(uint64_t seq, uint8_t stage, uint64_t aux,
                                    uint64_t aux2, uint64_t t_ns)
{
    latrec_stamp_at(latrec_tls, seq, stage, aux, aux2, t_ns);
}

/* Take a timestamp for a later latrec_tstamp_at(). Pairing these two, rather
 * than calling latrec_now_ns() directly, keeps the clock read out of a build
 * with the recorder compiled out: this is the point where the pair collapses
 * to nothing. */
static inline uint64_t latrec_tnow(void) { return latrec_now_ns(); }

/* Stamp into the calling thread's ring. */
static inline void latrec_tstamp(uint64_t seq, uint8_t stage, uint64_t aux, uint64_t aux2)
{
    latrec_stamp(latrec_tls, seq, stage, aux, aux2);
}

/* Publishes the seq of the message this thread is carrying, for code whose
 * signature cannot receive it: the connector is handed a byte buffer, not a
 * Pdu, libe3's emit entry points are handed neither, and an application's
 * indication or control handler is handed the message alone. Caller and callee
 * run on the same thread. latrec_ctx() reads back 0 from a thread with no ring.
 *
 * The value is sticky until overwritten, so a producer that wants its records
 * joined to libe3's outbound leg (via LATREC_EMIT_ENTER's aux) sets it once
 * per message, immediately before emitting. Readers must tolerate a stale or
 * zero value rather than treating it as authoritative.
 *
 * Both directions use it. Outbound, the producer sets it and libe3 reads it
 * into LATREC_EMIT_ENTER's aux. Inbound, libe3 sets it before invoking the
 * application handler, so the handler's own stages can key on the same seq as
 * the DELIVER_BEGIN/DELIVER_DONE pair around them; a dApp reads it with
 * latrec_ctx() on entry rather than being passed it. */
static inline void latrec_ctx_set(uint64_t seq)
{
    /* Threads without a ring share one disabled ring, so an unconditional
     * store here would be a write race between them. */
    latrec_t* r = latrec_tls;
    if (r->enabled) r->ctx_seq = seq;
}
static inline uint64_t latrec_ctx(void) { return latrec_tls->ctx_seq; }

#else /* !LIBE3_ENABLE_LATREC */

/* True no-op stubs, not just a disabled ring: every instrumented call site
 * collapses to nothing here, with no dependency on latrec.c (excluded from
 * the build in this configuration) and no linker symbol to resolve. */
static inline void latrec_set_output_dir(const char* dir) { (void)dir; }
static inline latrec_t* latrec_tls_open_as(const char* role) { (void)role; return NULL; }
static inline latrec_t* latrec_tls_open(void) { return NULL; }
static inline uint64_t latrec_seq_next(void) { return 0; }
static inline void latrec_tstamp(uint64_t seq, uint8_t stage, uint64_t aux, uint64_t aux2)
{
    (void)seq; (void)stage; (void)aux; (void)aux2;
}
static inline void latrec_tstamp_at(uint64_t seq, uint8_t stage, uint64_t aux,
                                    uint64_t aux2, uint64_t t_ns)
{
    (void)seq; (void)stage; (void)aux; (void)aux2; (void)t_ns;
}
/* No clock read at all here: the whole pair is compiled out. */
static inline uint64_t latrec_tnow(void) { return 0; }
static inline void latrec_ctx_set(uint64_t seq) { (void)seq; }
static inline uint64_t latrec_ctx(void) { return 0; }

#endif /* LIBE3_ENABLE_LATREC */

#ifdef __cplusplus
}
#endif

#endif /* LIBE3_LATREC_H */
