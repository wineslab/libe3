# libe3 Python bindings (`libe3py`)

SWIG bindings that let Python drive libe3 in the **dApp role**. A Python dApp can
delete its pure-Python E3AP stack (ZMQ + `asn1tools`) and let libe3 own E3AP on
both ends of the link — exactly the split OAI already uses on the RAN side. An
example consumer is available in
[dApp-library](https://github.com/wineslab/dApp-library).

## The split: E3AP in libe3, E3SM in Python

libe3 owns **E3AP**: transport, the setup handshake, subscribe/unsubscribe,
indication/control framing, and all wire encodings (ASN.1 / JSON / Protobuf,
selected at runtime by `E3Config.encoding`). Python owns **E3SM**: the
service-model payloads ride opaque, as `bytes`, and are encoded/decoded in
Python. `libe3py` never touches SM contents.

## Two layers

| Symbol | Purpose |
|--------|---------|
| `E3Config`, enums (`E3Role`, `E3LinkLayer`, `E3TransportLayer`, `EncodingFormat`, `ResponseCode`, `AgentState`, `ErrorCode`) | Configure a session. |
| `E3Agent` (minimal view) | Backwards-compatible seam: construct/config/start, push opaque bytes. Kept for the smoke test. |
| **`DAppSession` + `E3Event`** | The real dApp seam: full lifecycle + a **batched, lock-free inbound queue**. This is what a Python dApp consumes. |

See [`e3_dapp_session.hpp`](e3_dapp_session.hpp) for the authoritative,
Doxygen-documented API.

## Why batched drain + GIL release (not callbacks)

E3AP runs at real-time sub-millisecond cadence with very high message
throughput, so the binding must not become the bottleneck. At those rates the
limiters are the **count of GIL acquisitions**, the **count of Python↔C++
boundary crossings**, and libe3's C++ worker threads **stalling on the GIL** —
not payload copy size (E3AP payloads are small; bulk IQ travels out-of-band in
shared memory).

`DAppSession` therefore:

- registers libe3's C++ handlers itself and pushes each inbound message into a
  **bounded lock-free ring** (`libe3::LockFreeQueue`), incrementing a drop
  counter on overflow instead of blocking libe3;
- exposes **`poll_events(max_batch, timeout_ms)`**, which blocks for the *first*
  event (latency ≈ ring wakeup) then sweeps up to `max_batch` already-queued
  events in a single call — amortising one GIL acquire + one boundary crossing
  across the whole batch;
- is wrapped with `%module(threads="1")` (see [`libe3.i`](libe3.i)) so the GIL is
  **released** around every call; libe3's threads run freely while Python waits.

**Rejected alternatives:** SWIG *directors* run the Python callback on libe3's
inbound thread under the GIL (serialises dispatch, deadlock-prone if Python
re-enters libe3); a *per-message* poll pays one GIL round-trip per message and
caps throughput with no latency benefit over batching.

## Lifecycle

```
DAppSession(cfg)          # ctor registers all inbound handlers
  → start()               # connects; begins the handshake
  → wait_for_setup(ms)    # blocks until setup completes
  → dapp_id() / setup_ran_function_*()   # read what the RAN advertised
  → subscribe(rfid, telemetry, control, sub_time, periodicity)
  → loop:
      for ev in poll_events(max_batch, timeout_ms):
          dispatch(ev)                 # E3_EVENT_INDICATION / _XAPP_CONTROL / ...
      send_control(rfid, cid, payload_bytes)   # or send_report(...)
  → release()             # tell the RAN we are leaving
  → stop()                # tear down; unblocks any poll_events
```

## Minimal Python example

```python
import libe3py as e3

cfg = e3.E3Config()
cfg.role = e3.E3Role_DAPP
cfg.link_layer = e3.E3LinkLayer_ZMQ
cfg.transport_layer = e3.E3TransportLayer_IPC
cfg.encoding = e3.EncodingFormat_ASN1        # must match the RAN's E3Configuration
cfg.dapp_name = "MyDApp"

s = e3.DAppSession(cfg)
assert s.start() == 0                          # ErrorCode.SUCCESS
assert s.wait_for_setup(6000) == 0
print("dApp id:", s.dapp_id())

# Enumerate what the RAN advertised in the SetupResponse.
for i in range(s.setup_ran_function_count()):
    rfid = s.setup_ran_function_id(i)
    ran_function_data = s.setup_ran_function_data(i)   # opaque bytes -> decode in Python

s.subscribe(1, [1], [1], -1, -1)               # RAN function 1, telemetry {1}, control {1}

while True:
    for ev in s.poll_events(256, 100):         # drain up to 256 per wake, 100ms wait
        if ev.kind == e3.E3_EVENT_INDICATION:
            handle(ev.ran_function_id, ev.get_payload())   # get_payload() -> native bytes
        elif ev.kind == e3.E3_EVENT_SUBSCRIPTION_RESPONSE:
            ...
    # send an SM-encoded control back when needed:
    # s.send_control(1, 1, sm_encode(...))
```

## Build & install

The module is opt-in (`LIBE3_ENABLE_SWIG=OFF` by default). Build and install it
into the **active interpreter's** site-packages (activate your venv first):

```bash
./build_libe3 --install --enable-swig \
  --cmake-opt "-DLIBE3_ENABLE_ASN1=ON -DLIBE3_ENABLE_JSON=ON"
python3 -c "import libe3py; print('libe3py OK')"
```

Requires `swig >= 4.0` and Python development headers. The install drops
`_libe3py.so` + `libe3py.py` into `Python3_SITEARCH` — note that this is an
absolute path, so `--prefix` does **not** relocate the Python module; override
`-DLIBE3_PYTHON_INSTALL_DIR=<dir>` to stage it elsewhere.

## Service-model definitions

The Simple SM grammar is installed alongside the library under
`<datadir>/libe3/sm/sm_simple/` — both `e3sm_simple.asn` (ASN.1) and
`e3sm_simple.proto` (Protobuf) — discoverable via
`pkg-config --variable=smdir libe3`, so Python dApps can reuse the exact grammar
their C++ counterpart (`examples/simple_agent.cpp`) uses instead of copying it.
