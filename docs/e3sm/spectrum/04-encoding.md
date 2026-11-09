# E3SM-SPEC: encoding

How each message of the Spectrum service model is written on the wire: the APER widths, the JSON
keys, the Protocol Buffers messages, the size limits, and the place of the ASN.1 grammar.

This is clause 8 of the specification. The fields are in
[clause 6](02-ie-mapping.md). The encodings of the outer E3AP message are in the
[E3AP encoding chapter](../../e3ap/04-encoding.md).

## Principles

1. A service model message is an octet string for E3AP. It is not part of the E3AP grammar.
2. The RAN serves one encoding for each run. The service model messages use the same encoding as the
   E3AP messages of that run. The encodings are APER (ASN.1), JSON and Protocol Buffers.
3. A receiver decodes only the encoding of the run. A control in another encoding fails to decode, and
   the RAN sends a negative E3 Control Response.
4. One exception exists. The apply outcome is encoded with APER in every run, because it travels on the
   E2 interface, which has no JSON or Protocol Buffers form of it.

Table: Encodings by message

| Message | APER | JSON | Protocol Buffers |
|---|---|---|---|
| RAN function data | yes | yes | yes |
| Sensing indication | yes | yes | yes |
| Cell configuration indication | yes | yes | no |
| dApp control | yes | yes | yes |
| dApp report | yes | required, no codec yet | yes |
| xApp control | yes | required, no codec yet | yes |
| Apply outcome | yes | no | no |

"Yes" means that a grammar, a Protocol Buffers message or a codec exists. "Required, no codec yet"
means that this specification requires the JSON form of the JSON section and that no codec exists
today.

## APER

The grammar uses automatic tags. The encoding rules are ITU-T X.691 [4] in the aligned variant. The
text below states the consequence for each type. The widths are derived from the rules by hand. They
are not checked against an encoder.

Table: APER width by constraint

| Constraint | Width in the encoding |
|---|---|
| INTEGER with a range of 2 to 255 values | the smallest number of bits, with no alignment |
| INTEGER with a range of 256 values | 1 octet, aligned |
| INTEGER with a range of 257 to 65536 values | 2 octets, aligned |
| INTEGER with no constraint | a length octet, then the shortest two's complement octets |
| OCTET STRING with a size range | the length as a constrained number of bits, then the octets, aligned |
| OCTET STRING with no size constraint | a length octet for lengths below 128, then the octets, aligned |
| SEQUENCE OF with a size range | the count as a constrained number, then the items |
| BOOLEAN | 1 bit |
| SEQUENCE | one preamble bit for each OPTIONAL or DEFAULT field. One more bit first, when the sequence has an extension marker. |
| CHOICE with an extension marker | one extension bit, then the alternative index in the smallest number of bits |

Table: Widths of the fields in this service model

| Field | Range | Width |
|---|---|---|
| `Spectrum-RanFunctionData.name` | size 1..32 | 5 bit length, then the octets |
| `Spectrum-RanFunctionData.version` | 0..65535 | 2 octets |
| `Spectrum-RanFunctionData.description` | size 1..128 | 7 bit length, then the octets |
| `Spectrum-Indication` alternative | 2 alternatives, no extension marker | 1 bit |
| `sfn`, `slot` in the indication | 0..65535 | 2 octets each |
| `beam` | 0..3 | 2 bits |
| `shmWriteIdx` | 0..65535 | 2 octets |
| `nRanges` | 0..128 | 8 bits |
| `blacklistedPRBs`, `blockedPRBs` count | 0..273 | 2 octets |
| each PRB index | 0..272 | 2 octets |
| `maskPerSlot` count | 1..160 | 8 bits |
| each symbol mask | 0..16383 | 2 octets |
| `noiseFloorThreshold` | -100..100 | 8 bits |
| `averagingFrames` | 1..255 | 8 bits |
| `validityPeriod` | 1..3600000 | a 2 bit length, then 1 to 3 octets |
| `ulCarrierPRBs` | 1..273 | 2 octets |
| `ulOffsetToCarrier` | 0..272 | 2 octets |
| `ulSubcarrierSpacing` | 0..3 | 2 bits |
| `ulArfcnPointA` | 0..3279165 | a 2 bit length, then 1 to 3 octets |
| `slotsPerFrame` | 1..160 | 8 bits |
| `tddSlotPattern` | size 1..160 | 8 bit length, then the octets |
| `nrPci` | 0..1007 | 2 octets |
| `sfn`, `slot` in the apply outcome | 0..1023, 0..159 | 2 octets, 8 bits |

