#!/usr/bin/env python3
"""SWIG smoke test for libe3.

Validates that the SWIG-generated extension module imports, the E3Config
struct is constructible, the E3Role enum is exposed, and that flipping
role to DAPP and instantiating an E3Agent works at the Python level.

This is a minimal acceptance test for PR-3: it confirms the binding
compiles and exports the surface that a Python dApp will consume. The full
indication-handler bridge (Python callable -> C++ std::function) is a
separate work item in the Python-consumer adoption follow-up.

SPDX-License-Identifier: Apache-2.0
"""

import sys


def main() -> int:
    try:
        import libe3py
    except ImportError as e:
        print(f"FAIL: could not import libe3py: {e}", file=sys.stderr)
        return 1

    # E3Role enum is exposed
    if not hasattr(libe3py, "E3Role_RAN"):
        print("FAIL: libe3py.E3Role_RAN not exposed", file=sys.stderr)
        return 1
    if not hasattr(libe3py, "E3Role_DAPP"):
        print("FAIL: libe3py.E3Role_DAPP not exposed", file=sys.stderr)
        return 1

    # E3Config is constructible and has the role field
    cfg = libe3py.E3Config()
    if cfg.role != libe3py.E3Role_RAN:
        print(f"FAIL: default cfg.role = {cfg.role}, expected E3Role_RAN", file=sys.stderr)
        return 1

    cfg.role = libe3py.E3Role_DAPP
    if cfg.role != libe3py.E3Role_DAPP:
        print(f"FAIL: cfg.role assignment lost the change", file=sys.stderr)
        return 1

    cfg.dapp_name = "SmokeDApp"
    cfg.dapp_version = "0.1.0"
    cfg.vendor = "WinesLab"
    cfg.log_level = 0

    # E3Agent is constructible (we do NOT start it — no transport set up).
    agent = libe3py.E3Agent(cfg)
    if agent is None:
        print("FAIL: E3Agent(cfg) returned None", file=sys.stderr)
        return 1

    # State should be Uninitialized until init() is called.
    state = agent.state()
    if state != libe3py.AgentState_UNINITIALIZED:
        print(f"FAIL: initial state = {state}, expected UNINITIALIZED", file=sys.stderr)
        return 1

    # Verify we exposed the dApp verbs at the API level (they may return
    # error codes when called pre-init, which is fine for this smoke test).
    # Note: `subscribe` is deliberately not in the minimal SWIG seam because
    # it takes std::optional<> parameters that SWIG 4.x can't typemap; the
    # Python-consumer adoption layer will provide a Python helper that calls
    # the underlying interface via a wrapper.
    for verb in ("unsubscribe", "send_control", "send_report", "release"):
        if not hasattr(agent, verb):
            print(f"FAIL: E3Agent lacks dApp verb '{verb}'", file=sys.stderr)
            return 1

    # ------------------------------------------------------------------
    # DAppSession: the full batched dApp seam a Python dApp consumes.
    # ------------------------------------------------------------------
    for k in ("E3_EVENT_NONE", "E3_EVENT_INDICATION", "E3_EVENT_XAPP_CONTROL",
              "E3_EVENT_SUBSCRIPTION_RESPONSE", "E3_EVENT_SETUP_RESPONSE",
              "E3_EVENT_MESSAGE_ACK"):
        if not hasattr(libe3py, k):
            print(f"FAIL: libe3py.{k} event kind not exposed", file=sys.stderr)
            return 1

    session = libe3py.DAppSession(cfg)
    if session is None:
        print("FAIL: DAppSession(cfg) returned None", file=sys.stderr)
        return 1

    for verb in ("start", "wait_for_setup", "subscribe", "unsubscribe",
                 "send_control", "send_report", "send_message_ack", "release",
                 "stop", "dapp_id", "ran_identifier", "poll_events",
                 "dropped_events", "setup_response_code",
                 "setup_ran_function_count", "setup_ran_function_id",
                 "setup_ran_function_telemetry", "setup_ran_function_control",
                 "setup_ran_function_data"):
        if not hasattr(session, verb):
            print(f"FAIL: DAppSession lacks verb '{verb}'", file=sys.stderr)
            return 1

    # Pre-setup accessors return sentinels, never raise.
    if session.dapp_id() != -1:
        print(f"FAIL: pre-setup dapp_id={session.dapp_id()}, expected -1", file=sys.stderr)
        return 1
    if session.setup_response_code() != -1:
        print("FAIL: pre-setup setup_response_code should be -1", file=sys.stderr)
        return 1
    if session.dropped_events() != 0:
        print("FAIL: fresh session should report 0 dropped events", file=sys.stderr)
        return 1

    # Bytes typemaps must accept a bytes-like argument (return code irrelevant
    # pre-setup; the point is the conversion, not the call). send_indication
    # takes const std::vector<uint8_t>& — the const-ref in-typemap regression.
    payload = b"\x00\x01\x02"
    try:
        agent.send_indication(1, 1, payload)          # const-ref in-typemap
        session.send_control(1, 1, payload)           # by-value in-typemap
        session.send_report(1, payload)
    except TypeError as e:
        print(f"FAIL: bytes typemap rejected a bytes argument: {e}", file=sys.stderr)
        return 1
    # Out-typemap: setup accessors return native bytes (empty pre-setup).
    if not isinstance(session.setup_ran_function_data(0), bytes):
        print("FAIL: setup_ran_function_data did not return bytes", file=sys.stderr)
        return 1

    # GIL release: while poll_events blocks, a second Python thread must keep
    # running. A threads="1" regression would freeze the counter.
    import threading
    import time
    counter = {"n": 0}
    stop = threading.Event()

    def spin():
        while not stop.is_set():
            counter["n"] += 1
            time.sleep(0.001)

    t = threading.Thread(target=spin)
    t.start()
    before = counter["n"]
    session.poll_events(64, 300)   # blocks ~300 ms with no peer
    advanced = counter["n"] - before
    stop.set()
    t.join(timeout=1)
    if advanced < 10:
        print(f"FAIL: counter advanced only {advanced} during a 300ms poll_events "
              f"block — GIL not released", file=sys.stderr)
        return 1

    print("OK: libe3py imports, E3Config/E3Role/E3Agent constructible, "
          "dApp surface present, bytes typemaps + GIL release verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
