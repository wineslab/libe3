# Encoding of the service model IEs

## Encoding

### Choice of encoding

The IEs of the present document use the encoding of the E3AP session that carries
them (clause 8.1 of [1]). Table 8-15 gives the form of each IE in each encoding.

Table: Form of the service model IEs in each E3AP encoding

| E3AP encoding | Form of the IE | Place in the E3AP message |
|---|---|---|
| APER | the APER encoding of the ASN.1 type of clause 8.4 | the octet string of the E3AP field |
| JSON | one JSON value as clause 8.6 defines | the value of the E3AP key, nested in the E3AP message (clause 8.3 of [1]) |
| Protobuf | not defined by the present document | not defined |

A dApp shall use the encoding of its E3AP session for the IEs of the present document.
An E3 Agent that runs sessions with different encodings encodes the IEs for each
session apart.

> NOTE: Annex C lists a Protocol Buffers form as an open item.

### APER

The IEs are encoded with the aligned variant of the basic PER [3], as complete
encodings. The encoding of an IE is the octet string that E3AP carries. E3AP does not
add a length or a tag inside the octet string.

The following rules decide the size of most encodings.

- Each SEQUENCE starts with one extension bit and one presence bit for each optional field. The UE DATA IE has 60 optional fields, so its preamble takes 61 bits.
- A constrained INTEGER takes the smallest number of bits of its range when the range is up to 255 values, one octet for a range of 256 values, and two octets for a range of up to 65536 values, octet aligned.
- L1KPM-Uint32 takes a length of 2 bits and 1 to 4 octets.
- L1KPM-TimestampNs is not constrained. It takes one length octet and the octets of the value, at most 8. A time since 1970 (ID 78) takes 8 octets. A monotonic time (ID 4) takes 7 octets on a host that has been up between 1.6 and 417 days.
- L1KPM-Float32 is a fixed size OCTET STRING. It takes 4 octets, octet aligned, with no length.
- A SEQUENCE OF with a size constraint takes a length of the bits of its range.
- An OCTET STRING with a size constraint takes a length of the bits of its range, then its octets, octet aligned. For example, `shmName` (1 to 63 octets) takes a length of 6 bits, `name` (1 to 32 octets) 5 bits and the `description` of the RAN FUNCTION DATA IE (1 to 128 octets) 7 bits.

An E3 Agent may encode an INDICATION DATA IE once and send the same octets to all the
dApps with the same granted identifiers and the same subscription data.

## JSON encoding

### General

In an E3AP session with the JSON encoding, each IE of the present document is one JSON
value [4], nested in the E3AP message. The value of `protocolData` is the JSON object of
the INDICATION DATA IE. The value of `ranFunctionData` is the JSON object of the RAN
FUNCTION DATA IE. The value of `actionData` is the JSON object of the CONTROL DATA IE. The
service model payload is never a string that holds JSON text.

The following rules apply to every IE.

- A SEQUENCE is a JSON object. Each present field is a member. An absent field has no member. A sender shall not write `null`. A receiver shall treat a member with the value `null` as absent.
- The key of a field with a telemetry identifier is the JSON key of clause 7.3. The keys of the other fields are given in clauses 8.6.2 to 8.6.4.
- The members may come in any order. A receiver shall ignore a member that it does not know.
- An INTEGER is a JSON number without fraction and without exponent. A sender shall keep every value within the range of clause 8.4, so that the same IE can be encoded in APER.
- L1KPM-TimestampNs is a JSON number. A receiver shall read it as a 64 bit integer. A receiver that reads it as an IEEE 754 binary64 value loses the nanoseconds above 2^53.
- L1KPM-Float32 is a JSON number. A sender should write enough digits to restore the binary32 value, for example 9 significant digits. A value that is not finite is not sent (clause 8.1).
- L1KPM-Complex32 is a JSON array of two numbers, the real part first.
- An OCTET STRING of the present document holds text. It is a JSON string.
- L1KPM-StreamStatus is the string "available" or the string "unavailable".
- A SEQUENCE OF is a JSON array.
- The form of each CHOICE is given in clauses 8.6.3 and 8.6.4.

The JSON encoding does not check the ranges of the grammar. A receiver should check the
values that it uses.

### INDICATION DATA, CELL DATA and UE DATA

The keys of the fields with a telemetry identifier are those of Tables 7-3 to 7-6. The
lists use the keys of Table 8-16.

Table: JSON keys of the lists of the indication

