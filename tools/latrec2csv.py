#!/usr/bin/env python3
"""Convert a directory of .latrec rings into CSV. Requires numpy.

Runs entirely out of process: the rings are mmap'd read-only and the traced
processes are never touched, so conversion costs the measured threads nothing.
Records are valid iff t_ns != 0 (the writer releases t_ns last), so a ring can
be converted while its writer is still running or after it crashed.

Outputs, in --outdir (default <run>/csv):
  rings.csv         one row per ring: header fields, validity, wrap accounting
  records.csv       long format, one row per record; append-only and streamable
  <family>.csv      wide, one row per seq with a column per stage plus deltas

Stage families each own a private seq space (all start at 1), so joins are done
per family; joining across families by seq would fuse unrelated messages. Where
one family's records carry another's key, it is surfaced as its own column (see
AUX_COLS) rather than assumed -- origin_seq in libe3.csv is the OAI publish
sequence, and is what makes a producer-to-send join possible.

Capturing itself needs no wrapper script: build with -DLIBE3_ENABLE_LATREC=ON
and the rings are written for every run of that build -- there is no second,
per-run gate. Where they land is chosen with latrec_set_output_dir() /
latrec_open_in() / -DLATREC_DEFAULT_DIR= at configure time (see
docs/latrec.md). Pass --watch to have this tool wait for the rings to go
quiet first, e.g. as the last step of a capture script driving a
backgrounded traced process.
"""
import argparse
import glob
import os
import sys
import time

import numpy as np

# No .pyc next to the reader: a stale one masks an edit while debugging.
sys.dont_write_bytecode = True

# The record layout and the stage names come from latrec_reader.py, which is
# installed beside this script; this tool owns only the leg topology below.
# Nothing here re-describes the layout, so a header change cannot be half
# applied, and the reader's round-trip test keeps it honest against latrec.h.
# LIBE3_TOOLS overrides the lookup so a checkout can be run against its own
# reader while an older copy is installed.
_READER_DIRS = [
    os.environ.get("LIBE3_TOOLS", ""),
    os.path.dirname(os.path.abspath(__file__)),
    "/usr/local/share/libe3/tools",
    "/usr/share/libe3/tools",
]
for _d in _READER_DIRS:
    if _d and os.path.exists(os.path.join(_d, "latrec_reader.py")):
        sys.path.insert(0, _d)
        break
else:
    sys.exit("latrec_reader.py not found; install libe3 or set LIBE3_TOOLS=<dir>")
import latrec_reader as lr  # noqa: E402

STAGES = lr.STAGES
REC = np.dtype([("sc", "<u8"), ("t", "<u8"),
                ("aux", "<u8"), ("aux2", "<u8")])


# Which component a ring belongs to, matched on its role name (the part of
# <role>.<tid>.latrec before the tid). The stage catalog names operations, not
# components -- an indication encode is the same identifier whoever performed
# it -- so the ring is what says which side of the loop a record came from.
# First matching prefix wins; an unmatched ring lands in "other".
RING_COMPONENTS = [
    ("oai",      ("oai", "gnb", "phy", "mac", "l1_kpm", "spectrum")),
    ("ocudu",    ("ocudu", "jbpf", "e3controller")),
    ("cubb",     ("cubb", "aerial")),
    ("dapp",     ("dapp",)),
    ("xapp",     ("xapp", "xdevsm")),
    ("flexric",  ("flexric", "e2agent", "ric")),
    ("examples", ("sm_simple", "bench")),
    ("libe3",    ("libe3", "outbound", "inbound", "report", "setup", "session",
                  "context", "ex", "ctx")),
]

