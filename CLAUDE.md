# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What libe3 is

libe3 is a vendor-neutral C++17 library that implements the E3AP (E3 Application Protocol) on both sides of the O-RAN E3 interface: the RAN function (server) and the dApp (client). A single entry-point class backs both roles, switched by one config field. Transport (ZeroMQ or POSIX TCP/SCTP/Unix-domain IPC) and encoding (ASN.1 APER, JSON, and Protocol Buffers) are pluggable and selected at runtime. The data path is built for sub-millisecond, inference-driven control loops (lock-free queues, dedicated I/O threads, optional CPU pinning). It ships a C API and Python (SWIG) bindings.

## Build & test commands

`build_libe3` (repo-root script) is the canonical entry point. It wraps CMake, installs dependencies, and supports Make or Ninja. Run `./build_libe3 -h` for the authoritative, current flag list.

```bash
./build_libe3 -I                 # install system deps (once per machine; uses sudo)
./build_libe3                    # default: clean-ish Release build
./build_libe3 -c -r -t          # clean Release build + run tests
./build_libe3 -c -g -t          # clean Debug build + run tests
./build_libe3 --ninja -v         # use Ninja, verbose
./build_libe3 -g --sanitize-address   # Debug + AddressSanitizer (also --sanitize-thread)
./build_libe3 --docs             # build Doxygen docs (docs target)
./build_libe3 --install          # install to --prefix (default /usr/local)
./build_libe3 --deb --deb-arch all    # build .deb packages (amd64 + arm64)
```

Some features have no dedicated flag; enable them by passing raw CMake options:

```bash
./build_libe3 --cmake-opt "-DLIBE3_ENABLE_JSON=ON"              # JSON encoder (off by default)
./build_libe3 --cmake-opt "-DLIBE3_ENABLE_SWIG=ON"             # Python bindings
./build_libe3 --cmake-opt "-DLIBE3_BUILD_INTEGRATION_TESTS=ON" # multi-process integration tests
```

Plain CMake works too:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(nproc)"
cd build && ctest --output-on-failure
```

Running tests:

```bash
cd build
ctest                      # all tests
ctest -L integration       # only integration tests (opt-in build)
ctest -LE integration      # only unit tests
./test_mpmc_queue          # a single test binary directly
PYTHONPATH=build/swig python3 tests/test_swig_smoke.py   # SWIG bindings smoke test
```

**Build options** live in `cmake/libe3Options.cmake`: `LIBE3_BUILD_TESTS` (ON), `LIBE3_BUILD_EXAMPLES` (ON), `LIBE3_BUILD_INTEGRATION_TESTS` (OFF), `LIBE3_ENABLE_SWIG` (OFF), `LIBE3_ENABLE_ZMQ` (ON), `LIBE3_ENABLE_ASN1` (ON), `LIBE3_ENABLE_JSON` (OFF), `LIBE3_ENABLE_PROTOBUF` (OFF), `LIBE3_ENABLE_ASAN`/`LIBE3_ENABLE_TSAN` (OFF), `LIBE3_BUILD_DOCS` (OFF). **At least one of `LIBE3_ENABLE_ASN1` / `LIBE3_ENABLE_JSON` / `LIBE3_ENABLE_PROTOBUF` must be ON**; any combination can be built together and the encoder is chosen at runtime. `build_libe3` has a dedicated `--enable-protobuf` flag; JSON and SWIG use `--cmake-opt`.

## Architecture (big picture)

The design has one façade over three pluggable seams, plus a real-time data path. Understanding these relationships (which span many files) is the fastest way to be productive.

**Dual-role façade.** `E3Agent` (`include/libe3/e3_agent.hpp`) is the single public class for **both** the RAN and dApp roles; the role is chosen by `E3Config.role` (RAN vs DAPP), not by a separate class. It is a Pimpl over the internal `E3Interface` (`src/core/e3_interface.cpp`), which coordinates the full protocol lifecycle (Setup → Subscription → Control/Indication → Release) and owns the worker threads and queues. `E3Config` (in `types.hpp`) is the one struct that selects role, transport, encoding, endpoints, and real-time tuning.

**Three pluggable seams**, each behind an interface with a factory:
- **Transport** — `E3Connector` (`include/libe3/e3_connector.hpp`), implemented by the ZeroMQ and POSIX (TCP/SCTP/IPC) connectors in `src/connector/`, built by `connector_factory.cpp`.
- **Encoding** — `E3Encoder` (`include/libe3/e3_encoder.hpp`), implemented by the encoders in `src/encoder/` (ASN.1 APER, JSON, and Protocol Buffers), built by `encoder_factory.cpp`.
- **Service Model** — `ServiceModel` (`include/libe3/sm_interface.hpp`) is the RAN-side extension point for opaque, application-defined payload semantics. Its lifecycle is subscription-driven (`start()` on first subscriber to a RAN function, `stop()` on the last), managed via `SmRegistry` (`src/core/sm_registry.cpp`).

**Real-time data path.** Producers never touch the network directly. Lock-free MPMC ring buffers (`include/libe3/mpmc_queue.hpp`, wrapped by `lockfree_queue.hpp`) decouple work from dedicated I/O threads that `E3Interface` spawns: setup, inbound, and outbound, plus RAN-only threads that poll Service Models for telemetry and drain dApp reports off the receive path. Optional CPU affinity and niceness (Linux) are set from `E3Config` to cut scheduling jitter in sub-millisecond loops.

**State & types.** `SubscriptionManager` (RAN side) and `DAppSubscriptionState` (dApp side) track subscriptions and assigned IDs. The 11 E3AP message types are modeled as a `std::variant`-based `Pdu` in `include/libe3/types.hpp`. Each wire encoding has its own grammar under `messages/`, generated at build time into a dedicated library: ASN.1 in `messages/asn1/V1/e3ap-1.0.0.asn1` (`asn1c` -> `asn1_e3ap`, C) and Protocol Buffers in `messages/proto/V1/e3ap-1.0.0.proto` (`protoc` -> `pb_e3ap`, C++). Keep these grammars in sync with the `Pdu` structs when adding fields.

**Bindings & examples.** A C API lives in `include/libe3/c_api.h`; Python bindings via SWIG (`swig/libe3.i` → the `libe3py` module). `examples/simple_agent.cpp` (RAN) and `examples/simple_dapp.cpp` (dApp), with the reference Simple SM in `examples/sm_simple/`, are the best runnable illustration of the two roles (they require ASN.1).

## Conventions & gotchas

- **Strict warnings.** `cmake/libe3Compiler.cmake` enables a strict set (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast ...`, with `-Werror=return-type`). New code must compile clean.
- **Custom test framework.** Tests use a small header-only framework registered with CTest, not GoogleTest or Catch2. Follow the existing `tests/test_*.cpp` pattern when adding tests.
- **`VERSION` is the single source of truth** for the release version (consumed by CMake, `build_libe3`, and CI, which auto-tags on merge to main). Bump it (SemVer) when the public API/ABI changes.
- **Twin-repo coordination.** Per `CONTRIBUTING.md`, changes to the wire protocol, a Service Model, or the public API require paired PRs in the companion dApps framework and `dApp-openairinterface5g`. Open an issue first and make sure PR CI passes in both Debug and Release; the MPMC queue benchmark must not regress.

## Pointers

- `README.md` — architecture diagram, roles, transports, encodings, quick start.
- `CONTRIBUTING.md` — full PR/versioning process and contributor rules.
- `docs/` — Doxygen config; published API docs at https://wineslab.github.io/libe3/.
