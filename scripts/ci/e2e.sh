#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# End-to-end run for the cluster stage, inside the libe3-build image: one RAN and
# one dApp process per (link, transport) cell, then the integration tests.
# Prints one PASS or FAIL line per cell and exits non-zero if any cell failed.
#
#   scripts/ci/e2e.sh [BUILD_DIR]

set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
build=${1:-build}
export LIBE3_REQUIRE_SCTP=1
log_dir=$(mktemp -d)
failed=0

run_pair() {
    local link=$1 transport=$2 encoding=$3
    local cell="$link/$transport/$encoding"
    local agent_log="$log_dir/agent-$link-$transport-$encoding.log"
    local dapp_log="$log_dir/dapp-$link-$transport-$encoding.log"

    "$build/example_simple_agent" --link "$link" --transport "$transport" \
        --encoding "$encoding" >"$agent_log" 2>&1 &
    local agent=$!
    sleep 1

    timeout -k 10 25 "$build/example_simple_dapp" --link "$link" --transport "$transport" \
        --encoding "$encoding" --control --timed 8 >"$dapp_log" 2>&1
    local exit_code=$?

    kill -INT "$agent" 2>/dev/null
    for _ in $(seq 30); do kill -0 "$agent" 2>/dev/null || break; sleep 1; done
    kill -KILL "$agent" 2>/dev/null
    wait "$agent" 2>/dev/null

    local indications
    indications=$(grep -c "Indication #" "$dapp_log" || true)
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL $cell: dApp exit code $exit_code"
    elif [ "$indications" -eq 0 ]; then
        echo "FAIL $cell: no indication reached the dApp"
    else
        echo "PASS $cell: $indications indications"
        return 0
    fi
    tail -20 "$agent_log" "$dapp_log"
    failed=1
}

for cell in "zmq ipc" "zmq tcp" "posix ipc" "posix tcp" "posix sctp"; do
    for encoding in asn1 json protobuf; do
        # shellcheck disable=SC2086
        run_pair $cell "$encoding"
    done
done

ctest --test-dir "$build" -L integration --output-on-failure --timeout 300 || failed=1

exit "$failed"
