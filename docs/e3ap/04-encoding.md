# E3AP: encoding

How an implementation turns an E3AP message into bytes, how the bytes cross a link,
and the limits on their size. Worked examples are checked against the pinned test
vectors.

This is clause 8 of the specification. The index is [README.md](../e3ap/README.md). The
messages are in [02-ie-mapping.md](02-ie-mapping.md). The validation rules are in
[05-semantics-and-constraints.md](05-semantics-and-constraints.md). Code pointers and
build switches are in [integrator-guide.md](integrator-guide.md).

## What is on the wire

Every E3AP message is one `E3-PDU`. An implementation shall encode the `E3-PDU` with
one of three encodings.

Table: The three encodings of an E3-PDU

| Encoding | Rules | Reference |
|---|---|---|
| APER | Aligned Packed Encoding Rules of the ASN.1 grammar in the last clause of this chapter | X.691 [3] |
| JSON | One flat JSON object for each message. The keys are the ASN.1 field names | RFC 8259 [4] |
| Protobuf | One `E3Pdu` message of the proto3 schema of E3AP. The `oneof` case names the message | [i.1] |

The encoding is not part of the message. There is no indicator on the wire. Both
sides shall use the same encoding on a link. A receiver that gets bytes in another
encoding cannot decode them. An E3 Agent shall answer such a SETUP REQUEST message
with a negative SETUP RESPONSE message, if it can still send a response.

The payloads (`ranFunctionData`, `protocolData`, `actionData`, `reportData` and
`xAppControlData`) follow the encoding of the link. APER carries them as octet strings.
JSON carries them as nested JSON values, not as hexadecimal text. Protobuf carries
them as `bytes`.

## APER

### The rules that decide almost every byte

1. **Alignment.** In aligned PER a value that needs whole octets first skips to the next octet boundary. The skipped bits are zero. Small fields (extension bits, presence bits, choice indices, short length codes) are not aligned. They are packed from the most significant bit.
2. **Extension bit.** Every `SEQUENCE` and the `CHOICE` of `E3-PDU` has an extension marker. An encoder writes the extension bit as 0, unless the message carries an extension addition. Each marker costs one bit.
3. **Presence bits.** Each `OPTIONAL` field costs one bit. The bits come before the first field, in the order of the grammar. A cleared bit means that the field is absent and takes no more space.

### Rules for each construct

Table: APER bits for each construct of the grammar

| Construct | APER encoding | Where it appears |
|---|---|---|
| `INTEGER (1..65535)` | 16 bits, aligned: the value minus 1 | dApp, RAN function, telemetry, control and subscription identifiers |
| `INTEGER (1..4294967295)` | a 2 bit length code (octets minus 1), padding to the octet boundary, then the value minus 1 in that many octets | message identifier, sequence identifier |
| `INTEGER (0..86400)` | a 2 bit length code, padding, then the value in 1 to 3 octets | `subscriptionTime` |
| `INTEGER (0..60000000)` | a 2 bit length code, padding, then the value in 1 to 4 octets | `periodicity` |
| `INTEGER` with no constraint | a length octet, then the value in the fewest octets | `timestamp` |
| `ENUMERATED` with two values and no extension | 1 bit | `E3-ResponseCode` |
| `CHOICE` with 11 alternatives and an extension marker | 1 extension bit, then the index in 4 bits | `msg` |
| `SEQUENCE (SIZE (0..256)) OF` | the count in 16 bits, aligned, then the elements | identifier lists |
| `SEQUENCE (SIZE (0..64)) OF` | the count in 7 bits, then the elements | RAN function list |
| `OCTET STRING (SIZE (1..64))` | the length minus 1 in 6 bits, padding, then the octets | `E3-Name` |
| `OCTET STRING (SIZE (1..32))` | the length minus 1 in 5 bits, padding, then the octets | `E3-Version` |
| `OCTET STRING (SIZE (1..32768))` | the length minus 1 in 15 bits, padding, then the octets | payload fields |
| `OCTET STRING (SIZE (1..262144))` | the length in an aligned length determinant, then the octets | `ranFunctionData` |

The examples below pin the rows for the identifiers, the time limit, the interval,
the response code, the choice index and the short counts. The rows for the long
lists and for the payload strings come from X.691. No test vector covers them. They are
not checked.

### Worked examples

Each example is a complete `E3-PDU`. The message identifier is 42 and the timestamp is
1756800000123456789 nanoseconds. The bytes equal the pinned test vectors.

**Example 1: RELEASE message for dApp 7.**

```text
40 29 08 18 61 68 4a cc c3 cd 15 48 00 06
```