An encoded list of n PRB indices needs about 2 + 2n octets. The longest list, 273 indices, needs about
548 octets, and so it fits the E3AP limit of 32768 octets by a wide margin.

### Extension marker and extension alternative

The choices `Spectrum-DAppControlPayload`, `Spectrum-DAppReportPayload`,
`Spectrum-XAppControlPayload` and `Spectrum-ApplyOutcomePayload` have an extension marker.

* A root alternative uses one extension bit with the value "0". In a choice with one root alternative
  the alternative index takes no bits.
* An extension alternative uses the extension bit "1", then an index, then an open type. The open type
  holds a length and the encoded alternative.

`Spectrum-DAppControlPayload`, `Spectrum-DAppReportPayload` and `Spectrum-ApplyOutcomePayload` have one
root alternative, which takes the extension bit and no index. `Spectrum-XAppControlPayload` has two
root alternatives, `prbBlockedControl` and `configControl`, which take the extension bit and a 1 bit
index. `Spectrum-DAppControlPayload` also has `sensingPolicyControl` after the extension marker. Its
encoding is longer by the index and the length prefix than it would be in the root. The rule keeps
the encoding of the first alternative stable when a second alternative is added. Without the extension marker, going from one alternative
to two would change the width of the index from 0 bits to 1 bit. Every existing sender would then
break.

The choice `Spectrum-Indication` has two root alternatives and no extension marker. Its index takes
one bit. A third indication would change that width.

A sender that adds an alternative to a choice shall add it after the extension marker. A sender shall
not move an existing alternative.

## JSON

The JSON encoding follows RFC 8259 [5].

### RAN function data

```json
{"name": "spectrum_sm", "version": 1, "description": "Spectrum service model for sensing range indication, PRB block and sensing policy control"}
```

### Indication

The JSON indication is an object with the member of the alternative, `sensingIndication` or
`cellConfigIndication`, and a member `indicationType` with the value "sensing" or "cellConfig".
The key `indicationType` is not a field of the grammar. A codec adds it for readability. A receiver
shall use the member of the alternative to tell the two indications apart.

The members of the two alternatives use other names than the grammar.

### Sensing indication

The JSON sensing indication groups the reference to the ring in one object.

```json
{"indicationType": "sensing",
 "sensingIndication": {"sensing_shm": {"shm_name": "/e3_l2_sensing", "write_idx": 17, "n_ranges": 3},
                       "timestamp": 7078771133456789, "sfn": 512, "slot": 7, "beam": 0}}
```

The values in the example are for illustration. They are not taken from a captured message.

Table: JSON keys of the sensing indication, inside `sensingIndication`

| Key | Grammar field | Type |
|---|---|---|
| `sensing_shm.shm_name` | `shmName` | string |
| `sensing_shm.write_idx` | `shmWriteIdx` | number |
| `sensing_shm.n_ranges` | `nRanges` | number |
| `timestamp` | `timestamp` | number |
| `sfn` | `sfn` | number |
| `slot` | `slot` | number |
| `beam` | `beam` | number |

### Cell configuration indication

```json
{"indicationType": "cellConfig",
 "cellConfigIndication": {"timestamp": 7078771000000000, "ul_carrier_prbs": 273,
                          "ul_offset_to_carrier": 0, "ul_subcarrier_spacing": 1,
                          "ul_arfcn_point_a": 620000, "slots_per_frame": 20,
                          "tdd_slot_pattern": "DDDDDDDSUU", "nr_pci": 1}}
```

Table: JSON keys of the cell configuration indication, inside `cellConfigIndication`

