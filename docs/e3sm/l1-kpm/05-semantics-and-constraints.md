# E3SM-L1KPM: semantics and constraints

The meaning of the values, the units and clocks, what absent means, how long the shared
memory region lives, what is mandatory, and what happens on errors.

The elements are in the [information elements](02-ie-mapping.md). The procedures are in
the [procedures](03-procedure-flow.md).

## Units and clocks

Table: Units and clocks

| Value | Unit | Clock or reference |
|---|---|---|
| `timestamp` | nanoseconds | the monotonic clock of the RAN host (`CLOCK_MONOTONIC`) |
| `sfn` | frames | system frame number of the cell [i.1] |
| `slot` | slots | slot number in the frame. Its length depends on the numerology [i.1] |
| `validSymbolMask` | bit map | bit `s` stands for OFDM symbol `s` of the slot, `0..13` |
| `periodicity` (E3AP) | microseconds | interval between indications. `0` means one for each row |
| IQ sample | binary16 | the 16 bit FFT output multiplied by the scale factor `beta` |

The `timestamp` has the following rules.

* It is taken when the row became readable, after the samples were written. It is not the time of the radio slot.
* It is a time on the monotonic clock. The clock has an origin that is the same for all processes on one host and unrelated to the time of day.
* A consumer shall not subtract the value from a realtime timestamp. It shall not compare it with a timestamp from another host.
* A consumer may subtract it from another monotonic reading of the same host, for example from the latency recorder (see [latrec](../../utilities/latrec.md)).
* In the periodic mode the value is the time of the row that is sent, not the time of the indication.

## Optional and absent values

The grammar marks `iqSamplesRef`, `cellId`, `nRxAnt` and `validSymbolMask` as optional.

* A consumer shall accept an indication without any of them.
* Without `iqSamplesRef` the consumer has no IQ row for that indication. It shall not read the region.
* Without `validSymbolMask` the consumer should treat all 14 symbols as valid.
* Without `cellId` or `nRxAnt` the value is unknown. Absent does not mean `0`.

The RAN of the present specification sets all four fields in every indication. A value of
`0` for `cellId` is therefore not the same as an absent value, but the RAN sends `0` for
every cell. A consumer shall not use `cellId` to tell cells apart. See
[discrepancies](06-discrepancies.md).

## The shared memory region

### Lifetime

* The RAN creates the region when the first row is written. It removes an old region of the same name first.
* The region stays after the last dApp has left.
* The RAN removes the region when it destroys the service model.
* A dApp shall not create, resize or remove the region. It shall map the region read only.

### Same host rule

A reference names an object in the shared memory of the RAN host. The indication can
cross a host boundary, but the reference cannot. A dApp that runs on another host, or in
a container that does not share the shared memory namespace of the RAN, cannot read the
row. Such a dApp shall not use the service model.

### Rows and overwrite

* The region has 2 buffers of 16 rows, 32 rows in all.
* The RAN writes the rows in order and starts again at the first row.
* A row is valid from its indication until the RAN starts its 32nd write after it.
* The RAN does not wait for a reader.

[procedures](03-procedure-flow.md) defines how a dApp detects an overwrite.

### Antennas and subcarriers

* The region holds at most 4 antennas. If the cell has more, the RAN keeps the first 4. `nRxAnt` is then `4`.
* The region holds at most 273 PRBs. A cell with more PRBs is not supported. See the error table below.
* The RAN writes zero to the subcarriers above the PRBs of the cell.

### Scale factor

The FP16 value is the rounded product of the 16 bit signed FFT output and `beta`. The RAN
takes `beta` from its configuration. The default is `1/2048`. The value is not carried in
the region or in the indication. The consumer shall know `beta`.

## Mandatory behavior

Table: Mandatory behavior of E3SM-L1KPM

| Who | Rule |
|---|---|
| RAN | shall write the whole row before it publishes it |
| RAN | shall not write a published row again before 31 more rows are written |
| RAN | shall set `timestamp`, `sfn` and `slot` in every indication |
| RAN | shall produce rows only while at least one dApp is subscribed |
| RAN | shall refuse a control action for RAN function `2` |
| dApp | shall map the region read only and check the header `version` |
| dApp | shall ignore symbols whose bit in `validSymbolMask` is clear |
| dApp | shall detect an overwritten row and drop the copy |
| dApp | shall not mix `timestamp` with a realtime timestamp |

## Validation

* A RAN shall not send an indication whose `fhBufferIndex` is above `1` or whose `fhWriteIndex` is above `15`.
* A dApp should reject an indication with such values.
* A dApp should reject a `shmName` that it does not recognize.
* A dApp shall reject a region whose header `version` it does not support.

## Error handling

Table: Errors and what happens

| Condition | Result |
|---|---|
| The RAN cannot encode the indication | no indication is sent for that row. One warning is recorded. |
| The RAN cannot send to one dApp | that dApp misses the indication. One warning is recorded. |
| The cell has more than 273 PRBs | the region is not created. No row is published. The RAN tries again at each uplink slot. |
| The shared memory object cannot be created | no row is published. The dApp receives no indication. |
| Protocol Buffers is selected, but the RAN has no Protocol Buffers support | no indication is sent. An error is recorded. |
| A control action arrives for RAN function `2` | negative acknowledgment |
| A dApp releases while others stay | the rate is computed again. The production continues. |

## Extensibility

* The ASN.1 types are not extensible. Any new field is a wire change.
* The region header has 7 reserved words. A later version may use them. A consumer shall ignore reserved words and shall check `version`.
* The PUSCH and channel estimate areas of the region are reserved for later use.
