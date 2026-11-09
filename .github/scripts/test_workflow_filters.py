#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks on this repository's own workflow filters.

They guard what the filters are for: a PR that changes no code must not start the
build and test workflows, and a PR that changes code must start all of them.
Run: python3 -m unittest discover -s .github/scripts
"""
import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
WORKFLOWS = os.path.join(HERE, "..", "workflows")
sys.path.insert(0, HERE)
import workflow_triggers as wt  # noqa: E402

# The workflows that build or test and so start only when a changed file is code.
CODE_WORKFLOWS = ("pr-tests.yml", "e2e-dapp-test.yml", "e2e-topologies.yml", "full-loop-bench.yml",
                  "cluster-ci.yml")
# The pull request workflows that start for every PR. A new unfiltered workflow
# has to be added here on purpose, because it will run on documentation PRs too.
ALWAYS_ON = ("ci-report.yml", "commit-trailers.yml", "docs.yml")

NOT_CODE = (
    "README.md", "docs/interop.md", "docs/a/b/c.md", "CONTRIBUTING.md", "spec/E3AP.docx",
    "tools/oran_docx/gen.py", "tools/e3sm_l1kpm_v2/build.py", "LICENSE", "NOTICE",
    "CONTRIBUTORS", "CITATION.cff", "REUSE.toml", ".gitignore",
    ".github/workflows/docs.yml", ".github/scripts/ci_report.py", ".github/ISSUE_TEMPLATE/interop.yml",
)
CODE = (
    "src/core/e3_agent.cpp", "include/libe3/types.hpp", "tests/test_types.cpp", "CMakeLists.txt",
    "build_libe3", "pyproject.toml", "swig/e3_dapp_session.hpp", "examples/simple/a.cpp",
    "messages/asn1/V1/e3ap-1.0.0.asn1", "cmake/libe3SWIG.cmake", "tools/other/x.py",
    "scripts/create_deb.sh", "README.rst", "VERSION", "docs.cpp", "notes.mdx",
)


def triggers() -> dict[str, wt.Trigger]:
    return {t.file: t for t in wt.load(WORKFLOWS)}


def starting(files: list[str]) -> set[str]:
    should, _ = wt.expected(WORKFLOWS, files, exclude="")
    return {t.file for t in should}


class CodeFilters(unittest.TestCase):
    def test_the_filters_are_identical_apart_from_the_workflow_itself(self):
        found = triggers()
        shared = None
        for file in CODE_WORKFLOWS:
            paths = found[file].paths
            self.assertIsNotNone(paths, f"{file} has no `paths` filter")
            self.assertEqual(paths[-1], f".github/workflows/{file}",
                             f"{file}: the last pattern must bring back the workflow's own file")
            shared = shared or paths[:-1]
            self.assertEqual(paths[:-1], shared, f"{file}: filter differs from the others")

    def test_no_code_changes_start_none_of_the_code_workflows(self):
        for path in NOT_CODE:
            self.assertEqual(starting([path]) & set(CODE_WORKFLOWS), set(), path)

    def test_any_code_change_starts_all_of_them(self):
        for path in CODE:
            self.assertEqual(starting([path]) & set(CODE_WORKFLOWS), set(CODE_WORKFLOWS), path)

    def test_a_mixed_pr_starts_them(self):
        self.assertEqual(starting(["docs/a.md", "src/a.cpp"]) & set(CODE_WORKFLOWS), set(CODE_WORKFLOWS))

    def test_editing_a_workflow_starts_only_that_one_of_them(self):
        for file in CODE_WORKFLOWS:
            self.assertEqual(starting([f".github/workflows/{file}"]) & set(CODE_WORKFLOWS), {file})

    def test_the_filters_agree_with_detect_code_changes(self):
        """The script still gates the per-commit build; it must call the same paths code."""
        with open(os.path.join(HERE, "detect_code_changes.sh"), encoding="utf-8") as handle:
            match = re.search(r"^noncode='(.*)'$", handle.read(), re.M)
        self.assertIsNotNone(match, "noncode= line not found in detect_code_changes.sh")
        noncode = re.compile(match.group(1))
        for path in NOT_CODE + CODE:
            if path.startswith(".github/workflows/"):
                continue  # the filters bring a workflow's own file back on purpose
            script_says_code = not noncode.search(path)
            filters_say_code = bool(starting([path]) & set(CODE_WORKFLOWS))
            self.assertEqual(filters_say_code, script_says_code, path)


class WhatStartsForDocs(unittest.TestCase):
    def test_a_docs_only_pr_starts_only_the_always_on_workflows(self):
        self.assertEqual(starting(["docs/interop.md"]), set(ALWAYS_ON))

    def test_only_the_expected_workflows_are_unfiltered(self):
        unfiltered = {f for f, t in triggers().items()
                      if t.on_pull_request and t.paths is None and t.paths_ignore is None}
        self.assertEqual(unfiltered, set(ALWAYS_ON),
                         "an unfiltered pull_request workflow starts on every PR, docs-only ones too; "
                         "give it a `paths` filter or add it to ALWAYS_ON")


if __name__ == "__main__":
    unittest.main()