Table: Release message, byte by byte

| Bytes | Meaning |
|---|---|
| `40` | extension bit 0, timestamp present 1, length code `00` (one octet), 4 padding bits |
| `29` | message identifier 42, sent as 41 (42 minus 1) |
| `08` | the timestamp is 8 octets long |
| `18 61 68 4a cc c3 cd 15` | the timestamp, 1756800000123456789 |
| `48` | `msg` extension bit 0, index `1001` (9, the RELEASE message), message extension bit 0, 2 padding bits |
| `00 06` | dApp identifier 7, sent as 6 |

**Example 2: MESSAGE ACK with request identifier 42 and "negative".**

```text
40 29 08 18 61 68 4a cc c3 cd 15 50 29 80
```

Table: Message acknowledgement, byte by byte

| Bytes | Meaning |
|---|---|
| `40 29 08 18 61 68 4a cc c3 cd 15` | the envelope, as in example 1 |
| `50` | `msg` extension bit 0, index `1010` (10, the MESSAGE ACK message), message extension bit 0, length code `00` |
| `29` | `requestId` 42, sent as 41 |
| `80` | `responseCode` "negative" as bit 1, 7 padding bits |

**Example 3: SUBSCRIPTION REQUEST.** dApp 7, RAN function 1, telemetry identifiers 1
and 2, no control identifiers, time limit 60 seconds, interval 100 microseconds.

```text
40 29 08 18 61 68 4a cc c3 cd 15 13 00 06 00 00 00 02 00 00 00 01 00 00 00 3c 00 64
```

Table: Subscription request, byte by byte

| Bytes | Meaning |
|---|---|
| `40 29 08 18 61 68 4a cc c3 cd 15` | the envelope, as in example 1 |
| `13` | `msg` extension bit 0, index `0010` (2, the SUBSCRIPTION REQUEST message), message extension bit 0, presence bits `11` for `subscriptionTime` and `periodicity` |
| `00 06` | dApp identifier 7 |
| `00 00` | RAN function identifier 1 |
| `00 02` | the telemetry list has 2 elements |
| `00 00`, `00 01` | telemetry identifiers 1 and 2 |
| `00 00` | the control list has 0 elements |
| `00 3c` | length code `00` (one octet) with padding, then `subscriptionTime` 60 |
| `00 64` | length code `00` with padding, then `periodicity` 100 |

### Size of the encoding

The envelope with a timestamp of 1756800000123456789 takes 11 octets before the
message: 1 octet for the extension, presence and length bits, 1 octet for the message
identifier, 1 length octet and 8 octets of timestamp. A message without a timestamp is
9 octets shorter. A message identifier up to 256 takes 1 octet, up to 65536 takes 2,
up to 16777216 takes 3, and a larger one takes 4. A long list or a payload grows the
message by the size of its elements.

### A receiver that sees extension additions

A decoder shall ignore an extension addition that it does not know. `cause` and
`detail` are the first extension additions of `E3-SetupResponse`,
`E3-SubscriptionResponse` and `E3-MessageAck`, in that order. In
`E3-SubscriptionResponse` `ranFunctionIdentifier` and `periodicity` follow them. APER
sends one presence bit for each extension addition of the grammar, so the order is
part of the encoding: a decoder that has the additions in another order reads one
field as another. A change of a constraint is not an extension addition either. It
changes the bytes.

## JSON

An implementation shall encode an `E3-PDU` as one flat JSON object. The object has
the members `type`, `id` and `timestamp`. The members of the message follow at the
same level.

Table: Rules of the JSON encoding

| Rule | Value |
|---|---|
| `type` | the name of the message in camel case: `setupRequest`, `setupResponse`, `subscriptionRequest`, `subscriptionDelete`, `subscriptionResponse`, `indicationMessage`, `dAppControlAction`, `dAppReport`, `xAppControlAction`, `releaseMessage`, `messageAck` |
| Keys | the ASN.1 field names, for example `dAppIdentifier`, `ranFunctionIdentifier`, `telemetryIdentifierList`, `subscriptionTime`, `periodicity`, `subscriptionId`, `requestId`, `sequenceId` |
| `responseCode` | the string "positive" or the string "negative". An unknown string means "negative" |
| `cause` | the name of the `E3-Cause` value in camel case, for example `unsupportedIdentifier`. A name that the decoder does not know reads as `unspecified`. A decoder reads it from a SETUP RESPONSE, SUBSCRIPTION RESPONSE or MESSAGE ACK message |
| `detail` | the text, as a JSON string. A decoder also reads the key `message` as `detail`, when `detail` is not there |
| `timestamp` | written on every message. The value 0 means that there is no timestamp |
| Payload fields | a JSON object or array. `ranFunctionData`, `actionData`, `reportData` and `xAppControlData` shall be an object or an array. An empty payload does not encode. `protocolData` may be any valid JSON value |
| Wrapper | a message with a nested `data` wrapper is refused |
| Granted lists | `telemetryIdentifierList` and `controlIdentifierList` of a SUBSCRIPTION RESPONSE are present when the E3 Agent reports them. A missing list means "not reported". An empty list means "none granted". A decoder may also read the names `telemetryGrantedList` and `controlGrantedList` |
| Missing fields | a decoder sets a missing field to 0 or to the empty string. It does not refuse the message |

