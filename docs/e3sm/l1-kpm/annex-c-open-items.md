# Open items

## General

This annex lists the points of the present version that need a decision or a check
against implementations. Each item names the clause that it affects.

Some items describe where existing E3 Agents of the service model differ from the
present document. Such an item may end with its severity: "wrong" means a wrong result
with no error, "latent" means that only a corner case triggers it, and "doc" means that
only a comment or the documentation is affected. The [integrator guide](integrator-guide.md),
which is not part of the present document, gives the evidence in the code.

## Interaction with E3AP

Table: Open items on the interaction with E3AP

| # | Item | Clause | Current text | Possible resolution |
|---|---|---|---|---|
| C.1 | E3AP [1] has no service model field in the SUBSCRIPTION REQUEST message. | 6.6.2, 6.6.4 | The subscription data is applied with the control action Set Subscription Data, after the subscription. Until the control action, the dApp receives every cell and every UE. | Add an optional octet string, for example `subscriptionData`, as an extension addition of E3-SubscriptionRequest in [1]. The SUBSCRIPTION DATA IE is then carried there. The control action stays for changes during the subscription. |
| C.2 | A repeated SUBSCRIPTION REQUEST message returns the existing subscription in [1]. | 6.6.2 | A dApp deletes its subscription and subscribes again to change its identifiers. | Define in [1] whether a repeated request updates the identifiers and the interval of the subscription. |
| C.3 | [1] notes that some E3 Agents grant every requested identifier. Existing E3 Agents of the present service model do not look at the requested identifiers at all, and every dApp receives the same fields. Severity: doc. | 6.6.2 | The E3 Agent grants only the identifiers that it lists and whose stream is "available". | Align the implementations with clause 6.6.2. |
| C.4 | A Protocol Buffers form of the IEs is not defined. Existing E3 Agents have a Protocol Buffers form of an earlier grammar (Table C-4), in which every field is optional, so that a receiver cannot tell a missing field from `0`. On a Protocol Buffers session they send the RAN FUNCTION DATA IE as JSON text. Severity: wrong. | 8.5.1 | Not defined. | Define a Protocol Buffers form that mirrors clause 8.4, or state that the service model is not offered on a Protocol Buffers session. |
| C.17 | Rate of the indications in existing E3 Agents. They apply the smallest `periodicity` of all the dApps subscribed to RAN function `2` to every dApp, send the same payload to all of them, and compute the rate again when a dApp joins or leaves. A dApp with a longer interval receives the indications at the rate of the fastest dApp. They write a row and send an indication for every uplink or mixed slot, whether or not a cell receives PUSCH in it. With a period, they keep the latest row and send it when the period has passed. | 6.3, 6.6.2 | The E3 Agent applies the `periodicity` of each subscription to that subscription. The PUSCH path fires on a slot in which at least one cell receives PUSCH. When the interval has passed, the E3 Agent sends the indication of the next matching slot. | Align the implementations with clauses 6.3 and 6.6.2. |

## Content of the service model

Table: Open items on the content of the service model

