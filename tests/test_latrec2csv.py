#!/usr/bin/env python3
"""Tests for latrec2csv.py, driven by synthetic rings with known ground truth.

Each ring is written by hand with delays chosen in advance, so the CSVs can be
checked against the delays that were injected -- which a live capture can never
do, having no ground truth to compare against.

Run: python3 tests/test_latrec2csv.py [<path-to-latrec2csv.py>]
"""
import csv
import os
import shutil
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
CONVERTER = (sys.argv[1] if len(sys.argv) > 1
             else os.path.join(HERE, "..", "tools", "latrec2csv.py"))
MAGIC = 0x31524C41
HDR_LEN = 4096

_fail = []


def check(cond, what):
    print(f"  {'PASS' if cond else 'FAIL'}  {what}")
    if not cond:
        _fail.append(what)


REAL0 = 1_700_000_000_000_000_000


def write_ring(path, name, records, entries=4096, version=2, real0=REAL0):
    """records: list of (seq, stage, cpu, t_ns, aux, aux2).

    The closing realtime is real0 plus the monotonic span, so the reader's
    two-point fit sees the two clocks running at the same rate and reduces to
    a constant offset -- which is what a run under PTP looks like, and what
    lets a --wall test state an expected value.
    """
    t0_mono = records[0][3] if records else 0
    t1_mono = records[-1][3] if records else 0
    hdr = bytearray(HDR_LEN)
    struct.pack_into("<IIQQQQ", hdr, 0, MAGIC, version, entries,
                     len(records), t0_mono, real0)
    hdr[40:40 + len(name)] = name.encode()
    struct.pack_into("<IIQQ", hdr, 104, 32, 27,
                     t1_mono, real0 + (t1_mono - t0_mono))
    body = bytearray(entries * 32)
    for i, (seq, stage, cpu, t, aux, aux2) in enumerate(records):
        sc = (seq & 0xFFFFFFFFFFFF) | ((cpu & 0xFF) << 48) | ((stage & 0xFF) << 56)
        struct.pack_into("<QQQQ", body, i * 32, sc, t, aux, aux2)
    with open(path, "wb") as f:
        f.write(hdr)
        f.write(body)


def read_csv(path):
    with open(path) as f:
        return list(csv.DictReader(l for l in f if not l.startswith("#")))


