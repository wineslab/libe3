# E3SM-SPEC: identity and registration

The numbers and strings that identify the Spectrum service model, the data that a RAN
announces for it, and what a RAN shall provide to offer it.

This is clause 5 of the specification. The
fields are in [clause 6](02-ie-mapping.md). The bytes are in
[clause 8](04-encoding.md). The E3 Setup procedure itself is in the
[E3AP specification](../../e3ap/README.md).

## Identity

Table: Identity of the Spectrum service model

| Item | Value | Notes |
|---|---|---|
| E3 RAN function ID | `1` | The E3AP field `ranFunctionIdentifier`. Range 1 to 65535. |
| Service model name | `spectrum_sm` | Sent in the RAN function data. |
| Service model version | `1` | Sent in the RAN function data. Range 0 to 65535. |
| Service model description | `Spectrum service model for sensing range indication, PRB block and sensing policy control` | Sent in the RAN function data. |
| Telemetry identifiers | `1`, `2`, `3`, `4`, `5` | See the next table. |
| Control identifiers | `1`, `2` | See the next table. |

The E3 RAN function ID is a number in the E3 interface. It is not the RAN function ID
of the E2 interface. The E2 service model that carries dApp traffic to an xApp has its
own E2 RAN function ID, and it names the E3 RAN function in a header field. See the
[E2SM-DAPP specification](../../e2sm-dapp/README.md).

## Telemetry and control identifiers

An E3 Setup Response lists the telemetry identifiers and the control identifiers of
each RAN function. A dApp uses them in its subscription.

Table: Telemetry identifiers

| Identifier | Name | Content |
|---|---|---|
| 1 | sensing ranges | the reference to the sensing ranges of one slot |
| 2 | timestamp | the time of the sensing publish |
| 3 | SFN | the system frame number of the slot |
| 4 | slot | the slot number in the frame |
| 5 | beam | the beam index |

Table: Control identifiers

| Identifier | Name | Payload | Effect |
|---|---|---|---|
| 1 | PRB block | `Spectrum-PRBBlacklistControl` | installs or clears a PRB block |
| 2 | sensing policy | `Spectrum-SensingPolicyControl` | installs or clears a sensing policy |

The RAN emits all five telemetry values in one sensing indication. The service model
does not filter the indication by the telemetry identifiers that a dApp subscribed to.
A dApp can subscribe to any subset, and it still receives the whole indication.

A control message carries one control identifier. The identifier selects the handler.
The payload shall hold the alternative that matches the identifier. See clause 7.

## The RAN function data

The RAN function data is the `ranFunctionData` field of the RAN function definition in
the E3 Setup Response. It is an OCTET STRING of 1 to 262144 octets in the E3AP
grammar. The service model fills it with one `Spectrum-RanFunctionData` structure.

Table: Content of the RAN function data

| Field | Value | Limit |
|---|---|---|
| `name` | `spectrum_sm` | 1 to 32 octets |
| `version` | `1` | 0 to 65535 |
| `description` | `Spectrum service model for sensing range indication, PRB block and sensing policy control` | 1 to 128 octets |

The RAN encodes the structure with the encoding that the E3 interface uses in this run.
Clause 8 gives the three forms. A dApp decodes it with the same encoding.

The RAN function data is opaque to E3AP. It tells a dApp what the RAN function is. It
does not list the message types. A dApp finds out which messages exist from this
specification and from the version field.

## How a dApp finds the service model

1. The dApp sends an E3 Setup Request.
2. The RAN answers with an E3 Setup Response that holds a list of RAN function
   definitions.
3. The dApp looks for the entry with `ranFunctionIdentifier` 1.
4. The dApp decodes `ranFunctionData` and reads the name and the version.
5. The dApp sends an E3 Subscription Request for RAN function 1. It lists the telemetry
   identifiers and the control identifiers that it wants to use.

If the list has no entry with identifier 1, the RAN does not offer the service model.
The dApp shall not send Spectrum messages to that RAN.

## What a RAN shall provide

A RAN that offers the Spectrum service model shall do the following.

1. Register RAN function 1 with the control identifiers 1 and 2 and the telemetry
   identifiers 1 to 5.
2. Put a valid `Spectrum-RanFunctionData` in the RAN function data. The E3AP grammar
   needs at least one octet there. If the RAN cannot encode the structure, the RAN
   shall not register the service model.
3. Create the sensing ring when the service model starts, so that a dApp can map it
   at once. Clause 6 of [clause 6](02-ie-mapping.md) gives the layout.
4. Accept controls while the service model runs, and reject them when it is not
   running. See [clause 9](05-semantics-and-constraints.md).
5. Clear a held PRB block and a held sensing policy when the service model stops.
6. Send the sensing indication with the encoding of the E3 interface.

The RAN may offer RAN function 2 (the L1-KPM service model) at the same time. The
two service models are independent. The Spectrum service model carries no IQ samples.
IQ samples are carried by the L1-KPM service model, and a dApp joins the two streams
by `(sfn, slot)`. See the [E3SM-L1KPM specification](../l1-kpm/README.md).

## Lifecycle

The service model has two states: running and stopped.

* The service model starts when the first dApp subscribes to RAN function 1. A RAN
  may start it earlier, when it registers the service model.
* The service model stops when the last subscription to RAN function 1 ends.

While the service model runs, the RAN publishes indications and accepts controls.
When it stops, the RAN stops the telemetry worker, removes the sensing ring, and
clears the PRB block and the sensing policy. A control that arrives while the service
model is stopped gets a negative message acknowledge.
