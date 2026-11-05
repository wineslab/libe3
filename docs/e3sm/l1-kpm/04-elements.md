# Elements for E3SM service model

## General

The tables of this clause use the conventions of clause 6 of [1]. If a table and the
grammar of clause 8.4 disagree, the grammar takes precedence, except for the conditions
of presence, where the table takes precedence.

Most fields of the indication are optional in the grammar. Their presence follows the
subscription. The following rules apply to the INDICATION DATA IE, the CELL DATA IE and
the UE DATA IE.

1. A field that has a telemetry identifier is present only when that identifier is granted to the subscription, when the identifier belongs to the path of the indication or is shared, and when the RAN node has a value for the slot. Otherwise it is absent.
2. The `cells` list is present when at least one granted identifier of the path has cell scope or UE scope. It holds one entry for each cell that the subscription data selects and that has data on the path in the slot.
3. The `ues` list of a cell is present when at least one granted identifier of the path has UE scope. It holds one entry for each UE that the subscription data selects and that the path has in that cell in that slot.
4. A list is never empty. When a list would have no entry, it is absent.
5. `nCells` counts the entries of `cells`. `nUe` and `nSrsUe` count the entries of `ues` of their cell. They count what the indication carries, after the subscription data is applied.
6. A float field is absent when its value is not finite (not a number, or infinite).

An absent field means that the value is not reported. It does not mean zero.

In the semantics description, "ID" followed by a number gives the telemetry identifier
of the field, followed by its JSON key. The meaning of the value is in clause 7.3.

## Message functional definition and content

### Messages for E3 functional procedures

#### INDICATION DATA IE

The INDICATION DATA IE (L1KPM-Indication) is the content of `protocolData` of an
INDICATION MESSAGE message for RAN function `2`. It carries the slot scope.

Table: INDICATION DATA IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `timestamp` | O | | Timestamp, 8.3.9 | ID 4, `timestamp`. Monotonic clock of the E3 Agent host. |
| `timestampTai` | O | | Timestamp, 8.3.9 | ID 78, `timestamp_tai`. TAI. |
| `sfn` | O | | INTEGER (0..1023) | ID 5, `sfn`. |
| `slot` | O | | INTEGER (0..159) | ID 6, `slot`. |
| `nCells` | O | | INTEGER (0..16) | ID 10, `n_cells`. Number of entries of `cells`. |
| `cells` | C | 1 to 16 | SEQUENCE OF CELL DATA IE, 8.2.1.2 | Present when a granted identifier has cell scope or UE scope. |

#### CELL DATA IE

The CELL DATA IE (L1KPM-CellData) carries the cell scope of one cell.

Table: CELL DATA IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `cellId` | O | | Physical Cell ID, 8.3.6 | ID 7, `cell_id`. Shared. |
| `nRxAntSrs` | O | | INTEGER (1..64) | ID 9, `n_rx_ant_srs`. Shared. |
| `iqSamples` | O | | Shared Memory Reference, 8.3.10 | ID 1, `iq_samples`. PUSCH path. |
| `pduData` | O | | Shared Memory Reference, 8.3.10 | ID 2, `pdu_data`. PUSCH path. |
| `hEstimates` | O | | Shared Memory Reference, 8.3.10 | ID 3, `h_estimates`. PUSCH path. `rowByteOffset` is present. |
| `nRxAnt` | O | | INTEGER (1..64) | ID 8, `n_rx_ant`. PUSCH path. |
| `nBsAnts` | O | | INTEGER (1..64) | ID 11, `n_bs_ants`. PUSCH path. |
| `nUe` | O | | INTEGER (0..64) | ID 34, `n_ue`. PUSCH path. Number of entries of `ues`. |
| `srsIqSamples` | O | | Shared Memory Reference, 8.3.10 | ID 44, `srs_iq_samples`. SRS path. `rowByteOffset` is present. |
| `srsHest` | O | | Shared Memory Reference, 8.3.10 | ID 45, `srs_hest`. SRS path. |
| `srsRbSnr` | O | | Shared Memory Reference, 8.3.10 | ID 49, `srs_rb_snr`. SRS path. |
| `srsCellStartSym` | O | | INTEGER (0..13) | ID 52, `srs_cell_start_sym`. SRS path. |
| `srsCellNSrsSym` | O | | INTEGER (0..6) | ID 53, `srs_cell_n_srs_sym`. SRS path. |
| `nSrsUe` | O | | INTEGER (0..64) | ID 54, `n_srs_ue`. SRS path. Number of entries of `ues`. |
| `ues` | C | 1 to 64 | SEQUENCE OF UE DATA IE, 8.2.1.3 | Present when a granted identifier has UE scope. |
| `validSymbolMask` | O | | Symbol Bitmap, 8.3.7 | ID 1000, `valid_symbol_mask`. Shared. |

