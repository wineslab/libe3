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
 * Every latrec_* call is a no-op unless LATREC_DIR is set.
 *
 * File layout: a 4 KiB latrec_hdr followed by `entries` 32-byte records. v1
 * field offsets are frozen and v2 fields appended inside the header pad, so a
 * v1 reader parses a v2 file. Readers detect a byte-swapped LATREC_MAGIC.
 * 64-bit hosts only: a 32-bit host can tear the t_ns store.
 *
 * Installed as <libe3/latrec.h>; the OAI and flexric instrumentation include it
 * from there. Components that do not link libe3 mirror the layout instead --
 * spear-dApp/src/e3interface/latrec.py and the C++ dApps'
 * dapps/common/e3_manager/dapp_latrec.h -- and the stage catalog below carries
 * their ids too, so it stays the one place a new id is checked for collisions.
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

/* ---- stage catalog (stable IDs; never renumber, only append) -------------- */
enum {
    /* OAI producer — PHY thread (e3_ran_buffers.c) */
    LATREC_P0_PUSH_ENTRY   = 0x10, /* aux = sfn<<16|slot, aux2 = the enclosing RX
                                      slot's key, i.e. the seq of its P6..P8 */
    LATREC_P1_COPY_DONE    = 0x11, /* aux = bytes copied */
    LATREC_P2_MASK_DONE    = 0x12, /* aux = per-symbol UL validity mask */
    LATREC_P3_INFO_BUILT   = 0x13, /* aux = timestamp_ns (the in-band origin stamp) */
    LATREC_P4_CHAN_LOCKED  = 0x14, /* publish mutex taken; P3->P4 is time spent
                                      waiting on the consumer, which holds the
                                      same mutex while copying the snapshot out */
    LATREC_P5_PUBLISHED    = 0x15, /* aux = buffer_idx<<8|write_idx (ring position) */
    /* OAI PHY RX slot (executables/nr-gnb.c) — seq = RX slot counter */
    LATREC_P6_RX_ENTRY     = 0x16, /* rx_func entered; aux = sfn<<16|slot */
    LATREC_P7_UESPEC_DONE  = 0x17, /* PHY UL processing returned */
    LATREC_P8_UL_IND_DONE  = 0x18, /* UL indication handed to MAC and returned */
    /* Back on the producer's key: the one-shot bring-up the first push runs
     * (shm region create, and the FP16 table where the host has no F16C). Tens
     * of ms on the PHY thread, so P0->P9 is a slot-deadline event; it appears
     * once per run. aux2 = 1 ready, 0 failed (no publish follows). */
    LATREC_P9_RING_READY   = 0x19,

    /* OAI L1-KPM SM worker — emit thread (l1_kpm_sm.c) */
    LATREC_W0_WAKE         = 0x20, /* worker woken with a valid snapshot */
    LATREC_W1_SLOT_SELECT  = 0x21, /* snapshot taken; publishes the worker missed
                                      show as rows with P stages and no W stages */
    LATREC_W2_META_BUILT   = 0x22, /* aux = subscribers fetched */
    LATREC_W3_ENCODE_DONE  = 0x23, /* aux = encoded bytes */
    LATREC_W4_SENT_TO_E3   = 0x24, /* handoff into libe3; LE0 follows and carries
                                      this key in its aux, which is the join;
                                      aux = emits sent, aux2 = emits skipped */
    LATREC_W5_WAIT_ENTER   = 0x25, /* worker back at the wait, carrying the publish
                                      it just handled: W4->W5 is the emit tail
                                      (subscriber array freed, loop bookkeeping),
                                      W5 -> the next row's W0 is time blocked, and
                                      W4 -> the next row's W0 minus that is how
                                      long the worker was busy elsewhere. Stamped
                                      once per publish, so a periodic-mode wait
                                      that times out and loops does not restamp it */
    LATREC_W6_SKIPPED      = 0x26, /* publish produced no indication; aux2 = reason
                                      (LATREC_SKIP_*). Separates the three ways a
                                      row ends after W1. A publish still pending
                                      when the worker stops gets none: the reason
                                      is only known at the next fetch, which never
                                      comes. One row per teardown, bounded by
                                      T1_SM_STOP */

