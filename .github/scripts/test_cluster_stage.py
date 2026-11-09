#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks on cluster_stage.py: the stage never reads as passed unless a run reported.

Run: python3 -m unittest discover -s .github/scripts
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cluster_stage as cs  # noqa: E402

BENCH = """posix/tcp/asn1,1104
6,11.68,11.60,21.71,25.64
15,13.19,13.30,21.32,25.02
Total,65.78,64.47,94.56,137.85
posix/tcp/json,1043
6,12.00,11.90,22.00,26.00
Total,70.00,69.00,99.00,140.00
exit=0
"""


def run_with(status, results=None, reason="Succeeded", message=""):
    return {"status": {"conditions": [{"status": status, "reason": reason, "message": message}],
                       "results": [{"name": k, "value": v} for k, v in (results or {}).items()]}}


def all_results(e2e, bench="exit=0\n"):
    found = {"e2e": e2e}
    found.update({f"bench-{g}": bench for g in cs.BENCH_GROUPS})
    return found


class Settings(unittest.TestCase):
    def test_each_missing_value_is_named(self):
        full = {"OPENSHIFT_SA_TOKEN": "t\n", "CLUSTER_API_URL": "https://x:6443/", "TEKTON_NAMESPACE": "ns"}
        self.assertEqual(cs.settings(full), ("t", "https://x:6443", "ns"))
        for name in full:
            partial = {k: v for k, v in full.items() if k != name}
            with self.assertRaises(cs.StageError) as caught:
                cs.settings(partial)
            self.assertIn(name, str(caught.exception))

    def test_a_blank_value_counts_as_missing(self):
        with self.assertRaises(cs.StageError):
            cs.settings({"OPENSHIFT_SA_TOKEN": " \n", "CLUSTER_API_URL": "u", "TEKTON_NAMESPACE": "n"})


class Manifest(unittest.TestCase):
    def keys(self, template):
        affinity = template["podTemplate"]["affinity"]["nodeAffinity"]
        terms = affinity["requiredDuringSchedulingIgnoredDuringExecution"]["nodeSelectorTerms"][0]
        return {e["key"]: e.get("values") for e in terms["matchExpressions"]}

    def test_pinned_runs_keep_the_build_off_the_node(self):
        run = cs.render_run("b", "t", "node1")
        self.assertEqual(set(self.keys(run["spec"]["taskRunTemplate"])),
                         {"kubernetes.io/arch", "node-role.kubernetes.io/master"})

    def test_pinned_runs_put_the_tests_and_the_benchmark_on_the_node(self):
        run = cs.render_run("b", "t", "node1")
        specs = {s["pipelineTaskName"]: s for s in run["spec"]["taskRunSpecs"]}
        self.assertEqual(set(specs), {"e2e", "bench-zmq-ipc", "bench-zmq-tcp", "bench-posix-ipc",
                                      "bench-posix-tcp", "bench-posix-sctp"})
        for spec in specs.values():
            self.assertEqual(self.keys(spec), {"kubernetes.io/hostname": ["node1"]})

    def test_unpinned_runs_stay_on_amd64_workers(self):
        run = cs.render_run("b", "t")
        self.assertNotIn("taskRunSpecs", run["spec"])
        self.assertEqual(set(self.keys(run["spec"]["taskRunTemplate"])),
                         {"kubernetes.io/arch", "node-role.kubernetes.io/master"})

    def test_branch_and_tag_reach_the_pipeline(self):
        params = {p["name"]: p["value"] for p in cs.render_run("feat/x", "pr-7")["spec"]["params"]}
        self.assertEqual(params, {"GIT_REPO_BRANCH": "feat/x", "IMAGE_TAG": "pr-7"})


class Judging(unittest.TestCase):
    def test_states(self):
        self.assertEqual(cs.state(run_with("True"))[0], "succeeded")
        self.assertEqual(cs.state(run_with("False", reason="Failed"))[0], "failed")
        self.assertEqual(cs.state(run_with("Unknown", reason="Running"))[0], "running")
        self.assertEqual(cs.state({})[0], "running")

    def test_a_clean_run_has_no_problem(self):
        lines, problems = cs.judge(run_with("True", all_results("PASS zmq/ipc/asn1: 3 indications\nexit=0\n", BENCH)), "r", "")
        self.assertEqual(problems, [])
        self.assertIn("posix/tcp/asn1", "\n".join(lines))

    def test_a_failed_cell_is_a_problem_even_when_the_pipeline_succeeded(self):
        e2e = "PASS zmq/ipc/asn1: 3 indications\nFAIL posix/sctp/asn1: no indication reached the dApp\nexit=1\n"
        _, problems = cs.judge(run_with("True", all_results(e2e)), "r", "")
        self.assertEqual(problems, ["an end-to-end cell failed"])

    def test_a_missing_exit_line_is_a_problem(self):
        _, problems = cs.judge(run_with("True", all_results("PASS a: 1 indications\n")), "r", "")
        self.assertIn("an end-to-end cell failed", problems)

    def test_a_failed_benchmark_group_is_named(self):
        found = all_results("exit=0\n")
        found["bench-posix-sctp"] = "exit=1\n"
        _, problems = cs.judge(run_with("True", found), "r", "")
        self.assertEqual(problems, ["the benchmark for posix-sctp failed"])

    def test_a_failed_pipeline_is_a_problem_with_its_reason(self):
        _, problems = cs.judge(run_with("False", reason="Failed", message="build failed"), "r", "")
        self.assertEqual(len(problems), 1)
        self.assertIn("build failed", problems[0])

    def test_a_pipeline_with_no_results_is_a_problem(self):
        _, problems = cs.judge(run_with("True"), "r", "")
        self.assertTrue(problems)


class BenchParsing(unittest.TestCase):
    def test_cells_and_rows(self):
        cells = cs.parse_bench(BENCH)
        self.assertEqual([c["cell"] for c in cells], ["posix/tcp/asn1", "posix/tcp/json"])
        self.assertEqual(cells[0]["samples"], "1104")
        self.assertEqual(cells[0]["rows"]["6"], ("11.68", "11.60", "21.71", "25.64"))
        self.assertEqual(cells[1]["rows"]["Total"][1], "69.00")

    def test_the_table_has_one_line_per_cell(self):
        self.assertEqual(len(cs.bench_table(cs.parse_bench(BENCH))), 4)


class Gate(unittest.TestCase):
    def runs(self, **kinds):
        return [{"id": i, "name": n, "status": s, "conclusion": c}
                for i, (n, (s, c)) in enumerate(kinds.items(), start=1)]

    def test_waits_while_another_workflow_runs(self):
        runs = self.runs(a=("completed", "success"), b=("in_progress", None))
        self.assertEqual(cs.gate_verdict(runs, "99", ()), (False, []))

    def test_a_failure_is_named(self):
        runs = self.runs(a=("completed", "success"), b=("completed", "failure"))
        self.assertEqual(cs.gate_verdict(runs, "99", ()), (True, ["b"]))

    def test_the_stage_and_the_report_are_not_waited_for(self):
        runs = self.runs(a=("completed", "success"), **{"CI report": ("in_progress", None)})
        self.assertEqual(cs.gate_verdict(runs, "99", ("CI report",)), (True, []))

    def test_its_own_run_is_not_waited_for(self):
        runs = self.runs(a=("completed", "success"), me=("in_progress", None))
        self.assertEqual(cs.gate_verdict(runs, "2", ()), (True, []))


if __name__ == "__main__":
    unittest.main()
