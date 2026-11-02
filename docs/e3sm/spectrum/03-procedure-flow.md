# E3SM-SPEC: procedures

The procedures of the Spectrum service model: how a dApp subscribes, how the RAN publishes
sensing telemetry, how a control is applied, and how a report or an xApp decision travels.

This is clause 7 of the specification. The fields are in
[clause 6](02-ie-mapping.md). The E3AP procedures that carry the messages are in the
[E3AP procedure chapter](../../e3ap/03-procedure-flow.md). The numbered boxes of the loops are in
[Path A](../../control-loops/path-a-e3-loop.md) and [Path B](../../control-loops/path-b-e2-e3-loop.md).

## Overview

Table: Procedures and the E3AP message that carries each

| Procedure | Initiator | E3AP message | Service model structure | Loop boxes |
|---|---|---|---|---|
| Subscription | dApp | E3 Subscription Request and Response | none | none |
| Sensing indication | RAN | E3 Indication Message | `Spectrum-SensingIndication` | A1 to A10 |
| PRB block control | dApp | E3 dApp Control Action, answered by E3 Message Ack | `Spectrum-DAppControlData` | A11 to A19a |
| Sensing policy control | dApp | E3 dApp Control Action, answered by E3 Message Ack | `Spectrum-DAppControlData` | A11 to A19a |
| dApp report | dApp | E3 dApp Report | `Spectrum-DAppReportData` | A11 to A20 |
| xApp control relay | xApp, through the RAN | E3 xApp Control Action | `Spectrum-XAppControlData` | B7 to B15 |
| Apply outcome | RAN | none on E3. The E2 control outcome carries it. | `Spectrum-ApplyOutcomeData` | B16 to B19 |

## Subscription

### Purpose

A dApp subscribes to RAN function 1 to receive sensing indications and to be allowed to send
controls.

### Procedure text

1. The dApp sends an E3 Subscription Request. It names RAN function 1, lists the telemetry
   identifiers and the control identifiers that it uses, and may set `periodicity`.
2. The RAN answers with an E3 Subscription Response. For the response, see the E3AP
   specification.
3. When the first subscription to RAN function 1 starts, the RAN starts the service model. It
   creates the sensing ring and starts the telemetry worker. The RAN shall not start the service
   model before the first subscription. A control that arrives while the service model does not
   run is dropped.
4. When the subscriptions change, the RAN derives the emission period again. The period is the
   smallest `periodicity` of all subscribers. The unit is microseconds. If any subscriber has no
   `periodicity`, or sets "0", the period is "0", which means: send one indication for each sensing
   publish.

### Successful operation

The RAN publishes sensing indications to the dApp. The RAN accepts the controls of the dApp.

### Unsuccessful operation

If the E3 Subscription Response is negative, the dApp has no subscription. The dApp shall not send
controls. A control without a running service model gets a negative E3 Message Ack.

### Abnormal conditions

If the last subscriber leaves, the RAN stops the service model. See "Service model stop" below.

## Sensing indication

### Purpose

The RAN tells a dApp which resources of a slot are free, so that the dApp can analyze the spectrum.

### Procedure text

1. The scheduler of the RAN computes the sensing ranges of a slot and publishes them.
2. For every publish, the service model reads the ranges and writes them into the next slot of the
   sensing ring. The ring is written for every publish, also when the emission period is longer
   than the publish interval.
3. The service model builds one `Spectrum-SensingIndication` with the reference to the ring slot.
4. The service model sends the same encoded indication to every dApp that subscribed to RAN
   function 1, in one E3 Indication Message for each dApp. The `ranFunctionIdentifier` is "1".
5. With period "0", the service model sends one indication for each publish. With a period greater
   than "0", it sends one indication for each period. It then uses the latest publish.

### Successful operation

The dApp receives the indication, checks `sfn` and `slot` against the ring slot, and reads the
sensing ranges.

### Unsuccessful operation

If the RAN cannot create or write the sensing ring, the RAN shall send no indication. A reference to
a ring that does not exist would be useless. If the RAN cannot encode the indication, it sends none.

### Abnormal conditions

* The ring slot has a different `sfn` or `slot` than the indication. The RAN has overwritten the
  slot. The dApp shall drop the data of this indication.
