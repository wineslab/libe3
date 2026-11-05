# E3SM-SPEC: information elements

Every information element (IE) of the Spectrum service model: its type, its range, whether it is
present, and what it means. The layout of the sensing ring is in the last section.

This is clause 6 of the specification. The encoding of the
elements is in [clause 8](04-encoding.md). Where each message travels is in
[clause 7](03-procedure-flow.md).

## Overview

Each service model message is the content of one octet string field of an E3AP message. The
RAN or the dApp encodes it. E3AP does not look inside it.

Table: Messages of the Spectrum service model

| Message | Structure | Direction | E3AP carrier |
|---|---|---|---|
| RAN function data | `Spectrum-RanFunctionData` | RAN to dApp | `ranFunctionData` of the E3 Setup Response |
| Indication | `Spectrum-Indication`: a sensing indication or a cell configuration indication | RAN to dApp | `protocolData` of the E3 Indication Message |
| dApp control | `Spectrum-DAppControlData` | dApp to RAN | `actionData` of the E3 dApp Control Action |
| dApp report | `Spectrum-DAppReportData` | dApp to RAN, then to an xApp | `reportData` of the E3 dApp Report |
| xApp control | `Spectrum-XAppControlData` | xApp to dApp, through the RAN | `xAppControlData` of the E3 xApp Control Action |
| Apply outcome | `Spectrum-ApplyOutcomeData` | RAN to xApp | opaque outcome field of the E2 control outcome |

Table notation: "M" is mandatory, "O" is optional, and "D" is optional with a default value. A
range written `0..N` includes both ends.

## RAN function data

Table: `Spectrum-RanFunctionData`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `name` | OCTET STRING | size 1..32 | M | the name of the service model, "spectrum_sm" |
| `version` | INTEGER | 0..65535 | M | the version of the service model, "1" |
| `description` | OCTET STRING | size 1..128 | M | a text for an operator |

## Indication

Table: `Spectrum-Indication` (CHOICE)

| Alternative | Type | Description |
|---|---|---|
| `sensingIndication` | `Spectrum-SensingIndication` | the reference to the sensing ranges of one slot |
| `cellConfigIndication` | `Spectrum-CellConfigIndication` | the configuration of the uplink carrier |

The choice has two alternatives and no extension marker. The alternative tells a dApp which
indication it received.

### Sensing indication

The indication does not carry the sensing ranges. It carries a reference to a slot of the sensing
ring, and a dApp reads the ranges from the ring. See the last section of this clause.

Table: `Spectrum-SensingIndication`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `timestamp` | INTEGER | not constrained | M | nanoseconds on the monotonic clock of the RAN host, taken at the sensing publish |
| `sfn` | INTEGER | 0..65535 | M | the system frame number of the slot. A RAN sends 0..1023. |
| `slot` | INTEGER | 0..65535 | M | the slot number in the frame. A RAN sends 0..159. |
| `beam` | INTEGER | 0..3 | O | the beam index. A RAN that does not support several beams sends "0". |
| `shmName` | OCTET STRING | not constrained | M | the name of the POSIX shared memory region that holds the ring. The value is "/e3_l2_sensing". |
| `shmWriteIdx` | INTEGER | 0..65535 | M | the index of the ring slot that the RAN has just written. A RAN sends 0..255. |
| `nRanges` | INTEGER | 0..128 | M | the number of valid sensing ranges in that ring slot |

The `timestamp` uses the monotonic clock. It is only valid on the host of the RAN. Do not subtract
it from a realtime timestamp. See [clause 9](05-semantics-and-constraints.md).

### Cell configuration indication

The RAN samples the configuration of the uplink carrier about once a second and sends it in a cell
configuration indication. The values come from the common serving cell configuration of the cell
[i.6].

Table: `Spectrum-CellConfigIndication`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `timestamp` | INTEGER | not constrained | M | nanoseconds on the monotonic clock of the RAN host, taken when the RAN samples the configuration |
| `ulCarrierPRBs` | INTEGER | 1..273 | M | the bandwidth of the uplink carrier in PRBs, at the subcarrier spacing of the carrier |
| `ulOffsetToCarrier` | INTEGER | 0..272 | M | the first PRB of the carrier in the common resource grid, counted in PRBs from point A |
| `ulSubcarrierSpacing` | INTEGER | 0..3 | M | the subcarrier spacing of the carrier as the numerology index: "0" is 15 kHz, "1" 30 kHz, "2" 60 kHz and "3" 120 kHz [i.1] |
| `ulArfcnPointA` | INTEGER | 0..3279165 | M | point A of the uplink carrier as an NR-ARFCN [i.7]. A cell without an uplink point A of its own sends the downlink one. |
| `slotsPerFrame` | INTEGER | 1..160 | M | the number of slots in a frame of 10 ms at the subcarrier spacing of the carrier |
| `tddSlotPattern` | OCTET STRING | size 1..160 | O | one character for each slot of the TDD period: "D" downlink, "U" uplink, "S" special. The length is the period in slots. Absent for an FDD cell. |
| `nrPci` | INTEGER | 0..1007 | O | the physical cell identity [i.1] |
| extension marker | | | | the grammar can add fields |

