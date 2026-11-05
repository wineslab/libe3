# E3SM-L1-KPM

E3SM-L1-KPM is the E3 service model for the uplink layer 1 of a RAN node. It gives a
dApp the data of each uplink slot: the post-FFT IQ samples, the PUSCH channel estimates
and PDUs, the PUSCH decoding and signal quality figures of each UE, and the SRS samples,
estimates and measurements of each UE. Small values travel inside the E3 indication.
Large arrays stay in a shared memory region on the RAN host, and the indication tells
the dApp where to read them.

The service model rides on the E3 Application Protocol (E3AP) [1]. Every message of
this service model is an E3AP message. The service model defines only the content of
the E3AP fields that E3AP leaves to the service model.

| Chapter | Clause |
|---|---|
| Scope, references, terms, general | 1 to 4 (this file) |
| E3SM services | 5, [01-services.md](01-services.md) |
| RAN function service model description | 6, [02-service-model-description.md](02-service-model-description.md) |
| RAN function description | 7, [03-ran-function-description.md](03-ran-function-description.md) |
| Elements for the service model, ASN.1, JSON | 8, [04-elements.md](04-elements.md), [05-abstract-syntax.md](05-abstract-syntax.md), [06-encoding.md](06-encoding.md) |
| Handling of unknown, unforeseen and erroneous protocol data | 9, [07-error-handling.md](07-error-handling.md) |
| Shared memory region | Annex A (normative), [annex-a-shared-memory.md](annex-a-shared-memory.md) |
| Example message exchange | Annex B (informative), [annex-b-examples.md](annex-b-examples.md) |
| Open items | Annex C (informative), [annex-c-open-items.md](annex-c-open-items.md) |

For an engineer who adds E3SM-L1-KPM to a RAN or a dApp there is a separate guide:
[integrator-guide.md](integrator-guide.md). It is not part of the specification.

## Scope

The present document specifies the E3 Service Model "L1 KPM" (E3SM-L1-KPM). The service
model lets a dApp subscribe to the uplink layer 1 data of the cells that the E3 Agent of a
RAN node serves, and receive that data slot by slot.

The present document defines the following:

- the RAN function identity and the RAN Function Data that an E3 Agent announces in the Setup procedure of E3AP [1];
- the telemetry identifiers of the service model and the meaning of each value;
- the information elements carried in the E3AP messages, in ASN.1 [2] and in JSON [4];
- the subscription data that selects cells and UEs, and the control action that applies it;
- the shared memory region that holds the large data arrays, in Annex A.

The present document does not define E3AP messages or procedures. They are defined in
[1]. It does not define how a RAN node computes the values that it reports. It does not
define the transport between the dApp and the E3 Agent.

## References

### Normative references

References are specific. Only the cited version applies.

The following referenced documents are necessary for the application of the present
document.

[1] "[E3 Application Protocol (E3AP)](../../e3ap/README.md)", R000 v00.01.

[2] ITU-T Recommendation X.680 (2002-07): "Information technology: Abstract Syntax Notation One (ASN.1): Specification of basic notation".

[3] ITU-T Recommendation X.691 (2002-07): "Information technology: ASN.1 encoding rules: Specification of Packed Encoding Rules (PER)".

[4] IETF RFC 8259 (2017-12): "The JavaScript Object Notation (JSON) Data Interchange Format".

[5] IEEE Std 754-2019: "IEEE Standard for Floating-Point Arithmetic".

[6] IEEE Std 1003.1-2017: "IEEE Standard for Information Technology, Portable Operating System Interface (POSIX) Base Specifications, Issue 7".

[7] 3GPP TS 38.211: "NR; Physical channels and modulation".

[8] 3GPP TS 38.212: "NR; Multiplexing and channel coding".

[9] 3GPP TS 38.214: "NR; Physical layer procedures for data".

[10] 3GPP TS 38.331: "NR; Radio Resource Control (RRC); Protocol specification".

### Informative references

The following referenced documents are not necessary for the application of the
present document, but they assist the user with regard to a particular subject area.

[i.1] 3GPP TS 38.213: "NR; Physical layer procedures for control".

[i.2] 3GPP TS 38.215: "NR; Physical layer measurements".

[i.3] 3GPP TS 38.300: "NR; NR and NG-RAN Overall description; Stage 2".

[i.4] Google: "Protocol Buffers, Language Guide (proto 2)".


## Terms, symbols and abbreviations

### Terms

For the purposes of the present document, the terms given in [1] and the following apply.

**Blob**: a contiguous run of bytes in a buffer of the shared memory region that holds the data of one cell or of one UE for one slot.

**Buffer**: one half of a pair of equal areas of the shared memory region that hold the data of one stream. The E3 Agent writes one buffer while the dApp reads the other.

**Cell scope**: the place of a value in an entry of the cell list of an indication. The value describes one cell in one slot.

**E3 Agent**: the entity in the RAN node that terminates E3AP on the RAN side [1]. In the present document it hosts the RAN function, sends the indications and writes the shared memory region.

**Granted identifier**: a telemetry identifier or control identifier that the E3 Agent lists in its SUBSCRIPTION RESPONSE message for a subscription.

**Indication path**: the part of the uplink processing that triggers an indication. The present document defines the PUSCH path and the SRS path.

**Inline stream**: a stream whose value travels in the indication.

**PUSCH path**: the indication path that fires once for each slot with PUSCH reception in at least one cell.

**Region**: the POSIX shared memory object that holds the buffers of the shared memory streams.

**Row**: the part of a buffer that the E3 Agent writes for one slot. A row holds one blob, or the blobs of several UEs.

**Shared memory stream**: a stream whose value stays in the region. The indication carries a reference to it.

