#!/usr/bin/env python3
"""SWIG smoke test for libe3.

Validates that the SWIG-generated extension module imports, the E3Config
struct is constructible, the E3Role enum is exposed, and that flipping
role to DAPP and instantiating an E3Agent works at the Python level.

This is a minimal acceptance test for PR-3: it confirms the binding
compiles and exports the surface that spear-dApp will consume. The full
indication-handler bridge (Python callable -> C++ std::function) is a
separate work item in the spear-dApp adoption follow-up.

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
    # spear-dApp adoption layer will provide a Python helper that calls
    # the underlying interface via a wrapper.
    for verb in ("unsubscribe", "send_control", "send_report", "release"):
        if not hasattr(agent, verb):
            print(f"FAIL: E3Agent lacks dApp verb '{verb}'", file=sys.stderr)
            return 1

    print("OK: libe3py imports, E3Config/E3Role/E3Agent constructible, dApp surface present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
