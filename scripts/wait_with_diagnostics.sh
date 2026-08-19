#!/bin/bash
# Meant to be sourced, not executed: defines wait_with_diagnostics(), a bounded
# replacement for `wait <pid>` used by the E2E/Topologies CI workflows.
#
# The dApp side of each agent/dApp pairing in those workflows is already
# wrapped in `timeout`, but the backgrounded agent process is only ever sent
# `kill -INT` and then `wait`ed with no bound at all. If it ignores the signal
# or is genuinely deadlocked, that wait blocks until the job-level timeout,
# which is exactly the intermittent E2E/Topologies hang described in issue #60.

# wait_with_diagnostics <pid> <timeout_seconds> [<label>]
#
# Waits up to timeout_seconds for pid to exit. On timeout, dumps process state
# (ps stat/wchan, /proc/<pid>/status, open fds) and SIGKILLs it, so a stuck
# peer fails fast with evidence instead of hanging the job. Returns 124 if the
# timeout fired, otherwise the process's own exit status (best-effort - a
# backgrounded process reaped via `wait` after it already exited yields its
# real status; one that had to be killed has none to report, hence 124).
wait_with_diagnostics() {
    local pid="$1" timeout_s="$2" label="${3:-pid $1}" waited=0

    while kill -0 "$pid" 2>/dev/null; do
        if (( waited >= timeout_s )); then
            echo "::warning::${label} did not exit within ${timeout_s}s - dumping diagnostics and killing"
            ps -o pid,ppid,stat,wchan,cmd -p "$pid" 2>/dev/null || true
            head -20 "/proc/$pid/status" 2>/dev/null || true
            ls -l "/proc/$pid/fd" 2>/dev/null || true
            kill -9 "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            return 124
        fi
        sleep 1
        # Pre-increment: post-increment (waited++) evaluates to the
        # pre-increment value, so on the very first pass ((waited++)) is
        # ((0)) - a "false" arithmetic command that would trip `set -e` in
        # any caller that has it active (this function is sourced, not run
        # in a subshell, so it inherits the caller's shell options).
        ((++waited))
    done
    wait "$pid" 2>/dev/null
}