| Key | Grammar field | Type |
|---|---|---|
| `timestamp` | `timestamp` | number |
| `ul_carrier_prbs` | `ulCarrierPRBs` | number |
| `ul_offset_to_carrier` | `ulOffsetToCarrier` | number |
| `ul_subcarrier_spacing` | `ulSubcarrierSpacing` | number |
| `ul_arfcn_point_a` | `ulArfcnPointA` | number |
| `slots_per_frame` | `slotsPerFrame` | number |
| `tdd_slot_pattern` | `tddSlotPattern` | string of "D", "U" and "S", left out for an FDD cell |
| `nr_pci` | `nrPci` | number, left out when absent |

### dApp control

The JSON control has the same shape as the grammar. The key of the alternative is the discriminator.

```json
{"controlType": "prbBlacklist",
 "controlPayload": {"prbBlacklistControl": {"blacklistedPRBs": [10, 11, 12],
                                            "validityPeriod": 60000}}}
```

```json
{"controlType": "sensingPolicy",
 "controlPayload": {"sensingPolicyControl": {"maskPerSlot": [0, 0, 0, 16256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                                             "deactivate": false, "validityPeriod": 60000}}}
```

The key `controlType` is not a field of the grammar. A codec adds it for readability.

* A sender may omit `controlType`.
* If `controlType` is present, it shall be a string. A receiver rejects the control if the string is
  not the one that matches the control identifier ("prbBlacklist" for 1, "sensingPolicy" for 2).
* The object under `controlPayload` shall hold the key of the expected alternative. A control that
  holds another key is rejected.

The key `blacklistedPRBs` is required, also for a clear. An empty array clears the block. A message
without the key is rejected, and it does not clear the block. The key `maskPerSlot` is required and
shall hold at least one value.

### Unknown keys and ranges

A receiver ignores keys that it does not know. A receiver rejects an array element that is outside the
range of the grammar.

### dApp report and xApp control

An implementation shall write the dApp report and the xApp control in JSON with the same rules as the
dApp control: the members have the names of the grammar, the alternative of the payload is the member
of the payload object, and a member `reportType` or `controlType` names the alternative. The member
`timestamp` is the realtime time stamp in nanoseconds and is left out when it is absent.

```json
{"reportType": "prbBlacklist", "timestamp": 1756800000123456789,
 "reportPayload": {"prbBlacklistReport": {"blacklistedPRBs": [10, 11, 12]}}}
```

```json
{"controlType": "prbBlocked", "timestamp": 1756800000123456789,
 "controlPayload": {"prbBlockedControl": {"blockedPRBs": [10, 11, 12]}}}
```

The values of `reportType` and `controlType` are `prbBlacklist` and `prbBlocked`. The alternative
`configControl` has the value `config`. These forms are not
checked against a codec.

## Protocol Buffers

The messages use proto2, so that every field has explicit presence, as in ASN.1 OPTIONAL. The
package is "e3sm.spectrum.v1". A choice maps to a `oneof`. The member that is set is the
discriminator.

Table: Protocol Buffers messages

| Message | Fields (number: name, type) |
|---|---|
| `SpectrumRanFunctionData` | 1: name, bytes. 2: version, uint32. 3: description, bytes. |
| `SpectrumSensingIndication` | 1: timestamp, int64. 2: sfn, uint32. 3: slot, uint32. 4: beam, uint32. 5: shm_name, bytes. 6: shm_write_idx, uint32. 7: n_ranges, uint32. |
| `SpectrumPRBBlacklistControl` | 1: blacklisted_prbs, repeated uint32. 3: validity_period, uint32, in milliseconds. Number 2 is reserved. It held sampling_threshold in earlier revisions. |
| `SpectrumSensingPolicyControl` | 1: mask_per_slot, repeated uint32. 2: deactivate, bool, default false. 3: validity_period, uint32, in milliseconds. |
| `SpectrumDAppControlData` | oneof control_payload: 1: prb_blacklist_control. 2: sensing_policy_control. |
| `SpectrumPRBBlacklistReport` | 1: blacklisted_prbs, repeated uint32. |
| `SpectrumDAppReportData` | 3: timestamp, uint64. oneof report_payload: 1: prb_blacklist_report. |
| `SpectrumPRBBlockedControl` | 1: blocked_prbs, repeated uint32. |
| `SpectrumConfigControl` | 1: noise_floor_threshold, int32. 2: averaging_frames, uint32. 3: enable, bool. |
| `SpectrumXAppControlData` | 3: timestamp, uint64. oneof control_payload: 1: prb_blocked_control. 2: config_control. |

