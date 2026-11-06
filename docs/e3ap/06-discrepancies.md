# E3AP: implementation notes and known discrepancies

Places where implementations of E3AP, the three encodings and the grammar disagree.
Each entry says what differs, what it means for an implementer and the resolution
that this specification expects. This chapter is informative.

This note is informative. The index is [README.md](../e3ap/README.md). The named,
peer by peer ledger of changes is [../interop.md](../interop.md). Code pointers are in
[integrator-guide.md](integrator-guide.md).

Words used below: "implementation A" is the reference implementation of E3AP in this
repository. "Implementation B" is a second implementation of the E3 Agent and the dApp
that uses JSON over ZeroMQ. "Implementation C" is a third one that uses APER over SCTP. The OpenAirInterface implementation [i.4] is the RAN side
that runs on top of implementation A.

## Setup

Table: Setup discrepancies

| No. | What differs | Effect | Expected resolution |
|---|---|---|---|
| S1 | Implementation A does not compare the protocol version of the dApp. It logs it and answers "positive". Implementation B accepts only the exact string `1.0.0`. Implementation C accepts any string that starts with `1.` | A dApp that works with A can fail against B. A version string cannot flag a change of the wire | Accept any string that starts with `1.`. Use the string to flag a wire change |
| S2 | A dApp of implementation A treats a negative SETUP RESPONSE message as a completed setup. The application sees success, and the dApp has no dApp identifier | The application sends messages that the E3 Agent cannot match to a dApp | A dApp shall report a failed setup when the response is "negative" |
| S3 | A dApp of implementation A does not compare `requestId` of the SETUP RESPONSE message with its own request | A late response to an older attempt completes the current attempt | Compare the two identifiers and drop a response that does not match |
| S4 | A repeated SETUP REQUEST message from the same dApp registers a new dApp each time. The E3 Agent has no identity check | A retry after a slow response leaves a dApp identifier with no owner. The identifier stays until a RELEASE message names it | Let the dApp send a RELEASE message for an identifier it did not use. Or give the dApp an identity in the request |
| S5 | A negative response because of no free identifier carries a RAN function list. A negative response because of a bad request does not | The two negative forms differ for the receiver | Send no RAN function list in any negative response |
| S6 | When the E3 Agent cannot encode the SETUP RESPONSE message, implementation A ends the process | One bad RAN function stops the whole RAN node | Leave the failing RAN function out and answer. The implementation does so for the sizes it checks. Other failures still end the process |
| S7 | Implementation C reserves dApp identifiers 1 to 100 for peers that follow implementation A. The others use 1 to 65535 from the lowest free number | Identifiers from a peer of implementation A cannot cover the whole range in C | Accept 1 to 65535 from every peer |

## Subscription and control

Table: Subscription and control discrepancies

| No. | What differs | Effect | Expected resolution |
|---|---|---|---|
| C1 | Implementation A accepts a subscription to an unknown RAN function and to telemetry or control identifiers that the RAN function does not offer. It answers "positive" and grants what was asked | The dApp thinks it receives data. The service model fails to start, and only a log line says so | The E3 Agent answers "negative" with an unknown RAN function or identifier. Implementation C already does |
| C2 | A repeated subscription returns the existing identifier in A. B refuses it and C creates a second one | A dApp that renews a subscription gets different results | Return the existing subscription, as A does |
| C3 | A repeated subscription in A keeps the old details but echoes the new lists and the new interval in the response | The response does not describe what the E3 Agent holds | Echo the stored details, or replace them |
| C4 | `periodicity` is in microseconds in A and in implementation B. It is in milliseconds in C, with the range 0 to 60000 | A value of 100000 asks C for 100 seconds and is refused | Read microseconds everywhere. Until then the caller that talks to C converts the value |
| C5 | `subscriptionTime` is acted on by A and B. C decodes it and ignores it | A subscription that was asked for 60 seconds lasts as long as the session in C | C ends the subscription when the time is up |
| C6 | Nothing tells the dApp that a subscription ran out. A RELEASE message cannot name one subscription | A dApp learns by its own deadline | Keep the dApp deadline counted from the response, as A does |
| C7 | A dApp CONTROL ACTION message for an unknown RAN function, or for one whose service model does not run, is dropped with no response in A | The dApp waits for an acknowledge that does not come | Answer with a MESSAGE ACK message "negative" |
| C8 | A MESSAGE ACK message has no dApp identifier. The E3 Agent sends it to every dApp | A dApp can read the acknowledge of another dApp | Add a dApp identifier, or let the dApp compare `requestId` with its own messages |
| C9 | A dApp REPORT message is not validated by A: dApp identifier, RAN function and sequence identifier are not checked, and there is no response | A bad report reaches the RIC side | Validate the report before it leaves the E3 Agent |
| C10 | An xApp CONTROL ACTION message with `sequenceId` 0 is accepted by the call in A and fails later in the encoder | The error is far from the call | Refuse the value at the call |
| C11 | Implementation C answers a control with a response message and has fields for fragmenting payloads up to 16 MiB. A and B do not | A dApp of A cannot use them | Keep these optional and negotiate them in Setup |

