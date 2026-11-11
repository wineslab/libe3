#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks on which workflows start for a pull request, and on what counts as code.

The build and test stages are called by ci.yml, which skips them when no changed file is
code (test_ci_gates.py); the workflows that start on their own are listed here.
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

# The pull request workflows. Each starts for every PR; the stages of the CI are called by
# ci.yml and do not start on their own (test_ci_gates.py). A new workflow that starts on
# pull_request has to be added here on purpose, because it will run on documentation PRs too.
ALWAYS_ON = ("ci.yml", "ci-report.yml", "docs.yml")

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
    def test_detect_code_changes_agrees_with_the_lists_above(self):
        """The script that says whether a PR changes code; the CI stages and the per-commit build use it."""
        with open(os.path.join(HERE, "detect_code_changes.sh"), encoding="utf-8") as handle:
            match = re.search(r"^noncode='(.*)'$", handle.read(), re.M)
        self.assertIsNotNone(match, "noncode= line not found in detect_code_changes.sh")
        noncode = re.compile(match.group(1))
        for path in NOT_CODE:
            self.assertTrue(noncode.search(path), f"{path} should not count as code")
        for path in CODE:
            self.assertFalse(noncode.search(path), f"{path} should count as code")


class WhatStartsForDocs(unittest.TestCase):
    def test_a_docs_only_pr_starts_only_the_always_on_workflows(self):
        self.assertEqual(starting(["docs/interop.md"]), set(ALWAYS_ON))

    def test_a_code_pr_starts_the_same_ones(self):
        self.assertEqual(starting(["src/core/e3_agent.cpp"]), set(ALWAYS_ON))

    def test_only_the_expected_workflows_are_unfiltered(self):
        unfiltered = {f for f, t in triggers().items()
                      if t.on_pull_request and t.paths is None and t.paths_ignore is None}
        self.assertEqual(unfiltered, set(ALWAYS_ON),
                         "an unfiltered pull_request workflow starts on every PR, docs-only ones too; "
                         "give it a `paths` filter or add it to ALWAYS_ON")


if __name__ == "__main__":
    unittest.main()
