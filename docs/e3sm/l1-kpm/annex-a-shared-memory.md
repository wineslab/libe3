# Shared memory region

## General

The shared memory streams of clause 7.4 stay in one region on the host of the RAN node.
The region is a POSIX shared memory object [6]. Its name is the `memoryKey` of the stream
descriptors, for example `/e3_ran_buffers`. All shared memory streams of one E3 Agent use
the same region.

The following rules apply.

- The E3 Agent creates the region, sets its size and writes its header before it sends the first indication that refers to it. It removes an old object of the same name first.
- If the E3 Agent cannot create the region, it sends no indication that refers to it. The subscription stays active.
- The E3 Agent shall give the region access rights that let the dApps of the host open it for reading, for example the permissions `0644`.
- A dApp shall open and map the region for reading only. It shall not create, resize, write or remove the region. It should map the region once and keep the mapping while it uses the service model.
- The E3 Agent can create the region at the first slot that it writes. A dApp that opens the region before the first indication that refers to it may find no object, and should retry until that indication arrives.
- All multi-octet values in the region have the byte order of the host.
- The region stays in place while the service model runs, also when no dApp is subscribed. The E3 Agent removes the region when the service model stops.
- When the E3 Agent restarts, it creates the region again. A dApp that holds a mapping of the old region shall map the new one. The dApp sees the restart as the end of its E3AP association.
- A dApp that runs on another host, or in a container that does not share the IPC namespace of the E3 Agent, cannot use the region (clause 6.5).

## Region layout

The region holds a header and then one pair of buffers for each shared memory stream, in
the order of Table A-1. The sizes come from the header (clause A.3). A buffer size of `0`
means that the stream has no buffer.

Table: Parts of the region, in order

| Part | Stream | Offset from the start of the region | Size in octets |
|---|---|---|---|
| Header | | `0` | `200` |
| IQ samples, buffer 0 and buffer 1 | ID 1 | `200` | `2 x fh_buffer_size` |
| PUSCH PDU data, buffer 0 and buffer 1 | ID 2 | `200 + 2F` | `2 x pusch_buffer_size` |
| PUSCH channel estimates, buffer 0 and buffer 1 | ID 3 | `200 + 2F + 2P` | `2 x hest_buffer_size` |
| SRS IQ samples, buffer 0 and buffer 1 | ID 44 | `200 + 2F + 2P + 2H` | `2 x srs_iq_buffer_size` |
| SRS SNR of each PRG, buffer 0 and buffer 1 | ID 49 | `200 + 2F + 2P + 2H + 2Q` | `2 x srs_rb_snr_buffer_size` |
| SRS channel estimates, buffer 0 and buffer 1 | ID 45 | `200 + 2F + 2P + 2H + 2Q + 2R` | `2 x srs_hest_buffer_size` |

In the table, F is `fh_buffer_size`, P is `pusch_buffer_size`, H is `hest_buffer_size`,
Q is `srs_iq_buffer_size` and R is `srs_rb_snr_buffer_size`. Buffer 1 of a stream starts
right after buffer 0. Let `base(s)` be the offset of the pair of stream `s`, and
`size(s)` the size of one of its buffers. Buffer `b` of stream `s` starts at
`base(s) + b x size(s)`.

## Header

The header has 50 fields of 32 bits, unsigned. It is written once, when the region is
created.

Table: Fields of the header

| Offset | Field | Meaning |
|---|---|---|
| 0 | `version` | Version of the layout. The present document defines `0x00010100` (1.1.0): major version in bits 16 to 23, minor version in bits 8 to 15, patch in bits 0 to 7. |
| 4 | `fh_buffer_size` | Size of one buffer of ID 1, in octets. |
| 8 | `pusch_buffer_size` | Size of one buffer of ID 2, in octets. |
| 12 | `hest_buffer_size` | Size of one buffer of ID 3, in octets. |
| 16 | `num_fh_samples` | Number of 16 bit words of one row of ID 1. |
| 20 | `num_fh_rows` | Number of rows of one buffer of ID 1. |
| 24 | `num_pusch_rows` | Number of rows of one buffer of ID 2. |
| 28 | `num_hest_rows` | Number of rows of one buffer of ID 3. |
| 32 | `max_hest_samples_per_row` | Largest number of complex64 elements of one row of ID 3. |
| 36 | `srs_iq_buffer_size` | Size of one buffer of ID 44, in octets. |
| 40 | `num_srs_iq_samples` | Number of 16 bit words of one row of ID 44. |
| 44 | `num_srs_iq_rows` | Number of rows of one buffer of ID 44. |
| 48 | `srs_hest_buffer_size` | Size of one buffer of ID 45, in octets. |
| 52 | `max_srs_hest_bytes_per_row` | Largest size of one row of ID 45, in octets. |
| 56 | `num_srs_hest_rows` | Number of rows of one buffer of ID 45. |
| 60 | `srs_rb_snr_buffer_size` | Size of one buffer of ID 49, in octets. |
| 64 | `max_srs_rb_snr_bytes_per_row` | Largest size of one row of ID 49, in octets. |
| 68 | `num_srs_rb_snr_rows` | Number of rows of one buffer of ID 49. |
| 72 | `reserved` | 32 fields, written as `0`. |

