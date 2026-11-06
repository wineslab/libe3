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
| `ran_identifier` | empty | an empty value is sent as `unknown` (`e3_interface.cpp:149`) |
| `dapp_name`, `dapp_version`, `vendor` | `libe3-dapp`, `0.0.0`, empty | empty vendor is sent as `unknown` |
| `e3ap_version` | `1.0.0` | sent in every SETUP REQUEST and SETUP RESPONSE. `init()` returns `INVALID_PARAM` if it is not 1 to 32 octets (`e3_interface.cpp:239-250`) |
| `link_layer` | ZMQ | POSIX if the build has no ZeroMQ |
| `transport_layer` | IPC | SCTP only with `LIBE3_ENABLE_SCTP` |
| `setup_port`, `subscriber_port`, `publisher_port` | 9990, 9999, 9991 | `subscriber_*` is the channel into the E3 Agent. `publisher_*` is the channel out of it |
| endpoints | `ipc:///tmp/dapps/setup`, `dapp_socket`, `e3_socket` | TCP and SCTP ignore the strings and use the ports |
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
| The RAN does not check the dApp's version | `e3_interface.cpp:926` (`handle_setup_request` only logs it) |
| E3 Agent binds the three ZeroMQ sockets | `src/connector/zmq_connector.cpp:130-141`, `201-218`, `265-272` |
| dApp connects: REQ, SUB, PUB | `zmq_connector.cpp:356-367`, `449-470`, `484-492` |
| dApp waits 200 ms after connecting the PUB socket (slow joiner) | `zmq_connector.cpp:505` |
| dApp opens a fresh REQ socket for each setup attempt | `zmq_connector.cpp:376-380` |
| ZeroMQ heartbeat 1000 ms, timeout 3000 ms, on the dApp SUB socket only | `zmq_connector.cpp:30-31`, `460-466` |
| IPC directory `/tmp/dapps` is created with mode 0777 | `zmq_connector.cpp:102-127` |
| TCP and SCTP: E3 Agent binds on all addresses (IPv4), backlog 5. The dApp connects to 127.0.0.1 only | `posix_connector.cpp:258-304`, `741` |
| SCTP is a one-to-one stream socket with `SCTP_NODELAY` and no stream or payload protocol identifier | `posix_connector.cpp:81-100` |
| The dApp retries each channel 10 times at 200 ms intervals | `posix_connector.cpp:754-758`, `800`, `820` |
| Lowest free dApp identifier and subscription identifier | `src/core/subscription_manager.cpp:58-81`, `210` |

### Procedures (chapter 03)

| Claim | Code pointer |
|---|---|
| Setup on the E3 Agent: decode, register, build the list, answer | `src/core/e3_interface.cpp:528-576`, `920-1032` |
| Negative reply for garbage or a wrong PDU type, with the cause protocolError | `e3_interface.cpp:1034-1068` |
| RAN function list limits: data 1 to 32768 octets, 256 identifiers, 64 functions | `e3_interface.cpp:949-990`, `src/core/sm_registry.cpp:35-57` |
| Encode failure of the SETUP RESPONSE ends the process | `e3_interface.cpp:1015-1021` |
| Setup on the dApp: 5 attempts, 4 receives of 500 ms each | `e3_interface.cpp:1318-1396` |
| A negative response still gives "success" to the dApp | `e3_interface.cpp:1390`, `1554-1557` |
| Subscription: the registration, the RAN function, the lists and every identifier are checked, and a refusal carries a cause | `e3_interface.cpp:881-914`, `1070-1143` |
| Repeat returns the existing identifier. Renewal after expiry keeps the identifier | `subscription_manager.cpp:186-204` |
| The response echoes the request lists and interval | `e3_interface.cpp:1127-1138` |
| Subscription expiry sweep every 250 ms | `e3_interface.cpp:40`, `778-802` |
| Subscription delete | `e3_interface.cpp:1145-1186` |
| The dApp keeps its own deadline from the response | `e3_interface.cpp:1595-1605` |
| Service model starts with the first subscriber and stops with the last | `subscription_manager.cpp:237-240`, `375-377`, `524-543`; `e3_interface.cpp:1858-1872` |
| dApp control: the registration, the RAN function, the control identifier and a running service model are checked. Otherwise a negative MESSAGE ACK with a cause | `e3_interface.cpp:1188-1251` |
| The core sends a MESSAGE ACK only to refuse a control. The service model answers for the controls it takes | `e3_interface.cpp:1198-1211`, `examples/sm_simple/simple_service_model.hpp:167` |
| The dApp handles every MESSAGE ACK, with no dApp filter | `e3_interface.cpp:1663-1677` |
| dApp report goes through a queue of 1024 and a worker | `e3_interface.cpp:643-673`, `756-777`, `1253-1261` |
| A report with `sequence_id` 0 is refused with `INVALID_PARAM` | `e3_interface.cpp:1808` |
| xApp control from the E3 Agent has no check of `sequence_id` | `src/core/e3_agent.cpp:230-251` |
| Release from the dApp: unregister, free identifiers, stop service models | `e3_interface.cpp:1263-1286`; `subscription_manager.cpp:83-137` |
| RELEASE from the E3 Agent to a dApp that sent nothing for `dapp_idle_timeout_s` (default 1800 s, 0 is off): queued first, then the removal a release from the dApp gets. Only what the dApp sends resets the time | `e3_interface.cpp:1288-1308`; `subscription_manager.cpp:402-409`, `411-423` |
| RELEASE from the E3 Agent: the dApp clears its session | `e3_interface.cpp:1517-1549` |
| Message identifiers: one counter, 1 to 4294967295 | `e3_interface.cpp:167-174` |
| A request identifier of 0 is replaced | `e3_interface.cpp:189-196` |
| The timestamp is set in the `Pdu` constructor | `types.hpp:426-437` |
| Outbound queue of 128 places, 10 ms pop | `e3_interface.cpp:265`, `694-754`; `include/libe3/lockfree_queue.hpp:55` |

