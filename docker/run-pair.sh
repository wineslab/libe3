#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Runs the simulated RAN and the simulated dApp as two containers that share a
# network namespace (loopback, so tcp and sctp work) and an IPC directory, and
# passes when the dApp exits 0 and prints at least one indication.
#
#   docker/run-pair.sh [LINK] [TRANSPORT] [ENCODING]     e.g. posix sctp asn1
#
# TAG picks the image tag (default: local). SECONDS_TO_RUN is how long the dApp
# runs (default: 8).

set -euo pipefail

link=${1:-posix}
transport=${2:-tcp}
encoding=${3:-asn1}
tag=${TAG:-local}
run_for=${SECONDS_TO_RUN:-8}

suffix=$$
ran=libe3-ran-$suffix
ipc=libe3-ipc-$suffix
args=(--link "$link" --transport "$transport" --encoding "$encoding")

cleanup() {
    docker rm -f "$ran" >/dev/null 2>&1 || true
    docker volume rm "$ipc" >/dev/null 2>&1 || true
}
trap cleanup EXIT

docker volume create "$ipc" >/dev/null
docker run -d --name "$ran" -v "$ipc:/tmp/dapps" "libe3-ran-sim:$tag" "${args[@]}" >/dev/null
sleep 1

set +e
dapp_log=$(timeout -k 10 $((run_for + 15)) docker run --rm --network "container:$ran" \
    -v "$ipc:/tmp/dapps" "libe3-dapp-sim:$tag" \
    "${args[@]}" --control --timed "$run_for" 2>&1)
dapp_exit=$?
set -e

docker kill -s INT "$ran" >/dev/null 2>&1 || true
echo "---- RAN (tail) ----";  docker logs --tail 20 "$ran" 2>&1 || true
echo "---- dApp (tail) ----"; tail -20 <<<"$dapp_log"

indications=$(grep -c "Indication #" <<<"$dapp_log" || true)
if [ "$dapp_exit" -ne 0 ]; then
    echo "FAIL $link/$transport: dApp exit code $dapp_exit"; exit 1
elif [ "$indications" -eq 0 ]; then
    echo "FAIL $link/$transport: no indication reached the dApp"; exit 1
fi
echo "PASS $link/$transport/$encoding: $indications indications"
