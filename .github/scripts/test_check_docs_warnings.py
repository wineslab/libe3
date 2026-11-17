#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Checks that check_docs_warnings.py fails on any Doxygen warning.

Run: python3 -m unittest discover -s .github/scripts
"""
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "check_docs_warnings.py")


def check(log):
    """Run the checker on `log`; return its exit code and the report fragment, if one was written."""
    with tempfile.TemporaryDirectory() as tmp:
        log_path = os.path.join(tmp, "doxygen_warnings.txt")
        fragment = os.path.join(tmp, "fragment.md")
        if log is not None:
            with open(log_path, "w", encoding="utf-8") as handle:
                handle.write(log)
        run = subprocess.run([sys.executable, SCRIPT, log_path, fragment], cwd=tmp, capture_output=True, text=True)
        text = None
        if os.path.exists(fragment):
            with open(fragment, encoding="utf-8") as handle:
                text = handle.read()
        return run.returncode, text


class EveryWarningFails(unittest.TestCase):
    def test_an_empty_log_passes(self):
        self.assertEqual(check("")[0], 0)

    def test_a_warning_with_a_file_fails(self):
        code, fragment = check("include/libe3/types.hpp:12: warning: Member x (variable) is not documented.\n")
        self.assertEqual(code, 1)
        self.assertIn("`include/libe3/types.hpp`", fragment)

    def test_a_warning_without_a_file_fails(self):
        code, fragment = check("<~MpmcQueue>:1: warning: parameters of member libe3::MpmcQueue::~MpmcQueue are not documented\n")
        self.assertEqual(code, 1)
        self.assertIn("(no file)", fragment)

    def test_an_error_fails(self):
        self.assertEqual(check("docs/a.md:3: error: Found unknown command '\\foo'\n")[0], 1)

    def test_a_missing_log_fails(self):
        self.assertEqual(check(None)[0], 1)

    def test_the_summary_counts_each_file(self):
        log = "".join(f"src/a.cpp:{n}: warning: gap\n" for n in range(3)) + "src/b.cpp:1: warning: gap\n"
        code, fragment = check(log)
        self.assertEqual(code, 1)
        self.assertIn("| 3 | `src/a.cpp` |", fragment)
        self.assertIn("| 1 | `src/b.cpp` |", fragment)


if __name__ == "__main__":
    unittest.main()