# Legs, defined once over the operation catalog rather than per component: a
# leg is one flow of messages through an ordered set of stages, and the same
# leg shape appears in whichever component performs those operations. Each
# component's CSV gets the legs whose stages its own rings actually carry, so
# a dApp ring yields the consume/apply legs and a RAN ring the source/apply
# ones without either being declared twice.
#
# Legs keep their own seq space -- every producer numbers from 1 -- so a table
# is keyed by (leg, seq), never by seq alone. Extras are stamped for a leg's
# messages but sit outside its ordered chain.
LEGS = [
    # Source side: record -> process -> encode the Service Model payload.
    # WAIT_ENTER/SKIPPED annotate a row rather than extending the chain: a
    # skipped row has no encode stages to order against.
    ("source",    [0x10, 0x11, 0x18, 0x19], [0x70, 0x71],
     "the source's own record counter"),
    # libe3 outbound. EMIT_ENTER is an extra, not a chain stage: only the emit
    # APIs stamp it, so putting it in the chain would blank outbound_total_us
    # for every ack and setup response. Its hop to ENQUEUE is in EXTRA_HOPS.
    ("outbound",  [0x21, 0x22, 0x23, 0x24], [0x20, 0x2F], "Pdu::enqueue_seq"),
    ("inbound",   [0x25, 0x26], [0x2F],
     "process-wide counter, allocated at RECV"),
    ("deliver",   [0x27, 0x28], [], "same seq as the enclosing inbound row"),
    ("report_q",  [0x29, 0x2A], [],
     "same seq as the inbound row that queued the report"),
    ("session",   [0x2B, 0x2C], [0x2F],
     "session-ring counter (language-binding seam)"),
    # Receiving side: decode the Service Model payload, then act on it. Which
    # of the three tails fills depends on the role -- a dApp reaches a decision
    # and builds output, a RAN SM installs a control, an xApp policy is applied.
    ("consume",   [0x1A, 0x1B, 0x40, 0x41], [0x43],
     "the decoded message's own key"),
    ("apply_ctrl", [0x48, 0x49], [],
     "E3 message_id of the control being applied (NOT globally unique)"),
    ("apply_pol", [0x42], [], "request_id (== E3 message_id)"),
    ("ack",       [0x4A, 0x4B], [], "the acknowledged message's id"),
    # Bootstrap. These seqs are E3 request message ids: they restart per agent
    # and are confined to a small range, so rows from several peers or several
    # runs in one capture can share a seq.
    ("setup",     [0x30, 0x31], [],
     "SetupRequest message_id (NOT globally unique)"),
    ("subscribe", [0x32, 0x33], [],
     "SubscriptionRequest/Response message id (NOT globally unique)"),
    ("sm_start",  [0x34], [], "process-wide counter"),
    ("sm_stop",   [0x35], [], "process-wide counter"),
    ("sm_status", [0x36, 0x37], [], "process-wide counter"),
    # E2-E3 bridge. Handed no id it could share with either side, so it numbers
    # locally and pairs with the E2 and E3 legs by time.
    ("bridge",    [0x50, 0x51], [], "process-wide counter"),
    # E2 legs. The xApp and the agent number independently, so an outbound leg
    # pairs with the peer's inbound leg by time, not by seq.
    ("e2ap_out",  [0x58, 0x59], [], "the stamping side's own counter"),
    ("e2ap_in",   [0x5A, 0x5B], [], "the stamping side's own counter"),
    ("e2sm_in",   [0x60, 0x61, 0x62], [], "the stamping side's own counter"),
    ("e2sm_out",  [0x63, 0x64], [], "the stamping side's own counter"),
    # Off-path load sampling, ~1 Hz on its own ring, unkeyed.
    ("context",   [0x72], [], "unkeyed (0)"),
]

# Hops between two stages of a leg that are not consecutive in its chain (or
# not in the chain at all). Emitted like any other hop column, blank unless
# both endpoints are present on the row.
EXTRA_HOPS = {
    "outbound": [(0x20, 0x21)],   # emit API entry -> enqueue
    # The emit tail: subscriber bookkeeping after the payload went out.
    "source":   [(0x19, 0x70)],
}

# Columns carrying a foreign leg's seq out of a record's aux field, so a row
# can be joined to the leg that produced it. Each entry is (stage, field,
# column).
#
# origin_seq is what closes the producer-to-libe3 join: a Service Model
# publishes its own record sequence with latrec_ctx_set() before emitting, and
# libe3 stamps it into EMIT_ENTER's aux. Join a component's source leg to
# libe3's outbound leg on origin_seq == seq to follow a record from data
# generation through to the send. It is 0 for PDUs whose producer set no
# context (acks, setup responses), and is only unique within one producer, so
# disambiguate by ring when a capture carries several.
AUX_COLS = {
    "outbound": [(0x20, "aux", "origin_seq")],
}


def ring_component(ring_name):
    """Component that owns a ring, matched on its role prefix."""
    role = (ring_name or "").rsplit(".", 1)[0].lower()
    for comp, prefixes in RING_COMPONENTS:
        for pre in prefixes:
            if role == pre or role.startswith(pre + ".") or role.startswith(pre + "_"):
                return comp
    return "other"


