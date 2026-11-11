# RAN function service model description

## RAN function overview

The RAN function of the present document is the uplink layer 1 of a RAN node. The E3
Agent of the same RAN node hosts it (clause 4.2). The RAN function receives the uplink
slots of one or more cells. For each slot it can make the following data available to
dApps.

- The post-FFT frequency domain IQ samples of the slot, before equalization.
- The PUSCH DMRS channel estimates of each UE group, before equalization.
- The PUSCH PDUs, that is the decoded transport blocks.
- For each UE with PUSCH in the slot: the allocation, the layers, the MCS, the decoding result, the HARQ state and the signal quality.
- The post-FFT IQ samples of the SRS symbols of the slot.
- For each UE with SRS in the slot: the SRS channel estimates, the SNR of each PRG, the wideband measurements and the SRS configuration.
- For each slot: the time, the frame and slot numbers, and the uplink symbols of the slot.

One E3 Agent serves all the cells of the RAN function. One indication carries the data
of all the cells that have data for the slot.

## Supported E3 services

### Telemetry

The E3 Agent supports the Subscription, Subscription delete and Indication procedures
of [1] for RAN function `2`. The INDICATION DATA IE carries the data. The E3 Agent
sends an indication only for a slot that has data for the subscription (clause 6.3).

### Control

The E3 Agent may support the dApp Control procedure of [1] for RAN function `2`, with
control identifier `1` (Set Subscription Data). An E3 Agent that supports it lists
control identifier `1` in the RAN function list. An E3 Agent that does not support it
gives an empty control identifier list.

## Indication paths

The data of a slot comes from two independent parts of the uplink processing.

- The PUSCH path runs once for each slot in which at least one cell receives PUSCH.
- The SRS path runs once for each slot in which at least one cell receives SRS. SRS slots are rarer than PUSCH slots. Their rate follows the SRS configuration of the cells [10].

Each indication belongs to one path. A slot with PUSCH and SRS gives two indications,
one for each path, with the same `sfn` and `slot`.

Each telemetry identifier belongs to the PUSCH path, to the SRS path, or to both
(shared). Clause 7.3 gives the path of each identifier. An indication of a path holds
only the identifiers of that path and the shared identifiers.

A subscription fires on a path when its granted identifiers include at least one
identifier of that path, or when all of its granted identifiers are shared. Table 6-1
lists the cases.

Table: Indication paths that fire for a subscription

| Granted identifiers | PUSCH path fires | SRS path fires |
|---|---|---|
| Only shared identifiers, for example ID 5 and ID 6 | yes | yes |
| Shared and PUSCH identifiers, for example ID 5 and ID 18 | yes | no |
| Shared and SRS identifiers, for example ID 33 and ID 55 | no | yes |
| PUSCH and SRS identifiers, with or without shared identifiers | yes | yes |
| Only PUSCH identifiers | yes | no |
| Only SRS identifiers | no | yes |

## Structure of an indication

An indication has three scopes.

- Slot scope: the values at the root of the INDICATION DATA IE. They describe the slot.
- Cell scope: the values of one entry of `cells`. There is one entry for each cell that has data for the slot on the path of the indication.
- UE scope: the values of one entry of `ues` of a cell. On the PUSCH path there is one entry for each UE with PUSCH in that cell in that slot. On the SRS path there is one entry for each UE with SRS in that cell in that slot.

```
INDICATION DATA (slot scope)
  timestamp, timestampTai, sfn, slot, nCells
  cells: one entry for each cell
    CELL DATA (cell scope)
      cellId, nRxAnt, nRxAntSrs, nBsAnts, nUe, nSrsUe, validSymbolMask, ...
      iqSamples, pduData, hEstimates, srsIqSamples, srsHest, srsRbSnr   (shared memory references)
      ues: one entry for each UE
        UE DATA (UE scope)
          rnti, rbStart, rbSize, mcsIndex, rsrp, sinr, srsWidebandSnr, ...
```

## Data delivery

A stream is delivered in one of two ways.

- Inline. The value is a field of the INDICATION DATA IE. All scalar values are inline.
- Shared memory. The array stays in the region of Annex A. The indication carries a shared memory reference (clause 8.3.10) that locates the array of a cell. UE scope fields locate the part of a UE inside the array of its cell.

Six streams use shared memory. They are listed in clause 7.4. The region is a POSIX
shared memory object [6] on the host of the RAN node that runs the E3 Agent. An indication can reach a dApp on
another host, but the shared memory reference that it carries cannot. A dApp that uses a shared memory
stream shall run on the same host, or share the IPC namespace of the E3 Agent.

The RAN FUNCTION DATA IE describes the delivery of each stream (clause 8.2.2.2).

## Procedures

### Setup

The E3 Agent shall list RAN function `2` in the RAN function list of each SETUP RESPONSE
message (clause 7.3 of [1]) while the service model runs. The entry shall hold the
following.

- `telemetryIdentifierList`: the identifiers of clause 7.3 for which the RAN node can produce values. An identifier that the RAN node cannot produce shall not be listed.
- `controlIdentifierList`: `1` if the E3 Agent supports Set Subscription Data, otherwise empty.
- `ranFunctionData`: the RAN FUNCTION DATA IE (clause 8.2.2.1), with one stream descriptor for each identifier of clause 7.3. A stream that the RAN node cannot produce has the status "unavailable".