## Release and link loss

Table: Release and link loss discrepancies

| No. | What differs | Effect | Expected resolution |
|---|---|---|---|
| R1 | Implementation A has no code that sends a RELEASE message from the E3 Agent. It reads one from a peer | A dApp of A learns of the end only from a lost link | Send a RELEASE message before the E3 Agent drops a dApp |
| R2 | Implementation C closes the association after 30 seconds idle and never sends a RELEASE message. Implementation B sends one after 1800 seconds idle, and only to a dApp with no subscription | The reason reported to the dApp differs | Agree one rule for idle dApps |
| R3 | On a POSIX link the E3 Agent of A does not detect that a dApp is gone. The registration, the subscriptions and the service model stay | A dead dApp keeps a service model running | Remove the dApp when its link closes |
| R4 | ZeroMQ has a heartbeat on the dApp side. POSIX has none | A silent peer on POSIX is not noticed | Add a keepalive, or a heartbeat message |
| R5 | A RELEASE message is not authenticated. Any dApp can name another dApp | A dApp can end another dApp | Bind the identifier to the connection |

## Encoding and framing

Table: Encoding and framing discrepancies

| No. | What differs | Effect | Expected resolution |
|---|---|---|---|
| E1 | `ranFunctionData` is 1 to 262144 octets in the grammar. Implementation A carries 1 to 32768 | A larger value is refused by A and accepted by C | Raise the limit of A, or lower the grammar. The grammar is the reference |
| E2 | The payload fields are 1 to 32768 octets. Implementation C allows 262144 for `ranFunctionData` only | The same | Keep 32768 for payloads |
| E3 | The framing on POSIX is not symmetric. The E3 Agent sends a 4 octet length and the dApp sends none | Two messages from a dApp can join into one, or one can split | Frame both directions with the same length prefix |
| E4 | A receiver of a framed message refuses more than 65536 octets. A receiver of an unframed message reads at most 60000 octets. The encoder buffer is 65536 octets | A message between 60000 and 65536 octets passes in one direction and not in the other | Use one limit for every direction and every link |
| E5 | A ZeroMQ message larger than the receive buffer is cut by the library | The receiver may read a short buffer as a whole message | Refuse a message that does not fit |
| E6 | SCTP in A is a stream socket with no stream identifier and no payload protocol identifier. Implementation C uses one association for each dApp and one PDU for each SCTP message | The two cannot interwork over SCTP | One mapping, agreed on the open issues of the interop tracker |
| E7 | JSON always writes `timestamp`, including 0. APER and Protobuf leave 0 out | A receiver that reads the key by name sees 0 | Treat 0 as absent in every encoding |
| E8 | In Protobuf a response code that is not "negative" reads as "positive". In APER and JSON it reads as "negative" | An unknown code is a success in one encoding | Read an unknown code as "negative" everywhere |
| E9 | JSON and Protobuf do not check ranges. APER does | A message that works in JSON can fail to encode in APER | Check the ranges in every encoder |
| E10 | The JSON decoder fills a missing field with 0 or with an empty string and does not refuse the message | A malformed message looks like a valid one | Refuse a message that lacks a mandatory field |
| E11 | The names of the setup strings and versions are `UTF8String` in the first grammar. They are `OCTET STRING` now | A peer built from the old grammar cannot decode a SETUP message | Rebuild all peers from one grammar revision |
| E12 | Implementation C compiles the grammar with another ASN.1 compiler. Its SETUP message encodes with other bytes | The two cannot share a process | Build the grammar with the same compiler revision, and compare the pinned vectors |
| E13 | The default encoding of a build that has only Protobuf is APER | The default fails to start | Choose the encoding explicitly |
| E14 | A telemetry list that is absent differs from an empty list in all three encodings. A peer that sends a different name (`telemetryGrantedList`) is read as an alias | The grant is read correctly in A and wrongly in a peer that does not know the alias | Use `telemetryIdentifierList` everywhere |

## Where to look

* [../interop.md](../interop.md) lists each open difference with its issue, and says which way implementation A goes.
* [05-semantics-and-constraints.md](05-semantics-and-constraints.md) says what the specification requires.
* [integrator-guide.md](integrator-guide.md) gives the code pointers for each entry.
