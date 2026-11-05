# RAN function description

## Description

The E3 Agent describes the RAN function of the present document in one entry of the RAN
function list of the SETUP RESPONSE message (E3-RanFunctionDefinition, clause 6.6 of [1]).
Table 7-1 gives the content of the entry.

Table: Content of the RAN function definition

| E3AP field | Content |
|---|---|
| `ranFunctionIdentifier` | `2` |
| `telemetryIdentifierList` | The identifiers of clause 7.3 that the RAN node can produce. |
| `controlIdentifierList` | `1` when the E3 Agent supports Set Subscription Data, otherwise empty. |
| `ranFunctionData` | The RAN FUNCTION DATA IE of clause 8.2.2.1, encoded as clause 8.5 defines. |

## RAN function identity

Table: Identity of E3SM-L1-KPM

| Item | Value | Where it is carried |
|---|---|---|
| RAN function identifier | `2` | `ranFunctionIdentifier` of every E3AP message of the service model |
| Short name | `L1-KPM` | `name` of the RAN FUNCTION DATA IE |
| Version | `1` | `version` of the RAN FUNCTION DATA IE |
| Description | Free text, for example `L1-KPM: post-FFT IQ, PUSCH and SRS telemetry` | `description` of the RAN FUNCTION DATA IE |

## Telemetry identifiers

### General

A telemetry identifier names one stream of the service model. The dApp lists the
identifiers in the SUBSCRIPTION REQUEST message. The E3 Agent grants them as clause
6.6.2 defines. Each identifier maps to one field of the INDICATION DATA IE, of the CELL
DATA IE or of the UE DATA IE.

The identifiers are allocated as follows.

- `1` to `79`: the identifiers of clauses 7.3.2 to 7.3.4.
- `80` to `999`: reserved. An E3 Agent shall not list them. A dApp shall not request them.
- `1000` to `65535`: additional identifiers. Clause 7.3.5 allocates `1000`. The other values are reserved.

The tables of clauses 7.3.2 to 7.3.5 give, for each identifier, the name of the stream,
the scope, the data type in the stream descriptor, and the meaning. The name of the
stream is the JSON key of its field and the `streamId` of its stream descriptor. The
ASN.1 field of each identifier and its range are given in clause 8.2.

### Shared identifiers

The identifiers of Table 7-3 are present on the PUSCH path and on the SRS path.

Table: Shared telemetry identifiers

| ID | Stream | Scope | Data type | Description |
|---|---|---|---|---|
| 4 | `timestamp` | Slot | uint64 | Time at which the data of the slot became available to the E3 Agent. Nanoseconds on the monotonic clock of the E3 Agent host (`CLOCK_MONOTONIC` [6]), see clause 8.3.9. |
| 5 | `sfn` | Slot | uint16 | System frame number of the slot [7]. |
| 6 | `slot` | Slot | uint16 | Number of the slot in the frame [7]. |
| 7 | `cell_id` | Cell | uint16 | Physical cell identity of the cell [7]. |
| 9 | `n_rx_ant_srs` | Cell | uint16 | Number of receive antennas of the SRS data of the cell. |
| 10 | `n_cells` | Slot | uint16 | Number of entries of `cells` in the indication. |
| 33 | `rnti` | UE | uint16 | C-RNTI of the UE in the cell [10]. |
| 78 | `timestamp_tai` | Slot | uint64 | Start of the slot on the air interface, derived from `sfn` and `slot` and the time synchronization of the RAN node. Nanoseconds since 1970-01-01T00:00:00 TAI. |

### PUSCH path identifiers

The identifiers of Table 7-4 are present only on the PUSCH path. A value "of the UE
group" is the same for every UE of the group (clause 3.1).

Table: PUSCH path telemetry identifiers