    /* libe3 indication API entry — the producer's thread crossing into libe3,
     * ahead of L0. seq = Pdu::enqueue_seq, allocated here so this record and
     * L0..L3 share one key; aux = the producer's own trace seq (whatever it last
     * passed to latrec_ctx_set(), 0 if none), which is what joins the producer's
     * stages to the outbound leg; aux2 = RAN function id. LE0->L0 is the Pdu
     * construction; the producer's own emit wrapper precedes LE0.
     * Numbered below 0x30 because the libe3 block is full; it grows downward. */
    LATREC_LE0_EMIT_ENTER  = 0x2F,

    /* libe3 outbound — LE0 (above) and the enqueue run on the caller's
     * thread, the rest on the publisher thread (e3_interface).
     * seq = Pdu::enqueue_seq. */
    LATREC_L0_ENQUEUE      = 0x30, /* aux2 = PduType */
    LATREC_L1_DEQUEUE      = 0x31, /* aux2 = PduType */
    LATREC_L2_ENCODE_DONE  = 0x32, /* aux = wire bytes */
    LATREC_L3_SEND_DONE    = 0x33, /* connector send() returned; aux = message_id.
                                      NOT the wire: a ZMQ PUB send only copies
                                      into ZMQ's queue and the socket write
                                      happens later on ZMQ's own io thread */

    /* libe3 inbound — receive path of either role. seq is allocated at L4 from
     * the process-wide counter, so it never collides with an outbound seq. */
    LATREC_L4_RECV         = 0x34, /* aux = wire bytes read */
    LATREC_L5_DECODED      = 0x35, /* aux = message_id, aux2 = PduType */
    LATREC_L6_DISPATCHED   = 0x36, /* handler returned; aux2 = PduType */
    LATREC_L7_REPORT_QUEUED = 0x37, /* dApp report handed to the report worker */
    LATREC_L8_REPORT_DONE  = 0x38, /* report worker finished the handler */

    LATREC_L9_DROP         = 0x39, /* aux2 = drop reason */

    /* libe3 setup handshake (RAN side). seq = request message_id. */
    LATREC_LS0_SETUP_RECV  = 0x3A, /* SetupRequest reached the setup thread */
    LATREC_LS1_SETUP_SENT  = 0x3B, /* SetupResponse written; aux2 = 1 accepted, 0
                                      rejected. A setup that never decoded into a
                                      request is rejected without an LS0, so this
                                      can appear with no LS0 to pair with        */

    /* libe3 dApp session ring — the batching seam the Python binding drains
     * (swig/e3_dapp_session.cpp). seq is allocated where the event is queued. */
    LATREC_LQ0_QUEUED      = 0x3C, /* event pushed to the session ring; aux = kind */
    LATREC_LQ1_POLLED      = 0x3D, /* handed to the binding; aux = position in
                                      batch, aux2 = batch size */