| # | Item | Clause | Current text | Possible resolution |
|---|---|---|---|---|
| C.5 | Empty slots with a UE filter. | 8.2.1.5 | No indication is sent when every granted identifier has UE scope and no listed UE has data. | Confirm. The alternative is to send the cell scope fields without `ues`. |
| C.6 | Identity of a UE in a filter. | 8.2.1.5 | The filter uses the C-RNTI and does not follow the UE when the RNTI changes. | Add a stable UE identity, or a notification when a filtered RNTI is released. |
| C.7 | Indications above the size limits. | 8.7.2 | The E3 Agent splits the cells, then the UEs of a cell, over several indications. | Confirm the rule with implementations. An alternative is fragmentation in [1]. |
| C.8 | Clock of the `timestamp` (ID 4). Existing E3 Agents read the monotonic clock of the host (`CLOCK_MONOTONIC`) when the data of the slot becomes readable, as the present document requires. A dApp on another host, or in another time namespace, cannot compare such a value with its own clock. | 8.3.9 | Monotonic clock of the E3 Agent host. ID 78 gives the time of the slot in TAI. | Confirm that no dApp needs ID 4 on the realtime clock. |
| C.9 | Reserved identifiers `80` to `999`. The numbers of the identifiers `1` to `6` come from a definition that other dApps use, and the alignment is tracked on the [interoperability page](../../interop.md). Existing code states that an identifier equals a bit position plus one. That is not a rule of the present document. | 7.3.1 | Reserved. | Keep the range free for alignment with other definitions that use the same numbering. |
| C.10 | Limits of 16 cells, 64 UEs for each cell and 128 streams. | 8.4 | Constants of the grammar. | Check against the largest deployments. A change of a limit changes the APER encoding. |
| C.11 | JSON form of the RAN FUNCTION DATA IE. | 8.6.3 | An object. A receiver also accepts an array of stream descriptors. | Confirm that existing E3 Agents move to the object form. |
| C.12 | Range of the counters of code blocks. | 8.2.1.3 | `cbErrors` and `cbCount` are `0..255`. The stream descriptors give wider data types. | Confirm the range. |
| C.18 | Form of the indication in existing E3 Agents. They send one flat indication for each published IQ row, with `iqSamplesRef`, `timestamp`, `sfn`, `slot`, `cellId`, `nRxAnt` and `validSymbolMask` at the root, and no `cells` list. In their grammar `timestamp`, `sfn` and `slot` are mandatory, the four other fields are optional, and no type has an extension marker. The APER form takes less than 50 octets, and the JSON form at most 212 octets. The JSON keys of the IQ reference are those of clause 8.6.2. | 8.2.1, 8.4 | The fields of a cell are in an entry of `cells`, and every SEQUENCE and CHOICE has an extension marker. | Align the implementations with clauses 8.2.1 and 8.4. A dApp cannot decode the flat form with the grammar of clause 8.4. |
| C.19 | Presence of the fields in existing E3 Agents. They list only the identifiers `1`, `4`, `5` and `6`, and set every field of the flat indication in every indication, whatever the subscription. `cellId`, `nRxAnt` and `validSymbolMask` have no identifier there, so a dApp can neither ask for them nor leave them out. A dApp that handles absent fields is not exercised by these E3 Agents. Severity: latent. | 6.6.3, 7.3 | A field is present only when its identifier is granted. `cellId`, `nRxAnt` and `validSymbolMask` are ID 7, ID 8 and ID 1000. | Align the implementations with clause 6.6.3, and list ID 7, ID 8 and ID 1000. |
| C.20 | `cellId` in existing E3 Agents is always `0`. A dApp cannot tell cells apart by it, and the value `0` does not mean that the cell is unknown. Severity: wrong. | 7.3.2 | ID 7 is the physical cell identity of the cell. | Fill `cellId` with the physical cell identity. |
| C.21 | Ranges in existing E3 Agents. Their grammar allows `sfn`, `slot`, `cellId` and `nRxAnt` up to `65535`, `bufferIndex` up to `255` and `writeIndex` up to `4294967295`. They send `sfn` in `0..1023`, `slot` in `0..159`, `bufferIndex` in `0..1` and `writeIndex` in `0..15`. A dApp cannot tell a value that NR never produces from an error. Severity: doc. | 8.2.1.1, 8.3.10, 9 | `sfn` is in `0..1023`, `slot` in `0..159`, `cellId` in `0..1007` and `nRxAnt` in `1..64`. A reference whose `bufferIndex` is not `0` or `1`, or whose `writeIndex` is not below `numRows`, is erroneous. | Align the implementations with clause 8.4. |
| C.22 | `timestamp` in existing E3 Agents is an unsigned 64 bit value. The ASN.1 codec stores it as a signed value and the Protocol Buffers form as `int64`, while the JSON form writes it unsigned. A value of 2^63 or more would be encoded as a negative number. The monotonic clock does not reach it. Severity: latent. | 8.3.9 | The value is in the range 0 to 2^63 - 1. | Align the implementations with clause 8.3.9. |
| C.23 | JSON in existing E3 Agents. The codec writes every key in every object, and builds the JSON text of the RAN FUNCTION DATA IE by text formatting, with no escaping. The values are constants today. A value with a quote would break the JSON. Severity: latent. | 8.6.1 | An absent field has no member. | Align the implementations with clause 8.6.1, and build the JSON with an encoder that escapes. |
| C.24 | Buffers of existing E3 Agents. They encode an indication into 512 octets and the RAN FUNCTION DATA IE into 256 octets. A new field can overflow them. The encoder then fails, and only the first failure is reported. | 8.7.1, 8.7.3 | An indication can reach 32768 octets, and a RAN FUNCTION DATA IE with 80 stream descriptors takes 3041 to 8322 octets in APER. | Size the buffers for the limits of clause 8.7.1. |
| C.27 | RAN FUNCTION DATA IE of existing E3 Agents. It holds only `name` `L1-KPM`, `version` `1` and `description` `L1-KPM SM: post-FFT IQ telemetry.`, with no `streams` and no extension marker. Its JSON form has 79 octets. A dApp of the present document cannot decode its APER form with the grammar of clause 8.4, and finds no `streams` in its JSON form. Severity: wrong. | 8.2.2.1 | `streams` is mandatory, with one stream descriptor for each identifier of clause 7.3. | Align the implementations with clause 8.2.2.1. |

