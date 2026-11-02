#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Fail when Doxygen reports a warning in a file the pull request changes.

    check_docs_warnings.py LOG FRAGMENT [--base SHA --head SHA]

The `docs` CMake target exits 0 whatever Doxygen printed, so the log is the only
place the warnings show up. With --base and --head the check is a partial scan:
only warnings in the files that `git diff base..head` lists fail it, and the
rest are reported as information, so a gap in a file the pull request does not
touch cannot block it. Without them every warning counts.

Doxygen gives some warnings (a function whose parameters are not all documented)
no file, only the member name in `<name>`. Those count when a changed C or C++
file declares or defines that member.

On a failure the whole log goes to the job output and a short summary to the
report fragment, which the CI report folds into its one comment per pull
request. The summary stays small on purpose: a comment holds about 65000
characters and a strict run reports over a thousand warnings.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

SHOWN_FILES = 25
SHOWN_WARNINGS = 20
LOCATED = re.compile(r"^(?P<path>[^\s:]+):(?P<line>\d+): warning: (?P<msg>.*)$")
MEMBER = re.compile(r"member (\S+) are not documented")
SOURCES = (".h", ".hpp", ".c", ".cpp", ".i")
NOT_A_TYPE = r"(?!(?:return|else|case|delete|new|throw|goto)\b)"


def changed_files(base, head):
    out = subprocess.run(["git", "diff", "--name-only", "--diff-filter=ACMR", f"{base}..{head}"],
                         capture_output=True, text=True, check=True).stdout
    return {line for line in out.splitlines() if line}


def changed_sources(files):
    texts = []
    for path in sorted(f for f in files if f.endswith(SOURCES)):
        try:
            with open(path, encoding="utf-8", errors="ignore") as handle:
                texts.append(handle.read())
        except OSError:
            continue
    return texts


def owns(texts, qualified):
    """True when a changed C or C++ file declares or defines the member `qualified`.

    A page of documentation that names the function, or a call to it, does not
    make it one of the pull request's own members, so only declarations count: the
    type that owns it, a definition `Owner::name(`, or for a plain function a line
    that starts with a return type and then the name."""
    parts = [part for part in qualified.split("::") if part]
    name = parts[-1]
    owner = parts[-2] if len(parts) > 1 else None
    patterns = []
    if owner:
        patterns.append(r"\b(?:class|struct)\s+%s\b" % re.escape(owner))
        patterns.append(r"\b%s::%s\s*\(" % (re.escape(owner), re.escape(name)))
    else:
        patterns.append(r"(?m)^[ \t]*%s(?:[\w:<>*&]+[ \t]+)+[*&]*%s\s*\(" % (NOT_A_TYPE, re.escape(name)))
    return any(re.search(pattern, text) for pattern in patterns for text in texts)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("fragment")
    parser.add_argument("--base")
    parser.add_argument("--head")
    args = parser.parse_args()

    if not os.path.isfile(args.log):
        print(f"::error::Doxygen wrote no warning log at {args.log}, so the build did not run")
        return 1
    with open(args.log, encoding="utf-8", errors="replace") as handle:
        raw = handle.read()
    lines = [line for line in raw.splitlines() if "warning:" in line]
    root = os.getcwd() + "/"

    scope = None
    if args.base and args.head:
        try:
            scope = changed_files(args.base, args.head)
        except (subprocess.CalledProcessError, OSError) as exc:
            print(f"::warning::cannot list the changed files ({exc}); scanning the whole tree")

    sources = changed_sources(scope) if scope is not None else []
    counted, elsewhere = [], []
    for line in lines:
        text = line.replace(root, "")
        match = LOCATED.match(text)
        if scope is None:
            counted.append((match.group("path") if match else None, text))
        elif match and not match.group("path").startswith("<"):
            (counted if match.group("path") in scope else elsewhere).append((match.group("path"), text))
        else:
            member = MEMBER.search(text)
            owned = member is None or owns(sources, member.group(1))
            (counted if owned else elsewhere).append((None, text))

    mode = "the files this pull request changes" if scope is not None else "the whole tree"
    if raw.strip():
        print(raw.rstrip("\n"))
    if not counted and not elsewhere:
        print("Doxygen reported no warnings.")
        return 0

    per_file = collections.Counter(path for path, _ in counted if path)
    unplaced = sum(1 for path, _ in counted if not path)
    total, fails = len(lines), len(counted)
    lead = (f"{fails} warning(s) in {mode}, which fail the check; {len(elsewhere)} more elsewhere, which do not."
            if scope is not None else f"{fails} warning(s) in {mode}, which fail the check.")
    fragment = ["### Doxygen warnings", "", lead + " The full list is in the job log and the `doxygen-warnings` artifact of the `Docs` workflow.", ""]
    if fails:
        fragment += ["| Warnings | File |", "|---:|---|"]
        for path, count in per_file.most_common(SHOWN_FILES):
            fragment.append(f"| {count} | `{path}` |")
        if unplaced:
            fragment.append(f"| {unplaced} | (no file, a member of a changed file) |")
        if len(per_file) > SHOWN_FILES:
            fragment += ["", f"…and {len(per_file) - SHOWN_FILES} more file(s)."]
        fragment += ["", f"First {min(SHOWN_WARNINGS, fails)}:", "", "```"]
        fragment += [text[:220] for _, text in counted[:SHOWN_WARNINGS]]
        fragment.append("```")
    else:
        elsewhere_files = collections.Counter(path for path, _ in elsewhere if path)
        fragment += ["Gaps in files this pull request does not change, by file:", "", "| Warnings | File |", "|---:|---|"]
        for path, count in elsewhere_files.most_common(10):
            fragment.append(f"| {count} | `{path}` |")
    with open(args.fragment, "w", encoding="utf-8") as handle:
        handle.write("\n".join(fragment) + "\n")

    print(f"{fails} of {total} warning(s) are in {mode}.")
    if fails:
        print(f"::error::Doxygen reported {fails} warning(s) in {mode}; the full list is above and in the doxygen-warnings artifact")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
