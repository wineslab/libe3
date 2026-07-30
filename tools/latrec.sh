#!/bin/bash
#
# latrec helper: turn tracing on for a run, then report on it.
#
#   . latrec.sh on               # SOURCE it: exports LATREC_DIR to a fresh dir
#   . latrec.sh on mytest        # ...with a name in it
#   . latrec.sh on /captures/r42 # ...under a given directory, in a per-host
#                                #    subdirectory (multi-host capture)
#   latrec.sh watch &            # convert automatically once the run goes idle
#   latrec.sh report             # convert + health-check the current LATREC_DIR
#   latrec.sh report <dir>...    # ...or the given ones; extra flags such as
#                                #    --wall and -o are passed to the converter
#   latrec.sh off                # unset LATREC_DIR (tracing back to a no-op)
#
# Environment:
#   LATREC_CONF           file sourced by `on`. Default: latrec.conf beside this
#                         script, if present.
#   LATREC_POD            subdirectory name under an absolute `on` path.
#                         Default: hostname -s.
#   LATREC_CPUS           cpu list for the conversion. Unset means no pinning.
#   LATREC_WATCH_PROCS    space-separated pgrep -f patterns `watch` treats as
#                         live writers. Unset means ring mtime alone.
#   LATREC_QUIET_SECS     `watch` idle threshold (default 15).
#   LATREC_WATCH_TIMEOUT  `watch` give-up timeout (default 7200).
#   LATREC_CONVERTER      path to latrec2csv.py, overriding the search.
#   LATREC_ENTRIES_LOG2_<ROLE>
#                         ring capacity for one writer role, as a power of two.
#                         ROLE is the ring name uppercased, non-alphanumerics
#                         as '_'.
#
# Tracing is gated entirely on LATREC_DIR: with it unset every stamp is one
# predicted branch and nothing is written, so the same binaries run untraced.
# Nothing needs rebuilding to switch it on or off.
#
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

_latrec_root() { cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd; }

# Search order: LATREC_CONVERTER, this script's own directory, a checkout beside
# it, then the install prefixes.
_latrec_converter() {
    local v="$1" c
    for c in "$LATREC_CONVERTER" "$v/latrec2csv.py" "$v/libe3/tools/latrec2csv.py" \
             /usr/local/share/libe3/tools/latrec2csv.py \
             /usr/share/libe3/tools/latrec2csv.py; do
        [ -n "$c" ] && [ -f "$c" ] && { echo "$c"; return 0; }
    done
    return 1
}