The PRB indices of the sensing ranges are absolute in the common resource grid. With the cell
configuration, a dApp places a range in frequency: the lowest frequency of the carrier is the
frequency of point A, plus `ulOffsetToCarrier` times 12 subcarriers at the subcarrier spacing.

A dApp that has not yet received a cell configuration indication shall not place the ranges in
frequency. It can still use the PRB indices.

## dApp control

### Envelope

Table: `Spectrum-DAppControlData`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `controlPayload` | `Spectrum-DAppControlPayload` | | M | the control. The alternative of the choice tells which control it is. |

Table: `Spectrum-DAppControlPayload` (CHOICE)

| Alternative | Type | Control identifier | Description |
|---|---|---|---|
| `prbBlacklistControl` | `Spectrum-PRBBlacklistControl` | 1 | install or clear a PRB block |
| extension marker | | | the grammar can add alternatives |
| `sensingPolicyControl` | `Spectrum-SensingPolicyControl` | 2 | install or clear a sensing policy. This is an extension alternative. |

The choice has one root alternative and one extension alternative. The PRB block control takes the
extension bit and no index. The sensing policy control takes the extension bit, an index and a
length. See [clause 8](04-encoding.md).

### PRB block control

Table: `Spectrum-PRBBlacklistControl`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `blacklistedPRBs` | SEQUENCE OF INTEGER | list size 0..273, each 0..272 | M | the complete list of PRB indices to block. The index is absolute in the carrier. An empty list clears the block. |
| `validityPeriod` | INTEGER | 1..3600000 | O | milliseconds that the block stays valid, up to one hour. The RAN decodes it and does not use it. |

The RAN blocks every listed PRB for all 14 symbols, in uplink and in downlink. The list replaces the
block that is active. The dApp sends the complete list for every change.

### Sensing policy control

Table: `Spectrum-SensingPolicyControl`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `maskPerSlot` | SEQUENCE OF INTEGER | list size 1..160, each 0..16383 | M | one 14 bit symbol mask for each slot of a frame. Bit s set means: keep symbol s of that slot free for sensing. |
| `deactivate` | BOOLEAN | | D | default "FALSE". When "TRUE", the RAN clears the policy and ignores the masks. |
| `validityPeriod` | INTEGER | 1..3600000 | O | milliseconds that the policy stays valid, up to one hour. The RAN decodes it and uses it only in a log line. |

The length of `maskPerSlot` shall equal the number of slots in a frame of the RAN. That number is 20
at 30 kHz subcarrier spacing. A list with another length gets a negative acknowledge. A message that
only clears the policy shall still carry at least one mask value.

## dApp report

Table: `Spectrum-DAppReportData`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `timestamp` | INTEGER | not constrained | O | nanoseconds on the realtime clock of the dApp host, taken when the dApp builds the report. The RAN relays it without a change. Absent means that the dApp did not stamp the report. |
| `reportPayload` | `Spectrum-DAppReportPayload` | | M | the report |

Table: `Spectrum-DAppReportPayload` (CHOICE)

| Alternative | Type | Description |
|---|---|---|
| `prbBlacklistReport` | `Spectrum-PRBBlacklistReport` | the PRBs that the dApp found occupied |
| extension marker | | the grammar can add alternatives |

Table: `Spectrum-PRBBlacklistReport`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `blacklistedPRBs` | SEQUENCE OF INTEGER | list size 0..273, each 0..272 | M | the PRB indices that the dApp holds in its current list |

The RAN does not decode the report. It relays the octets to the E2 side. A decoder at an xApp reads the
structure.

## xApp control

An xApp control reaches the dApp through the RAN. The dApp decides what to do with it. The usual action
is to send a PRB block control to the RAN.

Table: `Spectrum-XAppControlData`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `timestamp` | INTEGER | not constrained | O | nanoseconds on the realtime clock of the xApp host, taken when the xApp builds the control. The RAN relays it without a change. |
| `controlPayload` | `Spectrum-XAppControlPayload` | | M | the control |

Table: `Spectrum-XAppControlPayload` (CHOICE)

| Alternative | Type | Description |
|---|---|---|
| `prbBlockedControl` | `Spectrum-PRBBlockedControl` | the PRBs that the xApp suggests to block |
| `configControl` | `Spectrum-ConfigControl` | a change of the sensing configuration |
| extension marker | | the grammar can add alternatives |

Table: `Spectrum-PRBBlockedControl`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `blockedPRBs` | SEQUENCE OF INTEGER | list size 0..273, each 0..272 | M | the PRB indices to block |

Table: `Spectrum-ConfigControl`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `noiseFloorThreshold` | INTEGER | -100..100 | O | a new noise floor threshold |
| `averagingFrames` | INTEGER | 1..255 | O | a new length of the averaging window, in frames |
| `enable` | BOOLEAN | | O | switch spectrum monitoring on or off |

The service model does not define the unit of `noiseFloorThreshold`. The dApp and the xApp need to
agree on it.

