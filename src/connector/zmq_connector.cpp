/**
 * @file zmq_connector.cpp
 * @brief ZeroMQ connector implementation
 *
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zmq_connector.hpp"
#include "libe3/logger.hpp"
#include <zmq.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <grp.h>
#include <cerrno>
#include <chrono>
#include <thread>

namespace libe3 {

namespace {
constexpr const char* LOG_TAG = "ZmqConn";
constexpr const char* IPC_BASE_DIR = "/tmp/dapps";
constexpr int RECV_TIMEOUT_MS = 500;  // Timeout for graceful shutdown
constexpr int PUB_CONNECT_SETTLE_MS = 200;  // PUB/SUB slow-joiner settle (client outbound)
}

ZmqE3Connector::ZmqE3Connector(
    E3TransportLayer transport_layer,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint,
    uint16_t setup_port,
    uint16_t inbound_port,
    uint16_t outbound_port,
    size_t io_threads
)
    : transport_layer_(transport_layer)
    , io_threads_(io_threads)
    , setup_port_(setup_port)
    , inbound_port_(inbound_port)
    , outbound_port_(outbound_port)
{
    // For TCP, derive the endpoint strings from the port numbers. The
    // bind/connect-direction adjustment is handled at the call sites below
    // (RAN binds tcp://*:N; dApp connects to tcp://127.0.0.1:N). For IPC
    // both sides use the same ipc:// string.
    if (transport_layer == E3TransportLayer::TCP) {
        setup_endpoint_ = "tcp://*:" + std::to_string(setup_port);
        inbound_endpoint_ = "tcp://*:" + std::to_string(inbound_port);
        outbound_endpoint_ = "tcp://*:" + std::to_string(outbound_port);
    } else {
        setup_endpoint_ = setup_endpoint;
        inbound_endpoint_ = inbound_endpoint;
        outbound_endpoint_ = outbound_endpoint;
    }
    
    E3_LOG_INFO(LOG_TAG) << "Creating ZMQ connector";
    E3_LOG_DEBUG(LOG_TAG) << "  Setup endpoint: " << setup_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Inbound endpoint: " << inbound_endpoint_;
    E3_LOG_DEBUG(LOG_TAG) << "  Outbound endpoint: " << outbound_endpoint_;
}

ZmqE3Connector::~ZmqE3Connector() {
    dispose();
}

ErrorCode ZmqE3Connector::setup_initial_connection() {
    // Create ZMQ context
    context_ = zmq_ctx_new();
    if (!context_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create ZMQ context";
        return ErrorCode::CONNECTION_FAILED;
    }
    
    // Set I/O threads
    if (zmq_ctx_set(context_, ZMQ_IO_THREADS, static_cast<int>(io_threads_)) != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to set ZMQ I/O threads: " << zmq_strerror(errno);
        zmq_ctx_destroy(context_);
        context_ = nullptr;
        return ErrorCode::CONNECTION_FAILED;
    }
    
    // Create IPC directory if using IPC transport
    if (transport_layer_ == E3TransportLayer::IPC) {
        struct stat st{};
        if (stat(IPC_BASE_DIR, &st) == -1) {
            if (mkdir(IPC_BASE_DIR, 0777) == -1) {
                E3_LOG_ERROR(LOG_TAG) << "Failed to create IPC directory: " << strerror(errno);
                return ErrorCode::CONNECTION_FAILED;
            }
            E3_LOG_INFO(LOG_TAG) << "Created IPC directory: " << IPC_BASE_DIR;
        }
        
        // Set directory permissions
        if (chmod(IPC_BASE_DIR, 0777) != 0) {
            E3_LOG_WARN(LOG_TAG) << "Failed to set IPC directory permissions";
        }
        
        // Try to change group to "dapp"
        struct group* grp = getgrnam("dapp");
        if (grp) {
            if (chown(IPC_BASE_DIR, static_cast<uid_t>(-1), grp->gr_gid) == -1) {
                E3_LOG_WARN(LOG_TAG) << "Failed to change IPC directory group";
            }
        }
    }
    
    // Create REP socket for setup
    setup_socket_ = zmq_socket(context_, ZMQ_REP);
    if (!setup_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create setup socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    // Set receive timeout to allow graceful shutdown
    int recv_timeout = RECV_TIMEOUT_MS;
    zmq_setsockopt(setup_socket_, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    
    int ret = zmq_bind(setup_socket_, setup_endpoint_.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind setup socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    // Set IPC permissions if needed
    if (transport_layer_ == E3TransportLayer::IPC) {
        setup_ipc_permissions(setup_endpoint_);
    }
    
    E3_LOG_INFO(LOG_TAG) << "Setup socket bound to " << setup_endpoint_;
    connected_ = true;
    
    return ErrorCode::SUCCESS;
}

int ZmqE3Connector::recv_setup_request(std::vector<uint8_t>& buffer) {
    if (!setup_socket_) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    int ret = zmq_recv(setup_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        if (errno == EAGAIN) {
            // Timeout - return 0 to indicate no data (allows shutdown check)
            return 0;
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive setup request: " << zmq_strerror(errno);
        reset_setup_socket();
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(static_cast<size_t>(ret));
    E3_LOG_DEBUG(LOG_TAG) << "Received setup request: " << ret << " bytes";
    return ret;
}

ErrorCode ZmqE3Connector::send_response(const std::vector<uint8_t>& data) {
    if (!setup_socket_) {
        return ErrorCode::NOT_CONNECTED;
    }
    
    int ret = zmq_send(setup_socket_, data.data(), data.size(), 0);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send response: " << zmq_strerror(errno);
        reset_setup_socket();
        return ErrorCode::TRANSPORT_ERROR;
    }
    
    E3_LOG_DEBUG(LOG_TAG) << "Sent response: " << data.size() << " bytes";
    return ErrorCode::SUCCESS;
}

ErrorCode ZmqE3Connector::setup_inbound_connection() {
    if (!context_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    inbound_socket_ = zmq_socket(context_, ZMQ_SUB);
    if (!inbound_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create inbound socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    // Subscribe to all messages
    zmq_setsockopt(inbound_socket_, ZMQ_SUBSCRIBE, "", 0);

    // No ZMQ_CONFLATE: multiple dApps multiplex onto this SUB; conflating would
    // drop all but the newest message and starve all but one dApp (issue #15).

    // Set receive timeout to allow graceful shutdown
    int recv_timeout = RECV_TIMEOUT_MS;
    zmq_setsockopt(inbound_socket_, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    
    int ret = zmq_bind(inbound_socket_, inbound_endpoint_.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind inbound socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    if (transport_layer_ == E3TransportLayer::IPC) {
        setup_ipc_permissions(inbound_endpoint_);
    }
    
    E3_LOG_INFO(LOG_TAG) << "Inbound socket bound to " << inbound_endpoint_;
    return ErrorCode::SUCCESS;
}

int ZmqE3Connector::receive(std::vector<uint8_t>& buffer) {
    if (!inbound_socket_) {
        return static_cast<int>(ErrorCode::NOT_CONNECTED);
    }
    
    buffer.resize(DEFAULT_BUFFER_SIZE);
    int ret = zmq_recv(inbound_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        if (errno == EAGAIN) {
            // Timeout - return 0 to indicate no data (allows shutdown check)
            return 0;
        }
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive: " << zmq_strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    
    buffer.resize(static_cast<size_t>(ret));
    E3_LOG_TRACE(LOG_TAG) << "Received: " << ret << " bytes";
    return ret;
}

ErrorCode ZmqE3Connector::setup_outbound_connection() {
    if (!context_) {
        return ErrorCode::NOT_INITIALIZED;
    }
    
    outbound_socket_ = zmq_socket(context_, ZMQ_PUB);
    if (!outbound_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create outbound socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    int ret = zmq_bind(outbound_socket_, outbound_endpoint_.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to bind outbound socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    
    if (transport_layer_ == E3TransportLayer::IPC) {
        setup_ipc_permissions(outbound_endpoint_);
    }
    
    E3_LOG_INFO(LOG_TAG) << "Outbound socket bound to " << outbound_endpoint_;
    return ErrorCode::SUCCESS;
}

ErrorCode ZmqE3Connector::send(const std::vector<uint8_t>& data) {
    if (!outbound_socket_) {
        return ErrorCode::NOT_CONNECTED;
    }
    
    int ret = zmq_send(outbound_socket_, data.data(), data.size(), 0);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send: " << zmq_strerror(errno);
        return ErrorCode::TRANSPORT_ERROR;
    }
    
    E3_LOG_TRACE(LOG_TAG) << "Sent: " << data.size() << " bytes";
    return ErrorCode::SUCCESS;
}

namespace {
// Convert a bind-style endpoint ("tcp://*:9990") to a connect-style one
// ("tcp://127.0.0.1:9990"). IPC endpoints are returned unchanged.
std::string to_connect_endpoint(const std::string& bind_ep) {
    const std::string prefix = "tcp://*:";
    if (bind_ep.compare(0, prefix.size(), prefix) == 0) {
        return "tcp://127.0.0.1:" + bind_ep.substr(prefix.size());
    }
    return bind_ep;
}
}  // anonymous namespace

// ===========================================================================
// Client-side (dApp role) implementations
//
// The dApp ROLE inverts the bind/connect direction on all three channels:
//   - setup_endpoint    : REQ connect (RAN binds REP)
//   - publisher_endpoint: SUB connect (RAN binds PUB) — this is our INBOUND
//   - subscriber_endpoint: PUB connect (RAN binds SUB) — this is our OUTBOUND
// Socket TYPES on the inbound/outbound pair are unchanged; only the
// bind/connect direction flips and the role of the two endpoints swaps.
// ===========================================================================

ErrorCode ZmqE3Connector::setup_initial_connection_client() {
    // Create ZMQ context (same as RAN-side init)
    context_ = zmq_ctx_new();
    if (!context_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create ZMQ context";
        return ErrorCode::CONNECTION_FAILED;
    }
    if (zmq_ctx_set(context_, ZMQ_IO_THREADS, static_cast<int>(io_threads_)) != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to set ZMQ I/O threads: " << zmq_strerror(errno);
        zmq_ctx_destroy(context_);
        context_ = nullptr;
        return ErrorCode::CONNECTION_FAILED;
    }

    // REQ socket — the dApp INITIATES setup
    setup_socket_ = zmq_socket(context_, ZMQ_REQ);
    if (!setup_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create REQ setup socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }

    int recv_timeout = RECV_TIMEOUT_MS;
    zmq_setsockopt(setup_socket_, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    int linger = 0;  // Don't block on close if RAN never replied
    zmq_setsockopt(setup_socket_, ZMQ_LINGER, &linger, sizeof(linger));

    const std::string setup_cep = to_connect_endpoint(setup_endpoint_);
    int ret = zmq_connect(setup_socket_, setup_cep.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect setup socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    E3_LOG_INFO(LOG_TAG) << "Setup REQ socket connected to " << setup_cep;
    connected_ = true;
    return ErrorCode::SUCCESS;
}

ErrorCode ZmqE3Connector::send_setup_request_client(const std::vector<uint8_t>& data) {
    if (!setup_socket_) return ErrorCode::NOT_CONNECTED;
    int ret = zmq_send(setup_socket_, data.data(), data.size(), 0);
    if (ret < 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to send setup request: " << zmq_strerror(errno);
        return ErrorCode::TRANSPORT_ERROR;
    }
    E3_LOG_DEBUG(LOG_TAG) << "Sent SetupRequest: " << data.size() << " bytes";
    return ErrorCode::SUCCESS;
}

int ZmqE3Connector::recv_setup_response_client(std::vector<uint8_t>& buffer) {
    if (!setup_socket_) return static_cast<int>(ErrorCode::NOT_CONNECTED);
    buffer.resize(DEFAULT_BUFFER_SIZE);
    int ret = zmq_recv(setup_socket_, buffer.data(), buffer.size(), 0);
    if (ret < 0) {
        if (errno == EAGAIN) return 0;
        E3_LOG_ERROR(LOG_TAG) << "Failed to receive setup response: " << zmq_strerror(errno);
        return static_cast<int>(ErrorCode::TRANSPORT_ERROR);
    }
    buffer.resize(static_cast<size_t>(ret));
    E3_LOG_DEBUG(LOG_TAG) << "Received SetupResponse: " << ret << " bytes";
    return ret;
}

ErrorCode ZmqE3Connector::setup_inbound_connection_client() {
    if (!context_) return ErrorCode::NOT_INITIALIZED;

    // dApp INBOUND = RAN's publisher_endpoint (where RAN PUB-binds)
    inbound_socket_ = zmq_socket(context_, ZMQ_SUB);
    if (!inbound_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create inbound SUB socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    zmq_setsockopt(inbound_socket_, ZMQ_SUBSCRIBE, "", 0);
    // No ZMQ_CONFLATE: the RAN broadcasts per-dApp indications over one PUB and
    // each dApp filters its own; conflating would starve all but one (issue #15).
    int recv_timeout = RECV_TIMEOUT_MS;
    zmq_setsockopt(inbound_socket_, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    const std::string inbound_cep = to_connect_endpoint(outbound_endpoint_);
    int ret = zmq_connect(inbound_socket_, inbound_cep.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect inbound SUB to "
                              << inbound_cep << ": " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    E3_LOG_INFO(LOG_TAG) << "Inbound SUB socket connected to " << inbound_cep;
    return ErrorCode::SUCCESS;
}

ErrorCode ZmqE3Connector::setup_outbound_connection_client() {
    if (!context_) return ErrorCode::NOT_INITIALIZED;

    // dApp OUTBOUND = RAN's subscriber_endpoint (where RAN SUB-binds)
    outbound_socket_ = zmq_socket(context_, ZMQ_PUB);
    if (!outbound_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to create outbound PUB socket: " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }

    const std::string outbound_cep = to_connect_endpoint(inbound_endpoint_);
    int ret = zmq_connect(outbound_socket_, outbound_cep.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to connect outbound PUB to "
                              << outbound_cep << ": " << zmq_strerror(errno);
        return ErrorCode::CONNECTION_FAILED;
    }
    E3_LOG_INFO(LOG_TAG) << "Outbound PUB socket connected to " << outbound_cep;

    // ZMQ PUB/SUB slow-joiner: a freshly connected PUB silently drops outgoing
    // messages until the peer SUB's subscription has propagated to it. The
    // dApp's first outbound message is the subscription request; without a
    // brief settle it can be lost — reliably so for a second peer over TCP,
    // where connection establishment is slower. Wait for the link to settle.
    std::this_thread::sleep_for(std::chrono::milliseconds(PUB_CONNECT_SETTLE_MS));
    return ErrorCode::SUCCESS;
}

void ZmqE3Connector::dispose() {
    E3_LOG_DEBUG(LOG_TAG) << "Disposing ZMQ connector";
    
    if (setup_socket_) {
        zmq_close(setup_socket_);
        setup_socket_ = nullptr;
    }
    
    if (inbound_socket_) {
        zmq_close(inbound_socket_);
        inbound_socket_ = nullptr;
    }
    
    if (outbound_socket_) {
        zmq_close(outbound_socket_);
        outbound_socket_ = nullptr;
    }
    
    if (context_) {
        zmq_ctx_destroy(context_);
        context_ = nullptr;
    }
    
    // Clean up IPC files if using IPC transport AND we own them (RAN side
    // binds, which creates the socket files; dApp side connects, so it has
    // nothing to unlink).
    if (transport_layer_ == E3TransportLayer::IPC && role_ == E3Role::RAN) {
        // Extract file paths from IPC endpoints (ipc:///path)
        auto extract_path = [](const std::string& endpoint) -> std::string {
            const std::string prefix = "ipc://";
            if (endpoint.compare(0, prefix.length(), prefix) == 0) {
                return endpoint.substr(prefix.length());
            }
            return "";
        };

        std::string path;
        path = extract_path(setup_endpoint_);
        if (!path.empty()) unlink(path.c_str());

        path = extract_path(inbound_endpoint_);
        if (!path.empty()) unlink(path.c_str());

        path = extract_path(outbound_endpoint_);
        if (!path.empty()) unlink(path.c_str());

        rmdir(IPC_BASE_DIR);
    }
    
    connected_ = false;
    E3_LOG_INFO(LOG_TAG) << "ZMQ connector disposed";
}

void ZmqE3Connector::shutdown() {
    // ZMQ uses ZMQ_RCVTIMEO for timeout-based shutdown.
    // The receive loops will wake up periodically and check should_stop_.
    // No additional action needed here.
    E3_LOG_DEBUG(LOG_TAG) << "ZMQ connector shutdown requested";
}

void ZmqE3Connector::setup_ipc_permissions(const std::string& endpoint) {
    // Extract file path from IPC endpoint (ipc:///path)
    const std::string prefix = "ipc://";
    if (endpoint.compare(0, prefix.length(), prefix) != 0) {
        return;
    }
    
    std::string path = endpoint.substr(prefix.length());
    
    if (chmod(path.c_str(), 0666) == -1) {
        E3_LOG_WARN(LOG_TAG) << "Failed to set permissions on " << path << ": " << strerror(errno);
    } else {
        E3_LOG_DEBUG(LOG_TAG) << "Set permissions on " << path;
    }
}

bool ZmqE3Connector::reset_setup_socket() {
    E3_LOG_WARN(LOG_TAG) << "Resetting setup socket after error";

    if (setup_socket_) {
        zmq_close(setup_socket_);
        setup_socket_ = nullptr;
    }

    if (!context_) {
        E3_LOG_ERROR(LOG_TAG) << "Cannot reset setup socket without context";
        return false;
    }

    setup_socket_ = zmq_socket(context_, ZMQ_REP);
    if (!setup_socket_) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to recreate setup socket: " << zmq_strerror(errno);
        return false;
    }

    int recv_timeout = RECV_TIMEOUT_MS;
    zmq_setsockopt(setup_socket_, ZMQ_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    int ret = zmq_bind(setup_socket_, setup_endpoint_.c_str());
    if (ret != 0) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to rebind setup socket: " << zmq_strerror(errno);
        zmq_close(setup_socket_);
        setup_socket_ = nullptr;
        return false;
    }

    if (transport_layer_ == E3TransportLayer::IPC) {
        setup_ipc_permissions(setup_endpoint_);
    }

    E3_LOG_INFO(LOG_TAG) << "Setup socket reset and bound to " << setup_endpoint_;
    return true;
}

} // namespace libe3
