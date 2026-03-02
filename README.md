# libe3 - Vendor-Neutral E3AP C++ Library

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Version](https://img.shields.io/badge/version-0.0.1-green.svg)](VERSION)

**libe3** is a standalone, vendor-neutral C++ library for implementing the E3AP (E3 Application Protocol) in RAN functions such as DU, CU-CP, and CU-UP. It provides a clean object-oriented API for RAN vendors while hiding transport, encoding, and protocol complexity.

## Features

- **Vendor-Neutral API**: Clean E3Agent facade that hides implementation details
- **Multiple Transports**: Support for ZeroMQ and POSIX sockets (TCP, SCTP, Unix Domain)
- **Multiple Encodings**: ASN.1 APER (primary) and JSON encoders
- **Service Model Extensions**: Easy-to-implement SM interface for custom functionality
- **Simulation Mode**: Test without real RAN infrastructure
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
| `LIBE3_ENABLE_ZMQ` | ON | Enable ZeroMQ transport |
| `LIBE3_ENABLE_ASN1` | ON | Enable ASN.1 encoding |
| `LIBE3_ENABLE_JSON` | OFF | Enable JSON encoding (mutually exclusive with ASN.1) |
| `LIBE3_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `LIBE3_ENABLE_TSAN` | OFF | Enable ThreadSanitizer |

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
| `send_indication(dapp_id, ran_function_id, data)` | Send an indication to a specific dApp |
| `get_registered_dapps()` | Get list of registered dApp IDs |
| `get_dapp_subscriptions(dapp_id)` | Get subscriptions for a dApp |
| `get_ran_function_subscribers(ran_function_id)` | Get subscribers for a RAN function |
| `config()` | Access current `E3Config` |
| `dapp_count()` | Number of registered dApps |
| `subscription_count()` | Number of active subscriptions |

### E3Config

The `E3Config` structure contains agent configuration and defaults suitable
for local development (IPC) or production deployments.

| Field | Type | Description |
|-------|------|-------------|
| `ran_identifier` | string | Unique RAN identifier |
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

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the original E3AP implementation in OpenAirInterface by Andrea Lacava - Northeastern University
- The E3AP is a protocol inspired by O-RAN E2AP design patterns with due extensions for dApp-xApp management and synch
- The code rationale and architecture is partially insipired by the e2sim and ns-O-RAN projects

This work has been partially supported by OUSD(R&E) through Army Research Laboratory Cooperative Agreement Number W911NF-24-2-0065.
