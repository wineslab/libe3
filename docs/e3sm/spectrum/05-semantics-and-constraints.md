# E3SM-SPEC: semantics and constraints

Units, clocks, the meaning of absent and zero, the limits of a PRB block, the checks that a receiver
makes, and what happens when a message is wrong.

This is clause 9 of the specification. The fields are in
[clause 6](02-ie-mapping.md). The procedures are in [clause 7](03-procedure-flow.md).

## Units

Table: Units of the fields

| Field | Unit | Notes |
|---|---|---|
| `Spectrum-SensingIndication.timestamp` | nanoseconds | monotonic clock of the RAN host |
| `Spectrum-CellConfigIndication.timestamp` | nanoseconds | monotonic clock of the RAN host |
| `Spectrum-DAppReportData.timestamp` | nanoseconds since the Unix epoch | realtime clock of the dApp host |
| `Spectrum-XAppControlData.timestamp` | nanoseconds since the Unix epoch | realtime clock of the xApp host |
| `Spectrum-ApplyOutcomeData.timestamp`, `installedTimestamp`, `onAirTimestamp` | nanoseconds since the Unix epoch | realtime clock of the RAN host |
| `sfn` | frame number | 0..1023 in practice |
| `slot` | slot number | 0..159 in practice, 0..19 at 30 kHz |
| PRB index | PRB | absolute in the carrier, 0..272 |
| symbol mask | bit field | bit s is symbol s of the slot, 14 bits |
| `ulCarrierPRBs`, `ulOffsetToCarrier` | PRB | at the subcarrier spacing of the carrier |
| `ulSubcarrierSpacing` | numerology index | 0..3, the spacing is 15 kHz times 2 to the power of the index |
| `ulArfcnPointA` | NR-ARFCN | 0..3279165 [i.7] |
| `slotsPerFrame` | slots | in a frame of 10 ms |
| `validityPeriod` | milliseconds | 1..3600000, up to one hour |
| `noiseFloorThreshold` | not defined | the dApp and the xApp agree on it |
| `periodicity` in E3AP | microseconds | selects the indication period. See the E3AP specification. |

## Clocks

Two clocks exist in the service model. They shall not be mixed.

* The sensing indication and the cell configuration indication use the monotonic clock. It has a common
  origin for all processes on one host and no relation to the calendar. The sensing indication only has
  a meaning on the host of the RAN, because it holds a reference to local shared memory.
* The dApp report, the xApp control and the apply outcome use the realtime clock. Each of them crosses
  from one host to another. The RAN relays the first two without a change.

A receiver shall not subtract a monotonic value from a realtime value, or the reverse. The two share the
field name `timestamp` in the grammar, but they are not on the same clock.

An xApp can subtract the timestamp of a dApp report from `installedTimestamp` and from
`onAirTimestamp`, because they are on the realtime clock. The accuracy of the result is the accuracy of
the time synchronization between the hosts.

The latency recorder uses the monotonic clock. See [latrec](../../utilities/latrec.md). A payload
timestamp and a recorder timestamp shall not be subtracted.

## Absent and zero

Table: Meaning of an absent field

| Field | Absent means |
|---|---|
| `beam` | no beam information. A RAN that does not use several beams sends "0". |
| `validityPeriod` | no validity limit stated |
| `deactivate` | "FALSE" |
| `tddSlotPattern` | the cell is an FDD cell |
| `nrPci` | the RAN does not give the physical cell identity |
| report `timestamp` | the dApp did not stamp the report. This is a normal message. |
| xApp control `timestamp` | the xApp did not stamp the control. This is a normal message. |
| `installedTimestamp` | the control did not install |
| `onAirTimestamp`, `sfn`, `slot` of the outcome | no scheduler tick used the block |

A receiver shall not reject a report or an xApp control only because it has no `timestamp`.

A timestamp of "0" in an apply outcome is never sent. The RAN omits the field instead. A sender that
has no value shall omit the field.

## PRB block semantics

* The PRB list is complete and not incremental. The RAN replaces its block with the new list.
* An empty list clears the block.
* The RAN applies the list to uplink and downlink. The message has no direction field.
* The RAN blocks all 14 symbols of every listed PRB.
* A PRB that appears twice in the list is blocked once.
* The scheduler applies the block from the start of a following slot. It marks the PRBs as occupied in
  its resource map, so that every allocation step schedules around them.
* The `validityPeriod` of the control is accepted. The RAN does not use it. A dApp that needs a block
  to expire shall send a clearing control itself.

## Limits of a PRB block

A PRB block controls the allocation of the scheduler. It does not control the radio. The following
limits apply. A dApp and an operator shall know them.

1. **The block is uniform.** The RAN blocks every listed PRB for every channel. It does not protect or
   steer any channel. When a block hits a fixed PUCCH or SRS resource of a UE, the RAN logs the
   collision and does not move the resource.
