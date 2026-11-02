# E3SM-L1KPM: discrepancies, TODOs and dead code

Everything found while writing this specification where the grammar, the Protocol Buffers
file, the JSON codec and the behavior of the RAN disagree, where behavior is incomplete,
or where code is dead. Nothing here is a design decision. The decisions are in the other
sections.

Code locations for the evidence are kept in the [integrator guide](integrator-guide.md), which is not part of this document.

Severity: **wrong** means a wrong result with no error, **latent** means only a corner
case triggers it, **doc** means the documentation or a comment only.

## Where the grammar and the behavior disagree

Table: Grammar against behavior

| # | What | Effect | Severity |
|---|---|---|---|
| 1.1 | The grammar marks `iqSamplesRef`, `cellId`, `nRxAnt` and `validSymbolMask` as optional. The RAN sets all four in every indication. | A consumer that tests presence never sees an absent value from this RAN. A consumer that is written for the optional form is not exercised. | latent |
| 1.2 | `cellId` is always `0`. | The value does not identify a cell. | wrong |
| 1.3 | The grammar allows `sfn` up to `65535` and `slot` up to `65535`. NR allows `sfn` up to `1023` and `slot` up to `159`. | A consumer can receive a value that NR never produces and cannot tell it from an error. | doc |
| 1.4 | The grammar allows `fhBufferIndex` up to `255` and `fhWriteIndex` up to `4294967295`. The RAN uses `0..1` and `0..15`. | Same as above. | doc |
| 1.5 | `timestamp` is an unconstrained integer, the RAN holds an unsigned 64 bit value and the ASN.1 codec stores it as a signed value. | A value of `2^63` or more would be encoded as a negative number. The monotonic clock does not reach it. | latent |
| 1.6 | A comment in the grammar says that a subscription of a dApp grants only the four core telemetry identifiers, and that `cellId` and `nRxAnt` are optional for that reason. The RAN does not look at the identifiers. | The identifiers have no effect on the payload. | doc |
| 1.7 | No telemetry identifier exists for `cellId`, `nRxAnt` and `validSymbolMask`. | A dApp cannot ask for them or leave them out. | doc |

## Where the encodings disagree

Table: Encodings against each other

| # | What | Effect | Severity |
|---|---|---|---|
| 2.1 | The RAN function data is APER for an ASN.1 session and JSON for every other session. The Protocol Buffers message `L1KPMRanFunctionData` exists and is not used. | A dApp with a Protocol Buffers session receives JSON text in `ranFunctionData`. | wrong |
| 2.2 | `shmName` is an octet string in ASN.1 and Protocol Buffers and a text string in JSON. | A JSON consumer reads a string. The two other consumers read bytes. | doc |
| 2.3 | In ASN.1, `timestamp`, `sfn` and `slot` are mandatory. In Protocol Buffers every field is optional. | A Protocol Buffers consumer cannot tell a missing field from `0`. | latent |
| 2.4 | The JSON codec always writes all keys. The JSON text of the RAN function data is built by text formatting without escaping. | The values are constants today and are safe. A future value with a quote breaks the JSON. | latent |
| 2.5 | The Protocol Buffers file says that it mirrors a grammar file with a different name from the file that exists. | A reader looks for a file that is not there. | doc |
| 2.6 | `timestamp` is an unsigned 64 bit value in JSON and a signed 64 bit value in Protocol Buffers. | See 1.5. | latent |

## Shared memory

Table: Shared memory against its description

| # | What | Effect | Severity |
|---|---|---|---|
| 3.1 | A row has no marker (no `sfn`, no `slot`, no counter). | A dApp cannot detect an overwrite from the row. The overwrite rules of [procedures](03-procedure-flow.md) rest on the indications. In the periodic mode they cannot see a lag of 32 rows or more. | wrong |
| 3.2 | The header has no field for the scale factor `beta`. | A dApp has to learn `beta` from the configuration of the RAN. A wrong value gives wrong amplitudes with no error. | wrong |
| 3.3 | A comment in the RAN source says that the depth of the buffers gives about 64 ms before the RAN reuses a row. The code has 32 rows, and the time depends on the rate of uplink slots. | The 64 ms could not be reproduced from the code. | doc |
| 3.4 | The RAN writes only the first `nRxAnt` antennas of a row. A comment says that the RAN zero fills the rest "so a dApp always gets a buffer of this shape". | Only the subcarriers above the PRBs of the cell are zero filled by the writer. The antennas above `nRxAnt` keep the content of the region at creation. | latent |
| 3.5 | The PUSCH and channel estimate areas are in the total size and in the header, and no code writes them. The code does not give their offsets. | A consumer shall not read them. Their position is by size arithmetic only. | doc |
| 3.6 | If the cell has more than 273 PRBs, the creation of the region fails at every uplink slot. | One error line for each slot. No indication. | latent |
| 3.7 | The region is created at the first uplink slot after the first subscription, not at the subscription. | A dApp that opens the region right after its subscription can find no object. It should retry until the first indication. | latent |

## Dead and unused code

Table: Dead and unused code

| # | What | Notes |
|---|---|---|
| 4.1 | The producer keeps indices and sizes for the PUSCH and channel estimate areas (`pusch_buffer_index`, `hest_buffer_index`, `hest_data_size`). | They are always `0` and are not encoded. |
| 4.2 | The `valid` flag of the snapshot. | It is set and never read. |
| 4.3 | The header fields `num_pusch_rows`, `num_hest_rows`, `max_hest_samples_per_row`. | Constants. No code reads them. |

## Could not be tied to code

Table: Statements without code evidence

| # | Statement | Where it appeared |
|---|---|---|
| 5.1 | The meaning of the identifiers `2` and `3`, and the claim that "identifier equals bit position plus one". The claim is not a rule of this specification. The numbers come from the external definition of the dApps that use them. The alignment is tracked on the [interoperability page](../../interop.md). | a comment next to the identifiers |
| 5.2 | That the header layout matches an external one byte for byte. | a comment in the region code |
| 5.3 | That the TDD indexing of the symbol mask is correct for a TDD period that is shorter than a frame. | the code uses the slot number as the index into the TDD table |
| 5.4 | The value of the cadence of an absent `periodicity`. | the RAN reads it as `0` through a helper that was not opened |

## Open items

* Add a marker to each row (a slot counter), so a dApp can detect an overwrite.
* Carry `beta` in the header.
* Send the RAN function data as Protocol Buffers for a Protocol Buffers session.
* Decide whether `cellId` is mandatory and fill it.
* Insert the ASN.1 grammar into the clause "Abstract syntax" of [encoding](04-encoding.md).