> NOTE: The header gives the fields of ID 45 before those of ID 49. The buffers of the region are in the other order (Table A-1).

The `memorySizeBytes` and `numRows` of a stream descriptor shall equal the buffer size and
the number of rows of the header for the same stream.

A dApp shall check `version` before it reads a buffer. It shall accept a major version of
`1` with a minor version of `1` or more. It shall not use the region with another major
version. It shall ignore the reserved fields.

## Rows, buffers and reuse

Each stream has two buffers, numbered `0` and `1`, of `numRows` rows each. A row holds
the data that the E3 Agent writes for one cell in one slot (ID 1, 2, 3 and 44), or for one
UE in one slot (ID 45 and 49).

The E3 Agent shall write the rows of a stream as follows.

1. It fills the rows of one buffer in increasing order, from row `0`, with no gap. A slot fills one row for each cell, or for each UE.
2. When the buffer has no room for the rows of the next slot, the E3 Agent switches to the other buffer and starts again from its row `0`.
3. It writes all the data of a slot before it sends the indication that refers to it.

As a consequence, the data of a row stays unchanged until the E3 Agent has filled the
other buffer of the stream and starts to write the same buffer again. The E3 Agent does
not wait for a reader. It writes the rows of every slot, also of a slot whose indication
it does not send because of `periodicity`.

A dApp shall read the data of a row only after it has received the indication that
refers to it.

A dApp detects a reuse as follows. Let the dApp read the data of buffer `b` from an
indication. The data is overwritten once the dApp has received, for the same stream and
the same cell, a reference with `bufferIndex` equal to `1 - b`, followed by a reference
with `bufferIndex` equal to `b`. A dApp that has seen such a sequence shall discard the
data that it read from buffer `b` before the sequence.

When each slot fills exactly one row of a stream, as for ID 1 with one cell, a dApp can
detect a reuse one row earlier. Let the position of a row be
`numRows x bufferIndex + writeIndex`. Let `p` be the position of the row that the dApp
reads, `q` the position in the newest reference that the dApp has received for the
stream, and `d` the value `(q - p)` modulo `2 x numRows`. The row at `p` is overwritten
when at least one of the following is true.

1. `d` is `2 x numRows - 1`. The E3 Agent is writing the row at `p`, or has written it.
2. The dApp has received a later reference with the same position `p`.

A dApp that needs a consistent copy should copy the data first, then take the newest
reference again, then apply the rules of this clause. If a rule applies, it shall
discard the copy.

When `periodicity` is greater than `0`, the dApp does not receive every reference and
may miss a switch. A dApp in that mode should copy the data that it needs as soon as the
indication arrives, and complete the copy within the time that the E3 Agent needs to fill
one buffer of the stream.

## Location of the data

Table A-3 gives the position of the data of a cell, and of a UE, in the buffer `b` of
each stream. `b` is the `bufferIndex` of the shared memory reference of the cell. All
offsets are in octets from the start of the buffer.

Table: Position of the data of each stream in its buffer

| ID | Data of the cell | Data of the UE or of the UE group | Size of the data |
|---|---|---|---|
| 1 | `writeIndex x 2 x num_fh_samples` | none | `2 x num_fh_samples` |
| 2 | `writeIndex x (pusch_buffer_size / num_pusch_rows)` | cell position + `pduOffset` | `pduLen` |
| 3 | `rowByteOffset` | cell position + `8 x hOffset` | `8 x hSize` |
| 44 | `rowByteOffset` | none | `2 x num_srs_iq_samples` |
| 45 | none | `srsHestOffset` | `srsHestSize` |
| 49 | none | `srsRbSnrOffset` | `srsRbSnrSize` |

For ID 3, 44, 45 and 49 the data is located by the byte offsets, and `writeIndex` is not
needed to find it. It gives the number of the first row of the cell in the slot.

## Data formats

### IQ samples

The row of ID 1 holds the post-FFT frequency domain samples of the slot for one cell,
before equalization. A sample is two 16 bit words: the in-phase value, then the
quadrature value. Each word holds the bit pattern of an IEEE 754 binary16 value [5].

