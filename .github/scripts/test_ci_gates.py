#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks on the order of the CI stages in ci.yml, and on which stages a change starts.

Run: python3 -m unittest discover -s .github/scripts
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
WORKFLOWS = os.path.join(HERE, "..", "workflows")
sys.path.insert(0, HERE)
import workflow_triggers as wt  # noqa: E402

# The order the CI is meant to have: each stage and the stages it waits for.
GATES = {
    "commit-policy": set(),
    "unit-tests": {"commit-policy"},
    "wheels": {"unit-tests"},
    "latrec-portability": {"unit-tests"},
    "mpmc-bench": {"unit-tests"},
    "e2e-dapp": {"wheels", "latrec-portability", "mpmc-bench"},
    "e2e-topologies": {"wheels", "latrec-portability", "mpmc-bench"},
    "full-loop-bench": {"wheels", "latrec-portability", "mpmc-bench"},
}
FIRST_STAGE_AFTER_THE_GATE = ("e2e-dapp", "e2e-topologies", "full-loop-bench")
SECOND_STAGE = ("wheels", "latrec-portability", "mpmc-bench")


def jobs() -> dict[str, dict]:
    """Each job of ci.yml: its `needs`, its `uses` and its `if`."""
    with open(os.path.join(WORKFLOWS, "ci.yml"), encoding="utf-8") as handle:
        text = handle.read()
    body = text.split("\njobs:\n", 1)[1]
    found = {}
    for match in re.finditer(r"^  ([\w-]+):\n((?:    .*\n|\n)+)", body, re.M):
        block = match.group(2)
        needs = re.search(r"^    needs:\s*\[(.*?)\]", block, re.M)
        uses = re.search(r"^    uses:\s*(\S+)", block, re.M)
        cond = re.search(r"^    if:\s*(.+)$", block, re.M)
        found[match.group(1)] = {
            "needs": {n.strip() for n in needs.group(1).split(",")} if needs else set(),
            "uses": uses.group(1) if uses else None,
            "if": cond.group(1) if cond else "",
        }
    return found


def ancestors(graph: dict[str, dict], name: str) -> set[str]:
    seen: set[str] = set()
    todo = list(graph[name]["needs"])
    while todo:
        current = todo.pop()
        if current not in seen:
            seen.add(current)
            todo += list(graph[current]["needs"])
    return seen


class Gates(unittest.TestCase):
    def test_every_stage_waits_for_the_stages_before_it(self):
        graph = jobs()
        for stage, before in GATES.items():
            self.assertTrue(before <= graph[stage]["needs"], f"{stage} must wait for {sorted(before)}")

    def test_no_stage_runs_before_the_commit_policy(self):
        graph = jobs()
        for stage in GATES:
            if stage != "commit-policy":
                self.assertIn("commit-policy", ancestors(graph, stage), stage)

    def test_the_last_stage_waits_for_all_of_the_second(self):
        graph = jobs()
        for stage in FIRST_STAGE_AFTER_THE_GATE:
            self.assertTrue(set(SECOND_STAGE) <= ancestors(graph, stage), stage)

    def test_a_stage_that_may_be_skipped_does_not_block_the_next(self):
        graph = jobs()
        for stage in SECOND_STAGE + FIRST_STAGE_AFTER_THE_GATE:
            condition = graph[stage]["if"]
            self.assertIn("!failure()", condition, stage)
            self.assertIn("!cancelled()", condition, stage)

    def test_every_stage_calls_a_workflow_that_can_be_called(self):
        graph = jobs()
        for stage in GATES:
            uses = graph[stage]["uses"]
            self.assertIsNotNone(uses, f"{stage} does not call a workflow")
            path = os.path.join(WORKFLOWS, os.path.basename(uses))
            with open(path, encoding="utf-8") as handle:
                self.assertRegex(handle.read(), r"(?m)^  workflow_call:", f"{uses} lacks workflow_call")

    def test_no_stage_starts_on_its_own_for_a_pull_request(self):
        stage_files = {os.path.basename(job["uses"]) for job in jobs().values() if job["uses"]}
        started = {t.file for t in wt.load(WORKFLOWS) if t.on_pull_request}
        self.assertEqual(stage_files & started, set())


def run_detector(files: dict[str, str]) -> dict[str, str]:
    """Commit `files` on top of an empty repository and ask detect_changes.sh what they start."""
    repo = tempfile.mkdtemp()
    git = ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", "-C", repo]
    subprocess.run(git + ["init", "-q"], check=True)
    with open(os.path.join(repo, "seed"), "w", encoding="utf-8") as handle:
        handle.write("x")
    subprocess.run(git + ["add", "-A"], check=True)
    subprocess.run(git + ["commit", "-q", "-m", "base"], check=True)
    base = subprocess.run(git + ["rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    for name, text in files.items():
        os.makedirs(os.path.dirname(os.path.join(repo, name)) or repo, exist_ok=True)
        with open(os.path.join(repo, name), "w", encoding="utf-8") as handle:
            handle.write(text)
    subprocess.run(git + ["add", "-A"], check=True)
    subprocess.run(git + ["commit", "-q", "-m", "change"], check=True)
    head = subprocess.run(git + ["rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    out = subprocess.run(["bash", os.path.join(HERE, "detect_changes.sh"), base, head],
                         cwd=repo, check=True, capture_output=True, text=True).stdout
    return dict(line.split("=") for line in out.split())


NONE = {"code": "false", "wheels": "false", "latrec": "false", "mpmc": "false"}


class WhatAChangeStarts(unittest.TestCase):
    def expect(self, files, **changed):
        self.assertEqual(run_detector({f: "x\n" for f in files}), {**NONE, **changed}, files)

    def test_documentation_starts_nothing(self):
        self.expect(["docs/interop.md", "README.md"])

    def test_the_queue_starts_its_benchmark_only(self):
        self.expect(["include/libe3/mpmc_queue.hpp"], code="true", wheels="true", mpmc="true")

    def test_the_benchmark_source_starts_its_benchmark_only(self):
        self.expect(["tests/bench_mpmc_queue.cpp"], code="true", mpmc="true")

    def test_latrec_starts_its_portability_check_only(self):
        self.expect(["src/core/latrec.c"], code="true", wheels="true", latrec="true")
        self.expect(["include/libe3/latrec.h"], code="true", wheels="true", latrec="true")

    def test_other_code_starts_neither(self):
        self.expect(["src/core/e3_agent.cpp"], code="true", wheels="true")
        self.expect(["tests/test_types.cpp"], code="true")

    def test_a_change_to_the_ci_starts_every_stage(self):
        self.expect(["README.md", ".github/workflows/docs.yml"], code="true", wheels="true",
                    latrec="true", mpmc="true")
        self.expect([".github/scripts/ci_report.py"], code="true", wheels="true", latrec="true", mpmc="true")

    def test_other_files_under_github_start_nothing(self):
        self.expect([".github/ISSUE_TEMPLATE/interop.yml", ".github/CODEOWNERS"])


if __name__ == "__main__":
    unittest.main()
