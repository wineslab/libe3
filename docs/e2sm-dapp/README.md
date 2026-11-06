# E2SM-DAPP

E2SM-DAPP is the E2 service model that carries dApp traffic between an E2 node (a
RAN) and an xApp. The RAN relays each dApp report to the xApp as a RIC Indication.
It relays each xApp decision to the dApp as a RIC Control. The acknowledge reports
what applying the decision did on the air.

This directory is the specification of the service model: the grammar, what the
fields mean, and the encoding. A RAN or an xApp written from this text can
interoperate with any other. The integrator guide is not part of the specification.
It holds the pointers to code and artifacts.

## Scope

The present document specifies the E2 Service Model (E2SM) "dApp" (E2SM-DAPP) for the
RAN function that relays the traffic of dApps between an E2 node and a Near-RT RIC.

The RAN function offers two RIC services:

* The REPORT service delivers the reports of dApps to the xApp, and the map of the dApps
  that are connected to the E2 node.
* The CONTROL service delivers the decision of the xApp to the dApp, and returns the
  outcome of applying it.

The present document defines the RAN function definition, the information elements and
the procedures of this service model. It also defines the APER encoding of the
information elements and the handling of unknown and erroneous data.

This document is generated from the codebase and protocol definitions for the libe3
implementation in https://github.com/wineslab/libe3.

## References

### Normative references

References are either specific (identified by date of publication, edition number or
version number) or non-specific. For a specific reference, only the cited version
applies. For a non-specific reference, the latest version applies.

The following referenced documents are necessary for the application of the present
document.

1. O-RAN.WG3.TS.E2GAP: "O-RAN E2 General Aspects and Principles".
2. O-RAN.WG3.TS.E2AP: "O-RAN E2 Application Protocol (E2AP)".
3. O-RAN.WG3.TS.E2SM: "O-RAN E2 Service Model (E2SM)".
4. ITU-T Recommendation X.680: "Information technology: Abstract Syntax Notation One (ASN.1): Specification of basic notation".
5. ITU-T Recommendation X.681: "Information technology: Abstract Syntax Notation One (ASN.1): Information object specification".
6. ITU-T Recommendation X.691: "Information technology: ASN.1 encoding rules: Specification of Packed Encoding Rules (PER)".

### Informative references

The following referenced documents are not necessary for the application of the present
document, but they help the reader.

1. O-RAN.XXX.TS.E3AP-R000-v00.01: "E3 Application Protocol (E3AP)", https://github.com/wineslab/libe3/tree/main/docs/e3ap.
2. The OpenAirInterface implementation of E3AP and of this service model: https://github.com/duranta-project/openairinterface5g/pull/729.

## Terms, symbols and abbreviations

### Terms

For the purposes of the present document, the terms and definitions given in
O-RAN.WG3.TS.E2GAP [1], O-RAN.WG3.TS.E2SM [3] and the following apply.

* **dApp**: an application that runs next to the E2 node and talks to it over E3.
* **dApp E3 subscription map**: the list of the dApps connected to the E2 node, each with the E3 RAN functions it has subscribed to.
* **E3 RAN function**: a RAN function that is offered to dApps over E3. It is named by an E3 RAN function id. Do not confuse it with the E2 RAN function.
* **Sequence id**: the identifier of one detection-to-mitigation procedure. The dApp assigns it. Every later message of the procedure carries it unchanged.
* **Implementation A**: the original C implementation of the service model, in an E2 agent and a RIC library.
* **Implementation B**: the C++ and C codec that ships with this repository.
* **Implementation C**: a C++ RAN stack with a hand-ported codec.
* **OpenAirInterface implementation**: the RAN bridge described in reference [i.2].
* **xApp framework**: a Python framework that wraps implementation A.

### Symbols

Void.

### Abbreviations

For the purposes of the present document, the abbreviations given in O-RAN.WG3.TS.E2SM
[3] and the following apply. An abbreviation defined in the present document takes
precedence over the definition of the same abbreviation in [3].

