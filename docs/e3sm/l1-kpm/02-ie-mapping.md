# E3SM-L1KPM: information elements

Every information element of E3SM-L1KPM, with its type, range, presence and meaning, and
the layout of the shared memory region that the indication refers to.

See the [encoding](04-encoding.md) for the bytes of each element and the
[semantics and constraints](05-semantics-and-constraints.md) for the rules for the values.

## The elements at a glance

Table: Information elements of E3SM-L1KPM

| Element | Carried in | Direction | Purpose |
|---|---|---|---|
| L1KPM-Indication | `protocolData` of the E3 indication message | RAN to dApp | the reference to one IQ row and the data about that row |
| L1KPM-ShmRef | `iqSamplesRef` of L1KPM-Indication | RAN to dApp | the place of the IQ row in the shared memory region |
| L1KPM-RanFunctionData | `ranFunctionData` of the E3 Setup Response | RAN to dApp | name, version and description of the service model |
| Shared memory region | `/e3_ran_buffers` | RAN writes, dApp reads | the IQ samples |

Subscription requests, subscription responses and control actions use the E3AP elements
without change. The service model adds nothing to them.

## L1KPM-Indication

The indication payload. The RAN sends one for each published IQ row.

Table: Fields of L1KPM-Indication

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `iqSamplesRef` (JSON `iq_samples`) | L1KPM-ShmRef | | optional | Where to read the IQ row. |
| `timestamp` | integer | not constrained (carries a 64 bit unsigned value) | mandatory | Time in nanoseconds on the monotonic clock of the RAN host, taken when the row became readable. |
| `sfn` | integer | `0..65535` | mandatory | System frame number of the slot. The NR range is `0..1023`. |
| `slot` | integer | `0..65535` | mandatory | Slot number in the frame. The NR range depends on the numerology, at most `0..159`. |
| `cellId` (JSON `cell_id`) | integer | `0..65535` | optional | Cell identifier. |
| `nRxAnt` (JSON `n_rx_ant`) | integer | `0..65535` | optional | Number of receive antennas that the row holds, at most `4`. |
| `validSymbolMask` (JSON `valid_symbol_mask`) | integer | `0..16383` | optional | Bit map of the symbols that carry genuine uplink signal. |

The `validSymbolMask` has 14 bits. Bit `s` is set when symbol `s` of the slot carries
uplink signal according to the TDD configuration. All 14 bits are set for FDD and for a
slot that is entirely uplink. A slot that mixes downlink and uplink symbols is converted
as a whole. Its first symbols hold the RAN transmit signal that leaks into the receiver.
A consumer shall ignore the symbols whose bit is clear.

The RAN of the present specification sets `cellId`, `nRxAnt` and `validSymbolMask` in
every indication. The value of `cellId` is `0`. See
[discrepancies](06-discrepancies.md).

## L1KPM-ShmRef

Table: Fields of L1KPM-ShmRef

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `shmName` (JSON `shm_name`) | octet string (a text string in JSON) | length `1..63` | mandatory | Name of the shared memory object. The value is `/e3_ran_buffers`. |
| `fhBufferIndex` (JSON `fh_buffer_index`) | integer | `0..255` | mandatory | Number of the IQ buffer. The value is `0` or `1`. |
| `fhWriteIndex` (JSON `fh_write_index`) | integer | `0..4294967295` | mandatory | Number of the row in that buffer. The value is `0..15`. |

The row that the reference names holds the IQ samples of the slot that `sfn` and `slot`
name. The address of the row is given in the next clause.

## L1KPM-RanFunctionData

Table: Fields of L1KPM-RanFunctionData

| Field | Type | Range | Presence | Description |
|---|---|---|---|---|
| `name` | octet string | length `1..32` | mandatory | Short name. The value is `L1-KPM`. |
| `version` | integer | `0..65535` | mandatory | Version of the service model. The value is `1`. |
| `description` | octet string | length `1..128` | mandatory | Free text. |

## The shared memory region

The region is a POSIX shared memory object named `/e3_ran_buffers`. The RAN creates it,
sizes it and maps it for read and write. It is readable by all users (permission `0644`).
All values in the region have the byte order of the host.

### Region layout

Table: Parts of the region, in order

| Part | Offset | Size in bytes | Written by the RAN |
|---|---|---|---|
| Header | `0` | `64` | once, at creation |
| FH buffer 0 | `64` | `fh_buffer_size` | yes |
| FH buffer 1 | `64 + fh_buffer_size` | `fh_buffer_size` | yes |
| Areas for PUSCH and channel estimates | after FH buffer 1 | `2 * pusch_buffer_size + 2 * hest_buffer_size` | never |

The total size of the region is `64 + 2 * fh_buffer_size + 2 * pusch_buffer_size + 2 * hest_buffer_size`.
The areas after the FH buffers exist for compatibility. The RAN never writes them. A
consumer shall not read them.

### Header

The header has 16 fields of 4 bytes.

Table: Fields of the region header

| Field | Type | Value | Description |
|---|---|---|---|
| `version` | unsigned 32 bit | `1` | Version of the layout. |
| `fh_buffer_size` | unsigned 32 bit | `11741184` | Size of one FH buffer in bytes. |
| `pusch_buffer_size` | unsigned 32 bit | `80000` | Size of one PUSCH area. Not used. |
| `hest_buffer_size` | unsigned 32 bit | `8192` | Size of one channel estimate area. Not used. |
| `num_fh_samples` | unsigned 32 bit | `366912` | Number of 16 bit words in one row. |
| `num_fh_rows` | unsigned 32 bit | `16` | Number of rows in one FH buffer. |
| `num_pusch_rows` | unsigned 32 bit | `1` | Not used. |
| `num_hest_rows` | unsigned 32 bit | `1` | Not used. |
| `max_hest_samples_per_row` | unsigned 32 bit | `1024` | Not used. |
| `reserved` (7 fields) | unsigned 32 bit | `0` | Reserved. |

The header does not carry the FP16 scale factor (see below).

### IQ row

One row holds the samples of one uplink slot: 4 antennas, 14 symbols, 3276 subcarriers
(273 PRBs of 12 subcarriers), one sample of 4 bytes each. A row is 733824 bytes.

Table: Address of one sample

| Item | Value |
|---|---|
| Address of a row | `64 + fhBufferIndex * fh_buffer_size + fhWriteIndex * 733824` |
| Address of a sample | `row + ((antenna * 14 + symbol) * 3276 + subcarrier) * 4` |
| Antenna | `0..3`. Antennas at or above `nRxAnt` are not written. |
| Symbol | `0..13` |
| Subcarrier | `0..3275`. Subcarrier `0` is the lowest subcarrier of the uplink carrier. The order rises with frequency. |

The cell can have fewer than 273 PRBs. The RAN then writes zero to the subcarriers above
the PRBs of the cell. The number of PRBs of the cell is not part of the indication.

### Sample format

A sample is two unsigned 16 bit words: the in-phase value `I`, then the quadrature value
`Q`. Each word holds an IEEE 754 binary16 number [i.6]. The value is the rounded product
of the 16 bit signed FFT output and a scale factor `beta`.

The RAN takes `beta` from its configuration. The default is `1/2048`. The scale factor is
not carried in the header or in the indication. A consumer shall obtain it from the
configuration of the RAN.
