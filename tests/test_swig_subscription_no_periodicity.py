#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

"""A Python dApp gets the subscription response of a Service Model that keeps no interval.

The reference Service Model of example_simple_agent does not say how often it emits, so the
RAN's SubscriptionResponse carries no periodicity. The dApp asks for 5000 microseconds. What
the Python binding must do:
  - deliver the response, positive, with the grant (telemetry and control ids, a subscription id);
  - report the periodicity as -1, "not reported", and not as the 5000 that was asked for and
    not as 0, which would read as "every event";
  - keep the subscription working: indications arrive.

Over the POSIX link on IPC, the link that serves one endpoint. A RAN peer is needed, so the test
starts the shipped example agent and skips when it is not built.

Run: PYTHONPATH=build/swig python3 tests/test_swig_subscription_no_periodicity.py [path/to/example_simple_agent]
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True

_fail = []


def check(cond, what):
    print(f"  {'PASS' if cond else 'FAIL'}  {what}")
    if not cond:
        _fail.append(what)


def find_agent():
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = ([sys.argv[1]] if len(sys.argv) > 1 else []) + [
        "build/example_simple_agent", os.path.join(here, "..", "build", "example_simple_agent")]
    return next((os.path.abspath(c) for c in candidates if os.path.exists(c)), None)


def main():
    try:
        import libe3py
    except ImportError as exc:
        print(f"SKIP: libe3py is not importable ({exc})")
        return 0
    agent = find_agent()
    if agent is None:
        print("SKIP: example_simple_agent not built; no RAN peer available")
        return 0

    ipc = tempfile.mkdtemp(prefix="libe3_noperiod_")
    os.chmod(ipc, 0o777)
    ran = None
    try:
        ran = subprocess.Popen(
            [agent, "--link", "posix", "--transport", "ipc", "--encoding", "asn1",
             "--socket-dir", ipc, "--period-us", "20000"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.5)  # let the RAN bind before the dApp connects

        cfg = libe3py.E3Config()
        cfg.role = libe3py.E3Role_DAPP
        cfg.dapp_name = "NoPeriodicityDApp"
        cfg.link_layer = libe3py.E3LinkLayer_POSIX
        cfg.transport_layer = libe3py.E3TransportLayer_IPC
        cfg.log_level = 0
        cfg.setup_endpoint = f"ipc://{ipc}/setup"   # the one endpoint of the POSIX link

        session = libe3py.DAppSession(cfg)
        check(session.start() == 0, "the session starts")
        check(session.wait_for_setup(10000) == 0, "setup completes")

        request_id = session.subscribe(1, [1], [1], -1, 5000)
        check(request_id > 0, f"subscribe is accepted (request id {request_id})")

        response = None
        indications = 0
        deadline = time.time() + 10.0
        while time.time() < deadline and (response is None or indications < 3):
            for event in session.poll_events(64, 200):
                if event.kind == libe3py.E3_EVENT_SUBSCRIPTION_RESPONSE and response is None:
                    response = event
                elif event.kind == libe3py.E3_EVENT_INDICATION:
                    indications += 1

        check(response is not None, "the subscription response reaches Python")
        if response is not None:
            check(response.response_code == 0, "the response is positive")
            check(response.periodicity == -1,
                  f"the periodicity is reported as not reported, -1 (got {response.periodicity})")
            check(response.has_telemetry_granted and list(response.get_telemetry_granted()) == [1],
                  "the telemetry grant is there")
            check(response.has_control_granted and list(response.get_control_granted()) == [1],
                  "the control grant is there")
            check(response.subscription_id != 0, "the response names a subscription")
        check(indications >= 3, f"indications arrive ({indications})")

        session.stop()
    finally:
        if ran is not None:
            ran.terminate()
            try:
                ran.wait(timeout=10)
            except subprocess.TimeoutExpired:
                ran.kill()
        shutil.rmtree(ipc, ignore_errors=True)

    if _fail:
        print(f"{len(_fail)} check(s) failed")
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