| Field | JSON key | JSON type |
|---|---|---|
| `cells` of the INDICATION DATA IE | `cells` | array of CELL DATA objects |
| `ues` of the CELL DATA IE | `ues` | array of UE DATA objects |

A shared memory reference is a JSON object. Its keys depend on the stream, as Table
8-17 gives. Every reference has the key `shm_name`.

Table: JSON keys of the Shared Memory Reference for each stream

| ID | Key of the reference | Keys of `bufferIndex`, `writeIndex` and `rowByteOffset` |
|---|---|---|
| 1 | `iq_samples` | `fh_buffer_index`, `fh_write_index` |
| 2 | `pdu_data` | `pusch_buffer_index`, `pusch_write_index` |
| 3 | `h_estimates` | `hest_buffer_index`, `hest_write_index`, `hest_row_byte_offset` |
| 44 | `srs_iq_samples` | `srs_iq_buffer_index`, `srs_iq_write_index`, `srs_iq_row_byte_offset` |
| 45 | `srs_hest` | `srs_hest_buffer_index`, `srs_hest_write_index` |
| 49 | `srs_rb_snr` | `srs_rb_snr_buffer_index`, `srs_rb_snr_write_index` |

A dApp that finds no `valid_symbol_mask` in a cell, while it has requested ID 1000,
shall read the mask as `16383` (all symbols uplink).

The following object is the `protocolData` of an indication of the PUSCH path with one
cell and two UEs.

```json
{
  "timestamp": 7078771133456789,
  "sfn": 513,
  "slot": 9,
  "n_cells": 1,
  "cells": [
    {
      "cell_id": 1,
      "n_rx_ant": 4,
      "n_ue": 2,
      "valid_symbol_mask": 16383,
      "iq_samples": { "shm_name": "/e3_ran_buffers", "fh_buffer_index": 0, "fh_write_index": 42 },
      "ues": [
        { "rnti": 17921, "rb_start": 0,  "rb_size": 51, "mcs_index": 20, "tb_crc_fail": 0,
          "harq_process_id": 3, "rsrp": -81.4, "sinr": 18.6 },
        { "rnti": 17922, "rb_start": 51, "rb_size": 24, "mcs_index": 9,  "tb_crc_fail": 1,
          "harq_process_id": 0, "rsrp": -96.2, "sinr": 4.1 }
      ]
    }
  ]
}
```

### RAN FUNCTION DATA

Table: JSON keys of the RAN FUNCTION DATA IE and its parts

| IE | Field | JSON key | JSON type |
|---|---|---|---|
| RAN FUNCTION DATA | `name` | `name` | string |
| | `version` | `version` | number |
| | `description` | `description` | string |
| | `streams` | `streams` | array of STREAM DESCRIPTOR objects |
| STREAM DESCRIPTOR | `telemetryId` | `telemetryIdentifier` | number |
| | `streamId` | `stream_id` | string |
| | `dataType` | `data_type` | string |
| | `description` | `description` | string |
| | `status` | `status` | string |
| | `delivery` | `delivery_method` | object, see below |
| Delivery, `sharedMemory` | | `transport_type` | the string "shared_memory" |
| | Shared Memory Information | `shared_memory_info` | object |
| Delivery, `protocolData` | | `transport_type` | the string "protocolData" |
| | `keyword` | `keyword` | string |
| Shared Memory Information | `memoryKey` | `memory_key` | string |
| | `memorySizeBytes` | `memory_size_bytes` | number |
| | `numRows` | `num_rows` | number |

The member `transport_type` selects the alternative of the Delivery. A receiver shall
ignore the other members of `shared_memory_info` and of `delivery_method`. A receiver
that finds no `num_rows` may read `max_elements` instead.

For compatibility with existing E3 Agents, a receiver should also accept a
`ranFunctionData` that is a JSON array. The array is then the list of stream
descriptors, and the name, the version and the description are unknown. A sender of the
present document shall send the object form.

```json
{
  "name": "L1-KPM",
  "version": 1,
  "description": "L1-KPM: post-FFT IQ, PUSCH and SRS telemetry",
  "streams": [
    { "telemetryIdentifier": 1, "stream_id": "iq_samples", "data_type": "array(int16)",
      "status": "available",
      "delivery_method": { "transport_type": "shared_memory",
                           "shared_memory_info": { "memory_key": "/e3_ran_buffers",
                                                   "memory_size_bytes": 88058880, "num_rows": 120 } } },
    { "telemetryIdentifier": 18, "stream_id": "rsrp", "data_type": "float32",
      "status": "available",
      "delivery_method": { "transport_type": "protocolData", "keyword": "rsrp" } },
    { "telemetryIdentifier": 36, "stream_id": "ue_grp_idx", "data_type": "uint16",
      "status": "unavailable" }
  ]
}
```

