# libe3 - Vendor-Neutral E3AP C++ Library

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Version](https://img.shields.io/badge/version-0.0.4-green.svg)](VERSION)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://wineslab.github.io/libe3/)

**libe3** is a standalone, vendor-neutral C++ library for implementing the E3AP (E3 Application Protocol) on **both sides of the protocol** — RAN functions (DU, CU-CP, CU-UP) AND dApp clients. It provides a clean object-oriented API for both roles while hiding transport, encoding, and protocol complexity.

## Features

- **Vendor-Neutral API**: Clean E3Agent facade that hides implementation details
- **Both Roles**: Single `E3Agent` class acts as either **RAN** (server, binds sockets) or **dApp** (client, connects to a remote RAN), switched by `E3Config.role`
- **Multi-Peer dApps**: One dApp process can hold N `E3Agent` instances connecting to N different RAN agents (e.g. a DU-HIGH and a DU-LOW)
- **Multiple Transports**: Support for ZeroMQ and POSIX sockets (TCP, SCTP, Unix Domain)
- **Multiple Encodings**: ASN.1 APER (primary) and JSON encoders
- **Service Model Extensions**: Easy-to-implement SM interface for custom functionality
- **Python Bindings**: Optional SWIG bindings (`LIBE3_ENABLE_SWIG=ON`) so the same C++ library can back Python dApps
- **Thread-Safe**: Proper synchronization for concurrent dApp operations
- **Modern C++17**: Uses std::variant, std::optional, RAII patterns
- **Lightweight**: Minimal external dependencies

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        RAN Application                              │
│                        (DU, CU-CP, CU-UP)                           │
└────────────────────────────────┬────────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │        E3Agent          │  ◄── Public API
                    │      (Facade)           │
                    └────────────┬────────────┘
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
┌─────────▼─────────┐  ┌─────────▼─────────┐  ┌────────▼────────┐
│   E3Connector     │  │    E3Encoder      │  │ SubscriptionMgr │
│   (Transport)     │  │    (Encoding)     │  │   (dApp Track)  │
├───────────────────┤  ├───────────────────┤  └─────────────────┘
│ - ZmqConnector    │  │ - JsonEncoder     │
│ - PosixConnector  │  │ - Asn1Encoder     │
└───────────────────┘  └───────────────────┘

                         ┌─────────────────┐
                         │  ServiceModel   │  ◄── SM Extension Point
                         │   Interface     │
                         └─────────────────┘
```

## Quick Start

### Prerequisites

- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- pthreads
- `asn1c` — ASN.1 APER encoder, required by `messages/`
- `nlohmann-json3-dev` — Header-only JSON library; CMake expects the `nlohmann_json` target
- `libzmq3-dev` for ZMQ transport
 - `libsctp-dev` — SCTP development headers/libraries for POSIX/SCTP transport

### Install Dependencies

Install all required packages using the project's installer (recommended) or manually:

```bash
# Recommended
./build_libe3 -I

# Manual (Debian/Ubuntu)
sudo apt update
sudo apt install -y build-essential cmake pkg-config libzmq3-dev ninja-build git asn1c nlohmann-json3-dev libsctp-dev dpkg-dev debhelper fakeroot
```

The packaging tools (`dpkg-dev`, `debhelper`, `fakeroot`) are only needed by `scripts/create_deb.sh`.

### Building

The easiest way to build libe3 is using the provided build script:

```bash
cd libe3

# Basic release build
./build_libe3

# Debug build with tests
./build_libe3 -g -t

# Clean build with Ninja
./build_libe3 -c --ninja

# Documentation 
./build_libe3 --docs

# Install dependencies first (Ubuntu/Fedora/Arch/macOS)
./build_libe3 -I

# See all options
./build_libe3 --help
```

Alternatively, you can use CMake directly:

```bash
cd libe3
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `LIBE3_BUILD_TESTS` | ON | Build unit tests |
| `LIBE3_BUILD_EXAMPLES` | ON | Build examples |
| `LIBE3_BUILD_INTEGRATION_TESTS` | OFF | Build the integration test suite (multi-role end-to-end tests + full-loop benchmark) |
| `LIBE3_ENABLE_ZMQ` | ON | Enable ZeroMQ transport |
| `LIBE3_ENABLE_ASN1` | ON | Enable ASN.1 encoding |
| `LIBE3_ENABLE_JSON` | OFF | Enable JSON encoding (mutually exclusive with ASN.1) |
| `LIBE3_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `LIBE3_ENABLE_TSAN` | OFF | Enable ThreadSanitizer |
| `LIBE3_ENABLE_SWIG` | OFF | Build the SWIG-generated Python bindings (`_libe3py.so` + `libe3py.py`) |

### Running Tests

```bash
# With build script
./build_libe3 -t

# Or manually
cd build && ctest --output-on-failure
```

## Usage

### Basic E3 Agent

```cpp
#include <libe3/libe3.hpp>

int main() {
    // Configure the agent
    libe3::E3Config config;
    config.ran_identifier = "my-ran-001";
    config.link_layer = libe3::E3LinkLayer::POSIX;
    config.transport_layer = libe3::E3TransportLayer::IPC;
    config.encoding = libe3::EncodingFormat::JSON;

    // Create the agent
    libe3::E3Agent agent(std::move(config));

    // Initialize and start (start() will call init() if needed)
    if (agent.init() != libe3::ErrorCode::SUCCESS) {
        // handle initialization error
    }

    if (agent.start() != libe3::ErrorCode::SUCCESS) {
        // handle start error
    }

    // ... run your application ...

    agent.stop();
    return 0;
}
```

### dApp Role

The same `E3Agent` class can act as a dApp client. Set `E3Config.role` to `E3Role::DAPP` and register handlers for the messages flowing back from the RAN:

```cpp
#include <libe3/libe3.hpp>

int main() {
    libe3::E3Config config;
    config.role = libe3::E3Role::DAPP;
    config.dapp_name = "MyDApp";
    config.dapp_version = "1.0.0";
    config.vendor = "WinesLab";
    config.link_layer = libe3::E3LinkLayer::ZMQ;
    config.transport_layer = libe3::E3TransportLayer::IPC;

    libe3::E3Agent agent(std::move(config));

    agent.set_indication_handler([](const libe3::IndicationMessage& msg) {
        // Decode msg.protocol_data with your SM-specific encoder.
    });
    agent.set_subscription_response_handler([](const libe3::SubscriptionResponse& r) {
        // Inspect r.subscription_id, r.response_code.
    });

    if (agent.start() != libe3::ErrorCode::SUCCESS) {
        return 1;
    }
    if (agent.wait_for_setup(std::chrono::seconds(5)) != libe3::ErrorCode::SUCCESS) {
        return 1;
    }

    agent.subscribe(/*ran_function_id=*/1, /*telemetry_ids=*/{1}, /*control_ids=*/{1});

    // ... do work; control loop on the application side ...

    agent.release();
    agent.stop();
    return 0;
}
```

See `examples/simple_dapp.cpp` for a complete reference dApp using the Simple service model. It pairs with `examples/simple_agent.cpp` (and is wire-compatible with `spear-dApp`'s Python `simple_dapp.py`).

#### Multi-peer dApps

A single dApp process can connect to multiple RAN agents simultaneously by instantiating multiple `E3Agent` objects, each with its own configuration:

```cpp
// Connect one dApp process to a DU-HIGH and a DU-LOW
libe3::E3Config cfg_high; cfg_high.role = libe3::E3Role::DAPP;
cfg_high.setup_endpoint      = "ipc:///tmp/dapps_high/setup";
cfg_high.subscriber_endpoint = "ipc:///tmp/dapps_high/dapp_socket";
cfg_high.publisher_endpoint  = "ipc:///tmp/dapps_high/e3_socket";

libe3::E3Config cfg_low = cfg_high;
cfg_low.setup_endpoint      = "ipc:///tmp/dapps_low/setup";
cfg_low.subscriber_endpoint = "ipc:///tmp/dapps_low/dapp_socket";
cfg_low.publisher_endpoint  = "ipc:///tmp/dapps_low/e3_socket";

libe3::E3Agent dapp_to_high(std::move(cfg_high));
libe3::E3Agent dapp_to_low (std::move(cfg_low));

dapp_to_high.set_indication_handler([&](const libe3::IndicationMessage& m){ /* DU-HIGH */ });
dapp_to_low .set_indication_handler([&](const libe3::IndicationMessage& m){ /* DU-LOW  */ });

dapp_to_high.start();
dapp_to_low.start();
```

Each instance has its own threads, connector, encoder, and per-peer `DAppSubscriptionState`. The library guarantees that N coexisting `E3Agent` instances in one process do not share mutable state.

### Custom Service Model

Service Models implement the `libe3::ServiceModel` interface. Control action
handlers are registered from the SM implementation via `register_control_callback`
and the `E3Agent` routes incoming control actions to the appropriate SM.

```cpp
#include <libe3/libe3.hpp>

class MyKpmServiceModel : public libe3::ServiceModel {
public:
    std::string name() const override { return "KPM"; }
    uint32_t version() const override { return 2; }
    uint32_t ran_function_id() const override { return 100; }

    std::vector<uint32_t> telemetry_ids() const override { return {1}; }
    std::vector<uint32_t> control_ids() const override { return {1}; }

    ErrorCode init() override {
        register_control_callback(1, [](const std::vector<uint8_t>& data) {
            // decode and apply control action
            return ErrorCode::SUCCESS;
        });
        return ErrorCode::SUCCESS;
    }

    void destroy() override { stop(); }
    ErrorCode start() override { running_ = true; return ErrorCode::SUCCESS; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    std::vector<uint8_t> ran_function_data() const override { return {}; }

private:
    std::atomic<bool> running_{false};
};

// Register with agent
agent.register_sm(std::make_unique<MyKpmServiceModel>());
```

### Notes on Simulation / Local Testing

The library exposes IPC endpoints by default (see `E3Config` defaults) so
local testing with dApps on the same host is supported using the IPC transport.
There is no dedicated `simulation_mode` field in `E3Config`; instead adjust
endpoints and transport selection to run in isolated/local environments.


## API Reference

### E3Agent

The main facade class for RAN vendors.

| Method | Description |
|--------|-------------|
| `init()` | Initialize the agent |
| `start()` | Start processing threads |
| `stop()` | Stop the agent |
| `state()` | Get current state |
| `is_running()` | Check if agent is running |
| `register_sm(sm)` | Register a Service Model |
| `get_available_ran_functions()` | Get available RAN function IDs from registered SMs |
| `set_dapp_report_handler(handler)` | Set callback for incoming dApp reports (dApp → RAN) |
| `set_dapp_status_changed_handler(handler)` | Set callback for dApp status changes (connect, disconnect, subscribe, unsubscribe) |
| `send_indication(dapp_id, ran_function_id, data)` | (RAN) Send an indication to a specific dApp |
| `send_xapp_control(dapp_id, ran_function_id, data)` | (RAN) Forward an xApp control action to a specific dApp |
| `send_message_ack(request_id, response_code)` | (RAN) Send a message acknowledgment to a dApp |
| `get_registered_dapps()` | (RAN) Get list of registered dApp IDs |
| `get_dapp_subscriptions(dapp_id)` | (RAN) Get subscriptions for a dApp |
| `get_ran_function_subscribers(ran_function_id)` | (RAN) Get subscribers for a RAN function |
| `set_indication_handler(handler)` | (dApp) Callback for incoming `IndicationMessage` |
| `set_subscription_response_handler(handler)` | (dApp) Callback for incoming `SubscriptionResponse` |
| `set_setup_response_handler(handler)` | (dApp) Callback for incoming `SetupResponse` |
| `set_xapp_control_handler(handler)` | (dApp) Callback for incoming `XAppControlAction` |
| `set_message_ack_handler(handler)` | (dApp) Callback for incoming `MessageAck` |
| `wait_for_setup(timeout)` | (dApp) Block until setup handshake completes |
| `subscribe(ran_function_id, telemetry, control, ...)` | (dApp) Subscribe to a RAN function |
| `unsubscribe(ran_function_id)` | (dApp) Tear down a subscription |
| `send_control(ran_function_id, control_id, data)` | (dApp) Send a control action to the RAN |
| `send_report(ran_function_id, data)` | (dApp) Send a dApp report to the RAN |
| `release()` | (dApp) Send `ReleaseMessage` to the RAN |
| `dapp_id()` | (dApp) Identifier assigned by the RAN in `SetupResponse` |
| `subscribed_ran_functions()` / `active_subscription_ids()` | (dApp) Per-instance subscription state |
| `config()` | Access current `E3Config` |
| `dapp_count()` | (RAN) Number of registered dApps. (dApp) 0/1 based on assignment |
| `subscription_count()` | Number of active subscriptions on this side |

### E3Config

The `E3Config` structure contains agent configuration and defaults suitable
for local development (IPC) or production deployments.

| Field | Type | Description |
|-------|------|-------------|
| `role` | E3Role | `RAN` (default — binds sockets) or `DAPP` (connects to a remote RAN) |
| `ran_identifier` | string | Unique RAN identifier (used when role=RAN) |
| `dapp_name` | string | dApp name advertised in `SetupRequest` (used when role=DAPP) |
| `dapp_version` | string | dApp version advertised in `SetupRequest` |
| `vendor` | string | Vendor name for `SetupRequest` |
| `e3ap_version` | string | E3AP protocol version string |
| `link_layer` | E3LinkLayer | ZMQ or POSIX |
| `transport_layer` | E3TransportLayer | SCTP, TCP, IPC |
| `setup_endpoint` | string | Setup connection endpoint (default: ipc:///tmp/dapps/setup) |
| `subscriber_endpoint` | string | Subscriber endpoint (default: ipc:///tmp/dapps/dapp_socket) |
| `publisher_endpoint` | string | Publisher endpoint (default: ipc:///tmp/dapps/e3_socket) |
| `encoding` | EncodingFormat | ASN1 or JSON |
| `connect_timeout_ms` | uint32_t | Connect timeout (ms) |
| `recv_timeout_ms` | uint32_t | Receive timeout (ms) |
| `send_timeout_ms` | uint32_t | Send timeout (ms) |
| `receive_buffer_size` | size_t | Receive buffer size (bytes) |
| `send_buffer_size` | size_t | Send buffer size (bytes) |
| `io_threads` | size_t | Number of I/O threads |
| `log_level` | int | Logging level (0=none .. 5=trace) |

### ServiceModel

Interface for implementing custom service models.

| Method | Description |
|--------|-------------|
| `name()` | SM name (e.g., "KPM") |
| `version()` | SM version number |
| `ran_function_id()` | RAN function ID served by this SM |
| `telemetry_ids()` | Telemetry identifiers provided by the SM |
| `control_ids()` | Control identifiers accepted by the SM |
| `ran_function_data()` | Optional opaque bytes included in SetupResponse |
| `init()` | Initialize the SM (register callbacks) |
| `destroy()` | Destroy the SM and release resources |
| `start()` | Start SM processing (on first subscription) |
| `stop()` | Stop SM processing (on last unsubscription) |
| `is_running()` | Check if SM is currently running |

## Directory Structure

```
libe3/
├── CMakeLists.txt           # Main build configuration
├── README.md                # This file
├── LICENSE                  # Apache 2.0 license
├── VERSION                  # Version file (used by CMake)
├── build_libe3              # Build script
├── cmake/
│   ├── libe3Version.cmake   # Version configuration
│   ├── libe3_version.hpp.in # Version header template
│   └── ...                  # Other CMake modules
├── include/
│   └── libe3/
│       ├── libe3.hpp        # Umbrella header
│       ├── types.hpp        # Type definitions
│       ├── e3_agent.hpp     # Main facade
│       ├── e3_connector.hpp # Transport interface
│       ├── e3_encoder.hpp   # Encoder interface
│       └── ...
├── messages/
│   ├── asn1/                # ASN.1 definitions
│   └── CMakeLists.txt       # ASN.1 build config
├── src/
│   ├── core/                # Core implementations
│   ├── connector/           # Transport implementations
│   └── encoder/             # Encoder implementations
├── tests/                   # Unit tests
└── examples/                # Example applications
```

## Versioning

The library version is managed via the `VERSION` file in the project root. During compilation, CMake reads this file and generates `libe3/version.hpp` with the version macros:

```cpp
#include <libe3/libe3.hpp>

// Access version information
const char* ver = libe3::version();  // e.g., "0.0.1"

int major, minor, patch;
libe3::version(major, minor, patch);  // e.g., 0, 0, 1
```

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Documentation

The full API reference is automatically generated with [Doxygen](https://www.doxygen.nl/) and published to GitHub Pages on every release.

🔗 **[https://wineslab.github.io/libe3/](https://wineslab.github.io/libe3/)**

To build the documentation locally:

```bash
# Install Doxygen and Graphviz (Ubuntu)
sudo apt-get install doxygen graphviz

# Build docs (output will be in build/docs/html/)
./build_libe3 --docs

# Open in browser
xdg-open build/docs/html/index.html
```

## Python Bindings (SWIG)

libe3 ships an optional SWIG-generated Python binding so the same C++ library can back Python dApps. It is **off by default** — enable it with `-DLIBE3_ENABLE_SWIG=ON`:

```bash
# Install SWIG and Python development headers (Ubuntu)
sudo apt-get install -y swig python3-dev

# Configure with bindings enabled
cmake -S . -B build -DLIBE3_ENABLE_SWIG=ON
cmake --build build --target libe3py -j $(nproc)

# Smoke test (also run automatically by CTest under label "swig")
PYTHONPATH=build/swig python3 tests/test_swig_smoke.py
```

The build produces `build/swig/_libe3py.so` and the matching `libe3py.py` shim. Python users can construct an `E3Config`, flip its `role` to `DAPP`, instantiate an `E3Agent`, and call the dApp verbs. The minimal seam intentionally **does not** wrap the encoders (`Asn1E3Encoder` / `JsonE3Encoder`) — per-SM encoding stays in Python, so existing `spear-dApp` SM decoders work unchanged. See `swig/libe3.i` for the exposed surface.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the original E3AP implementation in OpenAirInterface by Andrea Lacava - Northeastern University
- The E3AP is a protocol inspired by O-RAN E2AP design patterns with due extensions for dApp-xApp management and synch
- The code rationale and architecture is partially insipired by the e2sim and ns-O-RAN projects

This work has been partially supported by OUSD(R&E) through Army Research Laboratory Cooperative Agreement Number W911NF-24-2-0065.