def main():
    run = tempfile.mkdtemp(prefix="latrec2csv_test_")
    try:
        base = 1_000_000_000_000

        # libe3 outbound: 3 PDUs, each hop a known duration. ENQUEUE ->
        # DEQUEUE = 10 us, -> ENCODE_E3AP_DONE = 20 us, -> SEND_DONE = 40 us,
        # total 70 us.
        recs = []
        for n in range(3):
            t = base + n * 1_000_000
            recs += [(n + 1, 0x21, 4, t, 0, 5),
                     (n + 1, 0x22, 4, t + 10_000, 0, 5),
                     (n + 1, 0x23, 4, t + 30_000, 128, 0),
                     (n + 1, 0x24, 4, t + 70_000, 7, 0)]
        write_ring(os.path.join(run, "libe3.out.1.latrec"), "libe3.out.1", recs)

        # A second component, identified by its ring name rather than by the
        # stages it carries: the same BRIDGE_IN/BRIDGE_OUT ids would land under
        # whichever component's ring wrote them. Two messages through the
        # bridge, 2 us and 4 us.
        # Written in time order, as a single-writer ring always is: wrap
        # detection relies on that invariant.
        write_ring(os.path.join(run, "oai.relay.2.latrec"), "oai.relay.2", [
            (1, 0x50, 8, base + 500, 0, 0),
            (1, 0x51, 8, base + 2_500, 0, 0),
            (2, 0x50, 8, base + 3_000, 0, 1),
            (2, 0x51, 8, base + 7_000, 0, 1),
        ])
        # A drop belonging to the inbound leg only.
        write_ring(os.path.join(run, "libe3.in.3.latrec"), "libe3.in.3", [
            (900, 0x25, 2, base, 64, 0),
            (900, 0x26, 2, base + 3_000, 11, 5),
            (901, 0x25, 2, base + 10_000, 64, 0),
            (901, 0x2F, 2, base + 10_100, 0, 4),      # decode failure: no decode-done
        ])

        out = os.path.join(run, "csv")
        r = subprocess.run([sys.executable, CONVERTER, run, "-o", out],
                           capture_output=True, text=True)
        check(r.returncode == 0, f"converter exits 0 (stderr: {r.stderr.strip()[:80]})")

        # --- ground truth -------------------------------------------------
        libe3 = read_csv(os.path.join(out, "libe3.csv"))
        ob = [x for x in libe3 if x["leg"] == "outbound"]
        check(len(ob) == 3, "outbound leg has one row per PDU (3)")
        check(all(x["ENQUEUE__DEQUEUE_us"] == "10.000" for x in ob),
              "ENQUEUE->DEQUEUE reports the injected 10 us")
        check(all(x["DEQUEUE__ENCODE_E3AP_DONE_us"] == "20.000" for x in ob),
              "DEQUEUE->ENCODE_E3AP_DONE reports the injected 20 us")
        check(all(x["ENCODE_E3AP_DONE__SEND_DONE_us"] == "40.000" for x in ob),
              "ENCODE_E3AP_DONE->SEND_DONE reports the injected 40 us")
        check(all(x["outbound_total_us"] == "70.000" for x in ob),
              "outbound total reports the injected 70 us")

        # --- the component comes from the ring, not the stage id -----------
        oai = read_csv(os.path.join(out, "oai.csv"))
        br = [x for x in oai if x["leg"] == "bridge"]
        check(len(br) == 2, "the bridge leg has one row per message (2)")
        check([x["bridge_total_us"] for x in br] == ["2.000", "4.000"],
              "bridge rows report the injected 2 us and 4 us")
        check(not os.path.exists(os.path.join(out, "other.csv")),
              "a ring with a known role prefix is not filed under 'other'")

        # --- extras annotate rows, they do not create them ----------------
        inb = [x for x in libe3 if x["leg"] == "inbound"]
        check(len(inb) == 2, "inbound has 2 rows (one complete, one dropped)")
        dropped = [x for x in inb if x["seq"] == "901"][0]
        check(dropped["DROP_ns"] != "", "the drop is recorded on its inbound row")
        check(dropped["inbound_total_us"] == "",
              "a dropped message has no total")
        check(not any(x["leg"] == "outbound" and x["seq"] == "901" for x in libe3),
              "an inbound drop does not fabricate an outbound row")

        # --- long format --------------------------------------------------
        rows = read_csv(os.path.join(out, "records.csv"))
        check(len(rows) == len(recs) + 4 + 4,
              f"records.csv has every record ({len(rows)})")
        check({r["component"] for r in rows} == {"libe3", "oai"},
              "records.csv labels each record with its component")
        names = {r["stage_name"] for r in rows}
        check("RECV" in names and "0x25" not in names,
              "stages are named, not left as hex")
        ts = [int(r["t_ns"]) for r in rows]
        check(ts == sorted(ts), "records.csv is time ordered")

        # --- ring metadata --------------------------------------------------
        rings = read_csv(os.path.join(out, "rings.csv"))
        check(len(rings) == 3, "rings.csv has one row per ring")
        check(all(x["wrapped"] == "0" for x in rings), "no ring is flagged wrapped")

        # --- wrap accounting ------------------------------------------------
        run2 = tempfile.mkdtemp(prefix="latrec2csv_wrap_")
        try:
            cap = 64
            wrecs = [(i + 1, 0x25, 0, base + i * 1000, 0, 0) for i in range(cap)]
            # Rotate so the newest sits before the oldest, as a wrapped ring does,
            # and declare more written than the ring can hold.
            wrecs = wrecs[cap // 2:] + wrecs[:cap // 2]
            path = os.path.join(run2, "w.latrec")
            write_ring(path, "w", wrecs, entries=cap)
            with open(path, "r+b") as f:            # rec_count = 1.5 laps
                f.seek(16)
                f.write(struct.pack("<Q", cap + cap // 2))
            out2 = os.path.join(run2, "csv")
            subprocess.run([sys.executable, CONVERTER, run2, "-o", out2],
                           capture_output=True, text=True)
            w = read_csv(os.path.join(out2, "rings.csv"))[0]
            check(w["wrapped"] == "1", "a wrapped ring is detected")
            check(w["lost_records"] == str(cap // 2),
                  "lost_records = written - capacity")
        finally:
            shutil.rmtree(run2, ignore_errors=True)

        # --- two hosts, joined on the wall clock ----------------------------
        # Monotonic counts from boot, so two hosts share no origin: here they
        # are 5e15 ns apart. Both rings open at the same realtime, so with the
        # rates equal the mapping is exact and the injected 300 us must come
        # back out.
        nodes = [tempfile.mkdtemp(prefix="latrec2csv_node_") for _ in range(2)]
        try:
            # boot[node] is the monotonic reading at the shared realtime REAL0,
            # so a stage at boot + x happened at REAL0 + x on both hosts.
            boot = (1_000_000_000_000, 5_000_000_000_000_000)
            hop = 300_000
            a = [(1, 0x23, 0, boot[0], 0, 0), (1, 0x24, 0, boot[0] + 1_000, 0, 0)]
            b = [(1, 0x25, 0, boot[1] + 1_000 + hop, 0, 0),
                 (1, 0x26, 0, boot[1] + 2_000 + hop, 0, 0)]
            write_ring(os.path.join(nodes[0], "libe3.out.1.latrec"), "libe3.out.1", a,
                       real0=REAL0 + a[0][3] - boot[0])
            write_ring(os.path.join(nodes[1], "libe3.in.2.latrec"), "libe3.in.2", b,
                       real0=REAL0 + b[0][3] - boot[1])

            out3 = os.path.join(nodes[0], "csv")
            r = subprocess.run([sys.executable, CONVERTER, nodes[0], nodes[1],
                                "--wall", "-o", out3], capture_output=True, text=True)
            check(r.returncode == 0, f"--wall run exits 0 (stderr: {r.stderr.strip()[:80]})")
            rows = read_csv(os.path.join(out3, "records.csv"))
            check({x["ring"] for x in rows} == {"libe3.out.1", "libe3.in.2"},
                  "several run directories are read into one capture")
            ts = {x["stage_name"]: int(x["t_ns"]) for x in rows}
            check(all(t > 1_600_000_000_000_000_000 for t in ts.values()),
                  "--wall emits wall-clock nanoseconds, not monotonic")
            check(ts["RECV"] - ts["SEND_DONE"] == hop,
                  f"the cross-host hop is the injected {hop // 1000} us "
                  f"(got {(ts['RECV'] - ts['SEND_DONE']) / 1000.0} us)")
            check([int(x["t_ns"]) for x in rows] == sorted(int(x["t_ns"]) for x in rows),
                  "records from both hosts are merged in wall-clock order")

            r = subprocess.run([sys.executable, CONVERTER, nodes[0], nodes[1],
                                "-o", os.path.join(nodes[0], "csv2")],
                               capture_output=True, text=True)
            check("without --wall" in r.stdout,
                  "several directories without --wall are flagged")
        finally:
            for d in nodes:
                shutil.rmtree(d, ignore_errors=True)
    finally:
        shutil.rmtree(run, ignore_errors=True)

    print(f"\n{'FAILED: ' + str(len(_fail)) if _fail else 'all checks passed'}")
    return 1 if _fail else 0


if __name__ == "__main__":
    sys.exit(main())
