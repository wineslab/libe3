# E3SM-SPEC: the Spectrum service model

E3SM-SPEC is the E3 service model for spectrum sensing and spectrum sharing. A RAN
that offers it publishes a map of free time and frequency resources to dApps, and
the configuration of the uplink carrier that the map refers to. It also accepts two
controls: one blocks PRBs in the scheduler, the other asks the uplink scheduler to
keep symbols free for sensing. A dApp can report the PRBs it found occupied, and an
xApp can suggest PRBs to block.

This directory is the specification of the service model: what the fields mean, how
each message is encoded, and how the procedures run. The E3 Application Protocol
that carries these messages is specified in [the E3AP specification](../../e3ap/README.md).

| Item | Value |
|---|---|
| E3 RAN function ID | `1` |
| Service model name | `spectrum_sm` |
| Service model version | `1` |
| Telemetry identifiers | `1` to `5` (sensing ranges, timestamp, SFN, slot, beam) |
| Control identifiers | `1` (PRB block), `2` (sensing policy) |
| Encodings | APER, JSON, Protocol Buffers. The RAN serves one per run. |
| Message types | RAN function data, sensing indication, cell configuration indication, dApp control, dApp report, xApp control, apply outcome |

## Scope

The present document specifies the E3 Service Model (E3SM) "Spectrum" for the RAN
function that exposes spectrum sensing telemetry and spectrum control.

The document defines the identity of the RAN function and how a RAN announces it. It
defines the information elements of every message, and the procedures that use them.
It defines the encodings, the units, the clocks and the limits of each field. It also
defines how a receiver handles a message that is unknown or wrong.

The document does not define the E3 Application Protocol. It does not define the
E2 service model that carries dApp traffic to an xApp. It does not define how a
scheduler enforces a PRB block internally.

This document is generated from the codebase and protocol definitions for the libe3
implementation in https://github.com/wineslab/libe3.

## References

### Normative references

References are either specific (identified by date of publication, edition number or
version number) or non-specific. For specific references, only the cited version
applies. For non-specific references, the latest version of the referenced document
applies.

> NOTE: While any hyperlinks included in this clause were valid at the time of
> publication, their long-term validity cannot be guaranteed.

The following referenced documents are necessary for the application of the present
document.

[1] O-RAN.XXX.TS.E3AP-R000-v00.01: "E3 Application Protocol (E3AP)", https://github.com/wineslab/libe3/tree/main/docs/e3ap.

[2] ITU-T Recommendation X.680 (2002-07): "Information technology: Abstract Syntax Notation One (ASN.1): Specification of basic notation".

[3] ITU-T Recommendation X.681 (2002-07): "Information technology: Abstract Syntax Notation One (ASN.1): Information object specification".

[4] ITU-T Recommendation X.691 (2002-07): "Information technology: ASN.1 encoding rules: Specification of Packed Encoding Rules (PER)".

[5] IETF RFC 8259 (2017-12): "The JavaScript Object Notation (JSON) Data Interchange Format".

### Informative references

The following referenced documents are not necessary for the application of the
present document, but they assist the user with regard to a particular subject area.

[i.1] 3GPP TS 38.211: "NR; Physical channels and modulation".

[i.2] 3GPP TS 38.214: "NR; Physical layer procedures for data".

[i.3] 3GPP TS 38.300: "NR; NR and NG-RAN Overall description; Stage 2".

[i.4] Protocol Buffers Language Guide (proto2), https://protobuf.dev/programming-guides/proto2/.

[i.5] OpenAirInterface implementation of the Spectrum service model, https://github.com/duranta-project/openairinterface5g/pull/729.

[i.6] 3GPP TS 38.331: "NR; Radio Resource Control (RRC); Protocol specification".

[i.7] 3GPP TS 38.104: "NR; Base Station (BS) radio transmission and reception".

## Terms, symbols and abbreviations

### Terms

For the purposes of the present document, the terms and definitions of the E3AP
specification [1] and the following apply.

**Sensing range**: a rectangle in the time and frequency grid of one slot. It has a
first symbol, a number of symbols, a first PRB and a number of PRBs. No scheduled
transmission uses the resources inside it.

**Sensing ring**: a ring of fixed size slots in POSIX shared memory. The RAN writes one
ring slot for each sensing publish. It holds the sensing ranges of that publish.

