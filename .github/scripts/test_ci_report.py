#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks on what makes `CI report` fail: the cluster stage must never read as green by skipping.

Run: python3 -m unittest discover -s .github/scripts
"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ci_report as cr  # noqa: E402


def run(file, conclusion, name=None):
    return {"name": name or file, "path": f".github/workflows/{file}", "status": "completed",
            "conclusion": conclusion}


class Problems(unittest.TestCase):
    def test_a_skipped_cluster_stage_fails_the_report(self):
        problems = cr.find_problems([run("cluster-ci.yml", "skipped", "Cluster stage")], [], True)
        self.assertEqual(len(problems), 1)
        self.assertIn("maintainer", problems[0])

    def test_a_skipped_workflow_that_is_not_mandatory_is_fine(self):
        self.assertEqual(cr.find_problems([run("docs.yml", "skipped")], [], True), [])

    def test_a_successful_cluster_stage_is_fine(self):
        self.assertEqual(cr.find_problems([run("cluster-ci.yml", "success")], [], True), [])

    def test_a_failed_cluster_stage_fails_the_report(self):
        self.assertTrue(cr.find_problems([run("cluster-ci.yml", "failure")], [], True))


if __name__ == "__main__":
    unittest.main()
