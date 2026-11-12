# E3AP: integrator guide

For an engineer who adds E3AP to a RAN stack (the E3 Agent side) or to a dApp (the
client side). It is not part of the specification: the specification is the six
numbered files in [README.md](../e3ap/README.md). This guide holds the code pointers, the
configuration, the tests that pin the wire and a list of stale comments.

Paths are in this repository. Line numbers were read from the tree on top of the interop
changes that this branch builds on. They drift. Function names are the stable reference.
The OpenAirInterface side is the E3 agent in
`openair2/E3AP/` of the OpenAirInterface implementation.

## Choose a route

| Route | You get | Pick it when |
|---|---|---|
| **A. Link the library** | the E3 Agent and the dApp, three encodings, ZeroMQ and POSIX links, subscription manager, service model registry | your stack is C or C++, or Python through the bindings |
| **B. Generate code from the grammar** | the ASN.1 structures from `messages/asn1/V1/e3ap-1.0.0.asn1` | you want your own transport and state machine |
| **C. Write the codec by hand** | full control | your stack already has an APER library |

Whatever you choose, the pinned APER vectors in `tests/test_asn1_golden.cpp` are the
acceptance test. The same vectors are in [04-encoding.md](04-encoding.md#worked-examples).

## Configuration of the library

`E3Config` in `include/libe3/types.hpp` selects the role, the link, the encoding and
the endpoints.

| Field | Default | Notes |
|---|---|---|
| `role` | RAN | `E3Role::DAPP` for a dApp |
| `ran_identifier` | empty | an empty value is sent as `unknown` (`e3_interface.cpp:153`) |
| `dapp_name`, `dapp_version`, `vendor` | `libe3-dapp`, `0.0.0`, empty | empty vendor is sent as `unknown` |
| `e3ap_version` | `1.0.0` | sent in every SETUP REQUEST and SETUP RESPONSE. `init()` returns `INVALID_PARAM` if it is not 1 to 32 octets (`e3_interface.cpp:243-254`) |
| `link_layer` | ZMQ | POSIX if the build has no ZeroMQ. URING is the POSIX link driven through io_uring: the same endpoint, sockets and bytes, so every POSIX row of this table holds for it, and a URING peer works with a POSIX one. Linux with liburing only; `init()` fails where the build or the kernel lacks it ([docs/connectors/uring.md](../connectors/uring.md)) |
| `transport_layer` | IPC | SCTP only with `LIBE3_ENABLE_SCTP` |
| `setup_port`, `subscriber_port`, `publisher_port` | 9990, 9999, 9991 | The POSIX link uses `setup_port` alone. ZeroMQ uses all three: `subscriber_*` is the channel into the E3 Agent, `publisher_*` the channel out of it |
| `setup_endpoint`, `subscriber_endpoint`, `publisher_endpoint` | `ipc:///tmp/dapps/setup`, `dapp_socket`, `e3_socket` | The POSIX link uses `setup_endpoint` alone, and its IPC path is `/tmp/dapps/e3` while the field holds the default string (ZeroMQ shares the string, so it is mapped and not changed). For TCP and SCTP a `tcp://host:port` or `sctp://host:port` endpoint (a host in brackets for IPv6) names the host and the port; any other value means every address to listen on, `127.0.0.1` to connect to, and `setup_port` |
| `socket_buffer_bytes` | 2 MiB | POSIX over SCTP only. `SO_SNDBUF` and `SO_RCVBUF`, set on both peers before the association exists. The kernel clamps them to `net.core.wmem_max` and `net.core.rmem_max`. 0 keeps the kernel's size |
| `sctp_delayed_sack` | false | POSIX over SCTP only. false turns delayed SACK off on both peers |
| `allowed_peers` | empty | POSIX over TCP and SCTP, E3 Agent. Literal addresses a dApp may connect from; an IPv4 address is written in dotted form. Empty accepts every address. A connection from any other is closed at once |
| `maximum_associations` | 0 | POSIX, E3 Agent. Most dApps connected at once; a connection beyond it is closed at once. 0 is unlimited |
| `encoding` | ASN.1 if built, else JSON, else ASN.1 | a build with only Protobuf defaults to ASN.1 and fails to start |
| `io_thread_affinity`, `io_thread_niceness` | -1, 0 | Linux only. Apply to the inbound, outbound and report threads |

`connect_timeout_ms`, `recv_timeout_ms`, `send_timeout_ms`, `receive_buffer_size` and
`send_buffer_size` are not read by any code in `src/`. The real values are constants
in the connectors.

## Where each claim comes from

### Identity and links (chapter 01)

| Claim | Code pointer |
|---|---|
| Version string `1.0.0` | `include/libe3/types.hpp:475` |
| The RAN does not check the dApp's version | `e3_interface.cpp:990` (`handle_setup_request` only logs it) |
| E3 Agent binds the three ZeroMQ sockets | `src/connector/zmq_connector.cpp:130-141`, `201-218`, `265-272` |
| dApp connects: REQ, SUB, PUB | `zmq_connector.cpp:356-367`, `449-470`, `484-492` |
| dApp waits 200 ms after connecting the PUB socket (slow joiner) | `zmq_connector.cpp:505` |
| dApp opens a fresh REQ socket for each setup attempt | `zmq_connector.cpp:376-380` |
| ZeroMQ heartbeat 1000 ms, timeout 3000 ms, on the dApp SUB socket only | `zmq_connector.cpp:30-31`, `460-466` |
| IPC directory `/tmp/dapps` is created with mode 0777 | `zmq_connector.cpp:102-127` |
| TCP and SCTP: the E3 Agent listens on all addresses, IPv4 and IPv6, backlog 64, unless the endpoint names a host. The dApp connects to `127.0.0.1` unless the endpoint names a host | `PosixE3Connector::listen_on`, `connect_to` |
| SCTP is a one-to-one stream socket with two streams requested, `SCTP_NODELAY`, buffers and delayed SACK set before the association exists, a retransmission timeout of 120 ms at first and 20 to 250 ms after, payload protocol identifier `0x00E3A001` and a stream chosen from the kind of PDU. Linux's own smallest timeout, 1 s, let a burst of large PDUs stall a send past the 500 ms send timeout, and the E3 Agent then dropped the dApp (7 to 8 times in 100 runs of 2000 PDUs on either socket link; never with these values) | `tune_socket` in `socket_link.cpp`; `write_pdu`, `read_one`; `UringE3Connector::prepare_send`, `consume_into` |
| The dApp retries its connection 10 times at 200 ms intervals, and opens a new connection for each setup attempt | `PosixE3Connector::setup_initial_connection_client`, `send_setup_request_client` |
| Lowest free dApp identifier and subscription identifier | `src/core/subscription_manager.cpp:58-82`, `211` |

### Procedures (chapter 03)

| Claim | Code pointer |
|---|---|
| Setup on the E3 Agent: decode, register, build the list, answer | `src/core/e3_interface.cpp:568-616`, `984-1096` |
| Negative reply for garbage or a wrong PDU type, with the cause protocolError | `e3_interface.cpp:1098-1132` |
| RAN function list limits: data 1 to 32768 octets, 256 identifiers, 64 functions | `e3_interface.cpp:1013-1054`, `src/core/sm_registry.cpp:35-57` |
| Encode failure of the SETUP RESPONSE ends the process | `e3_interface.cpp:1079-1085` |
| Setup on the dApp: 5 attempts, 4 receives of 500 ms each | `e3_interface.cpp:1570-1653` |
| A negative response still gives "success" to the dApp | `e3_interface.cpp:1642`, `1820-1823` |
| Subscription: the registration, the RAN function, the lists and every identifier are checked, and a refusal carries a cause | `e3_interface.cpp:945-978`, `1134-1227` |
| Repeat returns the existing identifier. Renewal after expiry keeps the identifier | `subscription_manager.cpp:187-205` |
| The response lists the request lists for a new subscription and the lists the E3 Agent holds for a repeat. Its interval is the one the service model reports with `granted_periodicity()`, and none when it reports 0, nothing, or more than 60000000 | `e3_interface.cpp:1175-1178`, `1198-1218`; `sm_interface.hpp:97-113` |
| Subscription expiry sweep every 250 ms | `e3_interface.cpp:40`, `837-861` |
| Subscription delete | `e3_interface.cpp:1229-1270` |
| The dApp keeps its own deadline from the response | `e3_interface.cpp:1861-1871` |
| Service model starts with the first subscriber and stops with the last | `subscription_manager.cpp:238-241`, `376-378`, `535-554`; `e3_interface.cpp:2207-2221` |
| dApp control: the registration, the RAN function, the control identifier and a running service model are checked. Otherwise a negative CONTROL RESPONSE with a cause, whether the dApp asked or not | `e3_interface.cpp:1272-1366` |
| The service model answers with `respond_to_control`. The answer goes only if the control had `requestAck`, once, to the dApp that sent it. A control that asks and that the service model fails to take gets a negative answer with the cause unspecified. At most 1024 controls wait for an answer | `e3_interface.cpp:1330-1366`, `1368-1399`; `include/libe3/sm_interface.hpp:186`; `examples/sm_simple/simple_service_model.hpp:176` |
| A dApp reads a CONTROL RESPONSE only if it names its own dApp. The E3 Agent reads one from a registered dApp, for an xApp control | `e3_interface.cpp:1958-1982`, `1984-1996` |
| The dApp handles a deprecated MESSAGE ACK with no dApp filter. An E3 Agent of this version sends none | `e3_interface.cpp:1944-1958` |
| An xApp control that asks and finds no handler on the dApp gets a negative answer from the library, with the first control identifier that the RAN function advertises | `e3_interface.cpp:1907-1956` |
| dApp report goes through a queue of 1024 and a worker | `e3_interface.cpp:694-724`, `815-836`, `1401-1409` |
| A report with `sequence_id` 0 is refused with `INVALID_PARAM` | `e3_interface.cpp:2157` |
| xApp control from the E3 Agent has no check of `sequence_id`. It returns the message identifier, which is the `requestId` of the answer | `src/core/e3_agent.cpp:238-267` |
| Release from the dApp: unregister, free identifiers, stop service models | `e3_interface.cpp:1411-1442`; `subscription_manager.cpp:84-138` |
| RELEASE from the E3 Agent to a dApp that sent nothing for `dapp_idle_timeout_s` (default 1800 s, 0 is off): queued first, then the removal a release from the dApp gets. Only what the dApp sends resets the time | `e3_interface.cpp:1540-1560`; `subscription_manager.cpp:403-410`, `422-434` |
| RELEASE from the E3 Agent: the dApp clears its session | `e3_interface.cpp:1782-1815` |
| Message identifiers: one counter, 1 to 4294967295 | `e3_interface.cpp:171-178` |
| A request identifier of 0 is replaced | `e3_interface.cpp:193-200` |
| The timestamp is set in the `Pdu` constructor | `types.hpp:470-481` |
| Outbound queue of 128 places, 10 ms pop | `e3_interface.cpp:269`, `753-813`; `include/libe3/lockfree_queue.hpp:55` |

### Encoding and framing (chapter 04)

| Claim | Code pointer |
|---|---|
| POSIX over TCP: 4 octets of length in network order, then the E3-PDU, in both directions. A receiver refuses 0 and more than 65536 | `parse_stream_frame`, `write_stream_prefix` in `socket_link.cpp`, shared by both socket links; `write_pdu`, `pop_stream_frame` in `posix_connector.cpp`; `consume_into`, `prepare_send` in `uring_connector.cpp` |
| POSIX over IPC (a sequenced-packet socket) and over SCTP: one E3-PDU is one message, with no prefix. A message longer than 65536 octets is an error on IPC and on SCTP | `write_pdu`, `read_one`; `consume_into`; `set_sctp_send_info`, `sctp_ppid_acceptable` in `socket_link.cpp` |
| The first E3-PDU of a POSIX connection goes to the setup path, every later one to `receive` | `PosixE3Connector::pump`, `UringE3Connector::deliver` |
| Per-peer send timeout 500 ms | `tune_socket` in `socket_link.cpp`; `UringE3Connector::transmit` cancels a write still waiting then |
| A POSIX E3 Agent removes a dApp whose connection closed, with or without a message after its setup request. ZeroMQ cannot tell its peers apart, so there the idle time removes the dApp | `PosixE3Connector::drop_peer`, `UringE3Connector::drop`, `take_closed_peers`; `E3Interface::remove_dapps_of_closed_peers` |
| A POSIX E3 Agent binds a dApp identifier to the connection of the setup request it answered, and drops a later message that names it on another connection | `E3Interface::bind_dapp_to_connection`, `admit_dapp_message`, `last_setup_peer_token` |
| A POSIX E3 Agent sends each PDU on the connection of the dApp it names, and a PDU for a dApp it cannot tie to a connection to every connection | `E3Interface::outbound_loop_ran` (`target_dapp_of`), `PosixE3Connector::send_to`, `UringE3Connector::send_to` |
| A dApp that lost the E3 Agent registers again every `reconnect_interval_ms` (default 1000, 0 is off), with a new identifier and no subscriptions. `release()` and `stop()` end it | `e3_interface.cpp:1489-1538` |
| ASN.1 encoder buffer of 65536 octets | `src/encoder/asn1_encoder.hpp:39` |
| An empty RAN function list is omitted | `src/encoder/asn1_encoder.cpp:316` |
| A zero timestamp is left out in APER and Protobuf | `asn1_encoder.cpp:234-244` |
| JSON: flat object, payloads nested, `data` wrapper refused | `src/encoder/json_encoder.cpp:171-184`, `624-734`, `781` |
| JSON accepts `telemetryGrantedList` as an alias | `json_encoder.cpp:502-510` |
| Protobuf: response code and wrapper | `src/encoder/protobuf_encoder.cpp:48-51`, `121-130`, `407` |

### The pinned vectors

| Test | What it pins |
|---|---|
| `tests/test_asn1_golden.cpp` | the APER bytes of one PDU for each message, and the refusal of the 11 PDUs of an old grammar |
| `tests/test_asn1_size.cpp` | the bytes of a SETUP REQUEST and the size limits |
| `tests/test_interop_*.cpp` | the decode of the published JSON examples of a peer |
| `tests/test_encoder_equivalence.cpp` | the same PDU through the three encodings |

Update the hex in `tests/test_asn1_golden.cpp` and add a row to the change ledger in
[../interop.md](../interop.md) in the same pull request as any change of the wire.

## Stale comments in the code

These comments disagree with the code. They are not discrepancies of the protocol.

* `include/libe3/types.hpp:538` says dApp identifiers are 1 to 100. The range is 1 to 65535.
* `include/libe3/subscription_manager.hpp:115` says at most 101 dApps. `:148` says RAN function identifiers 0 to 255. `:34` and `:151` say that periodicity 0 means "the service model default".
* `types.hpp:214` and `e3_encoder.hpp:81` use `0.0.0` as the version example. The default is `1.0.0`.
* `types.hpp:486` and `include/libe3/c_api.h:166` leave Protobuf out of the encoding comments.
* `include/libe3/e3_agent.hpp` says `register_sm` returns `STATE_ERROR` when the agent runs. It does not.
* `LIBE3_PROTOCOL_VERSION`, `MAX_ACTION_DATA_SIZE`, `MAX_DAPP_REPORT_DATA_SIZE` and `MAX_XAPP_CTRL_DATA_SIZE` in `types.hpp` are not used.
* The README of the repository lists `send_control` and `send_report` without `sequence_id`, and lists the unused timeout and buffer fields.

## Checklist for a new E3 Agent

1. Open the channels with the same link, transport and encoding as your dApps. On a POSIX link that is one endpoint, and a dApp sent by an earlier layout of libe3 (three connections, no length prefix over IPC and SCTP) cannot connect to it.
2. Answer a bad SETUP REQUEST with a negative response, or the setup channel stalls.
3. Never end the process on a response that fails to encode. Leave the offending RAN function out.
4. Refuse a subscription to an unknown RAN function or identifier. Keep the repeat idempotent, and answer it with what the subscription holds.
5. Detect a dApp that disappears and remove its subscriptions. A closed connection is one sign, silence for a long time is the other.
6. Send a RELEASE message before you drop a dApp, where the link is still there.
7. Bind a dApp identifier to the connection it came on, where you can tell connections apart, and ignore a message that names it on another.
8. Check your bytes against the pinned vectors.