## Apply outcome

The apply outcome tells an xApp what the RAN did with a PRB block control that the xApp started. The
E2 service model carries it as an opaque field of the control outcome. See the
[E2SM-DAPP specification](../../e2sm-dapp/README.md). The outcome is encoded with APER only.

Table: `Spectrum-ApplyOutcomeData`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `timestamp` | INTEGER | not constrained | O | when the RAN built the outcome, in nanoseconds on the realtime clock. It is not the time of the application. |
| `applyOutcomePayload` | `Spectrum-ApplyOutcomePayload` | | M | the outcome |

Table: `Spectrum-ApplyOutcomePayload` (CHOICE)

| Alternative | Type | Description |
|---|---|---|
| `prbBlockApplyOutcome` | `Spectrum-PRBBlockApplyOutcome` | the outcome of a PRB block control |
| extension marker | | the grammar can add alternatives |

Table: `Spectrum-PRBBlockApplyOutcome`

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `installedTimestamp` | INTEGER | not constrained | O | when the RAN wrote the block into the scheduler, in nanoseconds on the realtime clock. Absent means that the control did not install. |
| `onAirTimestamp` | INTEGER | not constrained | O | when the first scheduler tick used the block, in nanoseconds on the realtime clock. Absent means that no tick used it: a later block replaced it, or the RAN timed out. |
| `sfn` | INTEGER | 0..1023 | O | the frame of that tick. Present together with `onAirTimestamp`. |
| `slot` | INTEGER | 0..159 | O | the slot of that tick. Present together with `onAirTimestamp`. |

The structure has an extension marker, so a later version can add fields.

## Constants

Table: Constants of the grammar

| Name | Value | Meaning |
|---|---|---|
| `maxPRBs` | 273 | the largest number of PRBs in a 5G NR carrier [i.2] |
| `maxPRBIndex` | 272 | `maxPRBs` minus 1 |
| `maxSlotsFrame` | 160 | the slots in a frame at the largest numerology; 20 at 30 kHz |
| `maxSlotIndex` | 159 | `maxSlotsFrame` minus 1 |
| `maxSymbolMask` | 16383 | the largest 14 bit symbol bitmap, 2 to the power 14, minus 1 |
| `maxARFCN` | 3279165 | the largest NR-ARFCN [i.7] |
| `maxPCI` | 1007 | the largest physical cell identity [i.1] |

## The sensing ring

The sensing ring is a region of POSIX shared memory. Its name is "/e3_l2_sensing". The RAN is the only
writer. A dApp on the same host maps it read only. A dApp on another host cannot use the sensing
indication, because the indication holds only a reference.

The region holds one header and 256 slots. All integers are in the byte order of the host. The RAN
creates the region with the permission "0644".

Table: Sensing ring header (64 octets at offset 0)

| Field | Type | Value | Description |
|---|---|---|---|
| `version` | 32 bit unsigned | 1 | the layout version |
| `slot_count` | 32 bit unsigned | 256 | the number of ring slots |
| `slot_stride` | 32 bit unsigned | 2072 | the size of one ring slot in octets |
| `max_ranges` | 32 bit unsigned | 128 | the capacity of one ring slot |
| `range_size` | 32 bit unsigned | 16 | the size of one sensing range record in octets |
| `reserved` | 11 times 32 bit unsigned | 0 | not used |

A dApp shall compare `slot_stride`, `max_ranges` and `range_size` with its own layout. If they differ,
the dApp shall not read the ring.

Table: Ring slot (2072 octets, the first one at offset 64)

| Field | Type | Description |
|---|---|---|
| `sfn` | 16 bit unsigned | the frame that the slot belongs to. The RAN writes it last. |
| `slot` | 16 bit unsigned | the slot in the frame. The RAN writes it last. |
| `beam` | 16 bit unsigned | the beam index |
| `n_ranges` | 16 bit unsigned | the number of valid records in `ranges`, 0..128 |
| `timestamp_ns` | 64 bit unsigned | nanoseconds on the monotonic clock when the slot was written |
| `seq` | 32 bit unsigned | a counter that increases with each write |
| `_pad` | 32 bit unsigned | 0 |
| `ranges` | 128 records of 16 octets | the sensing ranges. Only the first `n_ranges` are valid. |

Table: Sensing range record (16 octets)

| Field | Type | Description |
|---|---|---|
| `start_symbol` | 32 bit signed | the first symbol of the range in the slot |
| `num_symbols` | 32 bit signed | the number of symbols |
| `rb_start` | 32 bit signed | the first PRB. The index is absolute in the carrier. It is not relative to a bandwidth part. |
| `rb_size` | 32 bit signed | the number of PRBs |

The ring slot with index `shmWriteIdx` holds the data of the indication. Before a dApp uses the
ranges, it shall compare the `sfn` and the `slot` of the ring slot with the same fields of the
indication. If they differ, the RAN has overwritten the ring slot, and the dApp shall drop the data.
The ring holds 256 slots. At one write for each uplink slot, that is about 128 ms of history.