case "${1:-}" in
on)
    _V="$(_latrec_root)"
    # Optional deployment settings: ring sizes, watch patterns.
    _CONF="${LATREC_CONF:-$_V/latrec.conf}"
    [ -f "$_CONF" ] && . "$_CONF"
    # A fresh directory per run: ring files are named <role>.<tid>.latrec, so
    # reusing one leaves two runs side by side and the converter merges them
    # into a single set of CSVs without complaining.
    #
    # Given a path, each host gets its own subdirectory of it. Thread ids are
    # per PID namespace, so two containers running the same role can produce the
    # same file name, and one capture would overwrite the other on a shared
    # volume.
    case "${2:-}" in
        /*) LATREC_DIR="$2/${LATREC_POD:-$(hostname -s)}" ;;
        *)  LATREC_DIR="$_V/results/${2:-run}_$(date +%Y%m%d_%H%M%S)" ;;
    esac
    mkdir -p "$LATREC_DIR"
    export LATREC_DIR
    echo "tracing ON -> $LATREC_DIR"
    [ -f "$_CONF" ] && echo "settings from $_CONF"
    echo "start the stack from this shell, then: latrec.sh report"
    ;;
off)
    unset LATREC_DIR
    echo "tracing OFF (stamps are no-ops again)"
    ;;
report)
    shift
    ARGS=("$@")
    [ ${#ARGS[@]} -gt 0 ] || ARGS=("$LATREC_DIR")
    # Split flags from run directories; -o/--outdir consumes the next argument.
    FLAGS=()
    DIRS=()
    OUTDIR=""
    i=0
    while [ "$i" -lt "${#ARGS[@]}" ]; do
        a="${ARGS[$i]}"
        case "$a" in
            -o|--outdir)
                i=$((i + 1))
                OUTDIR="${ARGS[$i]:-}"
                [ -n "$OUTDIR" ] || { echo "$a needs a value" >&2; exit 1; }
                FLAGS+=("$a" "$OUTDIR")
                ;;
            -o*)        OUTDIR="${a#-o}";        FLAGS+=("$a") ;;
            --outdir=*) OUTDIR="${a#--outdir=}"; FLAGS+=("$a") ;;
            -*)         FLAGS+=("$a") ;;
            *)          DIRS+=("$a") ;;
        esac
        i=$((i + 1))
    done
    [ ${#DIRS[@]} -gt 0 ] || { echo "no LATREC_DIR set and none given" >&2; exit 1; }
    # Skip a directory with no rings; error only if none of them has any.
    KEEP=()
    for d in "${DIRS[@]}"; do
        [ -d "$d" ] || { echo "not a directory: $d" >&2; exit 1; }
        if [ "$(find "$d" -maxdepth 1 -name '*.latrec' | wc -l)" -gt 0 ]; then
            KEEP+=("$d")
        else
            echo "skipping $d: no .latrec files" >&2
        fi
    done
    [ ${#KEEP[@]} -gt 0 ] || {
        echo "no .latrec files in any given directory -- was LATREC_DIR set when the stack started?" >&2
        exit 1
    }
    V="$(_latrec_root)"
    CONV="$(_latrec_converter "$V")" \
        || { echo "latrec2csv.py not found; install libe3 or set LATREC_CONVERTER" >&2; exit 1; }
    # Pin only when LATREC_CPUS is set and the list is usable here.
    PIN=()
    if [ -n "${LATREC_CPUS:-}" ]; then
        if taskset -c "$LATREC_CPUS" true 2>/dev/null; then
            PIN=(taskset -c "$LATREC_CPUS")
        else
            echo "LATREC_CPUS=$LATREC_CPUS not usable here; converting unpinned" >&2
        fi
    fi
    "${PIN[@]}" python3 "$CONV" "${FLAGS[@]}" "${KEEP[@]}" || exit 1
    # With -o the converter writes straight into it, otherwise into <run>/csv.
    CSVDIR="${OUTDIR:-${KEEP[0]}/csv}"
    echo
    echo "--- capture health ---"
    # The closing clock pair, not rec_count, separates a killed writer from a
    # ring that flushed with no records.
    awk -F, 'NR>1 {
        if ($9 != 0 || $10 != 0)   { printf "  WRAPPED %-26s lost %s records\n", $2, $10; w++ }
        if ($14 == 0 || $15 == 0)  { printf "  NO EXIT FLUSH %-20s (killed, or still running)\n", $2; k++ }
        else if ($7 == 0)          { e++ }
    } END {
        if (!w) print "  no ring wrapped";
        if (!k) print "  every ring flushed at exit";
        if (e)  printf "  %d ring(s) flushed with no records: stage never fired on that thread\n", e;
    }' "$CSVDIR/rings.csv"
    echo "  CSVs: $CSVDIR/"
    ;;
watch)
    # Convert once the rings have stopped growing and, where LATREC_WATCH_PROCS
    # is set, no matching writer is alive: a writer that is stopping still has
    # records to flush, and converting mid-flush truncates the tail.
    #
    # pgrep sees only this PID namespace, so each host converts its own rings.
    # The merged conversion is a `report` over every directory afterwards.
    DIR="${2:-$LATREC_DIR}"
    [ -n "$DIR" ] || { echo "no LATREC_DIR set and none given" >&2; exit 1; }
    QUIET="${LATREC_QUIET_SECS:-15}"
    LIMIT="${LATREC_WATCH_TIMEOUT:-7200}"
    PROCS="${LATREC_WATCH_PROCS:-}"
    V="$(_latrec_root)"
    echo "watching $DIR (converts once idle for ${QUIET}s; gives up after ${LIMIT}s)"
    [ -n "$PROCS" ] && echo "also waiting for: $PROCS"
    waited=0
    while [ "$waited" -lt "$LIMIT" ]; do
        n=0
        for pat in $PROCS; do
            # PID 1 may not reap, so pgrep lists dead writers; count non-zombies.
            for p in $(pgrep -f "$pat" 2>/dev/null); do
                s=$(ps -o stat= -p "$p" 2>/dev/null | tr -d ' ')
                case "$s" in Z*|"") ;; *) n=$((n + 1)) ;; esac
            done
        done
        newest=$(find "$DIR" -maxdepth 1 -name '*.latrec' -newermt "-${QUIET} seconds" 2>/dev/null | wc -l)
        if [ "$n" -eq 0 ] && [ "$newest" -eq 0 ] && \
           [ "$(find "$DIR" -maxdepth 1 -name '*.latrec' | wc -l)" -gt 0 ]; then
            echo "capture idle -- converting"
            exec "$V/latrec.sh" report "$DIR"
        fi
        sleep 5; waited=$((waited + 5))
    done
    echo "gave up after ${LIMIT}s with $n writer(s) still alive" >&2
    exit 1
    ;;
*)
    sed -n '3,${/^# SPDX/q;s/^# \{0,1\}//p;}' "${BASH_SOURCE[0]:-$0}"
    ;;
esac
