# E3AP: identity and registration

The numbers and strings that identify E3AP, the roles of the two sides, the links that
carry the messages, and how a dApp registers with an E3 Agent.

This is clause 5 of the specification. The index is [README.md](../e3ap/README.md). The
messages are in [02-ie-mapping.md](02-ie-mapping.md). The procedures are in
[03-procedure-flow.md](03-procedure-flow.md). The bytes are in
[04-encoding.md](04-encoding.md).

## Identity

Table: Identity of E3AP

| Item | Value |
|---|---|
| Protocol name | E3AP, the E3 Application Protocol |
| Protocol version string | `1.0.0` |
| Grammar | module `E3`, ASN.1, APER (aligned). See [04-encoding.md](04-encoding.md#abstract-syntax) |
| Messages | 11, each inside one `E3-PDU` |
| Roles | dApp, E3 Agent |
| Encodings | APER, JSON, Protobuf |
| Link layers | ZeroMQ, POSIX sockets |
| Transport layers | IPC, TCP, SCTP |

The version string is a free text of 1 to 32 octets. Both sides write it in the Setup
procedure. This version of the specification uses `1.0.0`. The string names the
version of the protocol. It does not say which grammar revision a sender uses. A
change of a constraint changes the encoded bytes of APER, so both sides shall use the
same grammar revision.

### Two sides of one interface

E3AP is a client and server protocol.

* The **E3 Agent** is the server. It runs in the RAN node. It hosts the RAN functions, owns the identifiers and answers the requests of dApps.
* The **dApp** is the client. It connects to the E3 Agent, registers, subscribes, and then sends and receives messages.

One E3 Agent serves many dApps at the same time. A dApp uses one session with one
E3 Agent. A message in the direction from the E3 Agent to the dApps reaches every
connected dApp on some links. A dApp shall ignore a message that names another dApp
in `dAppIdentifier`.

### Identifiers

Table: Identifiers in E3AP

| Identifier | Range | Assigned by | Lifetime |
|---|---|---|---|
| dApp identifier | 1 to 65535 | the E3 Agent, in the Setup procedure | until the dApp releases or the session ends |
| Subscription identifier | 1 to 65535 | the E3 Agent, in the Subscription procedure | until the subscription is deleted, ends, or the dApp releases |
| RAN function identifier | 1 to 65535 | the RAN node | fixed for the life of the E3 Agent |
| Telemetry identifier, control identifier | 1 to 65535 | the service model of the RAN function | fixed for the life of the E3 Agent |
| Message identifier | 1 to 4294967295 | the sender of each message | one message |
| Sequence identifier | 1 to 4294967295 | the component that starts a procedure | one detection-to-mitigation procedure |

The E3 Agent assigns the lowest free number for a dApp identifier and for a
subscription identifier. A number becomes free again as soon as its owner goes away.

The RAN function identifier of E3AP is not the RAN function identifier of the E2
interface. See [E2SM-DAPP, identity](../e2sm-dapp/01-identity-and-registration.md#12-two-things-named-ran-function-id).

## Links and transports

E3AP does not define its own framing on a link that already keeps message
boundaries. It runs over a choice of a link layer and a transport layer. Both sides
shall use the same choice. There is no negotiation.

Table: Link layers and transport layers

| Link layer | Transport layer | Connections | Message boundary |
|---|---|---|---|
| ZeroMQ | IPC | 3 | one E3AP PDU is one ZeroMQ message |
| ZeroMQ | TCP | 3 | one E3AP PDU is one ZeroMQ message |
| POSIX | IPC | 1 | one E3AP PDU is one message, with no length prefix. See [04-encoding.md](04-encoding.md#framing-on-each-link) |
| POSIX | TCP | 1 | a length prefix on every E3AP PDU, in both directions |
| POSIX | SCTP | 1 | one E3AP PDU is one SCTP message, with no length prefix |

### The three channels

A dApp uses three channels to one E3 Agent. On a ZeroMQ link each channel has a
connection of its own. On a POSIX link all three travel on one connection.

Table: Channels between a dApp and an E3 Agent

| Channel | Direction | Carries | ZeroMQ socket pair |
|---|---|---|---|
| Setup | dApp to E3 Agent, then back | the SETUP REQUEST and SETUP RESPONSE messages | REQ on the dApp, REP on the E3 Agent |
| Inbound to the E3 Agent | dApp to E3 Agent | SUBSCRIPTION REQUEST, SUBSCRIPTION DELETE, dApp CONTROL ACTION, dApp REPORT, CONTROL RESPONSE (the answer to an xApp CONTROL ACTION) and RELEASE messages | PUB on the dApp, SUB on the E3 Agent |
| Outbound from the E3 Agent | E3 Agent to dApp | SUBSCRIPTION RESPONSE, INDICATION, xApp CONTROL ACTION, CONTROL RESPONSE (the answer to a dApp CONTROL ACTION) and RELEASE messages. A deprecated MESSAGE ACK message is no longer sent | PUB on the E3 Agent, SUB on the dApp |

The E3 Agent opens its endpoints and waits. The dApp connects to them. On a ZeroMQ link
the E3 Agent binds all three sockets. On a POSIX link it listens on one endpoint.

### Default endpoints

The default endpoints and ports of every link are in
[04-encoding.md](04-encoding.md#framing-on-each-link), with the framing of the link. An
implementation may use other values. Both sides shall use the same values.

## How a dApp registers

A dApp registers with the Setup procedure. The dApp sends a SETUP REQUEST message
with its protocol version, its name, its version and its vendor. The E3 Agent answers
with a SETUP RESPONSE message. On success the response holds the dApp identifier
and the list of RAN functions. See [03-procedure-flow.md](03-procedure-flow.md#setup).

Until the dApp has a dApp identifier, it shall not send any message on the other two
channels. On a POSIX link the SETUP REQUEST is the first message of the connection, and
the dApp identifier belongs to that connection.

### What the RAN function list holds

For each RAN function the E3 Agent lists:

* the RAN function identifier,
* the telemetry identifiers that the RAN function offers,
* the control identifiers that the RAN function accepts,
* the RAN function data, an octet string that only the service model reads.

The list holds at most 64 RAN functions. A RAN function with more than 256 telemetry
identifiers or more than 256 control identifiers does not fit. An E3 Agent shall
leave such a RAN function out of the list.

## Requirements for an E3 Agent

An implementation of the E3 Agent shall:

1. open its channels (on a POSIX link, one endpoint) and answer the Setup procedure before it accepts any other message,
2. assign a dApp identifier that no other registered dApp holds,
3. return its RAN identifier in every SETUP RESPONSE message,
4. list only RAN functions whose data is 1 to 262144 octets and whose identifier lists fit in 256 entries,
5. keep a subscription for each pair of dApp and RAN function, and
6. free the dApp identifier and all subscriptions of a dApp when it receives a RELEASE message from that dApp.

An implementation of the dApp shall:

1. send the SETUP REQUEST message first and wait for the response,
2. keep the dApp identifier from the response and put it in every later message,
3. ignore a message that names another dApp, and
4. retry the Setup procedure when no response arrives.

## Related

* [02-ie-mapping.md](02-ie-mapping.md): the messages and their fields.
* [03-procedure-flow.md](03-procedure-flow.md): the procedures in order.
