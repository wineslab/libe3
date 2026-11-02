# E3AP: information elements

The messages of E3AP and their information elements: names, types, ranges,
presence and meaning. The ASN.1 grammar is in [04-encoding.md](04-encoding.md#abstract-syntax).
If a table and the grammar disagree, the grammar takes precedence.

This is section 2 of the specification. The index is [README.md](../e3ap/README.md). The
procedures that use these messages are in [03-procedure-flow.md](03-procedure-flow.md).

## The messages at a glance

E3AP has eleven messages. Each one travels inside one `E3-PDU`.

Table: Messages of E3AP

| Message | CHOICE tag | Direction | Procedure |
|---|---|---|---|
| E3-SetupRequest | 0 | dApp to E3 Agent | Setup |
| E3-SetupResponse | 1 | E3 Agent to dApp | Setup |
| E3-SubscriptionRequest | 2 | dApp to E3 Agent | Subscription |
| E3-SubscriptionDelete | 3 | dApp to E3 Agent | Subscription delete |
| E3-SubscriptionResponse | 4 | E3 Agent to dApp | Subscription and Subscription delete |
| E3-IndicationMessage | 5 | E3 Agent to dApp | Indication |
| E3-DAppControlAction | 6 | dApp to E3 Agent | dApp Control |
| E3-DAppReport | 7 | dApp to E3 Agent | dApp Report |
| E3-XAppControlAction | 8 | E3 Agent to dApp | xApp Control |
| E3-ReleaseMessage | 9 | either | Release |
| E3-MessageAck | 10 | E3 Agent to dApp | acknowledgement of a dApp control action |

## Common types

Table: Types shared by the messages

| Type | ASN.1 definition | Range | Meaning |
|---|---|---|---|
| E3-DAppID | INTEGER | 1 to 65535 | The identifier of a dApp. The E3 Agent assigns it in the Setup procedure. |
| E3-MessageID | INTEGER | 1 to 4294967295 | The identifier of one message. The sender assigns it. |
| E3-SequenceID | INTEGER | 1 to 4294967295 | The identifier of one detection-to-mitigation procedure. The component that starts the procedure assigns it. No other component changes it. |
| E3-Name | OCTET STRING | 1 to 64 octets | The name of a dApp, a vendor or a RAN node. |
| E3-Version | OCTET STRING | 1 to 32 octets | A version string. |
| E3-ResponseCode | ENUMERATED | "positive" (0), "negative" (1) | The result of a request. The type has no extension marker. |
| E3-Timestamp | INTEGER | not constrained | Nanoseconds since the Unix epoch on the realtime (UTC) clock. |
| RAN function identifier | INTEGER | 1 to 65535 | The number of a RAN function. It is the same number in every message that names the function. |
| Telemetry identifier, control identifier | INTEGER | 1 to 65535 | The number of one telemetry stream or one control action of a RAN function. |
| Subscription identifier | INTEGER | 1 to 65535 | The number that the E3 Agent assigns to a subscription. |

A message identifier and a sequence identifier have the same range on purpose.
Both are counters of the component that creates them. A value of one can then
be stored in a field made for the other without loss.

## E3-PDU

The envelope of every message.

Table: E3-PDU

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `id` | M | | E3-MessageID | The identifier of this message. |
| `timestamp` | O | | E3-Timestamp | The time at which the sender made the message. A sender that does not want to read the clock may omit it. A missing timestamp is a normal message. |
| `msg` | M | | CHOICE | One of the eleven messages. The CHOICE has an extension marker. |

## E3-SetupRequest

A dApp sends it first. It asks the E3 Agent to register the dApp.

Table: E3-SetupRequest

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `e3apProtocolVersion` | M | | E3-Version | The version of E3AP that the dApp speaks. |
| `dAppName` | M | | E3-Name | The name of the dApp. |
| `dAppVersion` | M | | E3-Version | The version of the dApp. |
| `vendor` | M | | E3-Name | The name of the vendor of the dApp. |

## E3-SetupResponse

The E3 Agent answers a SETUP REQUEST message with it.

Table: E3-SetupResponse

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `requestId` | M | | E3-MessageID | The `id` of the E3-PDU that carried the request. |
| `responseCode` | M | | E3-ResponseCode | "positive" if the E3 Agent accepts the dApp. "negative" if it refuses. |
| `ranIdentifier` | M | | E3-Name | The name of the RAN node. |
| `e3apProtocolVersion` | O | | E3-Version | The version of E3AP that the E3 Agent speaks. |
| `dAppIdentifier` | O | | E3-DAppID | The identifier that the E3 Agent assigns to the dApp. It is present when the response code is "positive". |
| `ranFunctionList` | O | 0 to 64 | SEQUENCE OF E3-RanFunctionDefinition | The RAN functions that the dApp can use. It is present when the response code is "positive". |

## E3-RanFunctionDefinition

One entry of the RAN function list of a SETUP RESPONSE message.

Table: E3-RanFunctionDefinition

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The number of the RAN function. |
| `telemetryIdentifierList` | M | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The telemetry streams that the RAN function offers. |
| `controlIdentifierList` | M | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The control actions that the RAN function accepts. |
| `ranFunctionData` | M | 1 to 262144 octets | OCTET STRING | A description that only the service model of the RAN function can read. E3AP does not read it. |

## E3-SubscriptionRequest

A dApp sends it to use a RAN function.

Table: E3-SubscriptionRequest

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The identifier of the dApp. |
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The RAN function to use. |
| `telemetryIdentifierList` | M | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The telemetry streams that the dApp wants. |
| `controlIdentifierList` | M | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The control actions that the dApp will use. |
| `subscriptionTime` | O | 0 to 86400 | INTEGER | The time, in seconds, after which the subscription ends. "0" and a missing value both mean no limit. |
| `periodicity` | O | 0 to 60000000 | INTEGER | The time, in microseconds, between two indications. "0" means one indication for each event. A missing value means that the service model sets the rate. |

## E3-SubscriptionResponse

The E3 Agent answers a SUBSCRIPTION REQUEST message or a SUBSCRIPTION DELETE message with it.

Table: E3-SubscriptionResponse

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `requestId` | M | | E3-MessageID | The `id` of the E3-PDU that carried the request. |
| `dAppIdentifier` | M | | E3-DAppID | The identifier of the dApp. |
| `responseCode` | M | | E3-ResponseCode | The result of the request. |
| `subscriptionId` | O | | INTEGER (1 to 65535) | The identifier that the E3 Agent assigns to the subscription. It is present in the response to a request that succeeds. |
| `telemetryIdentifierList` | O | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The telemetry streams that the E3 Agent grants. A missing list means that the E3 Agent does not report the grant. An empty list means that it grants none. |
| `controlIdentifierList` | O | 0 to 256 | SEQUENCE OF INTEGER (1 to 65535) | The control actions that the E3 Agent grants. The same rule as for the telemetry list applies. |
| `ranFunctionIdentifier` | O | | INTEGER (1 to 65535) | The RAN function that the grant is for. This field is an extension addition. |
| `periodicity` | O | 0 to 60000000 | INTEGER | The interval that the E3 Agent grants, in microseconds. Same unit and range as in the request. This field is an extension addition. |

## E3-SubscriptionDelete

A dApp sends it to end a subscription.

Table: E3-SubscriptionDelete

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The identifier of the dApp. |
| `subscriptionId` | M | | INTEGER (1 to 65535) | The subscription to end. |

## E3-IndicationMessage

The E3 Agent sends it to deliver telemetry.

Table: E3-IndicationMessage

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The dApp that receives the telemetry. |
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The RAN function that made the telemetry. |
| `protocolData` | M | 1 to 32768 octets | OCTET STRING | The payload. Only the service model of the RAN function can read it. |

## E3-DAppControlAction

A dApp sends it to ask a RAN function to do something.

Table: E3-DAppControlAction

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The identifier of the dApp. |
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The RAN function that applies the action. |
| `controlIdentifier` | M | | INTEGER (1 to 65535) | The control action. |
| `sequenceId` | O | | E3-SequenceID | The sequence identifier of the E3-XAppControlAction that the dApp re-issues. It is missing when the dApp decided on the action itself. |
| `actionData` | M | 1 to 32768 octets | OCTET STRING | The payload. Only the service model can read it. |

## E3-DAppReport

A dApp sends it to give a result to a RIC.

Table: E3-DAppReport

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The identifier of the dApp. |
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The RAN function that the report is about. |
| `sequenceId` | M | | E3-SequenceID | The identifier that the dApp assigns to the procedure that the report starts. Every later message of the procedure carries it. |
| `reportData` | M | 1 to 32768 octets | OCTET STRING | The payload. Only the service model can read it. |

## E3-XAppControlAction

The E3 Agent sends it to hand an xApp decision to a dApp.

Table: E3-XAppControlAction

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The dApp that receives the decision. |
| `ranFunctionIdentifier` | M | | INTEGER (1 to 65535) | The RAN function that the decision is about. |
| `sequenceId` | M | | E3-SequenceID | The sequence identifier that the RIC side carried. The E3 Agent copies it without change. |
| `xAppControlData` | M | 1 to 32768 octets | OCTET STRING | The payload. Only the service model can read it. |

## E3-ReleaseMessage

Either side sends it to end the session of a dApp.

Table: E3-ReleaseMessage

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `dAppIdentifier` | M | | E3-DAppID | The dApp whose session ends. |

## E3-MessageAck

The E3 Agent, or the service model behind it, sends it to say what happened to a dApp control action. The message has no dApp identifier. A dApp shall ignore a MESSAGE ACK message whose `requestId` is not the identifier of a message that it sent.

Table: E3-MessageAck

| IE/Group Name | Presence | Range | IE type and reference | Semantics description |
|---|---|---|---|---|
| `requestId` | M | | E3-MessageID | The `id` of the E3-PDU of the dApp control action. |
| `responseCode` | M | | E3-ResponseCode | The result. |

## Presence notation

| Value | Meaning |
|---|---|
| M | Mandatory. The sender shall include the IE. |
| O | Optional. The sender may include the IE. |