The RAN FUNCTION DATA IE does not change while the service model runs. A dApp may keep it
for the whole association.

E3AP [1] does not accept an empty `ranFunctionData`. If the E3 Agent cannot build the RAN
FUNCTION DATA IE, it shall not list RAN function `2`.

### Subscription

The dApp sends a SUBSCRIPTION REQUEST message (clause 7.4 of [1]) with the following
content.

- `ranFunctionIdentifier`: `2`.
- `telemetryIdentifierList`: the identifiers that the dApp needs. The list should not be empty.
- `controlIdentifierList`: `1` if the dApp intends to send Set Subscription Data, otherwise empty.
- `periodicity`: optional. The interval between two indications of one path, in microseconds.
- `subscriptionTime`: optional, as defined in [1].

The E3 Agent shall grant the requested identifiers that are in its RAN function list
and whose stream status is "available". It shall not grant other identifiers. It shall
list the granted identifiers in the SUBSCRIPTION RESPONSE message. If no requested
telemetry identifier can be granted, the E3 Agent should answer "negative".

The E3 Agent shall handle `periodicity` as follows, for each indication path apart.

- `0`, or no value: one indication for each slot of the path that matches the subscription.
- A value `P` greater than `0`: at most one indication of the path in each interval of `P` microseconds. When the interval has passed, the E3 Agent sends the indication of the next matching slot of the path. The slots in between are not sent.

The E3 Agent shall report the interval that it applies in the `periodicity` field of
the SUBSCRIPTION RESPONSE message.

A new subscription has no subscription data. It covers every cell and every UE.

A dApp holds at most one subscription for RAN function `2` (clause 7.4 of [1]). To use
other telemetry identifiers, the dApp deletes its subscription and subscribes again.

### Indication

For each slot of a path that matches a subscription, the E3 Agent shall send one
INDICATION MESSAGE message (clause 7.6 of [1]) to the dApp of the subscription, with
`ranFunctionIdentifier` `2` and the INDICATION DATA IE in `protocolData`.

The INDICATION DATA IE shall hold:

- the fields of the granted identifiers of the path, for which the slot has a value, at their scope;
- the `cells` list when a granted identifier has cell scope or UE scope, with the cells that the subscription data selects and that have data on the path;
- in each cell, the `ues` list when a granted identifier has UE scope, with the UEs that the subscription data selects and that the path has in that cell in that slot.

A field whose identifier is not granted shall be absent. A list that would be empty
shall be absent. Clause 8.1 gives the rules in full.

Before it sends an indication that refers to a shared memory stream, the E3 Agent shall
have written the data of the slot to the region. The E3 Agent shall not write that data
again before the rules of Annex A allow it. A slot without an uplink symbol gives no
indication.

When several dApps hold subscriptions with the same granted identifiers and the same
subscription data, the E3 Agent may encode the INDICATION DATA IE once and send the same
octets to each of them.

> NOTE: In the timing model of the [Path A loop](../../control-loops/path-a-e3-loop.md), the E3 Agent records the data of the slot in box A1. The start of its processing ends box A1 and starts box A2. It encodes the INDICATION DATA IE in box A3. The dApp decodes the indication in box A9 and reads the shared memory streams in box A10.

### Set Subscription Data

The dApp sends a dApp CONTROL ACTION message (clause 7.7 of [1]) with
`ranFunctionIdentifier` `2`, `controlIdentifier` `1`, and the CONTROL DATA IE in
`actionData`, with the alternative `setSubscriptionData`.

The E3 Agent applies the SUBSCRIPTION DATA IE to the subscription that the dApp holds
for RAN function `2`. The pair of `dAppIdentifier` and `ranFunctionIdentifier` names that
subscription. The new subscription data replaces the old one as a whole. A SUBSCRIPTION
DATA IE without filters selects every cell and every UE again.

When the dApp set `requestAck` in the dApp CONTROL ACTION message, the E3 Agent shall answer with a CONTROL RESPONSE message. Its `requestId` holds the message identifier of the dApp CONTROL ACTION message. The service model defines no content for `controlOutcome`, so the field is absent. The `responseCode` is:

- "positive" when it applies the subscription data. It applies it from the next indication that it builds.
- "negative" when the dApp holds no subscription for RAN function `2`, when control identifier `1` is not granted to the subscription, when the payload cannot be decoded, or when the E3 Agent cannot apply a filter. The previous subscription data then stays in force.

> NOTE: E3AP [1] has no field that carries a service model payload in the SUBSCRIPTION REQUEST message. The present document therefore carries the subscription data in a dApp Control procedure. Annex C lists this as an open item.

### Subscription delete and release

When the dApp deletes its subscription (clause 7.5 of [1]), or when the association
ends (clause 7.10 of [1]), the E3 Agent stops the indications for that dApp and
discards its subscription data. The region stays in place while the service model
runs. The E3 Agent may stop writing a shared memory stream when no subscription grants
it.
