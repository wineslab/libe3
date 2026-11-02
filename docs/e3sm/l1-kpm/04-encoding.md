# E3SM-L1KPM: encoding

How the elements of E3SM-L1KPM become bytes: the choice of encoding, the rules of each
encoding, the size limits, and the ASN.1 clause.

The elements are in the [information elements](02-ie-mapping.md).

## Choice of encoding

The service model has one codec for each of three encodings. The RAN uses the encoding of
its E3AP session for the payloads of this service model. A RAN with an ASN.1 E3AP session
sends APER. A RAN with a JSON E3AP session sends JSON. A RAN with a Protocol Buffers
session sends Protocol Buffers. A dApp shall use the same encoding as its session.

Table: Where the payload sits in each E3AP encoding

| E3AP encoding | Payload form | Place in the E3 message |
|---|---|---|
| ASN.1 APER | bytes of the APER encoding of L1KPM-Indication | octet string `protocolData` |
| JSON | one JSON object [5] | the value of the key `protocolData`, as a nested object |
| Protocol Buffers | bytes of the message `L1KPMIndication` | bytes field `protocol_data` |

The RAN function data has its own form. It is APER when the session uses ASN.1. It is a
JSON object for every other encoding, also for Protocol Buffers. See
[discrepancies](06-discrepancies.md).

## ASN.1 APER

The grammar uses automatic tags and no extension marker. The encoding is APER [4].

Table: APER rules for the types of E3SM-L1KPM (derived from X.691, not checked with an encoder)

| Element | Constraint | APER form |
|---|---|---|
| L1KPM-Indication | 4 optional components | a preamble of 4 bits, in the order `iqSamplesRef`, `cellId`, `nRxAnt`, `validSymbolMask` |
| `timestamp` | integer, not constrained | one length octet, then the value in the fewest octets as a two's complement number |
| `sfn`, `slot`, `cellId`, `nRxAnt` | `0..65535` | 2 octets, octet aligned |
| `validSymbolMask` | `0..16383` | 2 octets, octet aligned |
| `shmName` | octet string, length `1..63` | a 6 bit length, then the octets, octet aligned |
| `fhBufferIndex` | `0..255` | 1 octet, octet aligned |
| `fhWriteIndex` | `0..4294967295` | a 2 bit length of 1 to 4 octets, then the value in the fewest octets, octet aligned |
| `name` | octet string, length `1..32` | a 5 bit length, then the octets |
| `version` | `0..65535` | 2 octets, octet aligned |
| `description` | octet string, length `1..128` | a 7 bit length, then the octets |

With all fields present, the payload is short. It needs less than 50 octets. The value was
not measured. The encoder of the RAN writes into a buffer of 512 bytes.

No byte level example is given. No example was checked against an encoder.

## JSON

The payload is one JSON object. All numbers are decimal integers. The RAN writes all keys
in every object.

Table: JSON keys of L1KPM-Indication

| Key | Type | Value |
|---|---|---|
| `iq_samples` | object | `shm_name` (string), `fh_buffer_index` (integer), `fh_write_index` (integer) |
| `timestamp` | integer | nanoseconds, monotonic clock |
| `sfn` | integer | system frame number |
| `slot` | integer | slot number |
| `cell_id` | integer | cell identifier |
| `n_rx_ant` | integer | number of antennas |
| `valid_symbol_mask` | integer | bit map |

The keys of the JSON form are the snake case names of the ASN.1 fields in
[information elements](02-ie-mapping.md). The longest object that the RAN can write has 212
bytes. A dApp shall accept the keys in any order and shall accept missing optional keys.

The JSON form of the RAN function data has the keys `name` (string), `version` (integer)
and `description` (string). Its length is 79 bytes.

## Protocol Buffers

The messages are in the proto2 syntax [i.4], in the package `e3sm.l1kpm.v1`. All fields
are optional.

Table: Protocol Buffers messages of E3SM-L1KPM

| Message | Field | Number | Type |
|---|---|---|---|
| L1KPMShmRef | `shm_name` | 1 | bytes |
| | `fh_buffer_index` | 2 | uint32 |
| | `fh_write_index` | 3 | uint32 |
| L1KPMIndication | `iq_samples_ref` | 1 | L1KPMShmRef |
| | `timestamp` | 2 | int64 |
| | `sfn` | 3 | uint32 |
| | `slot` | 4 | uint32 |
| | `cell_id` | 5 | uint32 |
| | `n_rx_ant` | 6 | uint32 |
| | `valid_symbol_mask` | 7 | uint32 |
| L1KPMRanFunctionData | `name` | 1 | bytes |
| | `version` | 2 | uint32 |
| | `description` | 3 | bytes |

The RAN of the present specification sets every field of L1KPMIndication. It does not use
L1KPMRanFunctionData.

## Size limits

Table: Size limits

| Item | Limit | Source |
|---|---|---|
| Payload in an E3 indication | `1..32768` octets | E3AP `protocolData` |
| Payload buffer of the RAN | 512 bytes | the RAN codec |
| RAN function data | `1..262144` octets in the grammar, `1..32768` octets in the E3AP implementation of this edition | E3AP `ranFunctionData` |
| RAN function data buffer of the RAN | 256 bytes | the RAN codec |
| `shmName` | `1..63` octets | ASN.1 |

## Abstract syntax

The abstract syntax of E3SM-L1KPM conforms to ITU-T X.680 [2] and X.681 [3]. It is
reserved for a later edition of the present document. The grammar is kept by the
OpenAirInterface implementation [i.7].

```asn1
-- ASN1START
-- The grammar of this service model is to be inserted here. See the OpenAirInterface implementation reference.
-- ASN1STOP
```
