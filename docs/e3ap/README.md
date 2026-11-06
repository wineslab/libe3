# E3AP

E3AP is the E3 Application Protocol. It connects a dApp to the RAN function that
hosts it. A dApp is a small application that runs next to the RAN and reacts in
less than a millisecond. E3AP lets the dApp discover what the RAN offers, ask
for telemetry, send control actions back, and send reports that a RIC can use.

E3AP carries the data of the E3 service models without reading it. Each service
model defines the meaning of its own payloads. See the [E3SM specifications](../e3sm/README.md).

## Scope

The present document specifies the E3 Application Protocol (E3AP) between a dApp
and an E3 Agent in a RAN node. It defines the messages, the procedures, the
information elements, the encodings and the handling of errors.

E3AP does not define the content of a service model payload. It does not define
how a RAN node produces telemetry or applies a control action. It does not define
the E2 interface. The E2SM-DAPP specification defines how a RIC reaches a dApp
through the E2 interface. See the [E2SM-DAPP specification](../e2sm-dapp/README.md).

This document is generated from the codebase and protocol definitions for the libe3
implementation in https://github.com/wineslab/libe3.

## References

### Normative references

References are specific. Only the cited version applies.

[1] ITU-T Recommendation X.680 (2002-07): "Information technology: Abstract Syntax Notation One (ASN.1): Specification of basic notation".

[2] ITU-T Recommendation X.681 (2002-07): "Information technology: Abstract Syntax Notation One (ASN.1): Information object specification".

[3] ITU-T Recommendation X.691 (2002-07): "Information technology: ASN.1 encoding rules: Specification of Packed Encoding Rules (PER)".

[4] IETF RFC 8259 (2017-12): "The JavaScript Object Notation (JSON) Data Interchange Format".

[5] IETF RFC 4960 (2007-09): "Stream Control Transmission Protocol".

[6] IETF RFC 9293 (2022-08): "Transmission Control Protocol (TCP)".

[7] ZeroMQ RFC 23 (ZMTP 3.0): "ZeroMQ Message Transport Protocol", https://rfc.zeromq.org/spec/23/.

### Informative references

[i.1] Protocol Buffers Language Guide (proto3), https://protobuf.dev/programming-guides/proto3/.

[i.2] O-RAN.XXX.TS.E2SM-DAPP-R000-v00.01: "E2 Service Model dApp (E2SM-DAPP)", https://github.com/wineslab/libe3/tree/main/docs/e2sm-dapp.

[i.3] O-RAN.XXX.TS.E3SM-SPEC-R000-v00.01: "E3 Service Model Spectrum (E3SM-SPEC)", https://github.com/wineslab/libe3/tree/main/docs/e3sm/spectrum, and O-RAN.XXX.TS.E3SM-L1-KPM-R000-v00.01: "E3 Service Model (E3SM), L1 KPM (E3SM-L1-KPM)", https://github.com/wineslab/libe3/tree/main/docs/e3sm/l1-kpm.

[i.4] The OpenAirInterface implementation of E3AP, https://github.com/duranta-project/openairinterface5g/pull/729.

## Terms, symbols and abbreviations

### Terms

For the purposes of the present document, the following terms apply.

**dApp**: an application that runs in the RAN node or next to it and uses E3AP to reach a RAN function.

**E3 Agent**: the entity in the RAN node that terminates E3AP on the RAN side. It hosts the RAN functions and serves the dApps.

**RAN function**: a part of the RAN node that a dApp can use through E3AP. A RAN function has a number, a list of telemetry identifiers, a list of control identifiers and a block of data that only its service model can read.

**Service model**: the definition of the payloads of one RAN function. E3AP carries the payloads as octet strings. The service model gives them their meaning.

**Telemetry identifier**: a number that names one stream of data that a RAN function can send to a dApp.

**Control identifier**: a number that names one control action that a dApp can send to a RAN function.

**Subscription**: the agreement between one dApp and one RAN function. It lists the telemetry streams and the control actions that the dApp uses.

**Indication**: a message that carries telemetry from the E3 Agent to a dApp.

**Report**: a message that carries a result from a dApp to the E3 Agent, for use by a RIC.

**Sequence identifier**: a number that joins all messages of one detection-to-mitigation procedure.