#### UE DATA IE

The UE DATA IE (L1KPM-UeData) carries the UE scope of one UE in one cell. On the PUSCH
path it holds the fields of Table 8-3. On the SRS path it holds the fields of Table 8-4.
`rnti` is shared and appears in both tables. Both tables describe one ASN.1 type.

Table: UE DATA IE, shared and PUSCH path fields

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `rnti` | O | | RNTI, 8.3.5 | ID 33, `rnti`. Shared. |
| `rbStart` | O | | INTEGER (0..274) | ID 25, `rb_start`. |
| `rbSize` | O | | INTEGER (1..275) | ID 26, `rb_size`. |
| `startSymbolIndex` | O | | INTEGER (0..13) | ID 27, `start_symbol_index`. |
| `nrOfSymbols` | O | | INTEGER (1..14) | ID 28, `nr_of_symbols`. |
| `nLayers` | O | | INTEGER (1..8) | ID 12, `n_layers`. |
| `nSubcarriers` | O | | INTEGER (0..3300) | ID 13, `n_subcarriers`. |
| `nDmrsEstimates` | O | | INTEGER (0..14) | ID 14, `n_dmrs_estimates`. |
| `dmrsSymbPos` | O | | Symbol Bitmap, 8.3.7 | ID 15, `dmrs_symb_pos`. |
| `qamModOrder` | O | | INTEGER (1..10) | ID 22, `qam_mod_order`. |
| `mcsIndex` | O | | INTEGER (0..31) | ID 23, `mcs_index`. |
| `mcsTableIndex` | O | | INTEGER (0..4) | ID 24, `mcs_table_index`. |
| `layerOffset` | O | | INTEGER (0..255) | ID 35, `layer_offset`. |
| `ueGrpIdx` | O | | INTEGER (0..255) | ID 36, `ue_grp_idx`. |
| `hOffset` | O | | Uint32, 8.3.4 | ID 37, `h_offset`. complex64 elements. |
| `hSize` | O | | Uint32, 8.3.4 | ID 38, `h_size`. complex64 elements. |
| `tbCrcFail` | O | | INTEGER (0..1) | ID 16, `tb_crc_fail`. |
| `cbErrors` | O | | INTEGER (0..255) | ID 17, `cb_errors`. |
| `cbCount` | O | | INTEGER (0..255) | ID 20, `cb_count`. |
| `tbSize` | O | | Uint32, 8.3.4 | ID 29, `tb_size`. Bytes. |
| `pduLen` | O | | Uint32, 8.3.4 | ID 30, `pdu_len`. Bytes. |
| `pduOffset` | O | | Uint32, 8.3.4 | ID 79, `pdu_offset`. Bytes. |
| `targetCodeRate` | O | | INTEGER (0..65535) | ID 31, `target_code_rate`. |
| `newDataIndicator` | O | | INTEGER (0..1) | ID 32, `new_data_indicator`. |
| `harqProcessId` | O | | INTEGER (0..31) | ID 41, `harq_process_id`. |
| `rvIndex` | O | | INTEGER (0..3) | ID 42, `rv_index`. |
| `rsrp` | O | | Float32, 8.3.2 | ID 18, `rsrp`. dB. |
| `rssi` | O | | Float32, 8.3.2 | ID 21, `rssi`. dB. |
| `noiseVar` | O | | Float32, 8.3.2 | ID 19, `noise_var`. dB. |
| `sinr` | O | | Float32, 8.3.2 | ID 39, `sinr`. dB. |
| `timingAdvance` | O | | Float32, 8.3.2 | ID 40, `timing_advance`. Microseconds. |
| `cfoHz` | O | | Float32, 8.3.2 | ID 43, `cfo_hz`. Hz. |