### SUBSCRIPTION DATA and CONTROL DATA

Table: JSON keys of the SUBSCRIPTION DATA IE and the CONTROL DATA IE

| IE | Field | JSON key | JSON type |
|---|---|---|---|
| SUBSCRIPTION DATA | `cellFilter` | `cell_filter` | array of numbers |
| | `ueFilter` | `ue_filter` | array of UE Reference objects |
| UE Reference | `cellId` | `cell_id` | number |
| | `rnti` | `rnti` | number |
| CONTROL DATA | `setSubscriptionData` | `set_subscription_data` | SUBSCRIPTION DATA object |

The CONTROL DATA object has exactly one member, the key of its alternative. The
following object is the `actionData` that keeps one UE of cell 1.

```json
{ "set_subscription_data": { "ue_filter": [ { "cell_id": 1, "rnti": 17921 } ] } }
```

An empty object `{}` as the value of `set_subscription_data` selects every cell and
every UE.

## Size limits

### Limits

Table: Size limits of the service model IEs

| Item | Limit | Source |
|---|---|---|
| INDICATION DATA IE in APER | 1 to 32768 octets | `protocolData`, clause 8.6 of [1] |
| CONTROL DATA IE in APER | 1 to 32768 octets | `actionData`, clause 8.6 of [1] |
| RAN FUNCTION DATA IE in APER | 1 to 262144 octets | `ranFunctionData`, clause 8.6 of [1] |
| E3AP message in JSON, with the nested IE | 60000 octets on reception | clause 8.6 of [1] |
| Entries of `cells` | 16 | `maxL1KPMCells` |
| Entries of `ues` of a cell | 64 | `maxL1KPMUesPerCell` |
| Stream descriptors | 128 | `maxL1KPMStreams` |
| Entries of `ueFilter` | 64 | `maxL1KPMFilterUes` |

> NOTE: Some E3AP implementations accept at most 32768 octets of `ranFunctionData`, less than [1] allows. The RAN FUNCTION DATA IEs of Table 8-21 fit in that limit.

### Indications above the limits

The E3 Agent shall not send an INDICATION DATA IE above the limits of Table 8-20. When
the data of one slot on one path would exceed them, the E3 Agent shall split the data
over several indications of the same path, in this order of preference.

1. Split the entries of `cells` over several indications. An entry of a cell is not split.
2. When the entry of one cell alone exceeds the limit, split its `ues` over several indications. Each of them repeats the cell scope fields of the cell.

Each indication of the split carries the same slot scope fields. `nCells`, `nUe` and
`nSrsUe` count the entries of the indication that carries them. A dApp recognizes the
indications of one slot by equal `sfn` and `slot` values.

### Typical sizes

Table 8-21 gives encoded sizes computed with the grammar of clause 8.4, with every field
present and the largest values. The JSON sizes are those of compact JSON text without
white space. They are informative.

Table: Encoded sizes of the service model IEs (informative)

| Case | APER octets | JSON octets |
|---|---|---|
| One UE DATA IE, every PUSCH path field | 80 | 593 |
| One UE DATA IE, every SRS path field | 77 | 641 |
| Indication, PUSCH path, every field, 1 cell, 1 UE | 184 | 1115 |
| Indication, PUSCH path, every field, 1 cell, 16 UEs | 1384 | 10026 |
| Indication, PUSCH path, every field, 1 cell, 64 UEs | 5224 | 38538 |
| Indication, PUSCH path, every field, 4 cells, 16 UEs each | 5467 | 39765 |
| Indication, PUSCH path, every field, 16 cells, 64 UEs each | 83239 | 614914 |
| Indication, SRS path, every field, 1 cell, 64 UEs | 5033 | 41681 |
| Indication, SRS path, every field, 16 cells, 64 UEs each | 80183 | 665202 |
| RAN FUNCTION DATA IE, 80 stream descriptors with descriptions | 8322 | 20906 |
| RAN FUNCTION DATA IE, 80 stream descriptors without descriptions | 3041 | 14345 |
| SUBSCRIPTION DATA IE, 16 cells and 64 UEs | 353 | 1989 |
| CONTROL DATA IE, one UE | 6 | 68 |

With every field present, the APER limit holds about 400 UEs in one indication, and the
JSON limit about 90.
