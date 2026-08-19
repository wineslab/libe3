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

Capturing itself needs no wrapper script: build with -DLIBE3_ENABLE_LATREC=ON,
`export LATREC_DIR=<dir>` around the run, `unset LATREC_DIR` when done. Pass
--watch to have this tool wait for the rings to go quiet first, e.g. as the
last step of a capture script driving a backgrounded traced process.
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


# One table per COMPONENT (the repo that owns the stamps), each holding one or
# more legs. A leg is one flow of messages through an ordered set of stages.
#
# Legs keep their own seq space -- every producer numbers from 1 -- so a
# component table is keyed by (leg, seq), never by seq alone. Stage sets are
# disjoint across the legs of a component, so a row only fills the columns of
# its own leg. Extras are stamped for a leg's messages but sit outside its
# ordered chain.
COMPONENTS = [
    ("oai", [
        # W5/W6 annotate a row rather than extending the chain: W4 stays the end
        # of the tap-to-libe3 total, and a W6 row has no W2..W4 to order against.
        ("iq",      [0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                     0x20, 0x21, 0x22, 0x23, 0x24], [0x19, 0x25, 0x26],
         "publish sequence"),
        ("rx_slot", [0x16, 0x17, 0x18], [], "PHY RX slot counter"),
        ("mac",     [0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68], [],
         "MAC scheduler tick counter"),
        # S7/S8 annotate a row rather than extending the chain, as W5/W6 and P9
        # do on the iq leg: S6 stays the end of the publish-to-libe3 total, and
        # an S8 row has no S3..S6 to order against.
        ("sensing", [0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76],
         [0x77, 0x78], "sensing publish sequence"),
        # Lifecycle: T0/T1 are point events, T2..T4 one chain per handler call.
        ("sm_start",  [0x69], [], "process-wide counter"),
        ("sm_stop",   [0x6A], [], "process-wide counter"),
        ("dapp_status", [0x6B, 0x6C, 0x6D, 0x6E], [], "process-wide counter"),
        # The relay is handed no id it could share with either side, so both of
        # these number locally and pair with the dApp/xApp legs by time.
        ("control", [0x90, 0x91], [], "process-wide counter"),
        ("report",  [0x98, 0x99], [], "process-wide counter"),
        # B2/B3 are stamped only by the prbBlock control; a sensingPolicy
        # control fills the rest of the chain and leaves those two empty.
        # B6 is stamped on the MAC thread but carries the installing control's
        # key, so control-to-on-air is a computed column. A tick that coalesced
        # several installs keys on the oldest and reports the count in aux2; a
        # clear with no control behind it lands on seq 0.
        ("prb_ctrl", [0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86], [],
         "E3 message_id of the dApp control (NOT globally unique)"),
    ]),
    ("libe3", [
        # LE0 is an extra, not a chain stage: only the emit APIs stamp it, so
        # putting it in the chain would blank outbound_total_us for every ack and
        # setup response. Its hop to L0 is declared in EXTRA_HOPS instead.
        ("outbound", [0x30, 0x31, 0x32, 0x33], [0x2F, 0x39], "Pdu::enqueue_seq"),
        ("inbound",  [0x34, 0x35, 0x36], [0x39],
         "process-wide counter, allocated at L4_RECV"),
        ("report",   [0x37, 0x38], [],
         "same seq as the inbound row that queued the report"),
        # Unlike the other legs, this seq is the E3 request message_id: it
        # restarts per agent and is confined to 1..1000, so rows from several
        # dApps or several runs in one capture can share a seq.
        ("setup",    [0x3A, 0x3B], [], "SetupRequest message_id (NOT globally unique)"),
        # The subscription request/response leg, distinct from setup above.
        # Numbered below the L block (see LATREC_LS2_SUB_RECV's comment in
        # latrec.h), so the chain is chronological, not numeric order.
        ("subscribe", [0x2C, 0x2B], [],
         "SubscriptionRequest/Response message id (NOT globally unique)"),
        ("session",  [0x3C, 0x3D], [0x39],
         "session-ring counter (dApp binding seam)"),
        ("connector", [0x3E, 0x3F], [],
         "same seq as the outbound row being sent"),
        # The dApp-role indication-delivery gap (E3Interface::handle_indication):
        # filter passed -> application callback returned. Same seq as the
        # enclosing inbound row; numbered below the L block, so chronological
        # order here is the reverse of numeric order (0x2E before 0x2D).
        ("callback", [0x2E, 0x2D], [], "same seq as the enclosing inbound row"),
    ]),
    ("dapp", [
        # D0/D1 precede the parse, so they carry no slot key and are their own
        # unkeyed leg; D2 is the first stage that can name the slot. No row can
        # hold both an unkeyed and a keyed stage.
        ("recv",    [0x40, 0x41], [], "unkeyed (0)"),
        # Chain order is chronological; D10..D12 were appended after the block
        # was in use, so it is not numeric order.
        ("ingest",  [0x42, 0x43, 0x44, 0x4B, 0x45, 0x4F, 0x46,
                     0x56, 0x47, 0x4C, 0x48, 0x49, 0x4A], [], "sfn<<16|slot"),
        ("sensing", [0x4D, 0x4E], [], "sfn<<16|slot of the RF=1 indication"),
        # adaptive_cuda: the GPU pipeline is 4 slots deep, so D18/D19 belong to
        # an older slot than the handler that retires them. Separate leg, and
        # D17 is the handoff that D18 later closes.
        ("gpu",     [0x5A, 0x5B, 0x5C], [], "sfn<<16|slot of the retired slot"),
        ("publish", [0x53, 0x50, 0x51, 0x52], [], "sfn<<16|slot, handed over at D9"),
        ("subscribe", [0x54, 0x55], [], "subscription request message id"),
        ("setup",     [0x57, 0x58, 0x59], [], "setup request message id"),
        ("ctrl_out",  [0xA3, 0xA4], [], "E3 message id; joins the gNB B stages"),
        # Point event on the shared PUB socket; no per-message key available.
        ("publish_tx", [0xA6], [], "unkeyed (0)"),
        # Incoming xApp control. K5 is stamped by the C++ dApps and K1/K2 by the
        # Python one, so a row carries one pair or the other, and the total
        # column fills only for the Python dApp.
        ("control", [0xA0, 0xA5, 0xA1, 0xA2], [], "request_id (== E3 message_id)"),
        ("report",  [0xA8, 0xA9], [], "process-wide report counter"),
    ]),
    ("flexric", [
        # Four legs, one per process/direction. The xApp and agent number
        # independently, so control-out pairs with agent-in by time, not seq.
        ("xapp_ctrl", [0xB0, 0xB1, 0xB2, 0xB3], [], "xApp control counter"),
        ("agent_ctrl", [0xB4, 0xB5, 0xB6, 0xB7], [], "agent inbound counter"),
        ("agent_ind", [0xB8, 0xB9, 0xBA], [], "agent outbound counter"),
        ("xapp_ind", [0xBB, 0xBC, 0xBD], [], "xApp inbound counter"),
        # XE is stamped on the dispatcher thread, which has neither the receive
        # loop's ring nor its key, so it is a point event paired with XD by time.
        ("xapp_report", [0xBE], [], "xApp dispatcher counter"),
    ]),
    ("examples", [
        # The shipped reference Simple Service Model (examples/sm_simple),
        # not a downstream repo -- its own purpose is to be measured by
        # bench_full_loop_latency.
        ("sm", [0xF0, 0xF1, 0xF2, 0xF3, 0xF4], [], "SM's own indication counter"),
        # bench_full_loop_latency's own minimal dApp handler. Same seq as the
        # indication it decoded (SimpleIndication::data1), which is the "sm"
        # leg's own key -- the two legs join directly on seq, unlike every
        # other cross-component pairing in this table.
        ("bench_dapp", [0xF6, 0xF7, 0xF8], [],
         "decoded indication's business seq (joins directly to examples.sm)"),
    ]),
]