Example of a SUBSCRIPTION REQUEST message (not checked against a pinned vector; the
order of the members does not matter):

```json
{
  "type": "subscriptionRequest",
  "id": 42,
  "timestamp": 1756800000123456789,
  "dAppIdentifier": 7,
  "ranFunctionIdentifier": 1,
  "telemetryIdentifierList": [1, 2],
  "controlIdentifierList": [],
  "subscriptionTime": 60,
  "periodicity": 100
}
```

JSON and Protobuf do not check the ranges of the grammar. A sender shall keep every
value inside the range of [02-ie-mapping.md](02-ie-mapping.md), so that the same
message can move to APER.

## Protobuf

The proto3 schema of E3AP has one message for each E3AP message and one `E3Pdu`
envelope. The `oneof` case of `E3Pdu` follows the order of the `CHOICE` tags.

Table: Rules of the Protobuf encoding

| Rule | Value |
|---|---|
| Names | the field names are the ASN.1 names in snake case, for example `dapp_identifier` |
| Optional fields | `optional` fields keep their presence |
| Zero values | `timestamp` and `sequence_id` of a dApp CONTROL ACTION message use the value 0 for "absent" |
| Granted lists | a wrapper message `IdentifierList` makes "not reported" differ from "none granted" |
| Response code | an enumeration with the values "positive" (0) and "negative" (1). A value that is not "negative" reads as "positive" |
| Cause | an enumeration `Cause` with the values of `E3-Cause`, and an `optional string detail`, on the three messages that carry them. A value that is not in the enumeration reads as unspecified |
| Unknown fields | a decoder ignores them |
| Empty envelope | an envelope with no `oneof` case does not decode |

## Framing on each link

### ZeroMQ

One E3AP message is one ZeroMQ message. There is no extra framing. The setup channel
uses the request and reply pattern, and the two other channels use publish and
subscribe. A subscriber shall subscribe to every message (an empty filter).