The cell configuration indication and the apply outcome have no Protocol Buffers message. A Protocol
Buffers indication is a `SpectrumSensingIndication`, and no `oneof` wraps it.

The field numbers are the contract between the sender and the receiver. A change of a number is a
change of the wire.

## Carrying in E3AP

Table: Where each message sits in an E3AP message

| Message | E3AP field | Limit |
|---|---|---|
| RAN function data | `ranFunctionData` of the RAN function definition | 1..262144 octets in the grammar, 1..32768 in the current E3AP implementation |
| Sensing indication | `protocolData` of the E3 Indication Message | 1..32768 octets |
| dApp control | `actionData` of the E3 dApp Control Action | 1..32768 octets |
| dApp report | `reportData` of the E3 dApp Report | 1..32768 octets |
| xApp control | `xAppControlData` of the E3 xApp Control Action | 1..32768 octets |

In the JSON encoding of E3AP, these fields hold a JSON value and not a string of hexadecimal digits.
See the [E3AP encoding chapter](../../e3ap/04-encoding.md).

## Size limits

Table: Size limits

| Limit | Value |
|---|---|
| octets of any message in an E3AP field | 1 to 32768 |
| PRB indices in one list | 0 to 273 |
| symbol masks in a sensing policy | 1 to 160 |
| characters of the TDD slot pattern | 1 to 160 |
| sensing ranges in one ring slot | 0 to 128 |
| ring slots | 256 |
| RAN function data, encoded | the RAN buffer holds 512 octets |
| sensing indication, encoded | the RAN buffer holds 512 octets |

## Abstract syntax with ASN.1

The ASN.1 definitions conform to ITU-T Recommendation X.680 [2] and X.681 [3]. The module name is
"SPECTRUM-SM". The module uses automatic tags.

The definitions are between the delimiters below. The tables in
[clause 6](02-ie-mapping.md) describe every type. If the ASN.1 and the tables disagree, the
ASN.1 takes precedence, except for the conditions of presence, where the tables take precedence.