Table: UE DATA IE, SRS path fields

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `srsWidebandSnr` | O | | Float32, 8.3.2 | ID 55, `srs_wideband_snr`. dB. |
| `srsSignalEnergy` | O | | Float32, 8.3.2 | ID 56, `srs_signal_energy`. |
| `srsNoiseEnergy` | O | | Float32, 8.3.2 | ID 57, `srs_noise_energy`. |
| `srsToa` | O | | Float32, 8.3.2 | ID 58, `srs_toa`. Microseconds. |
| `srsHdAntFlag` | O | | INTEGER (0..1) | ID 59, `srs_hd_ant_flag`. |
| `srsScCorr` | O | | Complex32, 8.3.3 | ID 60, `srs_sc_corr`. |
| `srsCsCorrRatioDb` | O | | Float32, 8.3.2 | ID 61, `srs_cs_corr_ratio_db`. dB. |
| `srsHestNPrbGrps` | O | | INTEGER (0..275) | ID 46, `srs_hest_n_prb_grps`. |
| `srsHestOffset` | O | | Uint32, 8.3.4 | ID 47, `srs_hest_offset`. Bytes. |
| `srsHestSize` | O | | Uint32, 8.3.4 | ID 48, `srs_hest_size`. Bytes. |
| `srsRbSnrOffset` | O | | Uint32, 8.3.4 | ID 50, `srs_rb_snr_offset`. Bytes. |
| `srsRbSnrSize` | O | | Uint32, 8.3.4 | ID 51, `srs_rb_snr_size`. Bytes. |
| `srsNValidPrg` | O | | INTEGER (0..275) | ID 76, `srs_n_valid_prg`. |
| `srsPrgSize` | O | | INTEGER (1..275) | ID 77, `srs_prg_size`. PRBs. |
| `srsAntPorts` | O | | INTEGER (1..8) | ID 62, `srs_ant_ports`. |
| `srsNSyms` | O | | INTEGER (1..14) | ID 63, `srs_n_syms`. |
| `srsNRepetitions` | O | | INTEGER (1..14) | ID 64, `srs_n_repetitions`. |
| `srsCombSize` | O | | INTEGER (2..8) | ID 65, `srs_comb_size`. |
| `srsCombOffset` | O | | INTEGER (0..7) | ID 66, `srs_comb_offset`. |
| `srsStartSym` | O | | INTEGER (0..13) | ID 67, `srs_start_sym`. |
| `srsCyclicShift` | O | | INTEGER (0..11) | ID 68, `srs_cyclic_shift`. |
| `srsFreqPosition` | O | | INTEGER (0..67) | ID 69, `srs_freq_position`. |
| `srsFreqShift` | O | | INTEGER (0..268) | ID 70, `srs_freq_shift`. |
| `srsFreqHopping` | O | | INTEGER (0..3) | ID 71, `srs_freq_hopping`. |
| `srsResourceType` | O | | INTEGER (0..2) | ID 72, `srs_resource_type`. |
| `srsTSrs` | O | | INTEGER (1..2560) | ID 73, `srs_t_srs`. Slots. |
| `srsTOffset` | O | | INTEGER (0..2559) | ID 74, `srs_t_offset`. Slots. |
| `srsUsage` | O | | INTEGER (0..15) | ID 75, `srs_usage`. |

#### CONTROL DATA IE

The CONTROL DATA IE (L1KPM-ControlData) is the content of `actionData` of a dApp
CONTROL ACTION message for RAN function `2`. The alternative matches the control
identifier of the message.

Table: CONTROL DATA IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| CHOICE | M | | | |
| > `setSubscriptionData` | | | SUBSCRIPTION DATA IE, 8.2.1.5 | Control identifier `1`. Clause 6.6.4. |

