\page latrec_guide Latency recording (latrec)

# Latency recording (latrec)

`latrec` is libe3's stage-timing mechanism: a per-thread, lock-free,
mmap-backed ring recorder for measuring the E3 control loop, defined in
[latrec.h](@ref latrec.h). This page covers the mechanism, the clock model,
ring naming and sizing, and the capture-to-CSV workflow.

## Why a ring, and why one gate

A stamp is one `clock_gettime(CLOCK_MONOTONIC)` read plus four stores into an
mmap-backed ring — no syscall, no allocation, no formatting, no lock. A
record is 32 bytes: `sc` (48-bit seq | 8-bit cpu | 8-bit stage), `t_ns`,
`aux`, `aux2`. One ring per writer thread, pre-faulted at open, sized as
`2^entries_log2` records plus a 4 KiB header.

latrec is a benchmarking tool, not a production feature: libE3's job is
serving a real-time protocol, not measuring itself. So it is gated once, at
build time, by **`LIBE3_ENABLE_LATREC`** (CMake option, `OFF` by default). A
normal build carries none of the mechanism at all — not even a
near-zero-cost runtime branch. Every `latrec_tstamp()` /
`latrec_tls_open_as()` call site already instrumented throughout the library
resolves to a true no-op inline stub when the flag is off, with no dependency
on `src/core/latrec.c`, which is excluded from the build in that
configuration. Enable it with:

```bash
./build_libe3 --install --cmake-opt "-DLIBE3_ENABLE_LATREC=ON"
```

There is **no second, per-run opt-in**. If the library was built with the
flag, it records; if it was not, there is nothing to record with. An earlier
revision gated capture again at runtime on a `LATREC_DIR` environment
variable; that is gone, because "instrumented build that silently does
nothing" is a configuration worth neither the ambiguity nor the branch.

### Matching the flag downstream

`LIBE3_ENABLE_LATREC` is exported as a `PUBLIC` compile definition, so a
downstream consumer linking against the libe3 CMake target inherits it
automatically. A pkg-config consumer inherits it too: `libe3.pc`'s `Cflags`
is built from the target's own `INTERFACE_COMPILE_DEFINITIONS`
(`cmake/libe3Install.cmake`), the same mechanism that already carries
`LIBE3_ENABLE_ASN1` / `LIBE3_ENABLE_JSON` through, so `pkg-config --cflags
libe3` reflects however the linked library was actually built — nothing to
pass explicitly in either case. A consumer that somehow defines it against a
libe3 built *without* it will not link: the two branches in
[latrec.h](@ref latrec.h) declare the same functions with real bodies and
with stub bodies respectively.

### Where the rings go

Three levels, most specific first:

- `latrec_open_in(&ring, dir, name, log2)` — the low-level explicit-ring API
  names its directory directly. Available in every build configuration,
  since it does not depend on the TLS layer.
- `latrec_set_output_dir(dir)` — sets the directory for the whole process's
  per-thread rings. Call it once at start-up, before the first
  `latrec_tls_open_as()`; rings already open are unaffected. It chooses
  *where*, never *whether*. Two processes that should not share rings — a
  test and a benchmark running concurrently — each set their own.
- `LATREC_DEFAULT_DIR` — the compiled-in fallback, `/tmp/latrec` unless
  overridden with `-DLATREC_DEFAULT_DIR=...` at configure time. When tests are
  being built (`LIBE3_BUILD_TESTS`), the build defaults it to
  `<build-dir>/latrec` instead, so a test that never names a directory leaves
  its rings inside the build tree rather than in `/tmp`. A deployment should
  set it explicitly.

