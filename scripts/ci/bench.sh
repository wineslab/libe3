#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Full-loop latency benchmark for the cluster stage, in the libe3-build image: one markdown table
# per (link, transport, encoding) cell on stdout. Without LINK and TRANSPORT it runs every cell.
# REPORT names a CSV of the same numbers ("link/transport/encoding,N", then
# "row,mean,p50,p99,max" in microseconds for rows 1 to 18 and Total).
#
#   scripts/ci/bench.sh [BUILD_DIR [LINK TRANSPORT]]

set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
build=${1:-build}
export LIBE3_REQUIRE_SCTP=1
failed=0
report=${REPORT:-/dev/null}
: >"$report"
cells=("zmq ipc" "zmq tcp" "posix ipc" "posix tcp" "posix sctp")
[ $# -ge 3 ] && cells=("$2 $3")

echo "node: ${NODE_NAME:-unknown}  kernel: $(uname -r)  cpus: $(nproc)"

for cell in "${cells[@]}"; do
    # shellcheck disable=SC2086
    set -- $cell
    for encoding in asn1 json protobuf; do
        echo
        echo "### $1/$2/$encoding"
        out=$("$build/test_bench_full_loop_latency" --link "$1" --transport "$2" \
                --encoding "$encoding" 2>/dev/null | grep -v '^\[20')
        echo "$out"
        samples=$(sed -n 's/.*(N=\([0-9]*\) after.*/\1/p' <<<"$out")
        if [ -z "$samples" ]; then
            echo "FAIL $1/$2/$encoding"
            failed=1
            continue
        fi
        {
            echo "$1/$2/$encoding,$samples"
            awk -F'|' '/^\| *([0-9]+|Total) *\|/ {
                for (i = 2; i <= 8; i++) gsub(/[ *]/, "", $i)
                print $2 "," $5 "," $6 "," $7 "," $8
            }' <<<"$out"
        } >>"$report"
    done
done

exit "$failed"
