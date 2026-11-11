/**
 * @file socket_link.hpp
 * @brief What the POSIX and the io_uring link share: the endpoint, the sockets and the framing
 *
 * Both links put the same bytes on the same sockets; they differ only in how they wait for
 * and move those bytes. Keeping the endpoint parsing, the socket options, the admission rules
 * and the framing here, once, is what makes a dApp of one link work with a RAN of the other.
 * Internal: not installed.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SOCKET_LINK_HPP
#define LIBE3_SOCKET_LINK_HPP

#include "libe3/e3_connector.hpp"
#include "libe3/types.hpp"

#include <sys/socket.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace libe3 {
namespace socket_link {

constexpr const char* IPC_BASE_DIR = "/tmp/dapps";   ///< Made by a RAN that listens on IPC, open to all
/// What E3Config holds for setup_endpoint unless the program sets it. The ZeroMQ link shares
/// the string, so the socket links map it to their own default instead of changing it.
constexpr const char* SHARED_DEFAULT_ENDPOINT = "ipc:///tmp/dapps/setup";
constexpr const char* DEFAULT_IPC_PATH = "/tmp/dapps/e3";   ///< The IPC endpoint of the socket links by default
constexpr int SEND_TIMEOUT_MS = 500;          ///< A send that waits this long drops its peer
constexpr int CONNECT_ATTEMPTS = 10;          ///< The dApp waits for a RAN that is not up yet
constexpr int CONNECT_PAUSE_MS = 200;         ///< Between two of those attempts
constexpr int LISTEN_BACKLOG = 64;            ///< Connections the kernel holds before they are accepted
constexpr size_t MAX_PDU = MAX_PROTOCOL_DATA_SIZE * 2;   ///< One limit for every transport and direction
constexpr uint32_t SCTP_PPID = 0x00E3A001;    ///< Project-private; no payload protocol id is registered for E3AP
constexpr int SCTP_PROCEDURE_STREAM = 0;      ///< SCTP stream of everything but an indication
constexpr int SCTP_INDICATION_STREAM = 1;     ///< SCTP stream of the indications a RAN sends
constexpr size_t TCP_PREFIX_BYTES = 4;        ///< TCP: each PDU follows its length, big-endian
/// SCTP retransmission timeout of each association, in ms: first, smallest and largest. Linux's own
/// smallest, 1 s, is longer than SEND_TIMEOUT_MS, so a burst of large PDUs that closes the peer's
/// window could stall a send until the RAN dropped the dApp; E3 runs between neighbours.
constexpr uint32_t SCTP_RTO_INITIAL_MS = 120;
constexpr uint32_t SCTP_RTO_MIN_MS = 20;      ///< See SCTP_RTO_INITIAL_MS
constexpr uint32_t SCTP_RTO_MAX_MS = 250;     ///< Half SEND_TIMEOUT_MS: one backed-off wait cannot reach it

/// Which end of a connection a socket is, as the options it needs differ.
enum class SocketKind {
    LISTENER,  ///< A RAN's listening socket
    CLIENT,    ///< A dApp's socket, before it connects
    ACCEPTED   ///< A RAN's socket for one accepted connection
};

/// Where the endpoint string says to listen or connect.
struct Target {
    std::string host;  ///< Empty: the default (any address to listen, loopback to connect)
    std::string port;  ///< Decimal port
};

/// How a socket is opened and tuned, the same for both links.
struct SocketSpec {
    E3TransportLayer transport;   ///< SCTP, TCP or IPC
    std::string path;             ///< IPC: the socket file
    Target target;                ///< TCP and SCTP: host and port
    PosixOptions options;         ///< Buffers, delayed SACK, admission
    const char* log_tag;          ///< The connector's log tag
};

/**
 * @brief The endpoint without a URI scheme ("ipc://", "tcp://").
 * @param endpoint An endpoint, with or without a scheme
 * @return What follows "://", or the endpoint as it is
 */
std::string strip_scheme(const std::string& endpoint);

/**
 * @brief The endpoint a socket link reports and uses: an IPC path, or the URI as given.
 *
 * IPC: the "ipc://" prefix is optional, and the library's shared default is replaced by
 * /tmp/dapps/e3.
 * @param transport The transport of the link
 * @param endpoint The endpoint the program configured
 * @return The IPC path, or for TCP and SCTP the endpoint unchanged
 */
std::string link_endpoint(E3TransportLayer transport, const std::string& endpoint);

