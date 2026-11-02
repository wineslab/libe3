# E3SM-L1KPM: procedure flow

The procedures of E3SM-L1KPM: how a dApp subscribes, how the RAN produces and sends
indications, how the dApp reads the IQ row, how a dApp detects an overwritten row, and how
a subscription ends.

The elements are in the [information elements](02-ie-mapping.md). The E3 messages are defined in the E3AP
specification [1]. The timing boxes named below are those of the
[Path A loop](../../control-loops/path-a-e3-loop.md).

## Overview

The service model uses four E3AP procedures and adds one shared memory read.

Table: Procedures of E3SM-L1KPM

| Procedure | E3AP message | Direction | Result |
|---|---|---|---|
| Setup | E3 Setup Request, E3 Setup Response | dApp to RAN, RAN to dApp | the dApp learns RAN function `2` and its telemetry identifiers |
| Subscription | E3 Subscription Request, E3 Subscription Response | dApp to RAN, RAN to dApp | the RAN starts to produce IQ rows for the dApp |
| Indication | E3 Indication Message | RAN to dApp | one reference to one IQ row |
| Release | E3 Subscription Delete, E3 Release Message | dApp to RAN | the RAN stops when the last dApp has left |

A control action for RAN function `2` is not a procedure of this service model. See
"Unsupported control" below.

## Setup

### Purpose

The RAN tells the dApp that RAN function `2` exists and what it offers.

### Procedure text

The RAN shall list RAN function `2` in the RAN function list of the E3 Setup Response. The
entry shall hold the telemetry identifiers `1`, `4`, `5` and `6`, an empty list of control
identifiers, and the RAN function data of [identity and registration](01-identity-and-registration.md).

### Abnormal conditions

If the RAN cannot build the RAN function data, it shall not register the service model.
The RAN function list then does not contain RAN function `2`.

## Subscription

### Purpose

A dApp asks the RAN to produce IQ rows and to send indications for them.

### Procedure text

The dApp sends an E3 Subscription Request. It shall set `ranFunctionIdentifier` to `2`. It
should list the telemetry identifiers it needs. It shall give an empty list of control
identifiers. It may set `periodicity` to the interval between indications in microseconds.
The value `0`, or no value, asks for one indication for each published row.

### Successful operation

The RAN accepts the subscription as E3AP defines. When the first dApp has subscribed, the
RAN shall start the production of IQ rows. From then on it creates the shared memory
region at the first uplink slot, if the region does not exist yet.

The RAN shall set the rate of indications to the smallest `periodicity` of all dApps that
are subscribed to RAN function `2`. If one dApp has no `periodicity`, or the value `0`,
the rate is one indication for each published row. The RAN shall compute the rate again
each time a dApp joins or leaves.

The RAN sends the same payload to every subscribed dApp. A dApp with a slow `periodicity`
therefore receives indications at the rate of the fastest dApp.

### Unsuccessful operation

E3AP defines the refusal of a subscription. The service model adds no other reason.

### Abnormal conditions

If the RAN cannot create the shared memory region, it shall not publish rows. The dApp
then receives no indication, although its subscription is active. See
[discrepancies](06-discrepancies.md).

## Indication

### Purpose

The RAN tells the dApp that a new IQ row is readable and where it is.

### Procedure text on the RAN side

The procedure runs for each uplink or mixed slot, while the production of IQ rows is on.

1. The RAN converts the frequency domain samples of the slot to FP16 and writes them to the next row of the region.
2. The RAN reads the monotonic clock. The value is the `timestamp` of the indication.
3. The RAN publishes the row. The next write goes to the next row.
4. The worker of the service model wakes up. The wake up ends box A1 and starts box A2.
5. If the rate is "one for each row", the worker builds the payload at once. If the rate is a period, the worker keeps the latest row and builds the payload when the period has passed. A row that a later row replaces is not sent.
6. The worker encodes the payload once (box A3). It sends the same bytes to each subscribed dApp as the `protocolData` of an E3 Indication Message with `ranFunctionIdentifier` `2`.

The RAN shall write the whole row before it publishes it. It shall not write a published
row again until it has written the 31 rows that follow it. The RAN shall not publish a row
for a slot that has no uplink symbol.

### Procedure text on the dApp side

1. The dApp decodes the E3 Indication Message and then the payload (box A9).
2. The dApp maps the region named in `shmName` for read only, once. It checks that the header `version` is `1`.
3. The dApp computes the address of the row from `fhBufferIndex` and `fhWriteIndex`, as [information elements](02-ie-mapping.md) defines.
4. The dApp reads the samples it needs, and reads the valid symbols given by `validSymbolMask` (box A10).
5. The dApp checks that the row was not overwritten while it read. See "Overwrite detection".

### Successful operation

The dApp has a consistent copy of the samples of the slot `sfn` and `slot`.

### Unsuccessful operation

If the RAN cannot encode the payload, it does not send the indication. It records a
warning once. The worker continues with the next row.

If the RAN cannot send the indication to a dApp, it counts the dApp as skipped for that
indication. It records a warning once.

### Abnormal conditions

* The dApp is on another host. The reference then names a region that does not exist or holds other data. The dApp shall not use it. See [semantics and constraints](05-semantics-and-constraints.md).
* The dApp reads the row late. The RAN may have reused the row. See the next clause.
* The RAN restarts. The RAN creates the region again. A dApp that holds a mapping of the old region shall map the new one. The dApp can see the restart as a loss of the E3 connection.

## Overwrite detection

The region has 32 rows in total: 2 buffers of 16 rows. The RAN writes the rows in a
fixed cycle: row 0 to 15 of buffer 0, then row 0 to 15 of buffer 1, then buffer 0 again.
The RAN writes every row of an uplink slot, also when the rate of indications is a
period and the indication for that row is not sent. The row of an indication is written
again when the RAN starts its 32nd write after it.

The region holds no marker in a row. A dApp cannot see from the row itself that the row
is new or old. The dApp shall decide from the indications it receives.

Let the position of a row be `16 * fhBufferIndex + fhWriteIndex`. Let `p` be the position
of the row that the dApp reads. Let `q` be the position in the newest indication that the
dApp has received. Let `d` be `(q - p)` modulo 32. A dApp shall treat the row at `p` as
overwritten when at least one of these is true.

1. `d` is `31`. The RAN is writing, or has written, the row at `p`.
2. The dApp has received a later indication with the same position `p`.

A dApp that needs a consistent copy should copy the samples first, then read `q` again,
then apply the two rules. If a rule applies, it shall drop the copy.

When the rate of indications is a period, rows between two indications are written but not
announced. The dApp then cannot count them. A lag of 32 rows or more is not detectable by
the rules above. A dApp in that mode should read each row as soon as the indication
arrives.

## Release

### Purpose

A dApp ends its use of the service model.

### Procedure text

The dApp ends the subscription with an E3 Subscription Delete, or leaves with an E3
Release Message. E3AP defines both.

### Successful operation

When the last dApp has left RAN function `2`, the RAN shall stop the production of IQ
rows. The RAN shall not publish rows while no dApp is subscribed.

The region stays in place until the service model is destroyed. A new subscription starts
the production again, and the region is reused.

## Unsupported control

The service model has no control action. If a dApp sends an E3 dApp Control Action for RAN
function `2`, the RAN shall answer with an E3 Message Ack whose response code is negative.
