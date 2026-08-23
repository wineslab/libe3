/* SWIG interface for libe3 — Python seam for Python consumers.
 *
 * Exposes two layers to Python:
 *   1. The minimal E3Agent view (swig/libe3_swig.hpp) — construct/config/start
 *      an agent and push opaque bytes (kept for backwards compatibility and the
 *      swig smoke test).
 *   2. The dApp session (swig/e3_dapp_session.hpp) — the full, batched,
 *      low-latency dApp seam (DAppSession + E3Event) that a Python dApp
 *      consumes in place of a pure-Python ZMQ + asn1tools implementation.
 *      SM-specific encoders stay in Python.
 *
 * threads="1": SWIG releases the Python GIL around every wrapped call, so
 * libe3's C++ worker threads never stall on Python while a Python thread is
 * blocked in DAppSession::poll_events (or any other libe3 call). Safe because
 * no wrapped C++ path calls back into Python (the inbound path is a lock-free
 * ring drained by poll_events, not a Python callback).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

%module(threads="1") libe3py

%{
#include "libe3/latrec.h"
#include "libe3/types.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/version.hpp"
#include "e3_dapp_session.hpp"
%}

%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"
%include "exception.i"

/* Access non-primitive struct members by value so member typemaps (e.g. the
 * std::vector<uint8_t> -> bytes map below) apply to getters. */
%naturalvar;

/* Opaque E3SM byte payloads cross as native Python bytes (not a wrapped
 * vector) — this is the throughput-critical field, so avoid per-element
 * conversion. Applies to E3Event::payload, DAppSession::setup_ran_function_data,
 * and the send_* action/report arguments. */
/* Methods returning std::vector<uint8_t> (e.g. DAppSession::setup_ran_function_data
 * and E3Event::get_payload below) cross as native Python bytes. */
%typemap(out) std::vector<uint8_t> {
    $result = PyBytes_FromStringAndSize(
        $1.empty() ? "" : reinterpret_cast<const char*>($1.data()),
        static_cast<Py_ssize_t>($1.size()));
}
/* bytes-like ($input) -> std::vector<uint8_t> (tmp). Shared by the by-value and
 * const-ref in-typemaps below; the const-ref one is what makes send_indication
 * (const std::vector<uint8_t>&) accept Python bytes. */
%define %BYTESLIKE_TO_VEC(tmp)
    if (PyBytes_Check($input)) {
        char* buf = nullptr; Py_ssize_t len = 0;
        PyBytes_AsStringAndSize($input, &buf, &len);
        tmp.assign(reinterpret_cast<const uint8_t*>(buf),
                   reinterpret_cast<const uint8_t*>(buf) + len);
    } else {
        Py_buffer view;
        if (PyObject_GetBuffer($input, &view, PyBUF_SIMPLE) == 0) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(view.buf);
            tmp.assign(p, p + view.len);
            PyBuffer_Release(&view);
        } else {
            SWIG_exception_fail(SWIG_TypeError,
                "expected a bytes-like object for E3SM payload");
        }
    }
%enddef
%typemap(in) std::vector<uint8_t> (std::vector<uint8_t> tmp) {
    %BYTESLIKE_TO_VEC(tmp)
    $1 = std::move(tmp);
}
%typemap(in) const std::vector<uint8_t>& (std::vector<uint8_t> tmp) {
    %BYTESLIKE_TO_VEC(tmp)
    $1 = &tmp;
}
%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING)
        std::vector<uint8_t>, const std::vector<uint8_t>& {
    $1 = (PyBytes_Check($input) || PyByteArray_Check($input) ||
          PyObject_CheckBuffer($input)) ? 1 : 0;
}

namespace std {
    %template(Uint32Vec) vector<uint32_t>;
}

%exception {
    try {
        $action
    } catch (const std::exception& e) {
        SWIG_exception(SWIG_RuntimeError, e.what());
    }
}

/* Types: bring in the enums (E3Role, E3LinkLayer, E3TransportLayer,
 * EncodingFormat, AgentState, ResponseCode, PduType, ErrorCode) and the
 * E3Config struct.  Skip anything SWIG can't parse cleanly — the
 * variant-based Pdu, std::function callbacks, and methods returning
 * std::optional. Python gets: build a config, drive a DAppSession, and
 * exchange opaque bytes; SM encode/decode stays in Python. */
%ignore libe3::PduChoice;
%ignore libe3::Pdu;
%ignore libe3::SetupRequest;
%ignore libe3::SetupResponse;
%ignore libe3::SubscriptionRequest;
%ignore libe3::SubscriptionDelete;
%ignore libe3::SubscriptionResponse;
%ignore libe3::IndicationMessage;
%ignore libe3::DAppControlAction;
%ignore libe3::DAppReport;
%ignore libe3::XAppControlAction;
%ignore libe3::ReleaseMessage;
%ignore libe3::MessageAck;
%ignore libe3::EncodedMessage;
%ignore libe3::Pdu::get_if;
%ignore libe3::RanFunctionDef;
%ignore libe3::RanFunctionDefinition;
%ignore libe3::DAppEntry;
%ignore libe3::SubscriptionEntry;
%ignore libe3::Timestamp;
%ignore libe3::ErrorCodeToString;