**Link layer**: the messaging library that moves E3AP messages. The link layer is ZeroMQ or POSIX sockets.

**Transport layer**: the network transport that the POSIX link layer uses. It is SCTP, TCP or inter-process communication (IPC).

### Symbols

Void.

### Abbreviations

For the purposes of the present document, the following abbreviations apply.

| Abbreviation | Meaning |
|---|---|
| APER | Aligned Packed Encoding Rules |
| ASN.1 | Abstract Syntax Notation One |
| E3AP | E3 Application Protocol |
| E3SM | E3 Service Model |
| IE | Information Element |
| IPC | Inter-Process Communication |
| JSON | JavaScript Object Notation |
| PDU | Protocol Data Unit |
| RAN | Radio Access Network |
| RIC | RAN Intelligent Controller |
| SCTP | Stream Control Transmission Protocol |
| SM | Service Model |
| TCP | Transmission Control Protocol |
| UTC | Coordinated Universal Time |
| ZMQ | ZeroMQ |

## General

### Notations

For the purposes of the present document, the following notations apply.

| Item | Notation |
|---|---|
| Procedure | The name of a procedure has an initial capital letter on each word. It is followed by the word "procedure". Example: Setup procedure. |
| Message | The name of a message is in capital letters. It is followed by the word "message". Example: SETUP REQUEST message. |
| IE | The name of an information element has an initial capital letter on each word. It is in italics. It is followed by "IE". Example: *Subscription Time* IE. |
| Value of an IE | The value is in quotation marks. Example: "positive". |

The ASN.1 name of a message has the prefix "E3-" and no spaces. Example: E3-SetupRequest.
The text uses the ASN.1 field name when it refers to a field. Example: `subscriptionTime`.

### Forwards and backwards compatibility

E3AP uses these means to stay compatible when it grows.

1. Every SEQUENCE and the main CHOICE in the ASN.1 grammar has an extension marker. A receiver shall ignore an extension addition that it does not know.
2. A new optional field goes after the extension marker. An old receiver skips it.
3. The `E3-ResponseCode` type has no extension marker. A new response code needs a new version of E3AP.
4. The Setup procedure carries a version string. An E3 Agent may answer with a negative response code when it does not support the version of the dApp. Implementations differ on this point.
5. A change to a constraint changes the encoded bytes. The sender and the receiver shall use the same version of the grammar.

## Where each part is specified

| Section | What it covers | File |
|---|---|---|
| 1. Identity and registration | version, roles, link layers, transports, endpoints, identifiers, how a dApp registers | [01-identity-and-registration.md](01-identity-and-registration.md) |
| 2. IE mapping | every message and information element: fields, types, ranges, presence, meaning | [02-ie-mapping.md](02-ie-mapping.md) |
| 3. Procedure flow | Setup, Subscription, Indication, Control, Report, Release, acknowledgement and the sequence identifier chain | [03-procedure-flow.md](03-procedure-flow.md) |
| 4. Encoding | APER, JSON and Protobuf rules, framing on each link, size limits, the ASN.1 grammar | [04-encoding.md](04-encoding.md) |
| 5. Semantics and constraints | units, clocks, absent versus zero, validation, error paths, what is mandatory | [05-semantics-and-constraints.md](05-semantics-and-constraints.md) |

An engineer who adds E3AP to a RAN stack or to a dApp finds code pointers and a
checklist in the [integrator guide](integrator-guide.md). The guide is not part of the specification.

## Related documents

* [../interop.md](../interop.md): what pairs with what, the JSON form in use, the field contracts where peers differ, and the change ledger.
* [../control-loops/path-a-e3-loop.md](../control-loops/path-a-e3-loop.md) and [../control-loops/path-b-e2-e3-loop.md](../control-loops/path-b-e2-e3-loop.md): the E3-only loop and the E2-E3 loop, with their box numbers.
* [../utilities/latrec.md](../utilities/latrec.md): the latency recorder. Its monotonic clock shall not be mixed with the realtime timestamps of E3AP.
* [../e2sm-dapp/README.md](../e2sm-dapp/README.md): how a RIC reaches a dApp through the E2 interface.
* [../e3sm/README.md](../e3sm/README.md): the E3 service models.