| ID | Stream | Scope | Data type | Description |
|---|---|---|---|---|
| 1 | `iq_samples` | Cell | array(int16) | Shared memory reference to the post-FFT frequency domain IQ samples of the slot, before equalization. Annex A.6.1. |
| 2 | `pdu_data` | Cell | array(uint8) | Shared memory reference to the PUSCH PDUs of the cell. Annex A.6.2. |
| 3 | `h_estimates` | Cell | array(complex64) | Shared memory reference to the PUSCH DMRS channel estimates of the UE groups of the cell, before equalization. Annex A.6.3. |
| 8 | `n_rx_ant` | Cell | uint16 | Number of receive antennas in the IQ samples of the cell. |
| 11 | `n_bs_ants` | Cell | uint8 | Number of base station antennas in the channel estimates of the cell. |
| 12 | `n_layers` | UE | uint8 | Number of PUSCH layers of the UE. |
| 13 | `n_subcarriers` | UE | uint16 | Number of subcarriers of the allocation of the UE group, 12 for each PRB. |
| 14 | `n_dmrs_estimates` | UE | uint8 | Number of DMRS symbols with a channel estimate, in the UE group. |
| 15 | `dmrs_symb_pos` | UE | uint16 | Bit map of the DMRS symbols of the PUSCH. Bit `s` (value 2^s) is set when OFDM symbol `s` of the slot carries DMRS [7]. |
| 16 | `tb_crc_fail` | UE | uint8 | Result of the CRC of the transport block: `0` pass, `1` fail [8]. |
| 17 | `cb_errors` | UE | uint32 | Number of code blocks of the transport block whose CRC failed [8]. |
| 18 | `rsrp` | UE | float32 | RSRP of the PUSCH of the UE, in dB. |
| 19 | `noise_var` | UE | float32 | Variance of noise plus interference of the UE, in dB. See the note below. |
| 20 | `cb_count` | UE | uint16 | Number of code blocks of the transport block [8]. |
| 21 | `rssi` | UE | float32 | RSSI of the UE group, in dB. |
| 22 | `qam_mod_order` | UE | uint8 | Modulation order Qm of the PUSCH [9]. |
| 23 | `mcs_index` | UE | uint8 | MCS index of the PUSCH [9]. |
| 24 | `mcs_table_index` | UE | uint8 | MCS table of the PUSCH [9]: `0` 64QAM, `1` 256QAM, `2` 64QAM low spectral efficiency, `3` 64QAM with transform precoding, `4` 64QAM low spectral efficiency with transform precoding. |
| 25 | `rb_start` | UE | uint16 | First PRB of the PUSCH allocation of the UE group, in the bandwidth part [9]. |
| 26 | `rb_size` | UE | uint16 | Number of PRBs of the PUSCH allocation of the UE group. |
| 27 | `start_symbol_index` | UE | uint8 | First OFDM symbol of the PUSCH of the UE group in the slot. |
| 28 | `nr_of_symbols` | UE | uint8 | Number of OFDM symbols of the PUSCH of the UE group. |
| 29 | `tb_size` | UE | uint32 | Size of the transport block, in bytes. |
| 30 | `pdu_len` | UE | uint32 | Number of bytes of the PDU of the UE in the PDU data: `tbSize` when the CRC passes, `0` when it fails. |
| 31 | `target_code_rate` | UE | uint16 | Target code rate R of the MCS, as R x 1024 x 10 [9]. Example: `3080` for R x 1024 = 308. |
| 32 | `new_data_indicator` | UE | uint8 | `1` for a new transmission, `0` for a retransmission. |
| 34 | `n_ue` | Cell | uint16 | Number of entries of `ues` of the cell in the indication. |
| 35 | `layer_offset` | UE | uint16 | Index of the first layer of the UE in the layer axis of the channel estimates of its UE group. `0` for FDM. |
| 36 | `ue_grp_idx` | UE | uint16 | Index of the UE group of the UE in the slot. |
| 37 | `h_offset` | UE | uint32 | Offset of the channel estimate blob of the UE group in the row of the cell, in complex64 elements. |
| 38 | `h_size` | UE | uint32 | Size of the channel estimate blob of the UE group, in complex64 elements. |
| 39 | `sinr` | UE | float32 | SINR of the UE, in dB. See the note below. |
| 40 | `timing_advance` | UE | float32 | Timing advance estimate of the UE, in microseconds. |
| 41 | `harq_process_id` | UE | uint8 | HARQ process number of the transport block. |
| 42 | `rv_index` | UE | uint8 | Redundancy version of the transmission [8]. |
| 43 | `cfo_hz` | UE | float32 | Carrier frequency offset estimate of the UE, in Hz. |
| 79 | `pdu_offset` | UE | uint32 | Offset of the PDU of the UE in the PDU row of its cell, in bytes. |

