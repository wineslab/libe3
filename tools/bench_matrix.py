#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Run the full-loop latency benchmark over links, transports and encodings, and compare them.

Each cell (link variant, transport, encoding) is run --rounds times. The rounds are interleaved
(round 1 of every cell, then round 2 of every cell, ...), so a drift of the machine (heat, other
tenants) hits every cell alike instead of the one that happened to run last. Each run prints the
18-phase table of test_bench_full_loop_latency; this script keeps, per run, the p50 and p99 of
the round trip, the p50 of the send calls (phases 5 and 14), of the two wire hops (6 and 15) and
of the two receive decodes, and the CPU the process spent per round trip.

A cell's number is the median over its rounds, with the range of the rounds beside it. The
fastest link of a transport is named only when its slowest round beats the best round of every
other link; otherwise the table says the leaders are within the run-to-run noise.

    tools/bench_matrix.py --bench build/test_bench_full_loop_latency \\
        --links zmq,posix,uring,uring-busy --transports ipc,tcp,sctp --rounds 5 \\
        --out-md comparison.md --out-csv runs.csv

The link variants are the io_uring link with ring options set through LIBE3_URING_* (see
docs/connectors/uring.md): uring (the defaults), uring-plain (no registered files), uring-fixedbuf
(registered buffers), uring-sqpoll (a kernel submission thread), uring-busy (busy polling for up
to --busy-us), uring-sqpoll-busy (both).
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import statistics
import subprocess
import sys
import time

VARIANTS = {
    "zmq": ("zmq", {}),
    "posix": ("posix", {}),
    "uring": ("uring", {}),
    "uring-plain": ("uring", {"LIBE3_URING_FIXED_FILES": "0", "LIBE3_URING_FIXED_BUFFERS": "0"}),
    "uring-fixedbuf": ("uring", {"LIBE3_URING_FIXED_BUFFERS": "1"}),
    "uring-sqpoll": ("uring", {"LIBE3_URING_SQPOLL": "1"}),
    "uring-busy": ("uring", {"LIBE3_URING_BUSY_POLL_US": "{busy}"}),
    "uring-sqpoll-busy": ("uring", {"LIBE3_URING_SQPOLL": "1", "LIBE3_URING_BUSY_POLL_US": "{busy}"}),
}

ROW = re.compile(r"^\|\s*(\w+)\s*\|.*\|\s*(-?[\d.]+)\s*\|\s*(-?[\d.]+)\s*\|\s*(-?[\d.]+)\s*\|\s*(-?[\d.]+)\s*\|\s*$")
CPU = re.compile(r"CPU per round trip, both ends: ([\d.]+) us user, ([\d.]+) us system; "
                 r"context switches per round trip: ([\d.]+) voluntary, ([\d.]+) involuntary")
SAMPLES = re.compile(r"\(N=(\d+) after")

# (column, table row, statistic): statistic is the column index of mean/p50/p99/max.
METRICS = [
    ("rt_p50", "Total", 1),
    ("rt_p99", "Total", 2),
    ("send_ind_p50", "5", 1),
    ("wire_ind_p50", "6", 1),
    ("send_ctrl_p50", "14", 1),
    ("wire_ctrl_p50", "15", 1),
]


def parse(output: str) -> dict[str, float] | None:
    rows = {}
    for line in output.splitlines():
        m = ROW.match(line)
        if m:
            rows[m.group(1)] = [float(m.group(i)) for i in range(2, 6)]
    if "Total" not in rows:
        return None
    out = {name: rows[row][stat] for name, row, stat in METRICS if row in rows}
    m = CPU.search(output)
    if m:
        out["cpu_us"] = float(m.group(1)) + float(m.group(2))
        out["ctxsw"] = float(m.group(3)) + float(m.group(4))
    m = SAMPLES.search(output)
    out["samples"] = float(m.group(1)) if m else 0.0
    return out


def run_one(args, variant: str, transport: str, encoding: str) -> tuple[dict | None, str]:
    link, extra = VARIANTS[variant]
    env = {k: v for k, v in os.environ.items() if not k.startswith("LIBE3_URING_")}
    env.update({k: v.format(busy=args.busy_us) for k, v in extra.items()})
    cmd = [args.bench, "--link", link, "--transport", transport, "--encoding", encoding,
           "--iterations", str(args.iterations), "--warmup", str(args.warmup)]
    if args.ran_cpu is not None:
        cmd += ["--ran-cpu", str(args.ran_cpu)]
    if args.dapp_cpu is not None:
        cmd += ["--dapp-cpu", str(args.dapp_cpu)]
    try:
        proc = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=args.timeout)
    except subprocess.TimeoutExpired:
        return None, "timeout"
    result = parse(proc.stdout)
    if proc.returncode != 0 or result is None:
        tail = (proc.stdout + proc.stderr).strip().splitlines()[-4:]
        return None, f"exit {proc.returncode}: " + " / ".join(tail)
    return result, ""


def median(values: list[float]) -> float:
    return statistics.median(values) if values else float("nan")


def fmt(values: list[float]) -> str:
    if not values:
        return "-"
    mid = median(values)
    if len(values) == 1:
        return f"{mid:.1f}"
    return f"{mid:.1f} [{min(values):.1f} to {max(values):.1f}]"