2. **Cell common channels are not protected.** The block takes effect on SSB, CORESET 0, PRACH and the
   cell common PUCCH. A connected UE tolerates a short block. A block over the downlink SSB or
   CORESET 0 region stops a new UE from synchronizing. A dApp should keep its list off these PRBs.
3. **A dedicated PUCCH or SRS under a block is dropped.** The RAN skips the reception without an error.
   The UE loses scheduling requests, periodic CSI or SRS while the block overlaps, and recovers after
   it lifts.
4. **CSI-RS still transmits on blocked downlink PRBs.** The resource map is an allocation tracker. It
   does not gate the transmission.
5. **A long block can stall a retransmission.** A retransmission keeps its transport block size. If a
   block covers the only PRBs that it can use, the scheduler retries until the inactivity timeout
   removes the UE. The RAN has no guard for this case.
6. **Reception is not gated.** A UE that transmits on a blocked uplink PRB is still received. The
   silence of a blocked PRB is best effort.
7. **Configured grants are not covered.** The RAN does not implement configured grant scheduling. If it
   does, a UE with a configured grant transmits on its own PRBs, regardless of the block.
8. **A very high subcarrier spacing needs care.** The cost of a block change grows with the size of the
   resource ring. It is small at 30 kHz. At millimeter wave spacing it can approach the slot deadline.
9. **Released PRBs return late.** After a block shrinks, freed uplink PRBs can stay marked for about one
   ring cycle, which is 10 to 20 ms at 30 kHz.

## Sensing policy semantics

* A policy has one 14 bit mask for each slot of a frame. Bit s set means: the uplink scheduler prefers
  allocations that do not use symbol s of that slot for the UE.
* The policy only states a preference. The scheduler can still use a masked symbol when it has no other
  allocation.
* `deactivate` "TRUE" clears the policy and the masks are ignored.
* The number of masks shall equal the number of slots in a frame of the RAN.
* The `validityPeriod` of the policy is accepted. The RAN does not use it. A policy does not expire.

## What does and does not touch the radio

Table: Effect of each function on the scheduler

| Function | Changes scheduling |
|---|---|
| sensing indication, cell configuration indication and the sensing ring | no. They only export data. |
| PRB block control | yes. It removes PRBs from allocation. |
| sensing policy control | yes. It changes the choice of time domain allocation. |
| dApp report | no |
| xApp control | not directly. A dApp may turn it into a PRB block control. |

## Validation

A receiver validates a message before it acts. The RAN applies these checks.

Table: Checks of the RAN on a control

| Check | Result when it fails |
|---|---|
| the service model runs | negative acknowledge |
| the control has at least one octet | negative acknowledge |
| the control identifier is "1" or "2" | negative acknowledge |
| the MAC scheduler exists | negative acknowledge |
| the payload decodes in the encoding of the run | negative acknowledge |
| the payload holds the alternative that matches the control identifier | negative acknowledge |
| the PRB list has at most 273 entries and every index is at most 272 | negative acknowledge |
| `maskPerSlot` has 1 to 160 entries, and every value is at most 16383 | negative acknowledge |
| the number of masks equals the slots in a frame of the RAN | negative acknowledge |
| the scheduler accepts the new block or policy | negative acknowledge |

The RAN answers every control with an E3 Message Ack. The response code is "positive" or "negative". The
acknowledge says only whether the RAN applied the control to the scheduler. It does not say that the
block is in the air. The apply outcome says that.

A negative acknowledge does not change the state that was in force before the control.

## Unknown identifiers and alternatives

* A control identifier other than "1" or "2" is rejected with a negative acknowledge.
* A choice alternative that the receiver does not know is rejected. The receiver shall not guess what it
  means. In APER, a receiver that does not know an extension alternative cannot read its content.
* A receiver ignores an extension field of a structure that it does not know, where the encoding allows
  it. This holds for the apply outcome and for the structures with an extension marker.
* A RAN function ID other than "1" in a control that reaches this service model is rejected.

## Concurrency and freshness of the ring

* The RAN has one writer for the sensing ring. No lock exists. The RAN writes a ring slot completely
  before it sends the indication that points to it.
* The RAN writes `sfn` and `slot` of the ring slot last. A dApp compares them with the indication. A
  mismatch means that the slot was overwritten, and the dApp shall drop it.
* A dApp reads the ring read only. It shall not write to it.

## What is mandatory

Table: Mandatory behavior

| Party | Mandatory |
|---|---|
| RAN | register RAN function 1 with the control identifiers 1 and 2 |
| RAN | send a valid RAN function data |
| RAN | create the sensing ring before the first indication |
| RAN | write the ring before it sends the indication that points to it |
| RAN | answer every control with an E3 Message Ack |
| RAN | clear the block and the policy when the service model stops |
| dApp | check `sfn` and `slot` of the ring slot against the indication |
| dApp | check the ring layout fields against its own |
| dApp | send the complete PRB list in every PRB block control |
| dApp | keep the `sequenceId` of an xApp control when it re-issues it as a PRB block control |
| xApp | decode the apply outcome with the service model that it sent the control to |

Everything else in the grammar is optional or has a default.