def load_ring(path, wall=False):
    """Return (meta, recs) with recs in write order, oldest first.

    Parsing and wrap recovery are libe3's (tools/latrec_reader.py); this only
    reshapes the result into the numpy arrays the CSV emitter wants.
    """
    try:
        r = lr.read_ring(path)
    except lr.BadRing as e:
        print(f"  ! {e}")
        return None, None
    if r.descents > 1:
        print(f"  ! {r.file}: {r.descents} timestamp descents; expected at most "
              f"one (wrap). Leaving the order untouched.")
    recs = np.empty(len(r.records), dtype=REC)
    for i, x in enumerate(r.records):
        t = r.mono_to_real_ns(x.t_ns) if wall else x.t_ns
        recs[i] = (x.seq | (x.cpu << 48) | (x.stage << 56), t, x.aux, x.aux2)
    meta = dict(ring=r.name, version=r.version, entries=r.entries,
                bytes=r.bytes, rec_size=r.rec_size, rec_count=r.rec_count,
                valid=r.valid, clock_ns_per_call=r.clock_ns_per_call,
                t0_mono_ns=r.t0_mono_ns, t0_real_ns=r.t0_real_ns,
                t1_mono_ns=r.t1_mono_ns, t1_real_ns=r.t1_real_ns,
                wrapped=int(r.wrapped), lost_records=r.lost_records,
                mono_real_offset_ns=r.mono_real_offset_ns, file=r.file)
    return meta, recs


def wait_for_quiet(dirs, quiet_secs, timeout_secs, poll_secs=2.0):
    """Block until every *.latrec file under `dirs` has been unchanged (by
    mtime and size) for `quiet_secs`, or `timeout_secs` has elapsed.

    A capture directory that has nothing in it yet, or gains a new ring
    mid-poll, resets the quiet clock -- the run is not idle until nothing
    has changed anywhere for a full quiet window. Giving up after the
    timeout is not an error: conversion is safe to run against a capture
    that is still growing (see the module docstring), so this only trades
    a possibly-incomplete tail for not hanging forever.
    """
    deadline = time.monotonic() + timeout_secs
    quiet_since = None
    last_state = None
    while True:
        state = {}
        for d in dirs:
            for path in glob.glob(os.path.join(d, "*.latrec")):
                try:
                    st = os.stat(path)
                except OSError:
                    continue
                state[path] = (st.st_mtime, st.st_size)

        now = time.monotonic()
        if state == last_state and state:
            quiet_since = quiet_since or now
            if now - quiet_since >= quiet_secs:
                return
        else:
            quiet_since = None
        last_state = state

        if now >= deadline:
            print(f"  ! --watch timed out after {timeout_secs}s; "
                  f"converting whatever is on disk now")
            return
        time.sleep(min(poll_secs, deadline - now) if deadline > now else poll_secs)


