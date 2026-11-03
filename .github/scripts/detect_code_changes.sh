#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Say whether a pull request changes code. The "Detect code changes" job of each
# test workflow runs this and skips its expensive steps when the answer is false,
# so a pull request that only touches documentation does not build or run tests.
#
#   detect_code_changes.sh <base-sha> <head-sha>      prints true or false
#
# Not code: Markdown, Word editions, docs/, .github/, the O-RAN Word generators
# under tools/oran_docx/ and tools/e3sm_l1kpm_v2/, license and citation files, REUSE.toml, .gitignore, and a
# pyproject.toml change that only edits the project links.
set -euo pipefail

base="$1"
head="$2"

echo "Changed files:" >&2
git diff --name-only "$base..$head" >&2

noncode='(\.md$)|(\.docx$)|(^docs/)|(^\.github/)|(^tools/oran_docx/)|(^tools/e3sm_l1kpm_v2/)|(^LICENSE$)|(^NOTICE$)|(^CONTRIBUTORS$)|(^CITATION\.cff$)|(^REUSE\.toml$)|(^\.gitignore$)'
files="$(git diff --name-only "$base..$head" | grep -vE "$noncode" || true)"

# pyproject.toml carries the build too, so it counts as code unless every changed
# line is a comment, a blank line or an entry of the project links.
if printf '%s\n' "$files" | grep -qx 'pyproject.toml'; then
    links='^[+-]([[:space:]]*|#.*|(Homepage|Documentation|Changelog|Source|Issues|Repository)[[:space:]]*=.*)$'
    if ! git diff -U0 "$base..$head" -- pyproject.toml \
        | grep -E '^[+-]' | grep -vE '^(\+\+\+|---)' | grep -qvE "$links"; then
        files="$(printf '%s\n' "$files" | grep -vx 'pyproject.toml' || true)"
    fi
fi

if [ -n "$files" ]; then echo true; else echo false; fi