**Slot scope**: the place of a value at the root of an indication. The value describes the slot as a whole.

**SRS path**: the indication path that fires once for each slot with SRS reception in at least one cell.

**Stream**: the data that one telemetry identifier names.

**Subscription data**: the selection of cells and UEs that applies to the subscription of a dApp.

**UE group**: a set of UEs that the receiver of the RAN node processes together, for example UEs that share PRBs in MU-MIMO.

**UE scope**: the place of a value in an entry of the UE list of a cell. The value describes one UE in one cell in one slot.

### Symbols

Void.

### Abbreviations

For the purposes of the present document, the abbreviations given in [1] and the following apply.

| Abbreviation | Meaning |
|---|---|
| APER | Aligned Packed Encoding Rules |
| CB | Code Block |
| CFO | Carrier Frequency Offset |
| CP | Cyclic Prefix |
| CRC | Cyclic Redundancy Check |
| DMRS | Demodulation Reference Signal |
| FDD | Frequency Division Duplex |
| FDM | Frequency Division Multiplexing |
| FFT | Fast Fourier Transform |
| HARQ | Hybrid Automatic Repeat Request |
| IQ | In-phase and Quadrature |
| KPM | Key Performance Measurement |
| MCS | Modulation and Coding Scheme |
| MU-MIMO | Multi-User Multiple Input Multiple Output |
| PDU | Protocol Data Unit |
| PRB | Physical Resource Block |
| PRG | Physical Resource Block Group |
| PUSCH | Physical Uplink Shared Channel |
| QAM | Quadrature Amplitude Modulation |
| RNTI | Radio Network Temporary Identifier |
| RSRP | Reference Signal Received Power |
| RSSI | Received Signal Strength Indicator |
| SFN | System Frame Number |
| SINR | Signal to Interference plus Noise Ratio |
| SNR | Signal to Noise Ratio |
| SRS | Sounding Reference Signal |
| TAI | International Atomic Time |
| TB | Transport Block |
| TDD | Time Division Duplex |
| UE | User Equipment |
| UTC | Coordinated Universal Time |

## General

### Notations

For the purposes of the present document, the notations of [1] apply, with the following additions.

| Item | Notation |
|---|---|
| Service model IE | The name of an information element of the present document is in capital letters and followed by "IE". Example: CELL DATA IE. Its ASN.1 type has the prefix "L1KPM-". Example: L1KPM-CellData. |
| Field | The ASN.1 field name in a monospaced font. Example: `rbStart`. |
| JSON key | The key in a monospaced font and in quotation marks when it differs from the field name. Example: "`rb_start`". |
| Telemetry identifier | "ID" followed by the number. Example: ID 25. |
| Range | `a..b` is an inclusive integer range. |

The presence column of the tables uses "M" for mandatory, "O" for optional and "C" for conditional, as in [1].

### Entities

The present document uses the entities of [1] as follows (Figure 5-1).

- The E3 Agent is part of the RAN node. It terminates E3AP on the RAN side and hosts the RAN function of the present document. It takes the data of each uplink slot from the layer 1 of the RAN node, writes the shared memory region of Annex A on the host of the RAN node, and sends the indications.
- The dApp runs in the RAN node or next to it, as [1] allows. It reaches the RAN function only through E3AP and the shared memory region. A dApp that reads shared memory streams runs on the host of the RAN node (clause 6.5).

How the E3 Agent obtains the data from the layer 1 inside the RAN node is out of the
scope of the present document.

### Relation to E3AP

The present document follows [1] in every respect that [1] defines. It adds no message,
no procedure and no field to E3AP. When the present document and [1] disagree on a
matter that [1] defines, [1] applies.

E3AP [1] offers a dApp a set of procedures to reach a RAN function. A service model
gives a meaning to the payloads that those procedures carry. E3SM-L1-KPM uses the
following E3AP procedures.

Table: E3AP procedures used by E3SM-L1-KPM

| E3AP procedure | Clause of [1] | Use in the present document | Service model payload |
|---|---|---|---|
| Setup | 7.3 | The E3 Agent announces the RAN function and its streams. | RAN FUNCTION DATA IE in `ranFunctionData` |
| Subscription | 7.4 | The dApp chooses the telemetry identifiers, the interval and the duration. | none |
| Subscription delete | 7.5 | The dApp ends its subscription. | none |
| Indication | 7.6 | The E3 Agent delivers the data of one slot of one indication path. | INDICATION DATA IE in `protocolData` |
| dApp Control | 7.7 | The dApp selects cells and UEs for its subscription. | CONTROL DATA IE in `actionData` |
| Release | 7.10 | The dApp or the E3 Agent ends the association. | none |

The dApp Report procedure and the xApp Control procedure of [1] are not used by the
present document.

E3AP carries the payloads of the present document in its octet string fields and does
not read them. The service model selects its payload by the field that carries it, as
Table 4-1 lists. In an E3AP session with the JSON encoding, the payloads are JSON
values nested in the E3AP message, as clause 8.3 of [1] defines.

### Forwards and backwards compatibility

The service model uses these means to stay compatible when it grows.

1. Every SEQUENCE and every CHOICE of the grammar has an extension marker. A receiver shall ignore an extension addition that it does not know.
2. A new telemetry identifier adds an optional field after the extension marker of the type that holds it. A receiver that does not know the identifier does not see the field.
3. The enumerations of the grammar have no extension marker. A new value needs a new version of the service model.
4. The `version` field of the RAN FUNCTION DATA IE carries the version of the service model. The present document defines version `1`. A dApp shall not decode the payloads of a RAN function whose version it does not support.
5. A JSON receiver ignores keys that it does not know (clause 8.6).
