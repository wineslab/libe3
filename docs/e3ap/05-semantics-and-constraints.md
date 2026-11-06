# E3AP: semantics and constraints

What the numbers mean, the units and clocks, which rules a message shall satisfy, and
what each side does when the input is wrong or unknown.

This is clause 9 of the specification. The index is [README.md](../e3ap/README.md). The
fields are in [02-ie-mapping.md](02-ie-mapping.md). The procedures are in
[03-procedure-flow.md](03-procedure-flow.md).

## Units and ranges

Table: Units and ranges

| Field | Unit | Range | Meaning of 0 | Meaning of absent |
|---|---|---|---|---|
| `subscriptionTime` | seconds | 0 to 86400 in APER. Any 32-bit value in JSON and Protobuf | no limit | no limit |
| `periodicity` | microseconds | 0 to 60000000 in APER. Any 32-bit value in JSON and Protobuf | one indication for each event | the service model sets the rate |
| `timestamp` | nanoseconds since the Unix epoch | not constrained | no timestamp (JSON and Protobuf) | no timestamp |
| Message identifier | none | 1 to 4294967295 | not valid | not applicable |
| Sequence identifier | none | 1 to 4294967295 | not valid on a report and on an xApp control. Means "absent" on a dApp control in Protobuf | on a dApp control: the dApp decided on its own |

Some implementations read `periodicity` in milliseconds. A sender that talks to such
an implementation shall convert the value.

## Clocks

The `timestamp` of the `E3-PDU` uses the realtime clock (UTC). A sender stamps the
message when it creates it, not when it encodes or sends it.

A receiver shall not subtract a realtime timestamp from a reading of a monotonic
clock, and it shall not do the reverse. The clocks have different origins. The
difference has no meaning.

Two hosts have different clocks. A difference of two realtime stamps from two hosts is
as good as the time synchronization between the hosts.

The timestamp is optional on purpose. A sender that does not want to read the clock on
its data path leaves it out. A receiver shall treat a message without a timestamp as a
normal message.

## Absent versus zero

Table: Absent, zero and empty

| Case | Meaning |
|---|---|
| `subscriptionTime` absent or 0 | the subscription has no time limit |
| `periodicity` absent | the service model sets the rate |
| `periodicity` 0 | one indication for each event |
| Granted list absent | the E3 Agent does not report the grant |
| Granted list present and empty | the E3 Agent grants nothing |
| `periodicity` absent in a response | the E3 Agent does not report an interval. It may still keep one |
| `sequenceId` absent on a dApp control | the dApp decided on its own. No procedure is behind it |
| `timestamp` absent | no producer reference |

JSON and Protobuf cannot tell "0" from "absent" for a number that has no presence
bit. For `timestamp`, `periodicity` of a response and `sequenceId` of a dApp control,
the value 0 stands for "absent".

## What is mandatory

Table: Mandatory content

| Message | Mandatory rule |
|---|---|
| E3-DAppReport | `sequenceId` shall be present and not 0 |
| E3-XAppControlAction | `sequenceId` shall be present and not 0. A relayed control without one cannot be matched to the acknowledge that the RIC owes the xApp |
| E3-SetupResponse | `ranIdentifier` shall be present. `dAppIdentifier` shall be present when `responseCode` is "positive" |
| E3-SubscriptionResponse | `subscriptionId` shall be present when the request succeeded |
| Every payload field | the payload shall have 1 or more octets |
| Every name and version | the text shall have 1 or more octets. A sender with no value sends a placeholder such as "unknown" |

## Validation

Table: What a receiver checks

| Check | Rule |
|---|---|
| Decoding | A message that does not decode is an abstract syntax error. See below |
| dApp identifier | The E3 Agent shall refuse a SUBSCRIPTION REQUEST message from a dApp that is not registered. The dApp shall ignore a message for another dApp |
| Subscription key | A dApp has at most one subscription for a RAN function |
| RAN function and identifiers | An E3 Agent shall refuse a subscription to a RAN function that it does not list, a subscription that names no identifier, and a subscription to telemetry or control identifiers that the RAN function does not offer. It shall refuse a dApp control action for a RAN function that it does not list and for a control identifier that the RAN function does not offer. The response carries the cause. See the Subscription and dApp Control procedures |
| Ranges | APER refuses a value outside its range at the encoder and the decoder. JSON and Protobuf do not. A sender shall keep every value in range |
| Payload content | E3AP does not check the payload. The service model does |

## Handling of unknown, unforeseen and erroneous data

### Abstract syntax errors

A message that does not decode is an abstract syntax error. This covers wrong bytes,
the wrong encoding, a value out of range for APER, an empty envelope for Protobuf and
an unknown `type` for JSON.

* On the setup channel the E3 Agent shall answer with a negative SETUP RESPONSE message. The response has no dApp identifier and no RAN function list.
* On another channel the receiver shall drop the message. It shall not send a response.

### Unexpected messages

A receiver that gets a message that is not valid on that channel or in that direction
shall drop it. Examples: an E3 Agent that gets an INDICATION message, and an E3
Agent that gets a MESSAGE ACK message from a dApp.

### Unknown extension additions

A decoder shall ignore an extension addition that it does not know. A decoder shall not
fail because of it.

### Unknown response codes

`E3-ResponseCode` has no extension marker. A value other than 0 reads as "negative"
in APER and JSON. In Protobuf a value other than "negative" reads as "positive".
New codes need a new version of E3AP. See [README.md](../e3ap/README.md).

### Unknown cause

`E3-Cause` has an extension marker. A value that the receiver does not know reads as
"unspecified". The response stays a refusal.

### Unknown RAN function or stopped service model

A dApp CONTROL ACTION message for an unknown RAN function, for a control identifier
that the RAN function does not offer, or for a RAN function whose service model does
not run is answered with a MESSAGE ACK message with `responseCode` "negative" and
the cause. It does not reach the service model.

### Queues that are full

The E3 Agent has a queue for outbound messages and a queue for reports. A full queue
drops the new message. The sender gets no notice, except a failed call on the E3 Agent
side.

### Lost links

A dApp that loses its link reports the reason "connection lost". It may run the Setup
procedure again by itself, and then has a new dApp identifier and no subscriptions. An E3 Agent that loses a dApp, or that has heard nothing from it for a long
time, should send a RELEASE message where the link is still there, and should remove the
registration and the subscriptions of that dApp. Some implementations do not detect this, so the
registration stays.

## Identifiers over time

* A dApp identifier is free again as soon as the dApp releases. A new dApp can get the same number.
* A subscription identifier is free again as soon as the subscription ends. A new subscription can get the same number.
* A message identifier is not reused in a realistic run. A 32-bit counter does not wrap.
* A sequence identifier belongs to one procedure. The component that starts the procedure shall not reuse it while the procedure is open.

## Subscription time

An E3 Agent that gets a `subscriptionTime` above 0 shall end the subscription when
the time is up. It sends no message to the dApp. It stops the service model if that
subscription was the last one for the RAN function.

The dApp keeps its own deadline, counted from the SUBSCRIPTION RESPONSE message. The
E3 Agent counts from the moment it handled the request, so the dApp deadline is never
earlier than that of the E3 Agent. A dApp that renews the moment its deadline passes
always reaches the E3 Agent after the old subscription is gone.

An E3 Agent that does not act on `subscriptionTime` keeps the subscription until the
dApp deletes it or releases. A dApp that needs to end a subscription shall delete it.
