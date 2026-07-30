#!/usr/bin/env python3
"""Round-trips a ring written by the C writer through tools/latrec_reader.py,
and checks the reader's stage table against the catalog in latrec.h.

This is the test that keeps the format from drifting. The record layout, the
header layout and the stage ids are mirrored in four writers (libe3, the C++
dApp, flexric, the Python dApp) and read by one reader; without a round trip
against the real writer, a header field added on one side is only caught by
someone noticing.

Usage: test_latrec_reader.py <path-to-latrec_fixture> <path-to-latrec.h>
"""
import os
import subprocess
import sys
import tempfile

# No .pyc next to the reader: it would litter the source tree, and a stale one
# silently masks an edit while debugging a drift failure.
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
import latrec_reader as lr  # noqa: E402

fails = []


def check(cond, what):
    print(("  PASS  " if cond else "  FAIL  ") + what)
    if not cond:
        fails.append(what)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    fixture, header = sys.argv[1], sys.argv[2]

    with tempfile.TemporaryDirectory() as d:
        out = subprocess.run([fixture, d, "libe3.fixture"], check=True,
                             capture_output=True, text=True).stdout
        expected = [tuple(int(x) for x in line.split())
                    for line in out.strip().splitlines()]
        rings = lr.read_dir(d)

        check(len(rings) == 1, f"one ring written and read back (got {len(rings)})")
        if not rings:
            return 1
        r = rings[0]

        # --- header ---
        check(r.version == lr.LATREC_VERSION,
              f"header version is {lr.LATREC_VERSION} (got {r.version})")
        check(r.rec_size == lr.REC_SIZE,
              f"rec_size is {lr.REC_SIZE} (got {r.rec_size})")
        check(r.name == "libe3.fixture" or r.name.startswith("libe3.fixture."),
              f"ring name carries the role (got {r.name!r})")
        check(r.entries == 4096, f"LATREC_ENTRIES_LOG2 honoured (got {r.entries})")
        check(r.rec_count == len(expected),
              f"rec_count written by the exit flush (got {r.rec_count})")
        check(r.clock_ns_per_call > 0, "measured clock cost recorded")
        check(r.t0_mono_ns > 0 and r.t0_real_ns > 0, "opening clock pair recorded")
        check(r.t1_mono_ns >= r.t0_mono_ns, "closing clock pair not before opening")
        check(not r.wrapped and r.lost_records == 0, "small capture did not wrap")

        # --- records, field by field against what the writer says it wrote ---
        got = [(x.seq, x.stage, x.aux, x.aux2) for x in r.records]
        check(got == expected,
              "every record round-trips (seq/stage/aux/aux2) in write order")
        check(all(x.t_ns > 0 for x in r.records), "every record has a nonzero t_ns")
        check([x.t_ns for x in r.records] == sorted(x.t_ns for x in r.records),
              "timestamps are non-decreasing in write order")
        # 48-bit seq and 64-bit aux must survive the packing.
        check(any(x.seq == 0x0000FFFFFFFFFFFF for x in r.records),
              "a full 48-bit seq survives the sc packing")
        check(any(x.aux == 0xFFFFFFFFFFFFFFFF for x in r.records),
              "a full 64-bit aux survives")

        # --- the monotonic -> realtime mapping used for cross-host joins ---
        first = r.records[0]
        real = r.mono_to_real_ns(first.t_ns)
        check(abs(real - (first.t_ns + r.mono_real_offset_ns)) < 1_000_000,
              "mono->real agrees with the constant offset to within 1 ms")
        check(r.mono_to_real_ns(r.t0_mono_ns) == r.t0_real_ns,
              "the mapping is exact at the opening clock pair")
        # Order must survive the mapping, or a cross-host join reorders a leg.
        mapped = [r.mono_to_real_ns(x.t_ns) for x in r.records]
        check(mapped == sorted(mapped), "mono->real preserves record order")
        check(real > 1_600_000_000_000_000_000,
              "mapped stamps are plausible wall-clock nanoseconds")

        # --- the stage table must match the catalog in latrec.h ---
        import re
        src = open(header).read()
        cat = {int(v, 16): n for n, v in
               re.findall(r'\b(?:LATREC_)([A-Z0-9_]+?)\s*=\s*(0x[0-9A-Fa-f]+)', src)}
        # Names in the header carry the LATREC_ prefix; the reader's table does not.
        missing = {hex(k): v for k, v in cat.items() if k not in lr.STAGES}
        extra = {hex(k): v for k, v in lr.STAGES.items() if k not in cat}
        check(not missing, f"every catalog stage is in the reader table (missing {missing})")
        check(not extra, f"no stage in the reader table is absent from the catalog (extra {extra})")
        mism = {hex(k): (cat[k], lr.STAGES[k]) for k in set(cat) & set(lr.STAGES)
                if cat[k] != lr.STAGES[k]}
        check(not mism, f"stage names agree with the catalog ({mism})")

    print(f"\n{'all checks passed' if not fails else str(len(fails)) + ' FAILED'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
