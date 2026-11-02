# E3SM-L1KPM

E3SM-L1KPM is the E3 service model that gives a dApp the frequency domain IQ samples of
each uplink slot of a RAN. The RAN does not send the samples in the E3 message. It
writes them to a POSIX shared memory region on its host. Each E3 indication carries a
small reference to the place in that region, plus the time, the frame number, the slot
number and a few cell parameters. The dApp reads the samples from the region.

This service model is telemetry only. It has no control actions.

| Item | Value |
|---|---|
| E3 RAN function ID | `2` |
| Short name | `L1-KPM` |
| RAN function revision (version in the RAN function data) | `1` |
| Telemetry identifiers | `1`, `4`, `5`, `6` |
| Control identifiers | none |
| Shared memory region | `/e3_ran_buffers` |
| Carried in | the `protocolData` field of the E3 indication message |
| Encodings | ASN.1 APER, JSON, Protocol Buffers (the encoding of the E3AP session) |

Everything of substance is in the numbered files. This page only says where.

## Scope

The present document specifies the E3 service model "L1 KPM" (E3SM-L1KPM) for a RAN
function that publishes the post-FFT uplink IQ samples of a cell to dApps on the same
host as the RAN.

The present document defines the RAN function identity and the RAN function data, the
information elements of the indication, the layout of the shared memory region that the
indication refers to, the procedures, the encodings and the rules for erroneous data.
The E3 messages that carry these elements are defined in the E3AP specification [1].

## References

### Normative references

The following referenced documents are necessary for the application of the present
document.

[1] O-RAN.WG3.TS.E3AP-R000-v00.01, "[E3 Application Protocol (E3AP)](../../e3ap/README.md)".

[2] ITU-T Recommendation X.680 (2021): "Information technology, Abstract Syntax Notation One (ASN.1): Specification of basic notation".

[3] ITU-T Recommendation X.681 (2021): "Information technology, Abstract Syntax Notation One (ASN.1): Information object specification".

[4] ITU-T Recommendation X.691 (2021): "Information technology, ASN.1 encoding rules: Specification of Packed Encoding Rules (PER)".

[5] IETF RFC 8259: "The JavaScript Object Notation (JSON) Data Interchange Format".

### Informative references

The following referenced documents are not necessary for the application of the present
document, but they assist the user with regard to a particular subject area.

[i.1] 3GPP TS 38.211: "NR; Physical channels and modulation".

[i.2] 3GPP TS 38.213: "NR; Physical layer procedures for control".

[i.3] 3GPP TS 38.300: "NR; NR and NG-RAN Overall description; Stage 2".

[i.4] Protocol Buffers Language Guide (proto2), https://protobuf.dev/programming-guides/proto2/.

[i.5] IEEE Std 1003.1 (POSIX.1): shared memory objects (`shm_open`, `mmap`).

[i.6] IEEE Std 754: "IEEE Standard for Floating-Point Arithmetic" (binary16 format).

[i.7] The OpenAirInterface implementation of E3AP and of this service model, https://github.com/duranta-project/openairinterface5g/pull/729.

## Terms, symbols and abbreviations

### Terms

For the purposes of the present document, the terms and definitions given in the E3AP
specification [1] and the following apply.

**Indication**: an E3 indication message whose `ranFunctionIdentifier` is `2`. Its `protocolData` field holds one L1-KPM indication payload.

**IQ row**: the IQ samples of one uplink slot, stored as one row of the shared memory region.

**Producer**: the RAN software that writes IQ rows to the region and publishes them.

**Consumer**: the dApp software that reads IQ rows from the region.

**Reference**: the set of values (region name, buffer index, row index) that locates one IQ row.

### Symbols

Void.

### Abbreviations

For the purposes of the present document, the abbreviations given in the E3AP
specification [1] and the following apply.

| Abbreviation | Meaning |
|---|---|
| APER | Aligned Packed Encoding Rules |
| FFT | Fast Fourier Transform |
| FH | Fronthaul (the name of the IQ buffers in the region) |
| FP16 | IEEE 754 binary16 floating point number |
| IQ | In-phase and Quadrature |
| PRB | Physical Resource Block |
| SFN | System Frame Number |
| TDD | Time Division Duplex |
| UL | Uplink |

## General

### Notations

For the purposes of the present document, the following notations apply.

| Notation | Use |
|---|---|
| `Name` | An information element, a field, a JSON key or a value in code. |
| "shall", "should", "may" | As defined in the O-RAN drafting rules. |
| `a..b` | An inclusive integer range. |
| A to B | A segment of the control loops, named by its boxes (see the [control loops](../../control-loops/path-a-e3-loop.md)). |

All multi-byte values in the shared memory region use the byte order of the host.

### Forwards and backwards compatibility

The ASN.1 types of this service model have no extension marker. A new field therefore
changes the APER encoding, and a peer that does not know it cannot decode the message.
The JSON and Protocol Buffers encodings ignore keys and fields that they do not know.

The region has a version number in its header. A consumer shall check it before it reads
a row. The present document defines version `1`.

## Where each part is specified

| Section | What it covers | File |
|---|---|---|
| 1. Identity and registration | RAN function ID, telemetry identifiers, RAN function data, what a RAN shall do to offer the service model | [01-identity-and-registration.md](01-identity-and-registration.md) |
| 2. Information elements | the indication, the reference, the RAN function data, the layout of the shared memory region | [02-ie-mapping.md](02-ie-mapping.md) |
| 3. Procedure flow | subscription, indication delivery, cadence, reading the region, overwrite detection, release | [03-procedure-flow.md](03-procedure-flow.md) |
| 4. Encoding | APER, JSON and Protocol Buffers rules, size limits, the ASN.1 clause | [04-encoding.md](04-encoding.md) |
| 5. Semantics and constraints | units, clocks, absent versus present, lifetime of the region, mandatory behavior, errors | [05-semantics-and-constraints.md](05-semantics-and-constraints.md) |
| 6. Discrepancies | where grammar, codecs and behavior disagree, dead code, open items | [06-discrepancies.md](06-discrepancies.md) |

For an engineer who adds E3SM-L1KPM to a RAN or a dApp there is a separate guide:
[integrator-guide.md](integrator-guide.md). It is not part of the specification.

## Related documents

* [E3AP specification](../../e3ap/README.md): the protocol that carries this service model.
* [latrec](../../utilities/latrec.md): the latency recorder. Its monotonic clock shall not be mixed with the realtime timestamps of other service models.
* [Path A, the E3-only loop](../../control-loops/path-a-e3-loop.md): the boxes that this service model takes part in (A1 to A3 on the RAN, A9 and A10 in the dApp).
* [E3SM Spectrum](../spectrum/README.md): the other E3 service model of the same RAN.