The row is an array `[N_ANT][14][3276][2]` of words, with the last index changing
fastest. N_ANT is `num_fh_samples / (2 x 14 x 3276)`. The word of antenna `a`, OFDM
symbol `s`, subcarrier `k` and component `c` (`0` in-phase, `1` quadrature) is at word
index `((a x 14 + s) x 3276 + k) x 2 + c` of the row. For example, with 4 antennas a row
holds `4 x 14 x 3276 x 2 = 366912` words, that is 733824 octets, and `num_fh_samples` is
`366912`.

- Subcarrier `0` is the lowest subcarrier of PRB 0 of the uplink carrier of the cell. The index rises with frequency. The row has room for 273 PRBs.
- Antennas from `nRxAnt` up, and subcarriers above the bandwidth of the cell, have no defined content. A dApp shall ignore them.
- When the cell has more receive antennas than the row holds, the E3 Agent writes the first N_ANT antennas and sets `nRxAnt` to N_ANT.
- A symbol whose bit is clear in `validSymbolMask` has no uplink signal. A dApp shall ignore it.
- With an extended CP, the symbols 12 and 13 have no defined content.

### PUSCH PDU data

The row of ID 2 holds the PUSCH PDUs of the UEs of the cell in the slot. The PDU of a UE
is the transport block as decoded, `pduLen` octets at offset `pduOffset` from the start
of the row. When the CRC of the transport block fails, `pduLen` is `0` and the row holds
no PDU for that UE.

### PUSCH channel estimates

The data of ID 3 holds one blob for each UE group of the cell in the slot. The blobs of
the cell are contiguous from the position of the cell. An element is a complex64 value:
two IEEE 754 binary32 values [5], the real part first.

The blob of a UE group is an array `[N_DMRS][N_SC][N_BS][N_L]` of elements, with the last
index changing fastest, where:

- N_DMRS is `nDmrsEstimates`: one estimate for each DMRS occasion of the PUSCH, in time order;
- N_SC is `nSubcarriers`: subcarrier `0` is the lowest subcarrier of the allocation of the UE group;
- N_BS is `nBsAnts`;
- N_L is the number of layers of the UE group, `hSize / (N_DMRS x N_SC x N_BS)`.

The element of estimate `d`, subcarrier `k`, antenna `a` and layer `l` is at element
index `((d x N_SC + k) x N_BS + a) x N_L + l` of the blob. The layers of a UE are the
layers `layerOffset` to `layerOffset + nLayers - 1` of its UE group. In FDM a UE is alone
in its group, `layerOffset` is `0` and N_L equals `nLayers`. In MU-MIMO the UEs of a
group have the same `hOffset` and `hSize`.

### SRS IQ samples

The row of ID 44 holds the post-FFT samples of the SRS symbols of the slot for one cell.
The samples have the format of clause A.6.1.

The row is an array `[N_ANT][6][3276][2]` of words, with the last index changing fastest.
N_ANT is `num_srs_iq_samples / (2 x 6 x 3276)`. The word of antenna `a`, SRS symbol `s`,
subcarrier `k` and component `c` is at word index `((a x 6 + s) x 3276 + k) x 2 + c` of
the row.

- SRS symbol `s` is OFDM symbol `srsCellStartSym + s` of the slot. Only the symbols `0` to `srsCellNSrsSym - 1` have content.
- Antennas from `nRxAntSrs` up, and subcarriers above the bandwidth of the cell, have no defined content. A dApp shall ignore them.

### SRS channel estimates

The data of ID 45 holds one blob for each UE with SRS in the slot. An element is two
signed 16 bit integers: the real part, then the imaginary part.

The blob of a UE is an array `[N_PORT][N_RX][N_PRG]` of elements, with the last index
changing fastest, where N_PORT is `srsAntPorts`, N_RX is `nRxAntSrs` and N_PRG is
`srsHestNPrbGrps`. The element of SRS port `p`, receive antenna `r` and PRG `g` is at
element index `(p x N_RX + r) x N_PRG + g` of the blob. PRG `0` is the lowest PRG of the
SRS of the UE. The blob has `4 x N_PORT x N_RX x N_PRG` octets, which is `srsHestSize`.

### SRS SNR of each PRG

The data of ID 49 holds one blob for each UE with SRS in the slot. The blob of a UE is
`srsNValidPrg` IEEE 754 binary32 values [5], the SNR of each PRG in dB, from the lowest
PRG up. The blob has `4 x srsNValidPrg` octets, which is `srsRbSnrSize`.
