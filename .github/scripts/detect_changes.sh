#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Say which stages of ci.yml a pull request needs, one `name=true|false` line each:
#
#   detect_changes.sh <base-sha> <head-sha>
#
#   code     the PR changes code (see detect_code_changes.sh): the build and test stages run
#   wheels   it changes what the libe3py wheels are built from
#   latrec   it changes latrec.h or latrec.c
#   mpmc     it changes the MPMC queue, or its benchmark
#
# A change to the CI itself (.github/workflows/ or .github/scripts/) sets all four, so that a PR
# that edits a stage is tested by it.
set -euo pipefail

base="$1"
head="$2"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
files="$(git diff --name-only "$base..$head")"

touches() { printf '%s\n' "$files" | grep -qE "$1"; }

code="$(bash "$here/detect_code_changes.sh" "$base" "$head" 2>/dev/null)"
wheels=false; latrec=false; mpmc=false

touches '^(pyproject\.toml|build_libe3|scripts/wheel_deps\.sh|tests/test_swig_smoke\.py|(cmake|swig|python|src|include|messages)/)' && wheels=true
touches '^(include/libe3/latrec\.h|src/core/latrec\.c)$' && latrec=true
touches '^(include/libe3/mpmc_queue\.hpp|tests/bench_mpmc_queue\.cpp)$' && mpmc=true
if touches '^\.github/(workflows|scripts)/'; then code=true; wheels=true; latrec=true; mpmc=true; fi

printf 'code=%s\nwheels=%s\nlatrec=%s\nmpmc=%s\n' "$code" "$wheels" "$latrec" "$mpmc"