```asn1
-- ASN1START
-- E3SM-SPEC, version 1

SPECTRUM-SM DEFINITIONS AUTOMATIC TAGS ::= BEGIN

-- ============================================================================
-- SM SPECTRUM RAN FUNCTION DATA STRUCTURES  
-- ============================================================================

Spectrum-RanFunctionData ::= SEQUENCE {
    name                    OCTET STRING (SIZE (1..32)),
    version                 INTEGER (0..65535),
    description             OCTET STRING (SIZE (1..128))
}

-- ============================================================================
-- INDICATION MESSAGE STRUCTURES
-- ============================================================================

Spectrum-CellConfigIndication ::= SEQUENCE {
    timestamp            INTEGER,
    ulCarrierPRBs        INTEGER (1..maxPRBs),
    ulOffsetToCarrier    INTEGER (0..maxPRBIndex),
    ulSubcarrierSpacing  INTEGER (0..3),
    ulArfcnPointA        INTEGER (0..maxARFCN),
    slotsPerFrame        INTEGER (1..maxSlotsFrame),
    tddSlotPattern       OCTET STRING (SIZE (1..maxSlotsFrame)) OPTIONAL,
    nrPci                INTEGER (0..maxPCI) OPTIONAL,
    ...
}

Spectrum-SensingIndication ::= SEQUENCE {
    timestamp    INTEGER,
    sfn          INTEGER (0..65535),
    slot         INTEGER (0..65535),
    beam         INTEGER (0..3) OPTIONAL,
    shmName      OCTET STRING,
    shmWriteIdx  INTEGER (0..65535),
    nRanges      INTEGER (0..128)
}

Spectrum-Indication ::= CHOICE {
    sensingIndication       Spectrum-SensingIndication,
    cellConfigIndication    Spectrum-CellConfigIndication
}

-- ============================================================================
-- CONTROL ACTION STRUCTURES  
-- ============================================================================

Spectrum-PRBBlacklistControl ::= SEQUENCE {
    blacklistedPRBs   SEQUENCE (SIZE (0..maxPRBs)) OF INTEGER (0..maxPRBIndex),
    validityPeriod          INTEGER (1..3600000) OPTIONAL
}

Spectrum-SensingPolicyControl ::= SEQUENCE {
    maskPerSlot             SEQUENCE (SIZE (1..maxSlotsFrame)) OF INTEGER (0..maxSymbolMask),
    deactivate              BOOLEAN DEFAULT FALSE,
    validityPeriod          INTEGER (1..3600000) OPTIONAL
}

Spectrum-DAppControlPayload ::= CHOICE {
    prbBlacklistControl     Spectrum-PRBBlacklistControl,
    ...,
    sensingPolicyControl    Spectrum-SensingPolicyControl
}

Spectrum-DAppControlData ::= SEQUENCE {
    controlPayload          Spectrum-DAppControlPayload
}

-- ============================================================================
-- dAPP REPORT STRUCTURES
-- ============================================================================

Spectrum-PRBBlacklistReport ::= SEQUENCE {
    blacklistedPRBs         SEQUENCE (SIZE (0..maxPRBs)) OF INTEGER (0..maxPRBIndex)
}

Spectrum-DAppReportPayload ::= CHOICE {
    prbBlacklistReport      Spectrum-PRBBlacklistReport,
    ...
}

Spectrum-DAppReportData ::= SEQUENCE {
    timestamp               INTEGER OPTIONAL,
    reportPayload           Spectrum-DAppReportPayload
}

-- ============================================================================
-- xAPP CONTROL ACTION STRUCTURES  
-- ============================================================================


Spectrum-PRBBlockedControl ::= SEQUENCE {
    blockedPRBs             SEQUENCE (SIZE (0..maxPRBs)) OF INTEGER (0..maxPRBIndex)
}

Spectrum-XAppControlPayload ::= CHOICE {
    prbBlockedControl       Spectrum-PRBBlockedControl,
    configControl           Spectrum-ConfigControl,
    ...
}

Spectrum-XAppControlData ::= SEQUENCE {
    timestamp               INTEGER OPTIONAL,
    controlPayload          Spectrum-XAppControlPayload
}

Spectrum-ConfigControl ::= SEQUENCE {
    noiseFloorThreshold     INTEGER (-100..100) OPTIONAL,
    averagingFrames         INTEGER (1..255) OPTIONAL,
    enable                  BOOLEAN OPTIONAL
}


-- ============================================================================
-- APPLY OUTCOME STRUCTURES
-- ============================================================================

Spectrum-PRBBlockApplyOutcome ::= SEQUENCE {
    installedTimestamp      INTEGER OPTIONAL,
    onAirTimestamp          INTEGER OPTIONAL,
    sfn                     INTEGER (0..1023) OPTIONAL,
    slot                    INTEGER (0..maxSlotIndex) OPTIONAL,
    ...
}

Spectrum-ApplyOutcomePayload ::= CHOICE {
    prbBlockApplyOutcome    Spectrum-PRBBlockApplyOutcome,
    ...
}

Spectrum-ApplyOutcomeData ::= SEQUENCE {
    timestamp               INTEGER OPTIONAL,
    applyOutcomePayload     Spectrum-ApplyOutcomePayload
}

-- ============================================================================
-- CONSTANTS AND CONSTRAINTS
-- ============================================================================
maxPRBs                     INTEGER ::= 273
maxPRBIndex                 INTEGER ::= 272
maxSlotsFrame               INTEGER ::= 160
maxSlotIndex                INTEGER ::= 159
maxSymbolMask               INTEGER ::= 16383
maxARFCN                    INTEGER ::= 3279165
maxPCI                      INTEGER ::= 1007

END
-- ASN1STOP
```