A CONTROL DATA IE whose alternative does not match the `controlIdentifier` of the
message is an error (clause 9).

#### SUBSCRIPTION DATA IE

The SUBSCRIPTION DATA IE (L1KPM-SubscriptionData) selects the cells and the UEs of a
subscription. Telemetry identifiers select fields. The subscription data selects rows.

Table: SUBSCRIPTION DATA IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `cellFilter` | O | 1 to 16 | SEQUENCE OF Physical Cell ID, 8.3.6 | The cells to report. Absent: every cell. |
| `ueFilter` | O | 1 to 64 | SEQUENCE OF UE Reference, 8.3.11 | The UEs to report. Absent: every UE. |

The E3 Agent applies the subscription data as follows.

- With `cellFilter`, `cells` holds only the listed cells.
- With `ueFilter`, the `ues` list of a cell holds only the listed UEs of that cell. A UE reference without `cellId` matches its RNTI in every cell.
- With both, a UE is reported when its cell is in `cellFilter` and the UE is in `ueFilter`.
- A listed UE that has no data on the path in the slot is absent from that indication.
- When every granted identifier of the path has UE scope and no listed UE has data on the path in the slot, the E3 Agent sends no indication of that path for the slot.
- The subscription data does not change the shared memory region. A blob holds the data of all the UEs of its cell. A dApp uses the UE scope locators to find the part of a UE.

> NOTE: An RNTI identifies a UE only in its cell and only while the RNTI is assigned. It changes, for example, at handover and at RRC re-establishment. A UE filter does not follow the UE when its RNTI changes.

### Messages for E3 global procedures

#### RAN FUNCTION DATA IE

The RAN FUNCTION DATA IE (L1KPM-RanFunctionData) is the content of `ranFunctionData` of
the entry for RAN function `2` in the SETUP RESPONSE message.

Table: RAN FUNCTION DATA IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `name` | M | 1 to 32 octets | OCTET STRING | Short name. The value is `L1-KPM`. |
| `version` | M | | INTEGER (0..65535) | Version of the service model. The value is `1`. |
| `description` | M | 1 to 128 octets | OCTET STRING | Free text. |
| `streams` | M | 1 to 128 | SEQUENCE OF STREAM DESCRIPTOR IE, 8.2.2.2 | One entry for each telemetry identifier of clause 7.3, in any order. |

The text fields hold ASCII characters.

#### STREAM DESCRIPTOR IE

The STREAM DESCRIPTOR IE (L1KPM-StreamDescriptor) describes one stream.

Table: STREAM DESCRIPTOR IE

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `telemetryId` | M | | Telemetry Identifier, 8.3.8 | The identifier of the stream. |
| `streamId` | M | 1 to 64 octets | OCTET STRING | The JSON key of the field of the stream, as in clause 7.3. |
| `dataType` | M | 1 to 32 octets | OCTET STRING | The data type of clause 7.3, for example `uint16`, `float32`, `array(int16)`. |
| `description` | O | 1 to 256 octets | OCTET STRING | Free text. |
| `status` | M | | Stream Status, 8.3.12 | Whether the RAN node can produce the stream. |
| `delivery` | C | | Delivery, 8.3.13 | How the stream is delivered. Present when `status` is "available". |

## Information element definitions

### General

This clause defines the information elements that the IEs of clause 8.2 use.

### Float32

A real number as an IEEE 754 binary32 value [5], in 4 octets, least significant octet
first. In APER it is an OCTET STRING of size 4 (L1KPM-Float32). In JSON it is a number.
A value that is not finite shall not be sent: the field is left out.

### Complex32

A complex number with two Float32 parts.

Table: Complex32

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `re` | M | | Float32, 8.3.2 | Real part. |
| `im` | M | | Float32, 8.3.2 | Imaginary part. |

In JSON a Complex32 is an array of two numbers, the real part first.

### Uint32

An unsigned integer, INTEGER (0..4294967295) (L1KPM-Uint32).

### RNTI