    /* libe3 connector — transport call boundaries (src/connector). seq is the
     * seq of the PDU being carried, so these nest inside L2..L3 / L4. */
    LATREC_LC0_SEND_ENTER  = 0x3E, /* connector send() entered; aux = bytes */
    LATREC_LC1_SEND_RETURNED = 0x3F, /* transport call returned; aux = bytes,
                                        aux2 = peers served (1 on the dApp's single
                                        socket, 0 for ZMQ, which does not expose a
                                        count). Carries the same caveat as
                                        L3_SEND_DONE: for ZMQ PUB this is a handoff,
                                        not a wire write */

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
    /* Numeric order is not chronological in this block: D10 closes the handler,
     * and D11/D12 sit inside D4->D5 and D7->D8. */
    LATREC_D10_HANDLER_OUT = 0x4A, /* handler returned */
    LATREC_D11_L2SCAN_DONE = 0x4B, /* /e3_l2_sensing ring scan returned;
                                      aux = 1 the slot was found */
    LATREC_D12_ENCODE_DONE = 0x4C, /* report encoded; aux = bytes, aux2 = PRBs */
    LATREC_D13_SENSE_IN    = 0x4D, /* RF=1 sensing handler entered: own leg,
                                      own (sfn, slot) space */
    LATREC_D14_SENSE_OUT   = 0x4E, /* sensing handler returned */
    LATREC_D15_CONVERT_DONE= 0x4F, /* FP16->FP32 parallel region joined, ahead
                                      of the serial mask + status tail */
    LATREC_D16_DETECT_PRE  = 0x56, /* accumulate + sqrt done, before the
                                      detector's own parallel region */
    /* GPU dApp only. Its pipeline is several slots deep, so the work a handler
     * call retires belongs to an older slot: D17 is keyed by the slot being
     * submitted, D18/D19 by the slot being retired. Separate from D6/D7/V1,
     * which bracket synchronous work on the CPU dApp. */
    LATREC_D17_GPU_SUBMIT  = 0x5A, /* slot handed to the GPU pipeline */
    LATREC_D18_GPU_RETIRED = 0x5B, /* the pipeline's completion wait returned */
    LATREC_D19_VIZ_SENT    = 0x5C, /* viz frame published on the ingest thread */

    /* dApp — publisher thread */
    LATREC_V0_SNAP_TAKEN   = 0x50, /* aux = frames skipped since last consume */
    LATREC_V1_QUANTIZED    = 0x51,
    LATREC_V2_PUBLISHED    = 0x52,
    LATREC_V3_WOKE         = 0x53, /* woken, before the snapshot copy: V3->V0 is
                                      the copy the ingest thread try_locks
                                      against */

    /* dApp E3 session lifecycle. seq = the request's message id. */
    LATREC_E0_SUB_SENT     = 0x54, /* subscriptionRequest published */
    LATREC_E1_SUB_CONFIRMED= 0x55, /* subscriptionResponse confirmed it;
                                      aux = granted subscription id */
    LATREC_E2_SETUP_SENT   = 0x57, /* setupRequest written to the REQ socket */
    LATREC_E3_SETUP_RESP   = 0x58, /* setupResponse returned: the REQ/REP round
                                      trip; aux = bytes */
    LATREC_E4_SETUP_READY  = 0x59, /* sockets rebuilt and the subscription
                                      thread spawned */

    /* slow-lane context records (1 Hz, own ring). RESERVED: not stamped yet. */
    LATREC_C0_CONTEXT      = 0x60, /* aux = involuntary ctx switches, aux2 = cur freq */

    /* OAI MAC slot pipeline (gNB_scheduler.c) — seq = scheduler tick counter,
     * all stamped on the L1 TX thread inside gNB_dlsch_ulsch_scheduler. */
    LATREC_M0_SLOT_ENTRY   = 0x61, /* scheduler entered; aux = sfn<<16|slot */
    LATREC_M1_LOCK_HELD    = 0x62, /* sched_lock acquired */
    LATREC_M2_BLOCK_APPLIED= 0x63, /* dApp PRB block OR'd into the vrb maps */
    LATREC_M3_UL_DONE      = 0x64, /* UL scheduling returned */
    LATREC_M4_DL_DONE      = 0x65, /* DL scheduling returned */
    LATREC_M5_PUCCH_DONE   = 0x66, /* SR reporting + PUCCH scheduling returned */
    LATREC_M6_SENSING_DONE = 0x67, /* sensing slot restore + scan + publish returned */
    LATREC_M7_SLOT_EXIT    = 0x68, /* scheduler done; aux = UL_tti_req PDUs */

