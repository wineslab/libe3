#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Full-loop latency benchmark for the cluster stage, inside the libe3-build image:
# every (link, transport) cell with every encoding, one markdown table per cell on
# stdout, the node and kernel first. Exits non-zero if a cell failed to run.
#
#   scripts/ci/bench.sh [BUILD_DIR]

set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
build=${1:-build}
export LIBE3_REQUIRE_SCTP=1
failed=0

echo "node: ${NODE_NAME:-unknown}  kernel: $(uname -r)  cpus: $(nproc)"

for cell in "zmq ipc" "zmq tcp" "posix ipc" "posix tcp" "posix sctp"; do
    # shellcheck disable=SC2086
    set -- $cell
    for encoding in asn1 json protobuf; do
        echo
        echo "### $1/$2/$encoding"
        if ! "$build/test_bench_full_loop_latency" --link "$1" --transport "$2" \
                --encoding "$encoding" 2>/dev/null | grep -v '^\[20'; then
            echo "FAIL $1/$2/$encoding"
            failed=1
        fi
    done
done

exit "$failed"
