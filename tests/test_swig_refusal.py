#!/usr/bin/env python3
"""A refused subscription reaches a Python dApp with the RAN's cause and text.

A real RAN agent that serves no RAN function, and a DAppSession that subscribes to
one. The response is negative, and the event says why: `cause` is the libe3 Cause
value (5, unsupportedRanFunction) and `detail` the RAN's text. Before, the event
had only response_code, so a dApp could not tell a refused control from a refused
function and had nothing to decide on.

SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
SPDX-License-Identifier: Apache-2.0
"""

import os
import sys
import tempfile
import time


def _config(libe3py, role, encoding, endpoints):
    cfg = libe3py.E3Config()
    cfg.role = role
    cfg.link_layer = libe3py.E3LinkLayer_ZMQ
    cfg.transport_layer = libe3py.E3TransportLayer_IPC
    cfg.encoding = encoding
    cfg.setup_endpoint, cfg.subscriber_endpoint, cfg.publisher_endpoint = endpoints
    cfg.log_level = 0
    cfg.ran_identifier = "refusal-test"
    cfg.dapp_name = "RefusalDApp"
    cfg.dapp_version = "1.0.0"
    cfg.vendor = "test"
    cfg.reconnect_interval_ms = 0
    return cfg


def main() -> int:
    try:
        import libe3py
    except ImportError as e:
        print(f"FAIL: could not import libe3py: {e}", file=sys.stderr)
        return 1

    # A fresh event says nothing was refused.
    ev = libe3py.E3Event()
    if ev.cause != -1 or ev.detail != "":
        print(f"FAIL: fresh E3Event has cause={ev.cause!r} detail={ev.detail!r}", file=sys.stderr)
        return 1
    if libe3py.Cause_UNSUPPORTED_RAN_FUNCTION != 5 or libe3py.Cause_UNSUPPORTED_IDENTIFIER != 6:
        print("FAIL: Cause constants are not the E3AP values", file=sys.stderr)
        return 1

    os.makedirs("/tmp/dapps", exist_ok=True)
    tag = f"{os.getpid()}_{int(time.time())}"
    endpoints = tuple(f"ipc:///tmp/dapps/swig_refusal_{tag}_{n}" for n in ("setup", "sub", "pub"))

    ran = dapp = None
    for encoding in (libe3py.EncodingFormat_JSON, libe3py.EncodingFormat_ASN1):
        ran = libe3py.E3Agent(_config(libe3py, libe3py.E3Role_RAN, encoding, endpoints))
        if ran.start() == 0:
            break
        ran = None
    if ran is None:
        print("FAIL: no encoding the RAN agent could start with", file=sys.stderr)
        return 1
    try:
        time.sleep(0.2)
        dapp = libe3py.DAppSession(_config(libe3py, libe3py.E3Role_DAPP, encoding, endpoints))
        if dapp.start() != 0 or dapp.wait_for_setup(5000) != 0:
            print("FAIL: the dApp could not set up with the RAN", file=sys.stderr)
            return 1

        request_id = dapp.subscribe(77, libe3py.Uint32Vec([1]), libe3py.Uint32Vec([]), -1, -1)
        if request_id <= 0:
            print(f"FAIL: subscribe returned {request_id}", file=sys.stderr)
            return 1

        response = None
        deadline = time.monotonic() + 5
        while response is None and time.monotonic() < deadline:
            for e in dapp.poll_events(16, 200):
                if e.kind == libe3py.E3_EVENT_SUBSCRIPTION_RESPONSE and e.request_id == request_id:
                    response = e
        if response is None:
            print("FAIL: no subscription response", file=sys.stderr)
            return 1
        if response.response_code != 1:
            print(f"FAIL: response_code={response.response_code}, expected 1 (negative)",
                  file=sys.stderr)
            return 1
        if response.cause != libe3py.Cause_UNSUPPORTED_RAN_FUNCTION:
            print(f"FAIL: cause={response.cause}, expected "
                  f"{libe3py.Cause_UNSUPPORTED_RAN_FUNCTION}", file=sys.stderr)
            return 1
        if not response.detail:
            print("FAIL: a refusal by libe3's RAN carries a detail", file=sys.stderr)
            return 1
        print(f"PASS: refused with cause {response.cause}: {response.detail}")
        return 0
    finally:
        if dapp is not None:
            dapp.stop()
        ran.stop()


if __name__ == "__main__":
    sys.exit(main())