    /* OAI service-model lifecycle (e3_sm_worker.c, e3_agent.c, ran_func_dapp.c).
     * T0/T1 are point events keyed on a process-wide seq; T2..T4 share one seq
     * carried across the handler by latrec_ctx_set(). aux = RAN function id. */
    LATREC_T0_SM_START     = 0x69, /* first subscription: worker + shm ring up */
    LATREC_T1_SM_STOP      = 0x6A, /* last unsubscribe/release: worker joined;
                                      aux2 = batches emitted */
    LATREC_T2_STATUS_IN    = 0x6B, /* dApp connect/disconnect handler entered */
    LATREC_T3_PERIOD_SET   = 0x6C, /* both SMs' emission cadences recomputed */
    LATREC_T4_RIC_UPDATED  = 0x6D, /* RIC Service Update returned (blocking SCTP);
                                      aux = 1 sent, 0 skipped (E2 setup pending) */
    LATREC_T5_STATUS_DONE  = 0x6E, /* handler returned */

    /* OAI sensing telemetry, RF=1 (MAC publish -> Spectrum SM worker) —
     * seq = sensing publish sequence. */
    LATREC_S0_RECORD_IN    = 0x70, /* MAC entered the range publish; S0->S1 is the
                                      seqlock write plus the condvar broadcast */
    LATREC_S1_PUBLISHED    = 0x71, /* ranges published; aux = sfn<<16|slot,
                                      aux2 = ranges */
    LATREC_S2_WORKER_WAKE  = 0x72, /* worker woken by the publish */
    LATREC_S3_RANGES_READ  = 0x73, /* ranges read back; aux = ranges */
    LATREC_S4_SHM_WRITTEN  = 0x74, /* written to /e3_l2_sensing; aux = slot index */
    LATREC_S5_ENCODE_DONE  = 0x75, /* aux = encoded bytes */
    LATREC_S6_SENT_TO_E3   = 0x76, /* aux = emits sent, aux2 = emits skipped */
    LATREC_S7_WAIT_ENTER   = 0x77, /* worker back at the wait, carrying the publish
                                      it just handled; the S mirror of W5 */
    LATREC_S8_SKIPPED      = 0x78, /* publish produced no indication; aux2 = reason
                                      (LATREC_SKIP_*); the S mirror of W6 */
    /* 0x79 is retired and not reused. */

    /* OAI dApp control -> air (Spectrum SM -> MAC) — seq = E3 message_id.
     * B0..B5 run on the libe3 inbound thread, B6 on the L1 TX thread. */
    LATREC_B0_CTRL_RECV    = 0x80, /* SM control handler entered; aux = control_id,
                                      aux2 = payload bytes */
    LATREC_B1_DECODED      = 0x81, /* payload decoded; aux = PRBs or slots */
    LATREC_B2_PREPARED     = 0x82, /* prbBlock only: mask array built and the PRB
                                      list formatted for the operator log */
    LATREC_B3_UL_INSTALLED = 0x83, /* prbBlock only: UL mask in. This is the leg
                                      that takes the MAC's sched_lock for the
                                      collision scan, so it couples the control
                                      thread to the slot-deadline thread */
    LATREC_B4_INSTALLED    = 0x84, /* mask/policy installed in the MAC; aux = ok */
    LATREC_B5_ACKED        = 0x85, /* MessageAck emitted; aux = response code,
                                      aux2 = reject reason, 0 when accepted */
    LATREC_B6_LIVE_ON_AIR  = 0x86, /* first scheduler tick that stamped a UL mask
                                      install OR clear into the vrb maps.
                                      aux = sfn<<16|slot, aux2 = installs this
                                      tick put on air (>1 means the ones behind
                                      the keyed install were coalesced into it).
                                      Stamped on the MAC thread but keyed on the
                                      control that installed the mask, so it
                                      joins B0..B5 directly. The SM clears on
                                      stop with no control behind it: that
                                      record carries seq 0 */

