#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Fail when Doxygen reported any warning.
#
#   check_docs_warnings.sh <doxygen-warnings-log> <report-fragment.md>
#
# The `docs` CMake target exits 0 whatever Doxygen printed, so the log is the only
# place the warnings show up. On a failure the warnings are also written to the
# report fragment, which the CI report folds into its one comment per pull request.
set -euo pipefail

log="$1"
fragment="$2"

if [ ! -f "$log" ]; then
    echo "::error::Doxygen wrote no warning log at $log, so the build did not run"
    exit 1
fi

count="$(grep -c 'warning:' "$log" || true)"
if [ "$count" -eq 0 ]; then
    echo "Doxygen reported no warnings."
    exit 0
fi

cat "$log"
echo "::error::Doxygen reported $count warning(s); fix them, the log is above"
{
    echo "### Doxygen warnings"
    echo ""
    echo "$count warning(s) from \`./build_libe3 --docs\`:"
    echo ""
    echo '```'
    cat "$log"
    echo '```'
} > "$fragment"
exit 1
