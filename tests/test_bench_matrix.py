#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks of tools/bench_matrix.py on canned benchmark output, without running the benchmark.

    test_bench_matrix.py TOOL

Properties verified:
  - every number the comparison uses is read from the right row and column of the 18-phase table,
    and the CPU line is summed;
  - a wire hop below zero (the receiver was quicker than the sender's own stamp) is read as it is;
  - output without a Total row is a failed run, not a zero;
  - the fastest link is named, its lead over POSIX is given, and two links whose rounds overlap
    are said to be within the noise;
  - a cell that was not run (ZeroMQ over SCTP) has no row.
"""
import importlib.util
import sys
import unittest

TOOL = sys.argv.pop(1) if len(sys.argv) > 1 else "tools/bench_matrix.py"
spec = importlib.util.spec_from_file_location("bench_matrix", TOOL)
bench_matrix = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench_matrix)


def table(total_p50, total_p99, wire6, wire15, send5, send14, cpu_user=40.0, cpu_sys=60.0):
    def row(num, p50, p99=0.0):
        return f"| {num:>5} | x | `A` to `B` |   1.00 | {p50:7.2f} | {p99:7.2f} |   9.00 |"
    lines = ["## Full-loop latency benchmark (N=1000 after 50 warmup)", "",
             "| # | Description | Tags | mean | p50 | p99 | max |", "|---|---|---|---|---|---|---|"]
    for n in range(1, 19):
        value = {5: send5, 6: wire6, 14: send14, 15: wire15}.get(n, 0.5)
        lines.append(row(str(n), value))
    lines.append(row("Total", total_p50, total_p99))
    lines.append("")
    lines.append(f"CPU per round trip, both ends: {cpu_user:.2f} us user, {cpu_sys:.2f} us system; "
                 "context switches per round trip: 5.00 voluntary, 0.00 involuntary.")
    return "\n".join(lines)


class Parse(unittest.TestCase):
    def test_reads_each_number_from_its_row_and_column(self):
        got = bench_matrix.parse(table(100.0, 150.0, 19.0, 20.0, 6.0, 6.5, 40.0, 60.0))
        self.assertEqual(got["rt_p50"], 100.0)
        self.assertEqual(got["rt_p99"], 150.0)
        self.assertEqual(got["wire_ind_p50"], 19.0)
        self.assertEqual(got["wire_ctrl_p50"], 20.0)
        self.assertEqual(got["send_ind_p50"], 6.0)
        self.assertEqual(got["send_ctrl_p50"], 6.5)
        self.assertEqual(got["cpu_us"], 100.0)
        self.assertEqual(got["samples"], 1000.0)

    def test_a_wire_hop_below_zero_is_read_as_it_is(self):
        # A receiver that polls can see the PDU before the sender has stamped its send as done.
        got = bench_matrix.parse(table(40.0, 90.0, -0.45, -1.20, 6.0, 6.5))
        self.assertEqual(got["wire_ind_p50"], -0.45)
        self.assertEqual(got["wire_ctrl_p50"], -1.20)

    def test_output_without_a_total_is_a_failed_run(self):
        self.assertIsNone(bench_matrix.parse("dapp setup failed\n"))


class Compare(unittest.TestCase):
    class Args:
        rounds = 3
        iterations = 1000
        warmup = 50

    def cells(self, values):
        return {key: {"ok": [{"rt_p50": v, "rt_p99": v * 2} for v in runs], "failed": []}
                for key, runs in values.items()}

    def test_names_the_fastest_and_its_lead_over_posix(self):
        cells = self.cells({("posix", "ipc", "asn1"): [100, 101, 102],
                            ("uring-busy", "ipc", "asn1"): [70, 71, 72]})
        text = bench_matrix.compare(cells, ["posix", "uring-busy"], ["ipc"], ["asn1"], self.Args)
        self.assertIn("fastest is **uring-busy** (71.0 us)", text)
        self.assertIn("Against posix (101.0 us): 30% less", text)
        self.assertNotIn("within the noise", text)

    def test_overlapping_rounds_are_within_the_noise(self):
        cells = self.cells({("posix", "ipc", "asn1"): [100, 104, 108],
                            ("uring", "ipc", "asn1"): [99, 103, 109]})
        text = bench_matrix.compare(cells, ["posix", "uring"], ["ipc"], ["asn1"], self.Args)
        self.assertIn("within the noise", text)

    def test_a_cell_that_was_not_run_has_no_row(self):
        cells = self.cells({("posix", "sctp", "asn1"): [100, 100, 100]})
        text = bench_matrix.compare(cells, ["zmq", "posix"], ["sctp"], ["asn1"], self.Args)
        self.assertNotIn("| sctp | asn1 | zmq |", text)
        self.assertIn("| sctp | asn1 | posix |", text)


if __name__ == "__main__":
    unittest.main()