> NOTE: A RAN node measures `noiseVar` and `sinr` either before or after equalization. The `description` of the stream descriptor of ID 19 and ID 39 should say which.

### SRS path identifiers

The identifiers of Table 7-5 are present only on the SRS path. The SRS parameters are
those of the SRS resource of the UE [7], [10].

Table: SRS path telemetry identifiers

| ID | Stream | Scope | Data type | Description |
|---|---|---|---|---|
| 44 | `srs_iq_samples` | Cell | array(int16) | Shared memory reference to the post-FFT IQ samples of the SRS symbols of the cell. Annex A.6.4. |
| 45 | `srs_hest` | Cell | array(int16) | Shared memory reference to the buffer of the SRS channel estimates of the UEs of the cell. Annex A.6.5. |
| 46 | `srs_hest_n_prb_grps` | UE | uint16 | Number of PRGs of the SRS channel estimate of the UE. |
| 47 | `srs_hest_offset` | UE | uint32 | Offset of the SRS channel estimate blob of the UE in the buffer, in bytes. |
| 48 | `srs_hest_size` | UE | uint32 | Size of the SRS channel estimate blob of the UE, in bytes. |
| 49 | `srs_rb_snr` | Cell | array(float32) | Shared memory reference to the buffer of the SNR of each PRG of the UEs of the cell. Annex A.6.6. |
| 50 | `srs_rb_snr_offset` | UE | uint32 | Offset of the SNR blob of the UE in the buffer, in bytes. |
| 51 | `srs_rb_snr_size` | UE | uint32 | Size of the SNR blob of the UE, in bytes. |
| 52 | `srs_cell_start_sym` | Cell | uint8 | First OFDM symbol with SRS in the slot, for the cell. |
| 53 | `srs_cell_n_srs_sym` | Cell | uint8 | Number of OFDM symbols with SRS in the slot, for the cell. |
| 54 | `n_srs_ue` | Cell | uint8 | Number of entries of `ues` of the cell in the indication. |
| 55 | `srs_wideband_snr` | UE | float32 | Wideband SNR of the SRS of the UE, in dB. |
| 56 | `srs_signal_energy` | UE | float32 | Wideband signal energy of the SRS of the UE, linear, in the scale of the receiver. |
| 57 | `srs_noise_energy` | UE | float32 | Wideband noise energy of the SRS of the UE, linear, in the scale of the receiver. |
| 58 | `srs_toa` | UE | float32 | Time of arrival estimate of the SRS of the UE, in microseconds. |
| 59 | `srs_hd_ant_flag` | UE | uint8 | `1` when the UE uses a high density of antenna ports and the SNR is not reliable, otherwise `0`. |
| 60 | `srs_sc_corr` | UE | array(float32) | Wideband correlation of adjacent subcarriers of the SRS of the UE, as a complex value. |
| 61 | `srs_cs_corr_ratio_db` | UE | float32 | Ratio of the correlation energy on the used cyclic shifts to the energy on the unused cyclic shifts, in dB. |
| 62 | `srs_ant_ports` | UE | uint8 | Number of SRS antenna ports (nrofSRS-Ports). |
| 63 | `srs_n_syms` | UE | uint8 | Number of SRS symbols (nrofSymbols). |
| 64 | `srs_n_repetitions` | UE | uint8 | Repetition factor (repetitionFactor). |
| 65 | `srs_comb_size` | UE | uint8 | Transmission comb K_TC. |
| 66 | `srs_comb_offset` | UE | uint8 | Comb offset of the transmission comb. |
| 67 | `srs_start_sym` | UE | uint8 | First OFDM symbol of the SRS of the UE in the slot. |
| 68 | `srs_cyclic_shift` | UE | uint8 | Cyclic shift of the SRS sequence. |
| 69 | `srs_freq_position` | UE | uint8 | Frequency domain position (freqDomainPosition). |
| 70 | `srs_freq_shift` | UE | uint16 | Frequency domain shift (freqDomainShift). |
| 71 | `srs_freq_hopping` | UE | uint8 | Frequency hopping parameter b_hop. |
| 72 | `srs_resource_type` | UE | uint8 | Time domain behavior of the resource: `0` aperiodic, `1` semi-persistent, `2` periodic. |
| 73 | `srs_t_srs` | UE | uint16 | Periodicity of the resource, in slots. |
| 74 | `srs_t_offset` | UE | uint16 | Slot offset of the resource. |
| 75 | `srs_usage` | UE | uint32 | Bit map of the usage of the SRS resource set: `0x1` beam management, `0x2` codebook, `0x4` non-codebook, `0x8` antenna switching. |
| 76 | `srs_n_valid_prg` | UE | uint16 | Number of valid PRGs in the SNR blob of the UE. |
| 77 | `srs_prg_size` | UE | uint16 | Number of PRBs in one PRG. |