    /* xApp <-> dApp control/report round-trip. Each side keys on a counter of
     * its own -- the relay is handed no id it could share -- so the legs are
     * paired by time, not by seq. */
    /* RAN E3 relay  (OAI: ran_func_dapp.c) — seq = process-wide counter */
    LATREC_G0_CTRL_RECV    = 0x90, /* xApp control reached ran_func_dapp (from E2)     */
    LATREC_G1_CTRL_E3_SENT = 0x91, /* control forwarded to dApp over E3; aux = bytes,
                                      aux2 = dApp id                                    */
    LATREC_G8_REP_RECV     = 0x98, /* dApp report received over E3; aux = bytes,
                                      aux2 = dApp id                                    */
    LATREC_G9_REP_TO_E2    = 0x99, /* report forwarded up toward E2/xApp; aux = RIC
                                      subscriptions it went to                          */
    /* dApp control handling — seq = request_id (== E3 message_id). K1/K2 are
     * stamped by the Python dApp, K5 by the C++ dApps, on the same handler. */
    LATREC_K0_CTRL_RECV    = 0xA0, /* control received from libe3 (handler entry)       */
    LATREC_K1_CTRL_APPLIED = 0xA1, /* control applied (callbacks done)                  */
    LATREC_K2_CTRL_ACKED   = 0xA2, /* MessageAck emitted back to the RAN                */
    LATREC_K5_CTRL_DECODED = 0xA5, /* control payload decoded; aux = PRBs               */
    /* dApp -> RAN control, on the dApp's own outbound path. seq = the E3
     * message id it sends, which is the key the RAN's B stages use. */
    LATREC_K3_CTRL_SENT    = 0xA3, /* control published to the RAN; aux = bytes         */
    LATREC_K4_ACK_RECV     = 0xA4, /* the RAN's MessageAck arrived                      */
    LATREC_K6_PUBLISHED    = 0xA6, /* PUB enqueue returned; aux = bytes, aux2 = 0 when
                                      ZMQ dropped it (HWM or no peer). Keyed by the
                                      published message's E3 id where it has one,
                                      else 0                                            */
    /* dApp report generation  (spear-dApp) — seq = process-wide report counter */
    LATREC_R0_REP_BUILT    = 0xA8, /* report built and queued; aux = bytes              */
    LATREC_R1_REP_SENT     = 0xA9, /* handed to libe3 for send; aux = bytes,
                                      aux2 = return code. R0->R1 is the outbound
                                      queue wait                                        */
    /* flexric E2 legs. Each process keys on a counter of its own, so the xApp
     * and agent legs pair with each other by time, not by seq. */
    /* xApp control out — seq = the xApp's local control counter */
    LATREC_X0_CTRL_REQ     = 0xB0, /* control requested; aux = E2 nodes targeted        */
    LATREC_X1_CTRL_SM_ENC  = 0xB1, /* E2SM control payload encoded; aux = bytes         */
    LATREC_X2_CTRL_E2AP_ENC= 0xB2, /* E2AP PDU encoded; aux = wire bytes                */
    LATREC_X3_CTRL_SCTP_SENT = 0xB3, /* SCTP send returned; aux = wire bytes            */
    /* E2 agent, control in (runs in the RAN process) — seq = agent-local counter.
     * G0/G1 nest inside X6..X7. */
    LATREC_X4_AG_E2AP_DEC  = 0xB4, /* E2AP PDU decoded; aux = bytes, aux2 = PDU type    */
    LATREC_X5_AG_CTRL_IN   = 0xB5, /* agent control handler entered                     */
    LATREC_X6_AG_SM_DEC    = 0xB6, /* E2SM header+message decoded; aux = bytes          */
    LATREC_X7_AG_CTRL_OUT  = 0xB7, /* handler returned; aux = RAN function id           */
    /* E2 agent, indication out — seq = agent-local counter */
    LATREC_X8_AG_IND_ENC   = 0xB8, /* indication payload built; aux = bytes             */
    LATREC_X9_AG_IND_E2AP  = 0xB9, /* E2AP PDU encoded; aux = wire bytes                */
    LATREC_XA_AG_IND_SENT  = 0xBA, /* SCTP send returned; aux = wire bytes              */
    /* xApp indication in — seq = the xApp's local inbound counter */
    LATREC_XB_IND_RECV     = 0xBB, /* E2AP bytes received; aux = bytes                  */
    LATREC_XC_IND_DEC      = 0xBC, /* E2AP PDU decoded; aux = PDU type                  */
    LATREC_XD_IND_DISPATCH = 0xBD, /* handed to the SM callback; aux = PDU type         */
    LATREC_XE_REP_RECV     = 0xBE, /* dApp report surfaced to the xApp app layer. The SM
                                      callback runs on the dispatcher thread, not the
                                      receive loop, so this carries its own seq and
                                      pairs with XD by time                             */