## Shared memory region

Table: Open items on the shared memory region

| # | Item | Clause | Current text | Possible resolution |
|---|---|---|---|---|
| C.13 | Scale of the IQ samples and of the SRS channel estimates. Existing E3 Agents write the rounded product of the 16 bit signed FFT output and a factor `beta` from their configuration, by default `1/2048`. Neither the region nor the indication carries `beta`, so a dApp that assumes a wrong value gets wrong amplitudes with no error. Severity: wrong. | A.6.1, A.6.5 | Not defined. A dApp gets relative values. | Carry a scale factor for each stream in the reserved fields of the header, or in the stream descriptor. |
| C.14 | Marker in a row. The same holds in existing E3 Agents. In the periodic mode a dApp then cannot detect a lag of a whole cycle of rows. Severity: wrong. | A.4 | A row has no slot number. A dApp detects a reuse only from the references that it receives. | Add the SFN and the slot to each row, or a counter of writes, so that a dApp can check a row after it has copied it. |
| C.15 | Meaning of `writeIndex` for the streams located by byte offsets. | A.5 | The number of the first row of the cell in the slot. | Confirm with implementations. |
| C.16 | Number of PRBs and of symbols of a row. Neither the indication nor the region carries the number of PRBs of the cell. An existing E3 Agent cannot create the region for a cell of more than 273 PRBs. It tries again at each uplink slot, logs one error for each slot, and sends no indication. Severity: latent. | A.6.1, A.6.4 | A row has room for 273 PRBs, and for 14 symbols (IQ) or 6 symbols (SRS IQ). | Carry the dimensions of a row in the header, so that a row can follow the carrier. |
| C.25 | Region of existing E3 Agents. Its header has 16 fields, 64 octets: the first 9 fields of Table A-2 with `version` `1`, then 7 reserved fields. The two IQ buffers follow at offset `64`, then areas for the PUSCH data and the channel estimates that are counted in the size and in the header but that no code writes. Typical values are `fh_buffer_size` `11741184`, `pusch_buffer_size` `80000`, `hest_buffer_size` `8192`, `num_fh_samples` `366912`, `num_fh_rows` `16`, `num_pusch_rows` `1`, `num_hest_rows` `1` and `max_hest_samples_per_row` `1024`. A slot fills one row of ID 1, and the rows of the two buffers are written in turn, 32 in all. The total size is `64 + 2 x fh_buffer_size + 2 x pusch_buffer_size + 2 x hest_buffer_size`. The code gives no offsets for the areas after the IQ buffers, and a dApp should not read them. Severity: doc. | A.2, A.3 | The header has 50 fields, 200 octets, and `version` is `0x00010100`. | Align the implementations with Annex A. Until then, a dApp of the present document rejects these regions, whose major version is `0` (clause A.3). |
| C.26 | Antennas in existing E3 Agents. A row holds 4 antennas. When the cell has more, the E3 Agent keeps the first 4 and sets `nRxAnt` to `4`. It writes only the first `nRxAnt` antennas, and writes zero to the subcarriers above the bandwidth of the cell. The antennas from `nRxAnt` up keep the content that the region had when it was created. Severity: latent. | A.6.1 | Antennas from `nRxAnt` up, and subcarriers above the bandwidth of the cell, have no defined content. | Confirm. A dApp of the present document ignores that content. |

## Protocol Buffers form of existing E3 Agents

Existing E3 Agents define the messages of Table C-4 in the proto2 syntax [i.4], in the
package `e3sm.l1kpm.v1`, for the flat indication of item C.18. The octets of
L1KPMIndication go in the E3AP bytes field `protocol_data`. Every field is optional. They set
every field of L1KPMIndication and do not use L1KPMRanFunctionData. An E3 Agent built
without Protocol Buffers support sends no indication on a Protocol Buffers session, and
records an error. A
Protocol Buffers form of the present document (item C.4) can start from these messages.

Table: Protocol Buffers messages of existing E3 Agents

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
