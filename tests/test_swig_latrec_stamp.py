#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

"""Smoke test for the latrec TLS stamping API bound through libe3py.

Exercises latrec_tls_open_as_py, latrec_seq_next_py, latrec_tstamp_py,
latrec_ctx_set_py/latrec_ctx_py, and latrec_tnow_py/latrec_tstamp_at_py --
the API a Python consumer (spear-dApp) records its own application-level
stages through, in place of a second, hand-rolled ring writer. Reads the
result back with tools/latrec_reader.py rather than re-decoding the ring
format here, and checks the stamped stage ids against libe3py's own
constants rather than a hand-copied table -- the two things this binding
exists so a downstream package does not have to do itself.

No RAN peer is needed: this only exercises the TLS convenience layer
directly, unlike test_swig_latrec.py's session-ring coverage.

With LIBE3_ENABLE_LATREC off, every one of these calls is a documented no-op
(see include/libe3/latrec.h) and no ring is ever created; this test passes
either way; it does not check the constants against the C header since the OFF
build's own tests already do (test_latrec_reader.py), just that calling the
bound API never raises regardless of what it was built with.

Run: PYTHONPATH=build/swig python3 tests/test_swig_latrec_stamp.py
"""
import os
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
import latrec_reader as lr  # noqa: E402

_fail = []


def check(cond, what):
    print(f"  {'PASS' if cond else 'FAIL'}  {what}")
    if not cond:
        _fail.append(what)


def main():
    try:
        import libe3py
    except ImportError as e:
        print(f"SKIP: libe3py not importable ({e})")
        return 0

    d = tempfile.mkdtemp(prefix="latrec_swig_stamp_")
    try:
        libe3py.latrec_set_output_dir_py(d)
        libe3py.latrec_tls_open_as_py("py.stamp_smoke")

        seq = libe3py.latrec_seq_next_py()
        libe3py.latrec_tstamp_py(seq, libe3py.LATREC_RECORD_BEGIN, 111, 222)

        # ctx_set/ctx_get: the bridge a producer uses to join its own stages
        # to libe3's outbound leg (see LATREC_EMIT_ENTER's doc comment).
        libe3py.latrec_ctx_set_py(seq)
        check(libe3py.latrec_ctx_py() == seq,
              f"latrec_ctx_py() reads back what latrec_ctx_set_py() wrote ({seq})")

        # tnow/tstamp_at: read the clock before an operation whose outcome is
        # only known afterwards, then stamp with that earlier reading -- the
        # same pattern libe3's own queue handoffs use, and for the same
        # reason (a consumer racing ahead of the producer's own stamp).
        t = libe3py.latrec_tnow_py()
        libe3py.latrec_tstamp_at_py(seq, libe3py.LATREC_PROCESS_BEGIN, 0, 0, t)

        rings = [n for n in os.listdir(d) if n.endswith(".latrec")]
        if not rings:
            # A build with LIBE3_ENABLE_LATREC off takes every call above as
            # a documented no-op and writes nothing; that is success, not an
            # empty test -- there is nothing else to check against.
            print("SKIP: no ring written (LIBE3_ENABLE_LATREC is off in this build)")
            return 1 if _fail else 0

        check(len(rings) == 1, f"exactly one ring for this thread ({len(rings)} found)")
        ring = lr.read_ring(os.path.join(d, rings[0]))
        check(ring.name.startswith("py.stamp_smoke"),
              f"ring is named after the role passed to latrec_tls_open_as_py ({ring.name})")

        by_stage = {r.stage: r for r in ring.records if r.seq == seq}
        check(libe3py.LATREC_RECORD_BEGIN in by_stage,
              "RECORD_BEGIN, stamped via latrec_tstamp_py, is in the ring")
        check(libe3py.LATREC_PROCESS_BEGIN in by_stage,
              "PROCESS_BEGIN, stamped via latrec_tstamp_at_py, is in the ring")
        if libe3py.LATREC_RECORD_BEGIN in by_stage:
            r = by_stage[libe3py.LATREC_RECORD_BEGIN]
            check((r.aux, r.aux2) == (111, 222),
                  f"RECORD_BEGIN carries the aux/aux2 it was stamped with ({r.aux}, {r.aux2})")
        if libe3py.LATREC_PROCESS_BEGIN in by_stage:
            check(by_stage[libe3py.LATREC_PROCESS_BEGIN].t_ns == t,
                  "PROCESS_BEGIN's timestamp is the one latrec_tnow_py() took, "
                  "not a fresh read at the stamp call")
    finally:
        libe3py.latrec_set_output_dir_py(None)
        shutil.rmtree(d, ignore_errors=True)

    print(f"\n{'FAILED: ' + str(len(_fail)) if _fail else 'all checks passed'}")
    return 1 if _fail else 0


if __name__ == "__main__":
    sys.exit(main())