    /* ---- reservations below: identifiers only, not yet stamped by libe3 --- */

    /* OCUDU: jbpf-based CU/DU IQ source and its E3 controller (ocudu-e3 jbpf
     * hook -> E3Controller IQ pipeline). Covers the same span OAI's P+W
     * blocks cover (IQ capture, pipeline processing, SM encode), collapsed
     * into one block. Reserved before the OCUDU work starts; not stamped
     * here. 0xC5-0xC7 left as headroom for the control-direction leg once
     * OCUDU's control path is wired (not yet wired as of this reservation). */
    LATREC_OC0_CAPTURE_ENTRY = 0xC0, /* jbpf capture_uplink_slot hook entered */
    LATREC_OC1_CAPTURE_DONE  = 0xC1, /* capture published to shared memory */
    LATREC_OC2_PIPELINE_WAKE = 0xC2, /* E3Controller IQ pipeline woken */
    LATREC_OC3_PIPELINE_DONE = 0xC3, /* E3Controller IQ pipeline processing returned */
    LATREC_OC4_ENCODE_DONE   = 0xC4, /* layer-1 SM encode done; ready for libe3's LE0 */

    /* cuBB: L1 data-lake IQ source (cuBB data lake -> the Aerial dApp's E3
     * manager), the "aerial" configuration's A1-A3 owner -- distinct from the
     * D/V/E/K/R blocks the Aerial C++ dApp already stamps downstream of
     * libe3. Reserved; not stamped here. 0xD5-0xD7 headroom. */
    LATREC_CB0_LAKE_READ_ENTRY = 0xD0, /* cuBB data-lake IQ read entered */
    LATREC_CB1_LAKE_READ_DONE  = 0xD1, /* IQ block read returned */
    LATREC_CB2_AGENT_WAKE      = 0xD2, /* cuBB E3 agent woken with a new IQ block */
    LATREC_CB3_AGENT_DONE      = 0xD3, /* cuBB E3 agent processing returned */
    LATREC_CB4_ENCODE_DONE     = 0xD4, /* E3 manager E3SM encode done; ready for
                                           libe3's LE0; anchor for the
                                           backend=native vs backend=libe3
                                           A/B comparison */

    /* xDevSM: Python xApp/service-model framework. Closes Path B's "no stage
     * block for the Python framework" gap (E2SM decode / xApp processing /
     * E2SM encode, between the flexric binding and the E2AP encode on the
     * xApp side). Reserved; not stamped here. 0xE5-0xE7 headroom. Prefix is
     * "PY" (Python), not "XD": LATREC_XD_IND_DISPATCH above already names an
     * unrelated flexric identifier and re-using "XD" as a block prefix here
     * would be confusable with it. */
    LATREC_PY0_IND_DISPATCH = 0xE0, /* E2 indication handed from the flexric
                                        binding into the Python dispatcher */
    LATREC_PY1_SM_DECODED   = 0xE1, /* E2SM decode done */
    LATREC_PY2_XAPP_IN      = 0xE2, /* xApp callback entered */
    LATREC_PY3_XAPP_OUT     = 0xE3, /* xApp callback returned with a decision */
    LATREC_PY4_SM_ENCODED   = 0xE4  /* E2SM control payload encoded; handoff to
                                        the flexric xApp-side E2AP encode (X0) */

