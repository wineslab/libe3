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
