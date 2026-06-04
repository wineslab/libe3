/* SWIG interface for libe3 — minimal seam for Python consumers.
 *
 * Wraps the dApp-relevant public surface of libe3 so that spear-dApp's
 * src/e3interface/ layer (currently a pure-Python ZMQ + asn1tools
 * implementation) can be replaced with a thin Python shim around this
 * C++ library. SM-specific encoders stay in Python.
 *
 * SWIG sees a streamlined view of the API via swig/libe3_swig.hpp; the
 * C++ side links against the real libe3.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

%module libe3py

%{
#include "libe3/types.hpp"
#include "libe3/e3_agent.hpp"
#include "libe3/version.hpp"
%}

%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"
%include "exception.i"

namespace std {
    %template(BytesVec) vector<uint8_t>;
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
 * E3Config struct.  Skip anything that SWIG can't easily parse — the
 * variant-based Pdu, the std::function callbacks, and methods returning
 * std::optional. Python users get the minimal seam: construct a config,
 * instantiate the agent, drive its lifecycle, send/receive opaque bytes.
 */
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
 * returns). We instead include a streamlined SWIG view of the API.
 */
%include "libe3_swig.hpp"