def compare(cells: dict, links: list[str], transports: list[str], encodings: list[str], args) -> str:
    lines = []
    lines.append("## Connector comparison: full-loop round trip\n")
    lines.append(f"Same machine, {args.rounds} interleaved round(s) per cell, {args.iterations} round trips "
                 f"per run after {args.warmup} of warm-up. Each value is the median over the rounds of "
                 "that round's statistic, with the range of the rounds in brackets. Microseconds. "
                 "CPU is user plus system time of the process (both ends) per round trip.\n")
    header = ("| Transport | Encoding | Link | Round trip p50 | Round trip p99 | Wire RAN to dApp p50 "
              "| Wire dApp to RAN p50 | Send indication p50 | Send control p50 | CPU per round trip | Runs |")
    lines.append(header)
    lines.append("|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    verdicts = []
    for transport in transports:
        for encoding in encodings:
            ranked = []
            for link in links:
                runs = cells.get((link, transport, encoding))
                if runs is None:
                    continue  # not run (ZeroMQ over SCTP)
                ok = runs["ok"]
                col = lambda name: [r[name] for r in ok if name in r]  # noqa: E731
                if ok:
                    ranked.append((median(col("rt_p50")), link, col("rt_p50")))
                failed = f" ({len(runs['failed'])} failed)" if runs["failed"] else ""
                lines.append(
                    f"| {transport} | {encoding} | {link} | {fmt(col('rt_p50'))} | {fmt(col('rt_p99'))} "
                    f"| {fmt(col('wire_ind_p50'))} | {fmt(col('wire_ctrl_p50'))} | {fmt(col('send_ind_p50'))} "
                    f"| {fmt(col('send_ctrl_p50'))} | {fmt(col('cpu_us'))} | {len(ok)}{failed} |")
            if not ranked:
                continue
            ranked.sort()
            best_mid, best_link, best_runs = ranked[0]
            text = f"- **{transport}/{encoding}**: fastest is **{best_link}** ({best_mid:.1f} us)"
            others = ranked[1:]
            if others:
                clear = all(max(best_runs) < min(runs) for _, _, runs in others)
                second_mid, second_link, _ = others[0]
                text += f", then {second_link} ({second_mid:.1f} us, {100 * (second_mid - best_mid) / second_mid:.0f}% slower)"
                if not clear:
                    text += "; the leaders overlap round to round, so the order of the first two is within the noise"
                posix = [(m, r) for m, link, r in ranked if link == "posix"]
                if posix and best_link != "posix":
                    p_mid = posix[0][0]
                    text += f". Against posix ({p_mid:.1f} us): {100 * (p_mid - best_mid) / p_mid:.0f}% less"
            verdicts.append(text + ".")
    lines.append("")
    lines.append("### Fastest link per transport\n")
    lines.extend(verdicts)
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bench", required=True, help="path of test_bench_full_loop_latency")
    parser.add_argument("--links", default="zmq,posix,uring", help=f"comma list of {', '.join(VARIANTS)}")
    parser.add_argument("--transports", default="ipc,tcp")
    parser.add_argument("--encodings", default="asn1")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--warmup", type=int, default=50)
    parser.add_argument("--busy-us", type=int, default=1000, help="busy-poll budget of the uring-*busy variants")
    parser.add_argument("--ran-cpu", type=int)
    parser.add_argument("--dapp-cpu", type=int)
    parser.add_argument("--timeout", type=int, default=300, help="seconds one run may take")
    parser.add_argument("--out-md", help="write the comparison here as well as to stdout")
    parser.add_argument("--out-csv", help="write every run here")
    args = parser.parse_args()

    links = [x for x in args.links.split(",") if x]
    for link in links:
        if link not in VARIANTS:
            parser.error(f"unknown link variant {link}")
    transports = [x for x in args.transports.split(",") if x]
    encodings = [x for x in args.encodings.split(",") if x]

    cells: dict = {}
    rows = []
    # ZeroMQ has no SCTP transport.
    order = [(l, t, e) for t in transports for e in encodings for l in links
             if not (VARIANTS[l][0] == "zmq" and t == "sctp")]
    for rnd in range(args.rounds):
        # Rotate the order each round so no cell always runs right after the same neighbour.
        shift = rnd % len(order)
        for link, transport, encoding in order[shift:] + order[:shift]:
            started = time.time()
            result, error = run_one(args, link, transport, encoding)
            runs = cells.setdefault((link, transport, encoding), {"ok": [], "failed": []})
            if result is None:
                runs["failed"].append(error)
                print(f"round {rnd + 1} {link}/{transport}/{encoding}: FAILED {error}", file=sys.stderr)
                continue
            runs["ok"].append(result)
            rows.append({"round": rnd + 1, "link": link, "transport": transport, "encoding": encoding,
                         **{k: f"{v:.3f}" for k, v in result.items()}})
            print(f"round {rnd + 1} {link}/{transport}/{encoding}: round trip p50 {result['rt_p50']:.1f} us "
                  f"({time.time() - started:.1f} s)", file=sys.stderr)

    report = compare(cells, links, transports, encodings, args)
    print(report)
    if args.out_md:
        with open(args.out_md, "w", encoding="utf-8") as handle:
            handle.write(report)
    if args.out_csv and rows:
        fields = sorted({k for r in rows for k in r}, key=lambda k: (k not in ("round", "link", "transport", "encoding"), k))
        with open(args.out_csv, "w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
    failed = sum(len(r["failed"]) for r in cells.values())
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