**Sensing publish**: the moment the scheduler hands the sensing ranges of one slot to
the service model.

**PRB block**: a set of PRBs that the scheduler treats as occupied, in uplink and in
downlink, until the set is replaced or cleared.

**Sensing policy**: a list of symbol masks, one for each slot of a frame. The uplink
scheduler prefers time domain allocations that leave the masked symbols free.

**Cell configuration**: the geometry and the slot structure of the uplink carrier that
the sensing ranges refer to: its bandwidth, its offset in the resource grid, its
subcarrier spacing, its point A and its TDD pattern.

**Apply outcome**: the report of the RAN on what applying a PRB block control did, and
when.

### Symbols

Void.

### Abbreviations

For the purposes of the present document, the abbreviations of the E3AP
specification [1] and the following apply. An abbreviation defined in the present
document takes precedence over the definition of the same abbreviation, if any, in
the E3AP specification.

| Abbreviation | Meaning |
|---|---|
| APER | Aligned Packed Encoding Rules |
| ARFCN | Absolute Radio Frequency Channel Number |
| FDD | Frequency Division Duplex |
| PCI | Physical Cell Identity |
| PRB | Physical Resource Block |
| SFN | System Frame Number |
| SM | Service Model |
| TDD | Time Division Duplex |
| UL | Uplink |
| DL | Downlink |

## General

### Notations

For the purposes of the present document, the following notations apply.

**Message**: a message is written in capital letters, for example E3 RAN FUNCTION DATA.

**Information element**: an information element (IE) is written in italic font with its
name as in the grammar, for example `validityPeriod`.

**Value**: the value of an IE is written in quotation marks, for example "0".

**Clause numbers**: a reference to a clause of another specification names that
specification, for example "E3AP [1]".

### Forwards and backwards compatibility

The grammar of the service model is version 1. The payload choices of the dApp
control, the dApp report, the xApp control and the apply outcome carry an extension
marker, and so do the cell configuration indication and the apply outcome structure.
The indication choice, the sensing indication and the RAN function data have none. A receiver that meets an
alternative it does not know shall reject the message as described in clause 9. A
receiver shall ignore an extension field that it does not know, where the encoding
allows it.

An extension alternative in an APER choice changes the width of the choice index. A
sender and a receiver therefore need the same version of the grammar for the
messages that already exist. Clause 8 gives the rule and the effect.

## Where each part is specified

| Section | What it covers | File |
|---|---|---|
| Identity and registration | RAN function ID, name, version, telemetry and control identifiers, the RAN function data, how a dApp finds the service model | [01-identity-and-registration.md](01-identity-and-registration.md) |
| IE mapping | every information element with type, range, presence and meaning, and the layout of the sensing ring | [02-ie-mapping.md](02-ie-mapping.md) |
| Procedure flow | telemetry, controls, report, xApp control relay and the apply outcome, with the E3AP message that carries each | [03-procedure-flow.md](03-procedure-flow.md) |
| Encoding | APER widths, JSON keys, Protocol Buffers messages, size limits and the ASN.1 clause | [04-encoding.md](04-encoding.md) |
| Semantics and constraints | units, clocks, absent versus zero, PRB limits, validation and error handling | [05-semantics-and-constraints.md](05-semantics-and-constraints.md) |
| Examples | a complete exchange in APER and JSON, from the setup to the PRB block, and an apply outcome | [07-examples.md](07-examples.md) |

For an engineer who adds the service model to a RAN or to a dApp there is a separate
guide: [integrator-guide.md](integrator-guide.md). It is not part of the
specification.

## Related documents

* [E3AP specification](../../e3ap/README.md): the protocol that carries every message of
  this service model.
* [Path A](../../control-loops/path-a-e3-loop.md) and
  [Path B](../../control-loops/path-b-e2-e3-loop.md): the control loops and their box
  numbers, which [03-procedure-flow.md](03-procedure-flow.md) uses.
* [latrec](../../utilities/latrec.md): the latency recorder. Its monotonic clock is not
  the realtime clock of the payload timestamps in this service model.
* [E2SM-DAPP specification](../../e2sm-dapp/README.md): the E2 service model that
  carries a dApp report to an xApp and an xApp decision back.
