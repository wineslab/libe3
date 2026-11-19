#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Fail when Doxygen reports any warning or error, in any file.

    check_docs_warnings.py LOG FRAGMENT

The `docs` CMake target exits 0 whatever Doxygen printed, so the log is the only
place the warnings show up. Every line of it that says `warning:` or `error:`
counts, whether or not Doxygen could give it a file, and whether or not the pull
request touches that file: the tree is meant to stay at zero.

On a failure the whole log goes to the job output and a short summary to the
report fragment, which the CI report folds into its one comment per pull
request. The summary stays small on purpose: a comment holds about 65000
characters.
"""
import argparse
import collections
import os
import re
import sys

SHOWN_FILES = 25
SHOWN_WARNINGS = 20
FINDING = re.compile(r"\b(?:warning|error):", re.IGNORECASE)
LOCATED = re.compile(r"^(?P<path>[^\s:]+):\d+: ")


def findings(raw, root):
    """The lines of a Doxygen log that are a warning or an error, with `root` cut from paths."""
    return [line.replace(root, "") for line in raw.splitlines() if FINDING.search(line)]


def summary(lines):
    """The report fragment for `lines`, a non-empty list of findings."""
    per_file = collections.Counter()
    for line in lines:
        match = LOCATED.match(line)
        per_file[match.group("path") if match and not match.group("path").startswith("<") else "(no file)"] += 1
    fragment = ["### Doxygen warnings", "",
                f"{len(lines)} warning(s), which fail the check. The full list is in the job log and the "
                "`doxygen-warnings` artifact of the `Docs` workflow.", "",
                "| Warnings | File |", "|---:|---|"]
    for path, count in per_file.most_common(SHOWN_FILES):
        fragment.append(f"| {count} | `{path}` |")
    if len(per_file) > SHOWN_FILES:
        fragment += ["", f"…and {len(per_file) - SHOWN_FILES} more file(s)."]
    fragment += ["", f"First {min(SHOWN_WARNINGS, len(lines))}:", "", "```"]
    fragment += [line[:220] for line in lines[:SHOWN_WARNINGS]]
    fragment.append("```")
    return "\n".join(fragment) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("fragment")
    args = parser.parse_args()

    if not os.path.isfile(args.log):
        print(f"::error::Doxygen wrote no warning log at {args.log}, so the build did not run")
        return 1
    with open(args.log, encoding="utf-8", errors="replace") as handle:
        raw = handle.read()
    lines = findings(raw, os.getcwd() + "/")

    if raw.strip():
        print(raw.rstrip("\n"))
    if not lines:
        print("Doxygen reported no warnings.")
        return 0

    with open(args.fragment, "w", encoding="utf-8") as handle:
        handle.write(summary(lines))
    print(f"::error::Doxygen reported {len(lines)} warning(s); the full list is above and in the doxygen-warnings artifact")
    return 1


if __name__ == "__main__":
    sys.exit(main())