/**
 * @brief Host and port of "tcp://host:port" or "sctp://host:port" (a host in brackets for IPv6).
 *
 * Anything else means the default host and @p port.
 * @param endpoint_uri The endpoint the program configured
 * @param port The port to use when the endpoint names none
 * @return The host (empty for the default) and the port
 */
Target parse_target(const std::string& endpoint_uri, uint16_t port);

/**
 * @brief A socket of the link's kind; SCTP without the build support fails loudly.
 * @param spec The link
 * @param family AF_INET or AF_INET6 (IPC ignores it)
 * @return The socket, or -1 with errno set
 */
int make_socket(const SocketSpec& spec, int family);

/**
 * @brief Set what a socket of @p kind needs: timeouts, no-delay, SCTP streams, buffers, SACK and timers.
 * @param spec The link
 * @param fd The socket
 * @param kind Which end of a connection it is
 */
void tune_socket(const SocketSpec& spec, int fd, SocketKind kind);

/**
 * @brief Bind and listen on the endpoint, IPv4 and IPv6 alike when no host is named.
 * @param spec The link
 * @param nonblocking Set O_NONBLOCK on the listener (the POSIX link polls; io_uring must not have it)
 * @return The listening socket, or -1
 */
int listen_on(const SocketSpec& spec, bool nonblocking);

/**
 * @brief Connect to the endpoint once.
 * @param spec The link
 * @return The connected socket, or -1 with errno set
 */
int connect_to(const SocketSpec& spec);

/**
 * @brief Whether a peer that just connected from @p address may stay (RAN role).
 * @param spec The link, with the peer list and the cap
 * @param address Where the peer connected from
 * @param connected How many peers are connected already, for the association cap
 * @return true to keep the connection, false to close it
 */
bool admit(const SocketSpec& spec, const sockaddr_storage& address, size_t connected);

/**
 * @brief The text of a peer's address, with an IPv4-mapped IPv6 address shown as IPv4.
 * @param address The peer's address
 * @return Its literal text, or empty for a family other than IPv4 and IPv6
 */
std::string address_text(const sockaddr_storage& address);

/** @brief What the bytes at the front of a TCP stream hold. */
enum class StreamFrame {
    PDU,        ///< A whole PDU: its length is in @p pdu_size, its bytes follow the prefix
    NEED_MORE,  ///< Not yet a whole PDU
    INVALID     ///< A length of zero or above MAX_PDU: the stream cannot be trusted any more
};

/**
 * @brief Read the PDU at the front of buffered TCP bytes.
 * @param data The bytes, prefix first
 * @param size How many there are
 * @param pdu_size Set to the PDU's length when the result is PDU (the frame is TCP_PREFIX_BYTES + it),
 *        and to the length the prefix gave when it is INVALID
 * @return What the bytes hold
 */
StreamFrame parse_stream_frame(const uint8_t* data, size_t size, size_t& pdu_size);

/**
 * @brief The TCP prefix of a PDU of @p size bytes.
 * @param prefix Where the four octets go
 * @param size The length of the PDU
 */
void write_stream_prefix(uint8_t (&prefix)[TCP_PREFIX_BYTES], size_t size);

/** @brief Room for the control message an SCTP send carries. */
constexpr size_t SCTP_SEND_CONTROL_BYTES = 64;

/** @brief Room for the control message an SCTP receive asks for. */
constexpr size_t SCTP_RECEIVE_CONTROL_BYTES = 64;

/**
 * @brief Put the SCTP send info (payload protocol id, stream for @p kind) into @p msg.
 * @param msg The message to send; its control fields are set
 * @param control Where the control message is built; at least SCTP_SEND_CONTROL_BYTES, aligned for cmsghdr
 * @param kind What the PDU is for, which picks the stream
 */
void set_sctp_send_info(struct msghdr& msg, void* control, PduClass kind);

/**
 * @brief Whether what an SCTP recvmsg returned is a notification of the stack, not data.
 * @param msg The message as recvmsg filled it
 * @return true for a notification
 */
bool sctp_is_notification(const struct msghdr& msg);

/**
 * @brief Whether a received SCTP message may be taken, by its payload protocol id.
 *
 * 0 and the id libe3 stamps are taken. A message without the receive info is taken too.
 * @param msg The message as recvmsg filled it, with its control data
 * @param log_tag Where to log a refusal
 * @return false for another payload protocol id
 */
bool sctp_ppid_acceptable(const struct msghdr& msg, const char* log_tag);

}  // namespace socket_link
}  // namespace libe3

#endif  // LIBE3_SOCKET_LINK_HPP
