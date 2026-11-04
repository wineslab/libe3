#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Tests for workflow_triggers.py. Run: python3 -m unittest discover -s .github/scripts"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import workflow_triggers as wt  # noqa: E402


def trigger(text: str, file: str = "x.yml") -> wt.Trigger:
    return wt.parse(file, text)


class Selects(unittest.TestCase):
    def test_positive_list_picks_any_match(self):
        pats = ("src/**", "include/**")
        self.assertTrue(wt.selects(pats, "src/core/a.cpp"))
        self.assertFalse(wt.selects(pats, "docs/a.md"))

    def test_star_does_not_cross_a_slash_but_double_star_does(self):
        self.assertTrue(wt.selects(("src/*.cpp",), "src/a.cpp"))
        self.assertFalse(wt.selects(("src/*.cpp",), "src/core/a.cpp"))
        self.assertTrue(wt.selects(("**.md",), "docs/deep/a.md"))
        self.assertTrue(wt.selects(("**.md",), "README.md"))

    def test_root_file_pattern_matches_only_the_root(self):
        self.assertTrue(wt.selects(("LICENSE",), "LICENSE"))
        self.assertFalse(wt.selects(("LICENSE",), "docs/LICENSE"))

    def test_negation_removes_and_a_later_positive_puts_it_back(self):
        pats = ("**", "!.github/**", ".github/workflows/pr-tests.yml")
        self.assertTrue(wt.selects(pats, "src/a.cpp"))
        self.assertFalse(wt.selects(pats, ".github/workflows/docs.yml"))
        self.assertTrue(wt.selects(pats, ".github/workflows/pr-tests.yml"))

    def test_order_matters(self):
        self.assertFalse(wt.selects((".github/workflows/a.yml", "!.github/**"), ".github/workflows/a.yml"))


class Parse(unittest.TestCase):
    def test_bare_pull_request_is_unfiltered(self):
        t = trigger("name: A\n\non:\n  pull_request:\n  workflow_dispatch:\n\njobs: {}\n")
        self.assertTrue(t.on_pull_request)
        self.assertIsNone(t.paths)
        self.assertEqual(t.name, "A")

    def test_paths_with_quotes_comments_and_negation(self):
        t = trigger("name: B\non:\n  pull_request:\n    paths:\n      - '**'\n"
                    "      - '!docs/**'   # prose\n      - src/a.c\n  push:\n    paths:\n      - other\n")
        self.assertEqual(t.paths, ("**", "!docs/**", "src/a.c"))

    def test_push_paths_are_not_taken_for_pull_request_paths(self):
        t = trigger("name: C\non:\n  push:\n    paths:\n      - a\n  pull_request:\n")
        self.assertTrue(t.on_pull_request)
        self.assertIsNone(t.paths)

    def test_a_workflow_without_a_pull_request_trigger(self):
        t = trigger("name: D\non:\n  push:\n    branches: [main]\n")
        self.assertFalse(t.on_pull_request)

    def test_inline_forms(self):
        self.assertTrue(trigger("name: E\non: pull_request\n").on_pull_request)
        self.assertTrue(trigger("name: F\non: [push, pull_request]\n").on_pull_request)
        self.assertFalse(trigger("name: G\non: push\n").on_pull_request)

    def test_paths_ignore(self):
        t = trigger("name: H\non:\n  pull_request:\n    paths-ignore:\n      - '**.md'\n")
        self.assertEqual(t.paths_ignore, ("**.md",))
        self.assertIsNone(t.paths)

    def test_a_paths_key_with_no_items_is_treated_as_unfiltered(self):
        t = trigger("name: I\non:\n  pull_request:\n    paths:\n")
        self.assertIsNone(t.paths)
        self.assertTrue(wt.starts(t, ["docs/a.md"]))


class Starts(unittest.TestCase):
    def test_unfiltered_always_starts(self):
        self.assertTrue(wt.starts(trigger("name: A\non:\n  pull_request:\n"), ["docs/a.md"]))

    def test_paths_starts_when_any_file_is_selected(self):
        t = trigger("name: A\non:\n  pull_request:\n    paths:\n      - src/**\n")
        self.assertTrue(wt.starts(t, ["docs/a.md", "src/a.c"]))
        self.assertFalse(wt.starts(t, ["docs/a.md"]))

    def test_paths_ignore_starts_unless_every_file_is_ignored(self):
        t = trigger("name: A\non:\n  pull_request:\n    paths-ignore:\n      - docs/**\n")
        self.assertTrue(wt.starts(t, ["docs/a.md", "src/a.c"]))
        self.assertFalse(wt.starts(t, ["docs/a.md", "docs/b.md"]))

    def test_no_files_starts(self):
        t = trigger("name: A\non:\n  pull_request:\n    paths:\n      - src/**\n")
        self.assertTrue(wt.starts(t, []))

    def test_no_pull_request_trigger_never_starts(self):
        self.assertFalse(wt.starts(trigger("name: A\non:\n  push:\n"), ["src/a.c"]))


if __name__ == "__main__":
    unittest.main()