* **APER**: Aligned Packed Encoding Rules
* **E3AP**: E3 Application Protocol
* **E3SM**: E3 Service Model
* **gNB**: next generation Node B
* **MAC**: Medium Access Control
* **PRB**: Physical Resource Block
* **RIC**: RAN Intelligent Controller

## General

### Notations

For the purposes of the present document, the following notations apply:

* A service name is written in upper case characters, followed by the word "service". Example: REPORT service.
* A procedure name is written with the first letter of each word in upper case, followed by the word "procedure". Example: RIC Control procedure.
* A message name is written in upper case, followed by the word "message". Example: RIC CONTROL REQUEST message.
* An information element name is written with the first letter of each word in upper case and in italic font, followed by the abbreviation "IE". Example: *RAN Function Definition* IE.
* A value of an information element is written between quotation marks. Example: "Value".
* An ASN.1 type or field name is written in a monospaced font.

### Forwards and backwards compatibility

The ASN.1 types of this service model are extensible. A decoder shall ignore extension
additions that it does not know. A sender shall not set the extension bit of a type
until this specification defines the additions. A change of the grammar that alters the
APER encoding of an existing message is a change of the E2 RAN function revision.

### Identity of the service model

Table: Identity of the E2SM-DAPP service model

| Item | Value |
|---|---|
| E2 RAN function ID | `255` |
| RAN function revision | `1` (one implementation announces `0`) |
| Short name | `E2SM-DAPP` |
| OID | `1.3.6.1.4.1.53148.1.1.255.3` |
| Grammar | module `E2SM-DAPP-IEs`, APER (aligned), see [04](04-encoding.md) |
| Report styles | 1 = E3 data report, 2 = E3 subscription map |
| Control style | 1 |
| Not used | call process ID, INSERT, POLICY, QUERY |

## Where each part is specified

Everything of substance is in the numbered files. This page only says where. The clause
numbers are those of the Word edition of the specification.

Table: Chapters of the specification

| Section | Clause in the Word edition | What it covers | File |
|---|---|---|---|
| 1. Identity and registration | 5 | RAN function id, name, OID, revision, what the definition contains, how it is announced in E2 Setup and RIC Service Update | [01-identity-and-registration.md](01-identity-and-registration.md) |
| 2. E2AP IE mapping | 6 | every information element with fields, types, ranges, optionality, formats and styles; which service uses which format; the elements that are not used; the control outcome payload and the Spectrum worked example | [02-ie-mapping.md](02-ie-mapping.md) |
| 3. Procedure flow | 7 | discovery of dApp ids, subscription, indication delivery, control relay, the deferred acknowledge, the `sequence-id` chain | [03-procedure-flow.md](03-procedure-flow.md) |
| 4. Encoding | 8 | APER rules, byte-level worked examples checked against the reference test vectors, size limits, the ASN.1 grammar | [04-encoding.md](04-encoding.md) |
| 5. Semantics and constraints | 9 | format and style numbers, units and clocks, absent versus zero, validation, error paths, unknown ids and formats, what is mandatory | [05-semantics-and-constraints.md](05-semantics-and-constraints.md) |

For an engineer who adds E2SM-DAPP to a RAN stack, there is a separate guide:
[integrator-guide.md](integrator-guide.md). It is not part of the specification. It
holds the code pointers, the list of artifacts (grammar, codec sources, reference test
vectors, tests, build options) and the code roots and revisions read.

## Related documents

* [path-b-e2-e3-loop.md](../control-loops/path-b-e2-e3-loop.md) and
  [path-a-e3-loop.md](../control-loops/path-a-e3-loop.md): the E2-E3 loop and its box
  numbers, which [03](03-procedure-flow.md) uses.
* [latrec.md](../utilities/latrec.md): the latency recorder. Its monotonic clock shall
  not be mixed with the realtime timestamps of E2SM-DAPP.
* [E3AP](../e3ap/README.md): the E3 Application Protocol. Its `E3-SequenceID` is the same
  value as the `sequence-id` of E2SM-DAPP.
* [E3SM](../e3sm/README.md): the E3 service models, such as Spectrum, whose payloads
  E2SM-DAPP carries.
* [integrator-guide.md](integrator-guide.md): the guide for an engineer who adds the
  service model to a RAN stack.
