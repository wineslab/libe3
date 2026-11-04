#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Say which workflows GitHub starts for a pull request, from the workflow files.

The CI report needs this to tell "a workflow that a `paths` filter kept from
starting" apart from "a workflow that should have started and did not". It reads
each file's `on.pull_request` block and applies GitHub's own filter rules to the
PR's changed files, so the filters in the workflows are the one definition of
what counts as code.

Only the forms this repository uses are parsed. A trigger that is not understood
counts as unfiltered, so the workflow is expected for every PR: a wrong guess
makes the report wait for a workflow that never starts, which fails loudly and
never lets a PR through untested.

GitHub looks at the first 300 changed files only. A PR larger than that can have
a workflow skipped that this module expects; the report then fails closed.
"""
import os
import re
from dataclasses import dataclass


@dataclass(frozen=True)
class Trigger:
    file: str  # file name under .github/workflows
    name: str  # display name
    on_pull_request: bool
    paths: tuple[str, ...] | None
    paths_ignore: tuple[str, ...] | None


def _unquote(value: str) -> str:
    value = value.strip()
    if value[:1] in ("'", '"') and value.find(value[0], 1) > 0:
        end = value.find(value[0], 1)
        return value[1:end]
    return value.split(" #", 1)[0].strip()


def _indent(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def _top_level_on(lines: list[str]) -> list[str]:
    """The lines under the top-level `on:` key, or its inline value."""
    for i, line in enumerate(lines):
        if re.match(r"^on:", line):
            inline = line.split(":", 1)[1].split(" #", 1)[0].strip()
            if inline:
                return [inline]
            block = []
            for follow in lines[i + 1:]:
                if follow.strip() and not follow.startswith((" ", "#")):
                    break
                block.append(follow)
            return block
    return []


def parse(file: str, text: str) -> Trigger:
    lines = text.splitlines()
    declared = re.search(r"(?m)^name:\s*(.+?)\s*$", text)
    name = _unquote(declared.group(1)) if declared else file
    block = _top_level_on(lines)

    if len(block) == 1 and not block[0].startswith(" "):
        # `on: pull_request` or `on: [push, pull_request]`
        return Trigger(file, name, "pull_request" in block[0], None, None)

    event_indent = None
    in_event = False
    on_pr = False
    keys: dict[str, list[str]] = {}
    current = None
    for line in block:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        ind = _indent(line)
        if event_indent is None:
            event_indent = ind
        if ind == event_indent:
            key = line.strip().split(":", 1)[0]
            in_event = key == "pull_request"
            on_pr = on_pr or in_event
            current = None
        elif in_event:
            stripped = line.strip()
            if stripped.startswith("- "):
                if current is not None:
                    keys[current].append(_unquote(stripped[2:]))
            else:
                key = stripped.split(":", 1)[0]
                current = key if key in ("paths", "paths-ignore") else None
                if current:
                    keys[current] = []

    def listed(key: str) -> tuple[str, ...] | None:
        items = keys.get(key)
        return tuple(items) if items else None

    return Trigger(file, name, on_pr, listed("paths"), listed("paths-ignore"))


def _glob(pattern: str) -> "re.Pattern[str]":
    """GitHub's filter glob: `**` crosses `/`, `*` does not, `?` and `+` apply to the previous character."""
    out = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**", i):
            out.append(".*")
            i += 2
            continue
        char = pattern[i]
        if char == "*":
            out.append("[^/]*")
        elif char in "?+":
            out.append(char)
        elif char == "[" and pattern.find("]", i) > i:
            end = pattern.find("]", i)
            out.append(pattern[i:end + 1])
            i = end + 1
            continue
        else:
            out.append(re.escape(char))
        i += 1
    return re.compile("^" + "".join(out) + "$")


def selects(patterns: tuple[str, ...], path: str) -> bool:
    """Whether the filter picks `path`: the last pattern that matches decides, `!` removes."""
    picked = False
    for pattern in patterns:
        negated = pattern.startswith("!")
        if _glob(pattern[1:] if negated else pattern).match(path):
            picked = not negated
    return picked


def starts(trigger: Trigger, files: list[str]) -> bool:
    """Whether GitHub starts this workflow for a PR that changes `files`."""
    if not trigger.on_pull_request:
        return False
    if not files:
        return True
    if trigger.paths is not None:
        return any(selects(trigger.paths, f) for f in files)
    if trigger.paths_ignore is not None:
        return any(not selects(trigger.paths_ignore, f) for f in files)
    return True


def load(workflows_dir: str) -> list[Trigger]:
    triggers = []
    for file in sorted(os.listdir(workflows_dir)):
        if not file.endswith((".yml", ".yaml")):
            continue
        with open(os.path.join(workflows_dir, file), encoding="utf-8") as handle:
            triggers.append(parse(file, handle.read()))
    return triggers


def expected(workflows_dir: str, files: list[str], exclude: str) -> tuple[list[Trigger], list[Trigger]]:
    """The PR workflows that should start for `files`, and those a filter keeps off."""
    should, kept_off = [], []
    for trigger in load(workflows_dir):
        if trigger.file == exclude or not trigger.on_pull_request:
            continue
        (should if starts(trigger, files) else kept_off).append(trigger)
    return should, kept_off
