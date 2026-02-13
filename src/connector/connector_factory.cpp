/**
 * @file connector_factory.cpp
 * @brief Factory for creating connector instances
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_connector.hpp"
#include "zmq_connector.hpp"
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
    size_t io_threads
) {
    switch (link_layer) {
        case E3LinkLayer::ZMQ:
            return std::make_unique<ZmqE3Connector>(
                transport_layer, setup_endpoint, inbound_endpoint, outbound_endpoint,
                setup_port, inbound_port, outbound_port, io_threads
            );
        
        case E3LinkLayer::POSIX:
            return std::make_unique<PosixE3Connector>(
                transport_layer, setup_endpoint, inbound_endpoint, outbound_endpoint,
                setup_port, inbound_port, outbound_port
            );
        
        default:
            E3_LOG_ERROR("ConnFactory") << "Unsupported link layer: "
                                        << static_cast<int>(link_layer);
            return nullptr;
    }
}

} // namespace libe3