### Encoding and framing (chapter 04)

| Claim | Code pointer |
|---|---|
| POSIX framing: 4 octets, network order, chunks of 8192 | `posix_connector.cpp:914-950`, `posix_connector.hpp:110` |
| Framed receive refuses 0 and more than 65536 | `posix_connector.cpp:976` |
| The dApp sends unframed | `posix_connector.cpp:660`, `777` |
| The E3 Agent reads one raw `recv()` of up to 60000 octets | `posix_connector.cpp:355`, `531` |
| Per-peer send timeout 500 ms | `posix_connector.cpp:41`, `181` |
| The E3 Agent does not notice a dApp that disappears | `posix_connector.cpp:552-563` |
| ASN.1 encoder buffer of 65536 octets | `src/encoder/asn1_encoder.hpp:39` |
| An empty RAN function list is omitted | `src/encoder/asn1_encoder.cpp:315` |
| A zero timestamp is left out in APER and Protobuf | `asn1_encoder.cpp:233-243` |
| JSON: flat object, payloads nested, `data` wrapper refused | `src/encoder/json_encoder.cpp:171-184`, `587-688`, `735` |
| JSON accepts `telemetryGrantedList` as an alias | `json_encoder.cpp:485-493` |
| Protobuf: response code and wrapper | `src/encoder/protobuf_encoder.cpp:48-51`, `121-130`, `374` |

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

* `include/libe3/types.hpp:523` says dApp identifiers are 1 to 100. The range is 1 to 65535.
* `include/libe3/subscription_manager.hpp:114` says at most 101 dApps. `:148` says RAN function identifiers 0 to 255. `:34` and `:151` say that periodicity 0 means "the service model default".
* `types.hpp:211` and `e3_encoder.hpp:81` use `0.0.0` as the version example. The default is `1.0.0`.
* `types.hpp:442` and `include/libe3/c_api.h:166` leave Protobuf out of the encoding comments.
* `include/libe3/e3_agent.hpp` says `register_sm` returns `STATE_ERROR` when the agent runs. It does not.
* `LIBE3_PROTOCOL_VERSION`, `MAX_ACTION_DATA_SIZE`, `MAX_DAPP_REPORT_DATA_SIZE` and `MAX_XAPP_CTRL_DATA_SIZE` in `types.hpp` are not used.
* The README of the repository lists `send_control` and `send_report` without `sequence_id`, and lists the unused timeout and buffer fields.

## Checklist for a new E3 Agent

1. Open the three channels with the same link, transport and encoding as your dApps.
2. Answer a bad SETUP REQUEST with a negative response, or the setup channel stalls.
3. Never end the process on a response that fails to encode. Leave the offending RAN function out.
4. Refuse a subscription to an unknown RAN function or identifier. Keep the repeat idempotent.
5. Detect a dApp that disappears and remove its subscriptions. Silence for a long time is one sign.
6. Send a RELEASE message before you drop a dApp, where the link is still there.
7. Check your bytes against the pinned vectors.
