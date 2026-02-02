# libe3 - Vendor-Neutral E3AP C++ Library

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)

**libe3** is a standalone, vendor-neutral C++ library for implementing the E3AP (E3 Application Protocol) in RAN functions such as DU, CU-CP, and CU-UP. It provides a clean object-oriented API for RAN vendors while hiding transport, encoding, and protocol complexity.

## Features

- **Vendor-Neutral API**: Clean E3Agent facade that hides implementation details
- **Multiple Transports**: Support for ZeroMQ and POSIX sockets (TCP, SCTP, Unix Domain)
- **Multiple Encodings**: JSON (built-in) with ASN.1 placeholder
- **Service Model Extensions**: Easy-to-implement SM interface for custom functionality
- **Simulation Mode**: Test without real RAN infrastructure
- **Thread-Safe**: Proper synchronization for concurrent dApp operations
- **Modern C++17**: Uses std::variant, std::optional, RAII patterns
- **Lightweight**: No heavy external dependencies

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          RAN Application                             │
│                    (DU, CU-CP, CU-UP, etc.)                         │
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
│ - PosixConnector  │  │ - Asn1Encoder*    │
└───────────────────┘  └───────────────────┘

                         ┌─────────────────┐
                         │  ServiceModel   │  ◄── SM Extension Point
                         │   Interface     │
                         └─────────────────┘
```

## Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- pthreads
- (Optional) ZeroMQ for ZMQ transport

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
    
    // Create and configure agent
    libe3::E3Agent agent(std::move(config));
    
    // Set up event callbacks
    agent.set_control_callback([](uint32_t dapp_id, uint32_t ran_func,
                                  const std::vector<uint8_t>& data) {
        std::cout << "Control action from dApp " << dapp_id << "\n";
    });
    
    // Initialize and start
    agent.init();
    agent.start();
    
    // ... run your application ...
    
    agent.stop();
    return 0;
}
```

### Custom Service Model

```cpp
#include <libe3/libe3.hpp>

class MyKpmServiceModel : public libe3::ServiceModel {
public:
    uint32_t id() const override { return 100; }
    std::string name() const override { return "KPM"; }
    std::string version() const override { return "2.0.0"; }
    
    std::vector<uint32_t> ran_function_ids() const override {
        return {100};
    }
    
    libe3::ErrorCode start() override {
        // Start collecting KPM data
        return libe3::ErrorCode::SUCCESS;
    }
    
    libe3::ErrorCode stop() override {
        return libe3::ErrorCode::SUCCESS;
    }
    
    bool is_running() const override { return running_; }
    
    std::optional<std::vector<uint8_t>> poll_indication_data() override {
        // Return collected KPM data
        return std::nullopt;
    }
    
    libe3::ErrorCode handle_control_action(const std::vector<uint8_t>& data) override {
        // Handle control actions
        return libe3::ErrorCode::SUCCESS;
    }
    
    void set_indication_callback(IndicationCallback cb) override {
        callback_ = std::move(cb);
    }

private:
    bool running_ = false;
    IndicationCallback callback_;
};

// Register with agent
agent.register_sm(std::make_unique<MyKpmServiceModel>());
```

### Simulation Mode

For testing without real RAN infrastructure:

```cpp
libe3::E3Config config;
config.ran_identifier = "simulated-ran";
config.simulation_mode = true;  // Enable simulation

libe3::E3Agent agent(std::move(config));
// Agent works without actual network connectivity
```

## API Reference

### E3Agent

The main facade class for RAN vendors.

| Method | Description |
|--------|-------------|
| `init()` | Initialize the agent |
| `start()` | Start processing threads |
| `stop()` | Stop the agent |
| `state()` | Get current state |
| `register_sm(sm)` | Register a Service Model |
| `set_control_callback(cb)` | Set control action handler |
| `set_indication_callback(cb)` | Set indication handler |
| `send_indication(dapp_id, data)` | Send indication to specific dApp |
| `get_registered_dapps()` | Get list of registered dApps |
| `dapp_count()` | Number of registered dApps |
| `subscription_count()` | Number of active subscriptions |

### E3Config

Configuration structure for the agent.

| Field | Type | Description |
|-------|------|-------------|
| `ran_identifier` | string | Unique RAN identifier |
| `link_layer` | E3LinkLayer | ZMQ, POSIX |
| `transport_layer` | E3TransportLayer | SCTP, TCP, IPC |
| `encoding` | EncodingFormat | JSON, ASN1 |
| `simulation_mode` | bool | Enable simulation mode |
| `setup_endpoint` | string | Setup connection endpoint |
| `subscriber_endpoint` | string | Subscriber endpoint |
| `publisher_endpoint` | string | Publisher endpoint |

### ServiceModel

Interface for implementing custom service models.

| Method | Description |
|--------|-------------|
| `id()` | Unique SM identifier |
| `name()` | SM name (e.g., "KPM") |
| `version()` | SM version string |
| `ran_function_ids()` | List of RAN function IDs |
| `start()` | Start the SM |
| `stop()` | Stop the SM |
| `is_running()` | Check if running |
| `poll_indication_data()` | Poll for indication data |
| `handle_control_action(data)` | Handle control action |
| `set_indication_callback(cb)` | Set indication callback |

## Directory Structure

```
libe3/
├── CMakeLists.txt           # Main build configuration
├── README.md                # This file
├── LICENSE                  # Apache 2.0 license
├── include/
│   └── libe3/
│       ├── libe3.hpp        # Umbrella header
│       ├── types.hpp        # Type definitions
│       ├── e3_agent.hpp     # Main facade
│       ├── e3_connector.hpp # Transport interface
│       ├── e3_encoder.hpp   # Encoder interface
│       └── ...
├── src/
│   ├── core/                # Core implementations
│   ├── connector/           # Transport implementations
│   └── encoder/             # Encoder implementations
├── tests/                   # Unit tests
└── examples/                # Example applications
```

## Integration with spear-dApp

libe3 is designed to integrate with the SPEAR dApp framework. See the [spear-dApp documentation](../spear-dApp/README.md) for details on building dApps that communicate with libe3-based RAN functions.

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the E3AP implementation in OpenAirInterface
- Inspired by O-RAN E2AP design patterns