def where(stage):
    """Leg that owns a stage, for the long-format table. The component is not
    derivable from the stage any more -- it comes from the ring."""
    for leg, chain, extras, _ in LEGS:
        if stage in chain or stage in extras:
            return leg
    return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", nargs="+",
                    help="directories containing *.latrec (one per node)")
    ap.add_argument("-o", "--outdir", default=None)
    ap.add_argument("--wall", action="store_true",
                    help="emit CLOCK_REALTIME instead of CLOCK_MONOTONIC, mapped "
                         "per ring from its clock pairs. Required to join records "
                         "from different hosts, whose monotonic origins are "
                         "unrelated; only as good as their clock sync (us under "
                         "PTP, ms under NTP). Within one host leave it off: "
                         "monotonic cannot be stepped.")
    ap.add_argument("--watch", action="store_true",
                    help="wait until every *.latrec file under the given "
                         "directories has stopped growing (see --quiet-secs) "
                         "before converting, instead of converting immediately. "
                         "For driving this from a capture script: run the traced "
                         "process(es) in the background, then run this tool with "
                         "--watch as the last step.")
    ap.add_argument("--quiet-secs", type=float, default=15.0,
                    help="with --watch, how long every ring must be unchanged "
                         "before the capture is considered done (default: 15)")
    ap.add_argument("--timeout", type=float, default=7200.0,
                    help="with --watch, give up waiting after this many seconds "
                         "and convert whatever is on disk (default: 7200)")
    args = ap.parse_args()

    if args.watch:
        wait_for_quiet(args.run, args.quiet_secs, args.timeout)

    # Several directories rather than one, so a multi-node capture is never
    # merged into a single directory: ring files are <role>.<tid>.latrec and
    # tids are per-host, so copying two nodes together can silently overwrite
    # a ring whose role also runs on the other node.
    files = []
    for d in args.run:
        found = sorted(glob.glob(os.path.join(d, "*.latrec")))
        if not found:
            sys.exit(f"no .latrec files in {d}")
        files += found
    if len(args.run) > 1 and not args.wall:
        print("  ! several run directories given without --wall: monotonic "
              "clocks from different hosts are not comparable")
    outdir = args.outdir or os.path.join(args.run[0], "csv")
    os.makedirs(outdir, exist_ok=True)

    metas, seqs, stages, cpus, tns, auxs, aux2s, rings = [], [], [], [], [], [], [], []
    for path in files:
        meta, recs = load_ring(path, args.wall)
        if meta is None:
            print(f"  ! {os.path.basename(path)}: bad magic, skipped")
            continue
        metas.append(meta)
        if len(recs) == 0:
            continue
        sc = recs["sc"]
        seqs.append(sc & 0x0000FFFFFFFFFFFF)
        stages.append((sc >> 56).astype("u1"))
        cpus.append(((sc >> 48) & 0xFF).astype("u1"))
        tns.append(recs["t"])
        auxs.append(recs["aux"])
        aux2s.append(recs["aux2"])
        rings.append(np.full(len(recs), meta["ring"], dtype=object))

    # rings.csv
    cols = ["file", "ring", "version", "entries", "bytes", "rec_size", "rec_count",
            "valid", "wrapped", "lost_records", "clock_ns_per_call",
            "t0_mono_ns", "t0_real_ns", "t1_mono_ns", "t1_real_ns",
            "mono_real_offset_ns"]
    with open(os.path.join(outdir, "rings.csv"), "w") as f:
        f.write(",".join(cols) + "\n")
        for m in metas:
            f.write(",".join(str(m[c]) for c in cols) + "\n")

    if not seqs:
        print("  no records in any ring")
        return 0

    seq = np.concatenate(seqs)
    stage = np.concatenate(stages)
    cpu = np.concatenate(cpus)
    t = np.concatenate(tns)
    aux = np.concatenate(auxs)
    aux2 = np.concatenate(aux2s)
    ring = np.concatenate(rings)
    order = np.argsort(t, kind="stable")
    seq, stage, cpu, t, aux, aux2, ring = (a[order] for a in (seq, stage, cpu, t, aux, aux2, ring))

    # records.csv -- long format, time ordered, every record from every ring
    with open(os.path.join(outdir, "records.csv"), "w") as f:
        f.write("ring,component,leg,seq,stage,stage_name,cpu,t_ns,aux,aux2\n")
        loc = {s: where(s) for s in set(int(x) for x in np.unique(stage))}
        nm = {s: STAGES.get(s, f"0x{s:02X}") for s in loc}
        comp_of = {r: ring_component(r) for r in set(str(x) for x in ring)}
        for i in range(len(t)):
            s = int(stage[i])
            f.write(f"{ring[i]},{comp_of[str(ring[i])]},{loc[s]},{seq[i]},{s},{nm[s]},"
                    f"{cpu[i]},{t[i]},{aux[i]},{aux2[i]}\n")

    # one wide CSV per family
    def sname(s):
        return STAGES.get(s, f"0x{s:02X}")

    # Stages seen per component, so each table declares only the legs its own
    # rings actually stamped. One operation id can appear under several
    # components (both sides of the loop encode E3SM); each gets its own row.
    comp_of_ring = {r: ring_component(r) for r in set(str(x) for x in ring)}
    comp_of_rec = np.array([comp_of_ring[str(r)] for r in ring])
    seen_by_comp = {}
    for c in sorted(set(comp_of_ring.values())):
        seen_by_comp[c] = {int(x) for x in np.unique(stage[comp_of_rec == c])}

    print(f"  {'table':14s} {'leg':9s} {'chain':<32} {'rows':>7} {'complete':>9}")
    for comp in sorted(seen_by_comp):
        seen = seen_by_comp[comp]
        in_comp = comp_of_rec == comp
        legs = [(leg, [s for s in ch if s in seen], [e for e in ex if e in seen], mean)
                for leg, ch, ex, mean in LEGS]
        legs = [l for l in legs if l[1]]
        if not legs:
            continue

        # Union of every stage this component stamps, in leg order; a row only
        # fills its own leg's columns.
        present, specs = [], []      # specs: (label, leg, from, to)
        auxspecs = []                # (label, leg, stage, field)
        for leg, chain, extras, _ in legs:
            present += chain + extras
            mine = set(chain) | set(extras)
            for a, b in zip(chain, chain[1:]):
                specs.append((f"{sname(a)}__{sname(b)}_us", leg, a, b))
            for a, b in EXTRA_HOPS.get(leg, []):
                if a in mine and b in mine:
                    specs.append((f"{sname(a)}__{sname(b)}_us", leg, a, b))
            if len(chain) > 1:
                specs.append((f"{leg}_total_us", leg, chain[0], chain[-1]))
            for s, field, label in AUX_COLS.get(leg, []):
                if s in mine:
                    auxspecs.append((label, leg, s, field))
        cols = ["leg", "seq"] + [sname(s) + "_ns" for s in present] + \
               [c for c, _, _, _ in specs] + [c for c, _, _, _ in auxspecs]

        path = os.path.join(outdir, f"{comp}.csv")
        with open(path, "w") as f:
            for leg, chain, _, mean in legs:
                f.write(f"# leg {leg}: seq = {mean}; chain "
                        + "->".join(sname(s) for s in chain) + "\n")
            f.write(",".join(cols) + "\n")
            for leg, chain, extras, _ in legs:
                mine = chain + extras
                # Chain stages define which messages exist in this leg; extras
                # only annotate them. A drop shared with another leg must not
                # conjure a row here for a message that leg never saw.
                #
                # A seq is not unique over a run: the dApp legs key on
                # sfn<<16|slot, which cycles every 1024 frames, and a few point
                # events are unkeyed (seq 0). Records are in time order, so a
                # chain stage arriving for a key whose open row already holds it
                # opens a new row -- it is the next message, not a correction.
                #
                # Unkeyed records are grouped by ring as well. A ring has one
                # writer, so this keeps two threads stamping the same unkeyed
                # stage from interleaving into each other's rows; keyed records
                # must NOT be grouped that way, since one message's stages
                # legitimately span several rings.
                chainset, extraset = set(chain), set(extras)
                auxfor = {}
                for _, lg, s, field in auxspecs:
                    if lg == leg:
                        auxfor.setdefault(s, []).append(field)
                rows_out, open_row, early = [], {}, {}
                for i in np.flatnonzero(in_comp & np.isin(stage, list(chainset | extraset))):
                    s, q = int(stage[i]), int(seq[i])
                    k = q if q else (0, ring[i])
                    cur = open_row.get(k)
                    if s in chainset:
                        if cur is None or s in rows_out[cur][1]:
                            rows_out.append((q, {}, {}))
                            cur = open_row[k] = len(rows_out) - 1
                            # An extra can precede its leg's first chain stage
                            # (an emit-API entry stamped ahead of the enqueue);
                            # it belongs to the row that stage opens.
                            for es, et, ea in early.pop(k, ()):
                                rows_out[cur][1][es] = et
                                rows_out[cur][2].update(ea)
                    axs = {(s, f): int(aux[i] if f == "aux" else aux2[i])
                           for f in auxfor.get(s, ())}
                    if cur is None:                  # extras never open a row
                        early.setdefault(k, []).append((s, int(t[i]), axs))
                        continue
                    rows_out[cur][1][s] = int(t[i])
                    # Foreign-leg keys, read off whichever record carries them.
                    rows_out[cur][2].update(axs)
                done = 0
                for q, row, arow in rows_out:
                    vals = [leg, str(q)]
                    vals += [str(row.get(s, "")) if s in mine else "" for s in present]
                    for _, lg, a, b in specs:
                        ok = lg == leg and a in row and b in row and row[b] >= row[a]
                        vals.append(f"{(row[b]-row[a])/1000.0:.3f}" if ok else "")
                    for _, lg, s, field in auxspecs:
                        v = arow.get((s, field)) if lg == leg else None
                        vals.append("" if v is None else str(v))
                    if len(chain) > 1 and chain[0] in row and chain[-1] in row:
                        done += 1
                    f.write(",".join(vals) + "\n")
                label = "->".join(sname(s).split("_")[0] for s in chain)
                print(f"  {comp + '.csv':14s} {leg:9s} {label:<32} "
                      f"{len(rows_out):>7} {done if len(chain) > 1 else len(rows_out):>9}")

    print(f"\n  rings={len(metas)} records={len(t)} -> {outdir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