On a ZeroMQ link over TCP the E3 Agent listens on the port of each channel. The
dApp connects. On a link over IPC the endpoints are the paths of [01-identity-and-registration.md](01-identity-and-registration.md#default-endpoints).

### POSIX sockets

The POSIX link layer uses stream sockets, one for each channel. The three transport
layers differ only in the socket type: Unix domain stream, TCP or SCTP in one-to-one
mode.

Table: Framing on the POSIX link layer

| Direction | Framing |
|---|---|
| E3 Agent to dApp (the setup response and the outbound channel) | 4 octets of length in network byte order, then the E3-PDU. The payload goes out in chunks of at most 8192 octets |
| dApp to E3 Agent (the setup request and the inbound channel) | none. One send is one E3-PDU |

A receiver of a framed message shall refuse a length of 0 and a length above 65536.

The framing is not symmetric. A stream socket does not keep message boundaries, so a
receiver of unframed messages can see two messages joined or one message split.

The E3 Agent sends every message of the outbound channel to every connected dApp.
It closes the connection to a dApp whose socket stalls for 500 ms.

## Size limits

Table: Size limits

| Item | Limit | Where it is enforced |
|---|---|---|
| Payload (`protocolData`, `actionData`, `reportData`, `xAppControlData`) | 1 to 32768 octets | APER |
| `ranFunctionData` | 1 to 262144 octets in the grammar | APER |
| Name | 1 to 64 octets | APER |
| Version | 1 to 32 octets | APER |
| Telemetry list, control list | 0 to 256 entries | APER |
| RAN function list | 0 to 64 entries | APER |
| Encoded E3-PDU | at most 65536 octets for an encoder buffer. A receiver reads 60000 octets from a ZeroMQ message or from an unframed stream | implementation |
| Framed message on POSIX | 1 to 65536 octets | the receiver |

## Abstract syntax

The ASN.1 definition of E3AP conforms to ITU-T X.680 [1] and X.681 [2]. If a table
and the grammar disagree, the grammar takes precedence, except for the conditions of
presence, where the table takes precedence.

```asn1
-- ASN1START
-- SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
-- SPDX-License-Identifier: Apache-2.0

E3 DEFINITIONS AUTOMATIC TAGS ::= BEGIN

    E3-SetupRequest ::= SEQUENCE {
        e3apProtocolVersion  E3-Version,
        dAppName             E3-Name,
        dAppVersion          E3-Version,
        vendor               E3-Name,
        ...
    }

    E3-SetupResponse ::= SEQUENCE {
        requestId            E3-MessageID,
        responseCode         E3-ResponseCode,
        ranIdentifier        E3-Name,
        e3apProtocolVersion  E3-Version OPTIONAL,
        dAppIdentifier       E3-DAppID OPTIONAL,
        ranFunctionList      SEQUENCE (SIZE (0..64)) OF E3-RanFunctionDefinition OPTIONAL,
        ...,
        cause                E3-Cause OPTIONAL,
        detail               E3-Text OPTIONAL
    }

    E3-SubscriptionRequest ::= SEQUENCE {
        dAppIdentifier          E3-DAppID,
        ranFunctionIdentifier   INTEGER (1..65535),
        telemetryIdentifierList SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535),
        controlIdentifierList   SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535),
        subscriptionTime        INTEGER (0..86400) OPTIONAL,
        -- Microseconds between indications, 0 = every event.
        periodicity             INTEGER (0..60000000) OPTIONAL,
        ...
    }

    E3-SubscriptionDelete ::= SEQUENCE {
        dAppIdentifier        E3-DAppID,
        subscriptionId        INTEGER (1..65535),
        ...
    }

    E3-SubscriptionResponse ::= SEQUENCE {
        requestId            E3-MessageID,
        dAppIdentifier       E3-DAppID,
        responseCode         E3-ResponseCode,
        subscriptionId       INTEGER (1..65535) OPTIONAL,
        telemetryIdentifierList SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535) OPTIONAL,
        controlIdentifierList   SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535) OPTIONAL,
        ...,
        -- Extension additions, in this order.
        cause                   E3-Cause OPTIONAL,
        detail                  E3-Text OPTIONAL,
        ranFunctionIdentifier   INTEGER (1..65535) OPTIONAL,
        -- Least microseconds the E3 Agent keeps between two indications. Absent: not reported.
        periodicity             INTEGER (0..60000000) OPTIONAL
    }

    E3-RanFunctionDefinition ::= SEQUENCE {
        ranFunctionIdentifier   INTEGER (1..65535),
        telemetryIdentifierList SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535),
        controlIdentifierList   SEQUENCE (SIZE (0..256)) OF INTEGER (1..65535),
        ranFunctionData         OCTET STRING (SIZE (1..262144)),
        ...
    }

    E3-IndicationMessage ::= SEQUENCE {
        dAppIdentifier          E3-DAppID,
        ranFunctionIdentifier   INTEGER (1..65535),
        protocolData            OCTET STRING (SIZE (1..32768)),
        ...
    }

    E3-DAppControlAction ::= SEQUENCE {
        dAppIdentifier          E3-DAppID,
        ranFunctionIdentifier   INTEGER (1..65535),
        controlIdentifier       INTEGER (1..65535),
        -- Present only when the dApp is re-issuing a control it received as an
        -- E3-XAppControlAction, carrying that action's sequenceId back down so the
        -- RAN can match the applied control to the xApp procedure that asked for
        -- it. Absent on a control the dApp decided on its own.
        sequenceId              E3-SequenceID OPTIONAL,
        actionData              OCTET STRING (SIZE (1..32768)),
        ...
    }

    E3-DAppReport ::= SEQUENCE {
        dAppIdentifier          E3-DAppID,
        ranFunctionIdentifier   INTEGER (1..65535),
        -- Assigned by the dApp. A report is the head of the E2-E3 loop, so this is
        -- the identifier every later message in that procedure carries: the RAN
        -- copies it into the E2SM-DAPP indication header as sequence-id, and an
        -- xApp reacting to this report echoes it on its control.
        sequenceId              E3-SequenceID,
        reportData              OCTET STRING (SIZE (1..32768)),
        ...
    }

    E3-XAppControlAction ::= SEQUENCE {
        dAppIdentifier          E3-DAppID,
        ranFunctionIdentifier   INTEGER (1..65535),
        -- Copied by the RAN's E2-E3 bridge from the E2SM-DAPP control header's
        -- sequence-id. Mandatory: a relayed xApp control that carried no id could
        -- never be matched to the control acknowledge the RAN owes the xApp.
        sequenceId              E3-SequenceID,
        xAppControlData         OCTET STRING (SIZE (1..32768)),
        ...
    }

    E3-ReleaseMessage ::= SEQUENCE {
        dAppIdentifier       E3-DAppID,
        ...
    }

    E3-MessageAck ::= SEQUENCE {
        requestId            E3-MessageID,
        responseCode         E3-ResponseCode,
        ...,
        cause                E3-Cause OPTIONAL,
        detail               E3-Text OPTIONAL
    }

    -- Correlation identifier for one detection-to-mitigation procedure, assigned
    -- by whichever component originates it and propagated unchanged by everyone
    -- downstream. It is what crosses into E2SM-DAPP as sequence-id, so the E2 and
    -- E3 legs of a loop join per message instead of by nearest-in-time pairing.
    --
    -- Same range as E3-MessageID deliberately: both are producer-assigned counters
    -- with the same lifetime, and a shared range means neither can silently
    -- truncate when one is stored in a field sized for the other. Unlike a message
    -- id it is not reassigned per hop.
    E3-SequenceID ::= INTEGER (1..4294967295)

    E3-DAppID ::= INTEGER (1..65535)
    E3-MessageID ::= INTEGER (1..4294967295)
    E3-Name ::= OCTET STRING (SIZE (1..64))
    E3-Version ::= OCTET STRING (SIZE (1..32))
    -- Deliberately not extensible. An extensible ENUMERATED makes asn1c pass
    -- unknown values through as bare integers, which would turn every
    -- response-code switch in the encoders into a partial function. Adding a
    -- response code is a wire change we are willing to pay for explicitly.
    E3-ResponseCode ::= ENUMERATED { positive(0), negative(1) }

    -- Why a RAN refused a request. Sent with a negative response next to a
    -- free-text detail. The names, the values and the text bound are OCUDU's,
    -- so a refusal reads the same on every RAN; the enumeration is extensible
    -- as it is there, and a receiver reads a value it does not know as
    -- unspecified. A negative response is still negative without either field.
    E3-Cause ::= ENUMERATED {
        unspecified(0), protocolError(1), setupRequired(2), setupAlreadyComplete(3),
        unsupportedProtocolVersion(4), unsupportedRanFunction(5), unsupportedIdentifier(6),
        invalidRequest(7), notFound(8), duplicate(9), resourceExhausted(10),
        authorizationRejected(11), payloadTooLarge(12), transportOverrun(13),
        shuttingDown(14), ...
    }

    -- UTF-8 text of up to 512 bytes, carried as an OCTET STRING as E3-Name is.
    E3-Text ::= OCTET STRING (SIZE (0..512))

    -- Nanoseconds since the Unix epoch on the realtime (UTC) clock. Never
    -- subtract it from a monotonic stamp.
    --
    -- Absent means "no producer reference", which is a normal message, not a
    -- malformed one: a producer that does not want the clock read on its data
    -- path simply omits it.
    --
    -- Unconstrained on purpose. asn1c renders this as a native signed integer
    -- and APER length-prefixes it, so the field widens by itself instead of
    -- capping the range, and the codec stays allocation-free on the
    -- indication path.
    --
    -- Do not "tighten" this to INTEGER (0..MAX): a semi-constrained INTEGER is
    -- APC_SEMI_CONSTRAINED, and asn1c 0.9.29's APER decoder has no branch for
    -- that flag and fails every decode. An explicit upper bound works but
    -- forces the arbitrary-precision INTEGER_t and a heap allocation per PDU.
    E3-Timestamp ::= INTEGER

    E3-PDU ::= SEQUENCE {
        id        E3-MessageID,
        timestamp E3-Timestamp OPTIONAL,
        msg       CHOICE {
            setupRequest          [0]  E3-SetupRequest,
            setupResponse         [1]  E3-SetupResponse,
            subscriptionRequest   [2]  E3-SubscriptionRequest,
            subscriptionDelete    [3]  E3-SubscriptionDelete,
            subscriptionResponse  [4]  E3-SubscriptionResponse,
            indicationMessage     [5]  E3-IndicationMessage,
            dAppControlAction     [6]  E3-DAppControlAction,
            dAppReport            [7]  E3-DAppReport,
            xAppControlAction     [8]  E3-XAppControlAction,
            releaseMessage        [9]  E3-ReleaseMessage,
            messageAck            [10] E3-MessageAck,
            ...
        },
        ...
    }

END
-- ASN1STOP
```