* The ring header has other values for `slot_stride`, `max_ranges` or `range_size` than the dApp
  expects. The dApp shall not read the ring.
* The dApp runs on another host than the RAN. The dApp cannot map the ring. It shall not use the
  Spectrum sensing telemetry.

## PRB block control

### Purpose

A dApp, or an xApp decision that the dApp re-issues, asks the RAN to stop using a set of PRBs.

### Procedure text

1. The dApp builds a `Spectrum-DAppControlData` with the alternative `prbBlacklistControl`. It
   lists all PRBs to block.
2. The dApp sends an E3 dApp Control Action. The `ranFunctionIdentifier` is "1" and the
   `controlIdentifier` is "1". The `actionData` field holds the encoded structure. If the dApp
   re-issues an xApp control, it also sets `sequenceId` to the value of that xApp control.
3. The RAN checks that the service model runs and that the control has a payload. Then it decodes
   the structure.
4. If the list is empty, the RAN clears the PRB block. Otherwise the RAN installs the block. It marks
   every listed PRB as occupied for all 14 symbols, in uplink and in downlink. The new list replaces
   the old one.
5. The RAN sends an E3 Message Ack with the response code of the result.
6. The scheduler applies the block from the start of a following slot. Every allocation step then
   treats the blocked PRBs as occupied.

### Successful operation

The response code of the E3 Message Ack is "positive". The scheduler uses the block from the next
slot start.

### Unsuccessful operation

The response code is "negative" when any of these is true.

* The service model is not running.
* The control has no payload.
* The control identifier is not "1" or "2".
* The MAC scheduler of the RAN is not available.
* The payload does not decode, holds another alternative, has a list longer than 273 entries, or has a
  PRB index above 272.
* The scheduler refuses to install the block.

A negative acknowledge leaves the previous block as it was.

### Abnormal conditions

The block affects the cell. The limits of a block are in
[clause 9](05-semantics-and-constraints.md). In short: the RAN does not keep the
cell common channels clear, and a long block can stall a retransmission.

## Sensing policy control

### Purpose

A dApp asks the RAN to keep some symbols of the uplink slots free, so that the sensing scan sees
clean spectrum.

### Procedure text

1. The dApp builds a `Spectrum-DAppControlData` with the alternative `sensingPolicyControl`. It
   gives one symbol mask for each slot of a frame.
2. The dApp sends an E3 dApp Control Action with `ranFunctionIdentifier` "1" and `controlIdentifier`
   "2".
3. The RAN decodes the structure.
4. If `deactivate` is "TRUE", the RAN clears the policy. Otherwise it installs the masks in the uplink
   scheduler. The uplink scheduler then prefers time domain allocations that do not use the masked
   symbols.
5. The RAN sends an E3 Message Ack.

### Successful operation

The response code is "positive". The uplink scheduler follows the policy from the next scan.

### Unsuccessful operation

The response code is "negative" for the cases of the PRB block control. It is also "negative" when the
number of masks differs from the number of slots in a frame of the RAN. A negative acknowledge leaves
the previous policy as it was.

### Abnormal conditions

If the dApp leaves without a deactivate control, the RAN clears the policy when the service model
stops.

## dApp report

### Purpose

A dApp tells the RAN, and through the RAN an xApp, which PRBs it found occupied.

### Procedure text

1. The dApp builds a `Spectrum-DAppReportData` with the alternative `prbBlacklistReport`. It may set
   `timestamp` to the time of its own realtime clock.
2. The dApp sends an E3 dApp Report. The `ranFunctionIdentifier` is "1". The `sequenceId` is
   mandatory in E3AP and the dApp assigns it. The `reportData` field holds the encoded structure.
3. The RAN does not decode the structure. It hands the octets and the `sequenceId` to the E2-E3
   bridge, which sends them to the xApp in an E2 indication. See the
   [E2SM-DAPP specification](../../e2sm-dapp/README.md).

### Successful operation

The xApp receives the report in an E2 indication. It decodes it with the Spectrum decoder, because the
E2 header names RAN function 1.

### Unsuccessful operation

A RAN without an E2 side drops the report. No error goes back to the dApp.