Ring capacity is still tunable per run without a rebuild, via
`LATREC_ENTRIES_LOG2_<ROLE>` and `LATREC_ENTRIES_LOG2` (see
[Ring naming and sizing](#ring-naming-and-sizing)). Those are sizing knobs,
not gates: they cannot switch recording off.

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
`<output-dir>/<role>.<tid>.latrec`, unique per writer thread. libe3's own
threads use the roles `libe3.setup`, `libe3.inbound`, `libe3.outbound`,
`libe3.report`, and (when the recorder is enabled) `libe3.context` for the
slow-lane sampler.

Capacity is `2^entries_log2` records, chosen per role in this order:
`LATREC_ENTRIES_LOG2_<ROLE>` (role uppercased, non-alphanumerics as `_`,
e.g. `LATREC_ENTRIES_LOG2_LIBE3_INBOUND`), else `LATREC_ENTRIES_LOG2`, else
the compiled-in default (`LATREC_TLS_ENTRIES_LOG2 = 18`, i.e. 256K records =
8 MiB per thread, file-backed and pre-faulted).

That default is deliberately modest. Because an enabled build records with no
per-run opt-out, *every* thread that stamps opens a ring — a full test suite
run against an enabled build allocates one per libe3 thread per process — so
the default has to be a size an ordinary run can absorb. **Raise it for a real
capture**, and prefer the per-role override to the global one, since roles
differ by orders of magnitude in stamp rate and sizing them together either
wastes space or wraps the busiest ring:

```bash
LATREC_ENTRIES_LOG2_LIBE3_INBOUND=24 ./my_benchmark   # 512 MiB for that role
```

A ring that wrapped keeps counting past its capacity in `rec_count`, so the
converter can tell you it happened even though the oldest records are gone.

Rings are not cleaned up on exit: a process that opened one leaves the file
behind for the converter to read. A long-running enabled build accumulates one
file per thread per run, so point `latrec_set_output_dir()` at a directory you
are willing to clear, and do not leave `LATREC_DEFAULT_DIR` pointing somewhere
space-constrained.

## Stage catalog

The stage catalog in [latrec.h](@ref latrec.h) names **operations on the
E3AP / E3SM / E2SM-DAPP path** — not components, and not positions in a
particular loop. Several components perform the same operation: an indication
is encoded by whichever Service Model produced it, and E3AP framing is the
same libe3 call whether the RAN or the dApp made it. They share the
identifier. Which component wrote a given record comes from its **ring name**,
and which concrete message it carried from the `aux` payloads, so a component
prefix on the stage id would only duplicate what the reader already knows.

That is why `tools/latrec2csv.py` keys its per-component tables off the ring's
role (`RING_COMPONENTS`) and its legs off the stage chain (`LEGS`), rather
than mapping stage-id ranges to repositories.

The identifiers name the boxes of the two documented loops:

| Group | Operations | Boxes |
|---|---|---|
| Source side | `RECORD_BEGIN`, `PROCESS_BEGIN` | A1, A2 |
| E3SM codec | `ENCODE_E3SM_*`, `DECODE_E3SM_*` | A3, A9, A12, A18 |
| E3AP codec, queue, transport (libe3) | `EMIT_ENTER`, `ENQUEUE`, `DEQUEUE`, `ENCODE_E3AP_DONE`, `SEND_DONE`, `RECV`, `DECODE_E3AP_DONE`, `DELIVER_*`, `REPORT_*`, `SESSION_*`, `DROP` | A4-A8, A13-A17, A19b, B10-B13 |
| Bootstrap | `SETUP_*`, `SUB_*`, `SM_START`, `SM_STOP`, `SM_STATUS_*` | precede the loop |
| Receiving application (dApp) | `PROCESS_DONE`, `CREATE_OUTPUT`, `APPLY_POLICY_DONE`, `ADMITTED` | A10, A11, B15 |
| Receiving application (RAN) | `APPLY_CONTROL_DONE`, `LIVE_ON_AIR` | A19a |
| Acknowledgment tail | `ACK_SENT`, `ACK_RECV` | — |
| E2-E3 bridge | `BRIDGE_IN`, `BRIDGE_OUT` | A20, B9 |
| E2AP codec and transport | `ENCODE_E2AP_*`, `DECODE_E2AP_*` | B1, B2, B6, B7 |
| E2SM codec and xApp decision | `DECODE_E2SM_*`, `XAPP_PROCESS_DONE`, `ENCODE_E2SM_*` | B3, B4, B5, B8 |
| Off-path | `WAIT_ENTER`, `SKIPPED`, `CONTEXT` | not boxes |

See [path-a-e3-loop.md](path-a-e3-loop.md) and
[path-b-e2-e3-loop.md](path-b-e2-e3-loop.md) for the box definitions, the
per-box segments, and the aggregates built on them.

### Attribution, and where the ring name stops being enough

"Which component wrote a record comes from its ring name" holds for a thread
that component owns. It does not hold for a **synchronous callback**, because
a stamp lands in the ring of the thread that executes it, not the ring of the
code that wrote it.

libe3 delivers an indication or a relayed xApp control by calling the
application's handler inline, on its own inbound thread — the one whose ring
is `libe3.inbound`. So a dApp's `PROCESS_DONE` and `CREATE_OUTPUT`, which are
the receiving application's operations and not the library's, are written into
a libe3 ring and filed by `tools/latrec2csv.py` under the `libe3` component.
The same applies to a RAN-side `APPLY_CONTROL_DONE` reached from a Service
Model's control handler.

Nothing is lost or misrecorded: the stage identifier still says exactly what
each record is, and the join is unaffected — `latrec_ctx_set()` publishes the
inbound seq precisely so the handler's stages key to the `DELIVER_BEGIN` /
`DELIVER_DONE` pair around them. Two consequences worth knowing before reading
the CSVs:

- **`libe3.csv` will contain application processing time.** Summing it as
  "library overhead" overstates the library. Separate the two by stage
  identifier — the *Receiving application* rows in the table above are the
  embedder's operations; the *E3AP codec, queue, transport* row is libe3's.
- **An embedder whose application also runs on its own threads elsewhere will
  see its operations split across two components**, depending on whether a
  given stage was reached from a libe3 callback or from its own thread. This
  shows up most sharply when comparing an application running over libe3
  against the same application over a different transport backend: identical
  code, different component attribution.

#### Possible follow-up: tier-aware attribution

The catalog already distinguishes the two cases — every identifier is either a
**protocol operation libe3 performs itself** (`ENQUEUE`, `SEND_DONE`, `RECV`,
`DECODE_E3AP_DONE`, `DELIVER_*`, ...) or an **operation the embedder performs**
(`RECORD_BEGIN`, `PROCESS_DONE`, `CREATE_OUTPUT`, `APPLY_CONTROL_DONE`,
`XAPP_PROCESS_DONE`, `APPLY_POLICY_DONE`, ...). That is one bit per
identifier, already implicit in the grouping above, and it is enough for the
converter to file an embedder operation under the application even when it was
written into a libe3 ring.

Note what this is *not*: it is a two-way library/embedder split, not a return
to mapping stage-id ranges to repositories. The latter is what the operation
catalog exists to avoid, and it stays gone — the table does not grow when a new
component starts stamping.

One gap has to be closed for this to work. A ring named `libe3.inbound` does
not say whether the process around it is a dApp or a RAN, so the converter
would know a record belongs to *the application* without knowing *which*. That
needs libe3's thread roles to carry the deployment, e.g. a prefix supplied by
the embedder so a dApp process names its threads `dapp.libe3.inbound` — new
public configuration, and the reason this is recorded as a follow-up rather
than done here.

### Granularity, and adding an identifier

One entry and one exit stamp per box. Boxes that hand off inside a single
function on a single thread share one boundary stamp rather than carrying a
separate exit and entry. Sub-steps *within* a box are deliberately not in the
catalog: a component that wants finer resolution for its own debugging should
keep that instrument to itself rather than adding it here.

Three kinds of record are exempt from the one-per-box rule, because they do
not describe where a message is: drop markers (`DROP`), skip markers
(`SKIPPED`), and load or idle-time samples (`WAIT_ENTER`, `CONTEXT`).

Identifiers are stable: from the current baseline on, **only append, never
renumber**. Add one only for an operation the catalog does not already name,
and add it to `STAGES` in `tools/latrec_reader.py` in the same commit —
`tests/test_latrec_reader.py` checks the two against each other, and
`tests/test_latrec.cpp` checks that no two identifiers collide.

## Capture → CSV workflow

```bash
# ... run the process(es); a LIBE3_ENABLE_LATREC=ON build records on its own ...

python3 tools/latrec2csv.py /tmp/latrec
# or, if the traced process runs in the background and you want the
# conversion to wait for it to go quiet first:
python3 tools/latrec2csv.py /tmp/latrec --watch
```

Outputs, under `<run>/csv` by default:

- `rings.csv` — one row per ring: header fields, validity, wrap accounting,
  `clock_ns_per_call` (the measured `clock_gettime()` cost for that ring).
- `records.csv` — long format, one row per record, time-ordered.
- `<component>.csv` — one wide table per component, identified by the role of
  the ring that wrote each record (`libe3`, `oai`, `dapp`, `xapp`, `flexric`,
  `examples`, ...). One row per `(leg, seq)`, with a column per stage
  timestamp plus computed hop and total deltas in microseconds. Because a
  component is a ring property, the same operation id can appear in more than
  one of these tables — both ends of a round trip encode a Service Model
  payload — and each table holds only what its own rings recorded.

`--wall` remaps `CLOCK_MONOTONIC` to `CLOCK_REALTIME` per ring, needed only
when joining a multi-host capture (pass every host's run directory).

## CI overhead gate

`tests/bench_latrec.cpp` asserts a ceiling on the recorder's own cost:
**≤400 ns per stamp into an open ring, ≤10 ns on a thread that has no ring**
(measured over 2,000,000 iterations). It builds and runs as part of the normal
CTest suite whenever `LIBE3_ENABLE_LATREC=ON`, so it re-checks on every change
to the recorder itself.

For the end-to-end cost, `tests/integration/bench_latrec_load.cpp` is the
ablation vehicle: build it twice, `-DLIBE3_ENABLE_LATREC=OFF` and `ON`, and
compare the independent atomic counters it keeps in its receiving handlers.
Those counters are latrec-free by construction, so they are comparable across
the two builds; the latrec-derived tables only exist in the `ON` build. There
is no third "compiled in but idle" configuration to measure.
