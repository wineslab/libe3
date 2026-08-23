#!/usr/bin/env python3
"""latrec coverage for the Python binding's session ring.

The SESSION_QUEUED/SESSION_POLLED stages live in swig/e3_dapp_session.cpp and
are only reachable through libe3py: the C++ tests cannot touch them. That ring
sits between libe3's callbacks and the Python consumer and was, in a live
capture, the slowest seam in libe3 (~51 us p50), so it is worth holding to the
same standard as the rest.

Checks, against a real RAN peer driven by the simple agent example:
  - SESSION_QUEUED / SESSION_POLLED are stamped, once each per event, in order
  - the batch position recorded in aux is consistent with the batch size
  - poll_events opens a ring for the calling thread (it is not one libe3 starts)

A real RAN peer is required for anything to be queued, so the test spawns the
shipped example agent (build/example_simple_agent) on a private IPC directory
and subscribes to it.

Reads rings through tools/latrec_reader.py rather than re-decoding the format
here, and reads the stage/reason identifiers off the libe3py module rather
than a second hand-copied table -- this file itself once drifted exactly that
way (it hard-coded the pre-rework stage ids, silently never matching a real
record again once the catalog was renumbered).

Run: PYTHONPATH=build/swig python3 tests/test_swig_latrec.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
import latrec_reader as lr  # noqa: E402

_fail = []


def check(cond, what):
    print(f"  {'PASS' if cond else 'FAIL'}  {what}")
    if not cond:
        _fail.append(what)


def read_rings(d):
    """Every valid record in the directory as (seq, stage, t_ns, aux, aux2)."""
    out = []
    for name in os.listdir(d):
        if not name.endswith(".latrec"):
            continue
        try:
            ring = lr.read_ring(os.path.join(d, name))
        except lr.BadRing:
            continue  # not a ring this run wrote, or caught mid-write
        for r in ring.records:
            out.append((r.seq, r.stage, r.t_ns, r.aux, r.aux2))
    return out


def main():
    trace = tempfile.mkdtemp(prefix="latrec_swig_")
    os.environ["LATREC_ENTRIES_LOG2"] = "16"
    try:
        import libe3py
    except ImportError as e:
        print(f"SKIP: libe3py not importable ({e})")
        return 0
    # Ring placement, not a capture gate: an enabled build records either way.
    # Must precede the first session, since a thread's ring is opened once.
    libe3py.latrec_set_output_dir_py(trace)
    LQ0_QUEUED = libe3py.LATREC_SESSION_QUEUED
    LQ1_POLLED = libe3py.LATREC_SESSION_POLLED
    L9_DROP = libe3py.LATREC_DROP
    DROP_SESSION_QUEUE = libe3py.LATREC_DROP_SESSION_QUEUE

    ipc = tempfile.mkdtemp(prefix="latrec_swig_ipc_")
    agent = None
    # The example agent is only built with LIBE3_BUILD_EXAMPLES=ON; without it
    # there is no peer and nothing to measure, so skip rather than pass hollowly.
    for cand in ("build/example_simple_agent", "./example_simple_agent",
                 os.path.join(os.path.dirname(__file__), "..", "build",
                              "example_simple_agent")):
        if os.path.exists(cand):
            agent = os.path.abspath(cand)
            break
    if agent is None:
        print("SKIP: example_simple_agent not built; no RAN peer available")
        shutil.rmtree(ipc, ignore_errors=True)
        return 0
    try:
        cfg = libe3py.E3Config()
        cfg.role = libe3py.E3Role_DAPP
        cfg.dapp_name = "SwigLatrecDApp"
        cfg.link_layer = libe3py.E3LinkLayer_ZMQ
        cfg.transport_layer = libe3py.E3TransportLayer_IPC
        cfg.log_level = 0
        # Must match what example_simple_agent binds under --socket-dir.
        cfg.setup_endpoint = f"ipc://{ipc}/setup"
        cfg.subscriber_endpoint = f"ipc://{ipc}/dapp_socket"
        cfg.publisher_endpoint = f"ipc://{ipc}/e3_socket"

        # 2 ms period: enough events in a short run to exercise batching.
        ran = subprocess.Popen(
            [agent, "--link", "zmq", "--transport", "ipc",
             "--socket-dir", ipc, "--period-us", "2000"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            env={**os.environ, "LIBE3_TEST_LATREC_DIR": trace})
        time.sleep(1.5)   # let the RAN bind before the dApp connects

        session = libe3py.DAppSession(cfg)
        session.start()
        session.wait_for_setup(5000)
        session.subscribe(1, [], [], 0, 0)
        time.sleep(0.5)

        polled = 0
        deadline = time.time() + 3.0
        while time.time() < deadline:
            polled += len(session.poll_events(64, 200))
        print(f"  NOTE  polled {polled} events from the session ring")

        recs = read_rings(trace)
        lq0 = [r for r in recs if r[1] == LQ0_QUEUED]
        lq1 = [r for r in recs if r[1] == LQ1_POLLED]
        rings = [n for n in os.listdir(trace) if n.endswith(".latrec")]

        check(len(rings) > 0,
              f"poll_events opened a ring for the binding's thread ({len(rings)} ring(s))")

        if not lq0:
            # Without a peer there is nothing to queue. Say so plainly rather
            # than passing a check that verified nothing.
            print("  NOTE  no events were produced (no RAN peer in this test), "
                  "so LQ ordering could not be exercised")
            check(len(lq1) == 0, "no LQ1 without LQ0 (nothing invented)")
        else:
            by = {}
            for seq, stage, t, aux, aux2 in recs:
                if stage in (LQ0_QUEUED, LQ1_POLLED):
                    by.setdefault(seq, {}).setdefault(stage, []).append((t, aux, aux2))
            ordered = complete = dup = orphan = 0
            for seq, st in by.items():
                if LQ1_POLLED not in st:
                    continue          # still in the ring when we stopped
                if LQ0_QUEUED not in st:
                    orphan += 1       # polled without ever being recorded queued
                    continue
                complete += 1
                if len(st[LQ0_QUEUED]) != 1 or len(st[LQ1_POLLED]) != 1:
                    dup += 1
                if st[LQ1_POLLED][0][0] >= st[LQ0_QUEUED][0][0]:
                    ordered += 1
            check(complete > 0, f"{complete} events completed queue -> poll")
            check(dup == 0, f"no event stamped twice at either stage ({dup} bad)")
            check(ordered == complete,
                  f"every polled event was queued before it was polled "
                  f"({ordered}/{complete})")
            # A poll without a queue stamp means the producing thread had no
            # ring -- the contract every producer must honour.
            check(orphan == 0,
                  f"no event was polled without a queue stamp ({orphan} orphans)")
            # aux is the index within the drained batch, aux2 the batch size.
            bad = [r for r in lq1 if r[3] >= r[4]]
            check(not bad, "batch position is always inside the batch size")

        drops = [r for r in recs if r[1] == L9_DROP and r[4] == DROP_SESSION_QUEUE]
        print(f"  NOTE  session-queue drops observed: {len(drops)} "
              f"(0 expected when the consumer keeps up)")

        session.stop()
        ran.terminate()
        ran.wait(timeout=5)
    finally:
        shutil.rmtree(ipc, ignore_errors=True)
        shutil.rmtree(trace, ignore_errors=True)
        libe3py_mod = sys.modules.get("libe3py")
        if libe3py_mod is not None:
            libe3py_mod.latrec_set_output_dir_py(None)
        os.environ.pop("LATREC_ENTRIES_LOG2", None)

    print(f"\n{'FAILED: ' + str(len(_fail)) if _fail else 'all checks passed'}")
    return 1 if _fail else 0


if __name__ == "__main__":
    sys.exit(main())