# Hops between two stages of a leg that are not consecutive in its chain (or not
# in the chain at all). Emitted like any other hop column, blank unless both
# endpoints are present on the row.
EXTRA_HOPS = {
    ("libe3", "outbound"): [(0x2F, 0x30)],   # emit API entry -> enqueue
    # P0->P9: the one-shot shm/table bring-up, on the row that paid for it.
    # W4->W5: the emit tail (subscriber array freed, worker bookkeeping).
    ("oai", "iq"): [(0x10, 0x19), (0x24, 0x25)],
    # S6->S7: the sensing emit tail, mirroring W4->W5.
    ("oai", "sensing"): [(0x76, 0x77)],
}

# Columns carrying a foreign leg's seq out of a record's aux field, so a row can
# be joined to the leg that produced it. Each entry is (stage, field, column).
#
# origin_seq is what closes the producer-to-libe3 join: an OAI SM publishes its
# publish sequence with latrec_ctx_set() before emitting, and libe3 stamps it
# into LE0's aux. Join oai.csv's iq/sensing legs to libe3.csv's outbound leg on
# origin_seq == seq to get a record from data generation through to the send.
# It is 0 for PDUs whose producer set no context (acks, setup responses), and is
# only unique within one producer -- the iq and sensing legs each number from 1,
# so disambiguate by ring when a capture carries both.
AUX_COLS = {
    ("libe3", "outbound"): [(0x2F, "aux", "origin_seq")],
    ("oai", "iq"):         [(0x10, "aux2", "rx_slot_seq")],
}


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
    """(component, leg) that owns a stage, for the long-format table."""
    for comp, legs in COMPONENTS:
        for leg, chain, extras, _ in legs:
            if stage in chain or stage in extras:
                return comp, leg
    return "other", ""


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
        for i in range(len(t)):
            s = int(stage[i])
            comp, leg = loc[s]
            f.write(f"{ring[i]},{comp},{leg},{seq[i]},{s},{nm[s]},"
                    f"{cpu[i]},{t[i]},{aux[i]},{aux2[i]}\n")

    # one wide CSV per family
    def sname(s):
        return STAGES.get(s, f"0x{s:02X}")

    seen = {int(s) for s in np.unique(stage)}
    print(f"  {'table':14s} {'leg':9s} {'chain':<32} {'rows':>7} {'complete':>9}")
    for comp, legs in COMPONENTS:
        legs = [(leg, [s for s in ch if s in seen], [e for e in ex if e in seen], mean)
                for leg, ch, ex, mean in legs]
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
            for a, b in EXTRA_HOPS.get((comp, leg), []):
                if a in mine and b in mine:
                    specs.append((f"{sname(a)}__{sname(b)}_us", leg, a, b))
            if len(chain) > 1:
                specs.append((f"{leg}_total_us", leg, chain[0], chain[-1]))
            for s, field, label in AUX_COLS.get((comp, leg), []):
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
                for i in np.flatnonzero(np.isin(stage, list(chainset | extraset))):
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
