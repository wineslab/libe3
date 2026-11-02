# E3SM-L1KPM: identity and registration

The numbers and strings that identify E3SM-L1KPM, the telemetry identifiers it offers, and
what a RAN does to announce the service model to a dApp.

See the [information elements](02-ie-mapping.md) for the fields and the
[procedures](03-procedure-flow.md) for the message flow.

## Identity

Table: Identity of E3SM-L1KPM

| Item | Value | Notes |
|---|---|---|
| E3 RAN function ID | `2` | E3AP field `ranFunctionIdentifier`, range `1..65535`. |
| Short name | `L1-KPM` | Carried in the RAN function data. |
| Version | `1` | Carried in the RAN function data. |
| Description | `L1-KPM SM: post-FFT IQ telemetry.` | Carried in the RAN function data. |
| Shared memory region | `/e3_ran_buffers` | POSIX shared memory object name. |
| Region header version | `1` | See the [information elements](02-ie-mapping.md). |
| Control identifiers | none | The service model has no control action. |

The value `2` is the identity of this service model in every E3 message. A dApp uses it in
the subscription request and finds it in each indication.

## Telemetry identifiers

A telemetry identifier names one item of data that a dApp can ask for in an E3
subscription request. The RAN lists the identifiers of the service model in the RAN
function list of the E3 Setup Response.

Table: Telemetry identifiers of E3SM-L1KPM

| Identifier | Meaning | Field of the indication |
|---|---|---|
| `1` | IQ samples | the reference to the IQ row (`iqSamplesRef`) |
| `4` | time stamp | `timestamp` |
| `5` | system frame number | `sfn` |
| `6` | slot number | `slot` |

The identifiers `2` and `3` are not used. The numbers are not defined by this
specification. They come from the definition that the dApps of the peer implementation
use, and they follow no rule that this specification states. The alignment of the
numbers is tracked on the [interoperability page](../../interop.md).

The three fields `cellId`, `nRxAnt` and `validSymbolMask` have no telemetry identifier.
The RAN sends them in every indication.

The RAN sends the same indication payload to every subscribed dApp. It does not remove
fields that a dApp did not request. See [semantics and constraints](05-semantics-and-constraints.md).

## RAN function data

The RAN function data is a short descriptor. The RAN sends it in the E3 Setup Response, in
the `ranFunctionData` field of the entry for RAN function `2`. The E3AP grammar requires
this field to be not empty, so the service model always supplies it.

Table: Values of the RAN function data

| Field | Value |
|---|---|
| `name` | `L1-KPM` |
| `version` | `1` |
| `description` | `L1-KPM SM: post-FFT IQ telemetry.` |

The encoding of the RAN function data is described in [encoding](04-encoding.md).

## What a RAN shall do to offer the service model

A RAN that offers E3SM-L1KPM shall do the following.

1. Register one E3 service model with RAN function ID `2`, the telemetry identifiers of the table above, and no control identifiers.
2. Supply the RAN function data of the table above in the E3 Setup Response.
3. Create the shared memory region `/e3_ran_buffers` with the layout of [information elements](02-ie-mapping.md) before it sends the first indication.
4. Write an IQ row and publish it for each uplink slot, while at least one dApp is subscribed to RAN function `2`.
5. Send one indication for each published row, or at the rate that the subscriptions ask for. See [procedures](03-procedure-flow.md).
6. Refuse a control action for RAN function `2` with a negative acknowledgment.

The RAN should make the region readable by other processes on the host, so that a dApp in
another container can map it read only.

## What a dApp shall do to use the service model

A dApp that uses E3SM-L1KPM shall do the following.

1. Run on the same host as the RAN, or share the IPC namespace with it, so that it can open the region by name. See [semantics and constraints](05-semantics-and-constraints.md).
2. Subscribe to RAN function `2` with the telemetry identifiers it needs.
3. Map the region read only, and check the header version.
4. Read a row only after the indication that refers to it has arrived.
