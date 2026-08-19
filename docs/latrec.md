\page latrec_guide Latency recording (latrec)

# Latency recording (latrec)

`latrec` is libe3's stage-timing mechanism: a per-thread, lock-free,
mmap-backed ring recorder for measuring the E3 control loop, defined in
[latrec.h](@ref latrec.h). This page covers the mechanism, the clock model,
ring naming and sizing, and the capture-to-CSV workflow.

## Why a ring, and why two gates

A stamp is one `clock_gettime(CLOCK_MONOTONIC)` read plus four stores into an
mmap-backed ring — no syscall, no allocation, no formatting, no lock. A
record is 32 bytes: `sc` (48-bit seq | 8-bit cpu | 8-bit stage), `t_ns`,
`aux`, `aux2`. One ring per writer thread, pre-faulted at open, sized as
`2^entries_log2` records plus a 4 KiB header.

latrec is gated twice, for two different reasons:

- **Build time — `LIBE3_ENABLE_LATREC` (CMake option, `OFF` by default).**
  libE3's job is serving a real-time protocol, not measuring itself, so a
  normal build carries none of this mechanism at all — not even a
  near-zero-cost runtime branch. Every `latrec_tstamp()`/`latrec_tls_open_as()`
  call site already instrumented throughout the library resolves to a true
  no-op inline stub when the flag is off, with no dependency on
  `src/core/latrec.c` (which is excluded from the build in that
  configuration). Enable it with:

  ```bash
  ./build_libe3 --install --cmake-opt "-DLIBE3_ENABLE_LATREC=ON"
  ```

  `LIBE3_ENABLE_LATREC` is exported as a `PUBLIC` compile definition, so a
  downstream consumer linking against the libe3 CMake target inherits it
  automatically. A pkg-config consumer (OAI, flexric) must pass a matching
  `-DLIBE3_ENABLE_LATREC=ON` explicitly in its own build — the same as it
  already must for `-DLIBE3_ENABLE_ASN1=ON`/`-DLIBE3_ENABLE_JSON=ON`, since
  pkg-config does not propagate any of these defines today.

- **Runtime — `LATREC_DIR` (environment variable).** Only matters inside an
  already-enabled build: it picks which individual runs actually capture.
  Unset, `latrec_open()` returns immediately and every stamp is one
  predicted-not-taken branch. Set it to a writable directory to capture:

  ```bash
  export LATREC_DIR=/tmp/my-capture
  ./example_simple_agent &
  ./example_simple_dapp
  unset LATREC_DIR
  ```

## Clock model

- **Same host.** `CLOCK_MONOTONIC` shares an origin across processes, so
  rings from the RAN, the dApp, and any co-located process join with no
  calibration. Deltas are exact.
- **Across hosts.** Every ring header carries `t0/t1_mono_ns` and
  `t0/t1_real_ns`, sampled at open and at close (and refreshed periodically
  by `latrec_heartbeat()`). `tools/latrec2csv.py --wall` uses those pairs to
  map each record onto `CLOCK_REALTIME`, so legs on different hosts line up.
  Accuracy is then bounded by the NTP or PTP discipline between those hosts.
- **Never mix.** Service Model payloads carry their own timestamps on their
  own clocks. A payload timestamp and a latrec `t_ns` must not be subtracted
  from each other.

## Ring naming and sizing

Per-thread rings, opened on first stamp via `latrec_tls_open_as(role)` (or
`latrec_tls_open()`, which uses the program name as role), are named
`<LATREC_DIR>/<role>.<tid>.latrec`, unique per writer thread. libe3's own
threads use the roles `libe3.setup`, `libe3.inbound`, `libe3.outbound`,
`libe3.report`, and (when the recorder is enabled) `libe3.context` for the
slow-lane sampler.