### Additional identifiers

Table: Additional telemetry identifiers

| ID | Stream | Scope | Data type | Path | Description |
|---|---|---|---|---|---|
| 1000 | `valid_symbol_mask` | Cell | uint16 | shared | Bit map of the uplink symbols of the slot in the cell. Bit `s` (value 2^s) is set when OFDM symbol `s` is an uplink symbol. |

The `validSymbolMask` follows the TDD configuration of the cell. All bits are set for
FDD and for a slot that is uplink only. In a slot that mixes downlink and uplink, the
downlink and guard symbols have their bit clear. The shared memory streams of
identifiers 1 and 44 hold every symbol of their range, whatever the mask. In a mixed slot,
the samples of a downlink symbol hold the transmit signal of the RAN node that leaks into
its receiver. A dApp shall ignore the samples of a symbol whose bit is clear. Bits 14 and
15 are not used. A dApp that receives no `validSymbolMask` for a cell should treat every
symbol of the slot as an uplink symbol.

Examples with a normal CP: `0x3FFF` for an uplink slot; `0x3000` for a slot with 10
downlink, 2 guard and 2 uplink symbols. With an extended CP the slot has 12 symbols and
an uplink slot gives `0x0FFF`.

## Shared memory streams

Six identifiers name shared memory streams. Table 7-7 lists them. The CELL DATA IE
carries a shared memory reference for each of them. To read a stream, a dApp needs the
values of the locator identifiers of the table. The E3 Agent does not add a locator
identifier that the dApp has not requested.

Table: Shared memory streams

| ID | Stream | Element | One row holds | Locator identifiers | Layout |
|---|---|---|---|---|---|
| 1 | `iq_samples` | IEEE 754 binary16 pair | one cell, one slot | 8 | Annex A.6.1 |
| 2 | `pdu_data` | octet | one cell, one slot | 30, 79 | Annex A.6.2 |
| 3 | `h_estimates` | IEEE 754 binary32 pair | the UE groups of one cell, one slot | 11, 12, 13, 14, 35, 37, 38 | Annex A.6.3 |
| 44 | `srs_iq_samples` | IEEE 754 binary16 pair | one cell, one slot | 9, 52, 53 | Annex A.6.4 |
| 45 | `srs_hest` | signed 16 bit pair | one UE, one slot | 9, 46, 47, 48, 62 | Annex A.6.5 |
| 49 | `srs_rb_snr` | IEEE 754 binary32 | one UE, one slot | 50, 51, 76 | Annex A.6.6 |

## Control identifiers

Table: Control identifiers

| Control identifier | Name | Payload in `actionData` | Procedure |
|---|---|---|---|
| 1 | Set Subscription Data | CONTROL DATA IE, alternative `setSubscriptionData` | clause 6.6.4 |

The values `2` to `65535` are reserved.

## Supported E3AP messages and service model IEs

Table: E3AP fields that carry the IEs of the present document

| E3AP message | E3AP field | IE of the present document | Clause |
|---|---|---|---|
| SETUP RESPONSE | `ranFunctionData` of the entry for RAN function `2` | RAN FUNCTION DATA IE | 8.2.2.1 |
| INDICATION MESSAGE | `protocolData` | INDICATION DATA IE | 8.2.1.1 |
| dApp CONTROL ACTION | `actionData` | CONTROL DATA IE | 8.2.1.4 |
| SUBSCRIPTION REQUEST | none | none | 6.6.2 |
| SUBSCRIPTION RESPONSE | none | none | 6.6.2 |
| MESSAGE ACK | none | none | 6.6.4 |