An RNTI [10], INTEGER (0..65535) (L1KPM-Rnti).

### Physical Cell ID

A physical cell identity [7], INTEGER (0..1007) (L1KPM-PhysCellId).

### Symbol Bitmap

A bit map of the OFDM symbols of a slot, INTEGER (0..16383) (L1KPM-SymbolBitmap). Bit
`s`, of value 2^s, stands for OFDM symbol `s` of the slot, `s` from `0` to `13`.

### Telemetry Identifier

A telemetry identifier of clause 7.3, INTEGER (1..65535) (L1KPM-TelemetryId). The range
is the range of the telemetry identifiers of [1].

### Timestamp

A time in nanoseconds since an epoch, as an unconstrained INTEGER (L1KPM-TimestampNs).
The epoch and the clock are those of the field that uses it (clause 7.3). The value
shall be in the range 0 to 2^63 - 1.

> NOTE: The type has no constraint so that a codec can hold it as a signed 64 bit integer. A semi-constrained form (0..MAX) is avoided on purpose. E3AP uses the same form for its timestamp [1].

The `timestamp` (ID 4) counts nanoseconds on the monotonic clock of the E3 Agent host,
`CLOCK_MONOTONIC` of [6]. Its origin is not related to the time of day. A dApp on the
same host may compare it with its own reading of that clock, for example to compute the
age of the data. A receiver shall not subtract it from a realtime timestamp, such as the
timestamp of an E3AP message [1], or from a timestamp of another host. ID 78 gives the
time of the slot on a time scale that all hosts share.

> NOTE: On Linux, a process in another time namespace than the E3 Agent reads the monotonic clock with an offset. It cannot compare its readings with ID 4.

### Shared Memory Reference

The Shared Memory Reference (L1KPM-ShmRef) locates the data of one cell in a buffer of
the region. The field that carries it names the stream.

Table: Shared Memory Reference

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `shmName` | M | 1 to 63 octets | OCTET STRING | Name of the POSIX shared memory object [6], for example `/e3_ran_buffers`. Equal to `memoryKey` of the stream descriptor. |
| `bufferIndex` | M | | INTEGER (0..255) | The buffer of the pair, `0` or `1`. |
| `writeIndex` | M | | Uint32, 8.3.4 | The row in that buffer. |
| `rowByteOffset` | C | | Uint32, 8.3.4 | Offset of the data of the cell from the start of the buffer, in bytes. Present for ID 3 and ID 44. Absent for the other streams. |

Annex A gives the address of the data from these values.

### UE Reference

The UE Reference (L1KPM-UeRef) names a UE in a UE filter.

Table: UE Reference

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `cellId` | O | | Physical Cell ID, 8.3.6 | The cell of the UE. Absent: any cell. |
| `rnti` | M | | RNTI, 8.3.5 | The RNTI of the UE. |

### Stream Status

ENUMERATED (available, unavailable) (L1KPM-StreamStatus).

- "available": the RAN node produces the stream. The E3 Agent lists the identifier in `telemetryIdentifierList` and may grant it.
- "unavailable": the RAN node cannot produce the stream. The E3 Agent does not list the identifier and shall not grant it.

### Delivery

The Delivery (L1KPM-Delivery) says how a stream is delivered.

Table: Delivery

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| CHOICE | M | | | |
| > `sharedMemory` | | | Shared Memory Information, 8.3.14 | The stream stays in the region. |
| > `protocolData` | | | Inline Information, 8.3.15 | The stream is a field of the indication. |

### Shared Memory Information

Table: Shared Memory Information

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `memoryKey` | M | 1 to 63 octets | OCTET STRING | Name of the POSIX shared memory object of the region. |
| `memorySizeBytes` | M | | Uint32, 8.3.4 | Size of one buffer of the stream, in bytes. |
| `numRows` | M | | INTEGER (1..65535) | Number of rows of one buffer of the stream. |

### Inline Information

Table: Inline Information

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `keyword` | M | 1 to 64 octets | OCTET STRING | The JSON key of the field in the indication. Equal to `streamId`. |