    /* 0xC5-0xC7, 0xD5-0xD7, 0xE5-0xE7, 0xE8-0xFF are free. */
};

/* D5 admission outcomes (aux2) */
enum {
    LATREC_OUT_PROCESSED = 0,
    LATREC_OUT_SHED_AGE  = 1,
    LATREC_OUT_SHED_LAG  = 2,
    LATREC_OUT_NO_RANGE  = 3
};

/* LATREC_W6_SKIPPED / LATREC_S8_SKIPPED reasons (aux2). The telemetry SMs drop a
 * fetched publish for reasons libe3 never sees, so they are numbered separately
 * from LATREC_DROP_*. The last two are reachable only on the sensing path. */
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

/* LATREC_L9_DROP reasons (aux2) */
enum {
    LATREC_DROP_QUEUE_PUSH   = 1,
    LATREC_DROP_ENCODE       = 2,
    LATREC_DROP_SEND         = 3,
    LATREC_DROP_DECODE       = 4,
    LATREC_DROP_REPORT_QUEUE = 5,
    LATREC_DROP_NO_HANDLER   = 6,
    LATREC_DROP_SESSION_QUEUE = 7
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

/* Open a ring, or return 0 when LATREC_DIR is unset. name ("process.thread")
 * becomes <LATREC_DIR>/<name>.latrec and must be unique per writer thread
 * across every process sharing LATREC_DIR: the file is opened O_TRUNC, so a
 * collision discards the other writer's ring. entries_log2 sets the capacity
 * (22 = 4 M records = 128 MiB). Returns 1 when tracing is on, -1 on error. */
static inline int latrec_open(latrec_t* r, const char* name, unsigned entries_log2)
{
    memset(r, 0, sizeof(*r));
    const char* dir = getenv("LATREC_DIR");
    if (!dir || !*dir) return 0;                     /* tracing disabled */

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

/* Hot path: one clock read and four stores. Single writer per ring. */
static inline void latrec_stamp(latrec_t* r, uint64_t seq, uint8_t stage,
                                uint64_t aux, uint64_t aux2)
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
    LATREC_STORE_RELEASE(&rec->t_ns, latrec_now_ns());
    /* Take the next record's line for write while the caller runs. */
    LATREC_PREFETCH_W(&r->recs[(i + 1) & r->mask]);
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

/* Ring of the calling thread. Never NULL: it refers to a disabled ring until
 * the thread calls latrec_tls_open(), so a stamp before that is discarded. */
extern __thread latrec_t* latrec_tls;

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

/* Stamp into the calling thread's ring. */
static inline void latrec_tstamp(uint64_t seq, uint8_t stage, uint64_t aux, uint64_t aux2)
{
    latrec_stamp(latrec_tls, seq, stage, aux, aux2);
}

/* Publishes the seq of the message this thread is carrying, for code whose
 * signature cannot receive it: the connector is handed a byte buffer, not a
 * Pdu, and libe3's emit entry points are handed neither. Caller and callee run
 * on the same thread. latrec_ctx() reads back 0 while tracing is off.
 *
 * The value is sticky until overwritten, so a producer that wants its records
 * joined to libe3's outbound leg (via LATREC_LE0_EMIT_ENTER's aux) sets it once
 * per message, immediately before emitting. Readers must tolerate a stale or
 * zero value rather than treating it as authoritative. */
static inline void latrec_ctx_set(uint64_t seq)
{
    /* Threads without a ring share one disabled ring, so an unconditional
     * store here would be a write race between them. */
    latrec_t* r = latrec_tls;
    if (r->enabled) r->ctx_seq = seq;
}
static inline uint64_t latrec_ctx(void) { return latrec_tls->ctx_seq; }

#ifdef __cplusplus
}
#endif

#endif /* LIBE3_LATREC_H */