%include "libe3/error_codes.h"
%include "libe3/types.hpp"

/* The real e3_agent.hpp uses C++ features SWIG 4.1 can't parse cleanly
 * (brace-initialised default args, std::function handlers, std::optional
 * returns). We instead include a streamlined SWIG view of the minimal API. */
%include "libe3_swig.hpp"

/* E3Event::payload is a std::vector<uint8_t> member; SWIG member getters return
 * a pointer (naturalvar can't make a by-value copy of an untemplated vector), so
 * expose the payload through a by-value method instead — methods use the
 * std::vector<uint8_t> -> bytes `out` typemap above. Python reads ev.get_payload(). */
%ignore libe3::py::E3Event::payload;
%extend libe3::py::E3Event {
    std::vector<uint8_t> get_payload() const { return $self->payload; }
}

/* The full dApp seam: DAppSession + E3Event. The batch returned by
 * poll_events crosses as an E3EventVec. */
%include "e3_dapp_session.hpp"

namespace std {
    %template(E3EventVec) vector<libe3::py::E3Event>;
}

/* Ring placement for a Python process. Only present when the library was built
 * with -DLIBE3_ENABLE_LATREC=ON; a no-op stub otherwise, so a caller does not
 * have to branch on the build. It chooses *where* the rings go, never whether
 * they are written: an enabled build records either way. Call it before the
 * first DAppSession, since a thread's ring is opened once. */
%inline %{
static void latrec_set_output_dir_py(const char* dir) {
    latrec_set_output_dir(dir);
}
%}

/* Stage catalog: bring in the enums (the stage identifiers themselves, plus
 * the D5/W6.S8/L9 reason codes carried in aux2) so a Python caller stamps
 * against the same constants tools/latrec_reader.py mirrors, rather than a
 * third hand-copied table -- exactly the drift this repo already guards
 * against for the C header and the Python reader. Skip everything else in
 * the file: the ring/header structs, the low-level explicit-ring API, the
 * always-on latrec_stamp(_at) and the raw __thread pointer are either
 * meaningless across the Python/C++ boundary or superseded by the curated
 * wrappers below. */
%ignore latrec_rec;
%ignore latrec_hdr;
%ignore latrec_t;
%ignore latrec_now_ns;
%ignore latrec_real_ns;
%ignore latrec_refresh_cpu;
%ignore latrec_measure_clock_ns;
%ignore latrec_open;
%ignore latrec_open_in;
%ignore latrec_stamp;
%ignore latrec_stamp_at;
%ignore latrec_heartbeat;
%ignore latrec_close;
%ignore latrec_tls;
%ignore latrec_set_output_dir;
%ignore latrec_tls_open_as;
%ignore latrec_tls_open;
%ignore latrec_seq_next;
%ignore latrec_tstamp_at;
%ignore latrec_tstamp;
%ignore latrec_tnow;
%ignore latrec_ctx_set;
%ignore latrec_ctx;
/* Portability/format internals, not stage identifiers -- carry the LATREC_
 * prefix but describe the build or the ring file, not something a stamp
 * carries. */
%ignore LATREC_HAVE_GETCPU;
%ignore LATREC_CPUID_ON_STAMP;
%ignore LATREC_MAGIC;
%ignore LATREC_VERSION;
%ignore LATREC_HDR_LEN;
%ignore LATREC_CLOCK_SLOW_NS;
%ignore LATREC_DEFAULT_DIR;
%include "libe3/latrec.h"

/* The TLS convenience layer, for a Python caller stamping its own
 * application-level boxes through the same rings libe3 writes to -- rather
 * than a second, hand-rolled ring writer in Python. Every one of these is a
 * no-op (not a branch: latrec.h compiles them to true no-op inline stubs)
 * when the library was built without -DLIBE3_ENABLE_LATREC, so a Python
 * caller never has to know which build it is linked against.
 *
 * latrec_tls_open_as_py returns nothing: the ring pointer it opens is an
 * implementation detail SWIG has no reason to expose, and every other call
 * here reads the calling thread's ring implicitly, the same as the C API. */
%inline %{
static void latrec_tls_open_as_py(const char* role) {
    latrec_tls_open_as(role);
}

static uint64_t latrec_seq_next_py() {
    return latrec_seq_next();
}

static void latrec_tstamp_py(uint64_t seq, uint8_t stage, uint64_t aux, uint64_t aux2) {
    latrec_tstamp(seq, stage, aux, aux2);
}

/* Pairs with latrec_tnow_py() for the same reason libe3's own queue handoffs
 * do: a stage that records "handed off to something else" has to read the
 * clock before the handoff, not after, or a consumer that runs first can
 * stamp its own completion before the producer reaches this call. */
static void latrec_tstamp_at_py(uint64_t seq, uint8_t stage, uint64_t aux, uint64_t aux2,
                                uint64_t t_ns) {
    latrec_tstamp_at(seq, stage, aux, aux2, t_ns);
}

static uint64_t latrec_tnow_py() {
    return latrec_tnow();
}

static void latrec_ctx_set_py(uint64_t seq) {
    latrec_ctx_set(seq);
}

static uint64_t latrec_ctx_py() {
    return latrec_ctx();
}
%}
