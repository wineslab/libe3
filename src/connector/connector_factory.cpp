/**
 * @file connector_factory.cpp
 * @brief Factory for creating connector instances
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_connector.hpp"
#if LIBE3_HAS_ZMQ
#include "zmq_connector.hpp"
#endif
#include "posix_connector.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

std::unique_ptr<E3Connector> create_connector(
    E3LinkLayer link_layer,
    E3TransportLayer transport_layer,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint,
    uint16_t setup_port,
    uint16_t inbound_port,
    uint16_t outbound_port,
    size_t io_threads,
    E3Role role
) {
    std::unique_ptr<E3Connector> conn;
    switch (link_layer) {
        case E3LinkLayer::ZMQ:
#if LIBE3_HAS_ZMQ
            conn = std::make_unique<ZmqE3Connector>(
                transport_layer, setup_endpoint, inbound_endpoint, outbound_endpoint,
                setup_port, inbound_port, outbound_port, io_threads
            );
            break;
#else
            // zmq_connector.cpp is not compiled in this configuration, so this
            // is a configuration error at runtime rather than a link error at
            // build time. E3LinkLayer::ZMQ stays in the enum either way: the
            // value can arrive from a config file the build knows nothing about.
            E3_LOG_ERROR("ConnFactory")
                << "ZMQ link layer requested but libe3 was built with "
                   "LIBE3_ENABLE_ZMQ=OFF";
            return nullptr;
#endif

        case E3LinkLayer::POSIX:
            conn = std::make_unique<PosixE3Connector>(
                transport_layer, setup_endpoint, inbound_endpoint, outbound_endpoint,
                setup_port, inbound_port, outbound_port
            );
            break;

        default:
            E3_LOG_ERROR("ConnFactory") << "Unsupported link layer: "
                                        << static_cast<int>(link_layer);
            return nullptr;
    }
    if (conn) conn->set_role(role);
    return conn;
}

} // namespace libe3