### Abnormal conditions

An xApp decoder that supports only one encoding cannot read a report in another encoding. See
[clause 8](04-encoding.md).

## xApp control relay

### Purpose

An xApp suggests PRBs to block. The dApp that runs next to the RAN decides what to do.

### Procedure text

1. The xApp sends an E2 control request. It holds a `Spectrum-XAppControlData` and a sequence
   identifier.
2. The RAN does not decode the structure. The E2-E3 bridge sends it to the dApp in an E3 xApp Control
   Action. The `ranFunctionIdentifier` is "1". The `sequenceId` is the value of the E2 sequence
   identifier. The `xAppControlData` field holds the octets.
3. The dApp decodes the structure and decides.
4. If the dApp acts on a `prbBlockedControl`, it sends a PRB block control to the RAN. The control
   carries the same `sequenceId`. The dApp may add its own PRBs to the list.
5. If the dApp does not act, it sends nothing. The RAN answers the xApp after a timeout. See the next
   clause.

### Successful operation

The RAN applies the PRB block and answers the xApp with the apply outcome.

### Unsuccessful operation

If the E2 side cannot start the procedure, for example when the table of open procedures is full, the
RAN answers the xApp at once and does not defer the answer.

### Abnormal conditions

The dApp can reject the control. The dApp can also send nothing back. In both cases the xApp learns
the result from the timeout answer.

## Apply outcome

### Purpose

The xApp learns when the block that it asked for is in effect.

### Procedure text

1. When the RAN receives the PRB block control with a `sequenceId`, it opens a record for that
   sequence identifier.
2. When the RAN has written the block into the scheduler, it stores the time of the realtime clock as
   `installedTimestamp`. If the installation failed, the RAN closes the record at once.
3. When the first scheduler tick uses the block, the scheduler stores the time of the realtime clock
   as `onAirTimestamp`, together with the `sfn` and the `slot` of the tick.
4. If a later block replaces this block before any tick used it, the RAN closes the record. It keeps
   `installedTimestamp` and sets no on-air value.
5. The RAN builds a `Spectrum-ApplyOutcomeData` from the record, and the E2-E3 bridge sends it to the
   xApp in the E2 control outcome. The control outcome also holds the `sequenceId`.
6. If the record stays open too long, the RAN closes it as failed. It sends an outcome with the
   values that it knows. The OpenAirInterface implementation waits 150 ms. The value is shorter than
   the timeout of the xApp side.

### Successful operation

The outcome holds `installedTimestamp`, `onAirTimestamp`, `sfn` and `slot`. The xApp can subtract
`installedTimestamp` from its own report timestamp, because all of them use the realtime clock.

### Unsuccessful operation

If the installation failed, the outcome holds no `installedTimestamp` and no on-air values. If the RAN
timed out, it holds the values that it had.

### Abnormal conditions

A record exists for a limited number of procedures at one time. The OpenAirInterface implementation
holds 64. A control that finds the table full is answered at once.

## Service model stop

When the last subscription to RAN function 1 ends, the RAN shall:

1. stop the telemetry worker and remove the sensing ring,
2. clear the sensing policy, and
3. clear the PRB block in uplink and downlink.

A held block or policy would otherwise stay in force when no dApp is there to manage it. A control
that arrives after the stop gets a negative E3 Message Ack.

## The loop from end to end

This is the order of one spectrum control loop with an xApp. The box numbers are those of the two
control loop pages.

1. The RAN publishes sensing ranges and sends an indication (A1 to A10).
2. The dApp analyzes the spectrum, finds occupied PRBs, and sends an E3 dApp Report with a new
   `sequenceId` (A11 to A20).
3. The E2-E3 bridge sends the report to the xApp in an E2 indication.
4. The xApp decides and sends an E2 control with the same `sequenceId`. The bridge sends an E3 xApp
   Control Action to the dApp (B7 to B15).
5. The dApp sends a PRB block control with the same `sequenceId` (B16, then A12 to A18).
6. The RAN installs the block (B17) and the scheduler puts it on air (B18).
7. The RAN sends an E3 Message Ack to the dApp and the E2 control outcome to the xApp (B19).