Capacity is `2^entries_log2` records, chosen per role in this order:
`LATREC_ENTRIES_LOG2_<ROLE>` (role uppercased, non-alphanumerics as `_`,
e.g. `LATREC_ENTRIES_LOG2_LIBE3_INBOUND`), else `LATREC_ENTRIES_LOG2`, else
the compiled-in default (`LATREC_TLS_ENTRIES_LOG2 = 22`, i.e. 4M records =
128 MiB per thread, file-backed and pre-faulted). Size a busy ring down (or
up) with the per-role override rather than the global one, since roles differ
by orders of magnitude in stamp rate and sizing them together either wastes
memory or wraps the busiest ring. A ring that wrapped keeps counting past its
capacity in `rec_count`, so the converter can tell you it happened even
though the oldest records are gone.

## Stage catalog

The stage catalog in `include/libe3/latrec.h` is a single, append-only,
cross-repository namespace: identifiers are stable and are only ever
appended, never renumbered. Reserve a block here before a new repository or
component starts stamping, so identifiers never collide.

| Block | Owner |
|---|---|
| `P`, `W`, `M`, `T`, `S`, `B`, `G` | OAI gNB (PHY, L1-KPM worker, MAC, SM lifecycle, sensing, control-to-air, E2-E3 relay) |
| `LE`, `LF`, `L`, `LS`, `LQ`, `LC` | libe3 (emit entry, dApp-role callback delivery, outbound/inbound, setup + subscription control plane, session ring, connector) |
| `D`, `V`, `E`, `K`, `R` | dApps (ingest, publisher, session lifecycle, control handling, report generation) |
| `X` | flexric E2 legs (xApp control out, agent in/out, indication in) |
| `C` | slow-lane context records (`C0_CONTEXT`: involuntary context switches, CPU frequency) |
| `OC` | *reserved* — OCUDU: `ocudu-e3` jbpf hook + `E3Controller` IQ pipeline |
| `CB` | *reserved* — cuBB L1 data-lake IQ source (Aerial) |
| `PY` | *reserved* — xDevSM Python xApp/service-model framework |
| `EX` | the shipped reference Simple Service Model (`examples/sm_simple`) |
| `BD` | the minimal example dApp handler inside `bench_full_loop_latency` |

*Reserved* blocks are identifiers only — nothing in this repository stamps
them; they exist so the owning repository's own instrumentation never
collides with anything already in this catalog.

## Capture → CSV workflow

```bash
export LATREC_DIR=/tmp/my-capture
# ... run the traced process(es) ...
unset LATREC_DIR

python3 tools/latrec2csv.py /tmp/my-capture
# or, if the traced process runs in the background and you want the
# conversion to wait for it to go quiet first:
python3 tools/latrec2csv.py /tmp/my-capture --watch
```

Outputs, under `<run>/csv` by default:

- `rings.csv` — one row per ring: header fields, validity, wrap accounting,
  `clock_ns_per_call` (the measured `clock_gettime()` cost for that ring).
- `records.csv` — long format, one row per record, time-ordered.
- `<component>.csv` — one wide table per owning component (`oai`, `libe3`,
  `dapp`, `flexric`), one row per `(leg, seq)` with a column per stage
  timestamp plus computed hop/total deltas in microseconds.

`--wall` remaps `CLOCK_MONOTONIC` to `CLOCK_REALTIME` per ring, needed only
when joining a multi-host capture (pass every host's run directory).

## CI overhead gate

`tests/bench_latrec.cpp` asserts a ceiling on the recorder's own cost:
**≤400 ns per stamp enabled, ≤10 ns disabled** (measured over 2,000,000
iterations). It builds and runs as part of the normal CTest suite whenever
`LIBE3_ENABLE_LATREC=ON`, so it re-checks on every change to the recorder
itself. See the PR that landed this page for the end-to-end ablation numbers
(achieved throughput and loop latency, `LIBE3_ENABLE_LATREC` on vs. off, and
capturing vs. not within an enabled build) alongside the microbenchmark.
