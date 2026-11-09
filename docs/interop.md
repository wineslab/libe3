# Interoperability

libe3 aims to steer the development of E3AP and to keep the RAN stacks aligned with each other, not
only with libe3. In practice:

- The grammar in this repository (`messages/asn1/V1/e3ap-1.0.0.asn1`) is the reference. JSON and
  Protobuf follow it.
- A change to the wire, or to anything a peer can observe, gets a row in the ledger below and a test
  or fixture, in the same PR.
- Where Aerial and OCUDU pull in different directions, libe3 says here which way it goes and why.
  Where libe3 differs from both, it says so too.
- A wire that OAI already runs is not changed without a ledger row that says who has to rebuild.

So far libe3 has moved toward its peers on identifier ranges, setup strings, size bounds, the
`periodicity` range and JSON nesting (see the ledger). It holds two things where OCUDU differs, and
gives the reasons below: the timestamp stays in the root of the envelope, and `E3-ResponseCode` stays
closed. The sections Aerial and OCUDU say what each stack should change to meet libe3 and the other.

What a libe3 program can expect from other E3 implementations, and a ledger of every change that
touched the wire or a peer-visible behavior. This page is the place to look first when libe3 and
another stack disagree.

The peers:

- **OAI**: the OpenAirInterface E3 agent and the dApp library.
- **Aerial**: NVIDIA `aerial-sample-apps` (dapps v1.1.0, `99b10eb`) and its agent in
  `aerial-cuda-accelerated-ran` (`4f65f97`).
- **OCUDU**: the OCUDU dApp platform (DeepSig, `38fc00bb526c`).

The open differences are tracked in [#84](https://github.com/wineslab/libe3/issues/84), one issue
each. This page does not copy them; it says what is settled and what changed.

How sure a statement is:

- **CI** means a test in this repository runs it, libe3 to libe3.
- **Source** means it was read from the other project's code at the commit above and has not been run.
- **Not run** means nobody has tried it.

No peer is run in CI. What CI does check is messages the peers wrote, and the bytes libe3 writes
(see Conformance fixtures below); every other cell that names OAI, Aerial or OCUDU is *Source* or
*Not run*.

## What pairs with what

| Peer | Link and transport | Encoding | libe3 role against it | Status |
|---|---|---|---|---|
| libe3 | ZMQ or POSIX, over IPC or TCP | ASN.1 | either | CI (`E2E dApp Integration`, `E2E Topologies`) |
| libe3 | POSIX over SCTP | ASN.1 | either | `test_role_pair_sctp` and the SCTP cases of the connector tests, on a host with the sctp kernel module |
| libe3 | ZMQ or POSIX, over IPC or TCP | JSON, Protobuf | either | CI unit and equivalence tests; the live example runs are manual |
| Aerial | ZMQ | JSON | dApp | CI for the messages: all 17 of Aerial's published examples decode. Source for the rest. Aerial's own dApps can use libe3 as a backend (`e3_backends.md` in `aerial-sample-apps`) |
| OCUDU | SCTP | ASN.1 | dApp | Source. SCTP is off by default in libe3 (`LIBE3_ENABLE_SCTP`); libe3's SCTP tests skip on a host without the sctp kernel module. OCUDU's libe3-compatible mode is the 0.1.3 grammar ([#89](https://github.com/wineslab/libe3/issues/89)) |
| OAI | ASN.1 or JSON, chosen at runtime by the `encoding` option of the gNB's E3 configuration (`asn1`, `json` or `protobuf`) | Same | OAI runs the RAN side, libe3 the library underneath | Source. Upstream OAI accepts only `libe3>=0.0.10, <0.2.0`, so it needs a rebuild for anything since 0.2.0 |

A libe3 RAN agent serves one link and one encoding, set in its `E3Config`
([#85](https://github.com/wineslab/libe3/issues/85)). Agents with different configurations can run at
the same time, each with its own ports or endpoints and `ran_identifier`: the multi-RAN topology job
in CI runs two this way. A RAN that has peers on different links or encodings, such as Aerial (ZMQ,
JSON) and OCUDU (SCTP, ASN.1), creates one agent for each. That wiring belongs to the RAN
application, not to the library, and a peer uses the agent set up for it. Mixed links in one RAN have
not been run: SCTP is off in CI ([#81](https://github.com/wineslab/libe3/issues/81)).

Defaults when nothing is set: ZeroMQ uses the setup, subscriber and publisher ports 9990, 9999 and
9991 and the IPC endpoints `/tmp/dapps/setup`, `dapp_socket` and `e3_socket`. The POSIX link uses one
port, 9990, and one IPC endpoint, `/tmp/dapps/e3` (`E3Config` in `include/libe3/types.hpp`).

## JSON as libe3 speaks it

Written by `src/encoder/json_encoder.cpp`.

**Envelope.** Every message is one flat JSON object with `type`, `id` and `timestamp`, then the
message's own members at the top level. A message with a nested `data` wrapper is refused. `type` is
the PDU name in camel case: `setupRequest`, `setupResponse`, `subscriptionRequest`,
`subscriptionDelete`, `subscriptionResponse`, `indicationMessage`, `dAppControlAction`, `dAppReport`,
`xAppControlAction`, `releaseMessage`, `controlResponse`.

**Key names** follow the ASN.1 field names: `dAppIdentifier`, `ranFunctionIdentifier`,
`telemetryIdentifierList`, `controlIdentifierList`, `subscriptionTime`, `periodicity`,
`subscriptionId`, `requestId`, `responseCode` (`"positive"` or `"negative"`), `sequenceId`,
`controlIdentifier`, `controlOutcome` (nested like `actionData`), `requestAck` (on a control, written
only when true).

**Opaque payloads are nested JSON, not hex.** `ranFunctionData`, `protocolData`, `actionData` and
`reportData` carry a JSON object or array. Anything else, an empty payload included, fails to encode
with `ENCODE_FAILED`. Before [#118](https://github.com/wineslab/libe3/pull/118) the first,
third and fourth were hex strings; see the ledger.

**Granted values.** A subscription response carries `telemetryIdentifierList`,
`controlIdentifierList`, `ranFunctionIdentifier` and `periodicity` when the RAN reports them.
Absent means not reported; an empty list means none granted. libe3 also reads Aerial's
`telemetryGrantedList` and `controlGrantedList` as aliases. The `periodicity` of a response is
the least time the RAN keeps between two indications of the subscription, in microseconds: Aerial's
agent returns the value it was asked for and skips an indication that comes sooner, and a libe3 Service
Model says what it keeps. A libe3 RAN does not echo the request, so a response without
`periodicity` means the RAN did not say, not that it keeps none. `ranFunctionIdentifier` is for
information and a dApp already has it from its own request. OCUDU's response carries neither.

**The `timestamp` key** is written on every message. It is not in Aerial's schema. Aerial's parsers
read by key, so it is expected to be ignored (not run).

## Field contracts

Where one field means different things to different peers. Add a row when a field is settled.

| Field | libe3 | Aerial | OCUDU |
|---|---|---|---|
| `periodicity` unit | microseconds | microseconds | **milliseconds** |
| `periodicity` range | 0 to 60000000 over ASN.1; any `uint32` over JSON and Protobuf | 0 and up, no maximum | 0 to 60000 |
| `periodicity` 0 | every event | as fast as possible, every SRS slot | every event |
| `periodicity` default when unset | the Service Model's own rate | 100000 | not stated |
| `periodicity` in a subscription response | the least time the Service Model keeps between two indications, when it says so; absent otherwise | the requested value (100000 when none), which the agent also uses as a gate | not carried |
| dApp id and subscription id | 1..65535; the RAN hands out the lowest free one | counts up from 1, no cap | dApp ids 1..100 reserved for libe3-compatible peers, 101..65535 native |
| Message id | 1..4294967295; a peer's id in a setup request is echoed unless it is 0 | one counter from 1, shared by every message type | 1..4294967295 (its libe3-compatible codec: 1..1000) |
| Setup names (`dAppName`, `vendor`, `ranIdentifier`) | `OCTET STRING`, 1..64 bytes over ASN.1; any length over JSON and Protobuf | JSON string | `OCTET STRING`, 1..64 bytes |
| Setup versions (`e3apProtocolVersion`, `dAppVersion`) | `OCTET STRING`, 1..32 bytes over ASN.1; any length over JSON and Protobuf | JSON string | `OCTET STRING`, 1..32 bytes |
| Telemetry and control id lists | 0..256 entries over ASN.1; any length over JSON and Protobuf | JSON array, no limit | 0..256 entries |
| `ranFunctionData` | 1..262144 bytes on the wire over ASN.1, limited to 1..32768 | JSON array or object, no limit | 1..262144 bytes |
| RAN functions in a setup response | 0..64 over ASN.1; the RAN refuses to register a 65th | JSON array, no limit | 0..64 |
| `subscriptionTime` | 0..86400 seconds over ASN.1, any `uint32` over JSON and Protobuf; 0 or absent is no limit. The RAN removes the subscription when the time is up and sends nothing; the dApp forgets it too, counted from the response | Seconds, no maximum; 0 is no limit. The agent removes the subscription within a second of its time and sends nothing; the dApp manager forgets it on its own clock | 0..86400 seconds; decoded and not acted on |
| `timestamp` in the `E3-PDU` | In the root, before the message: `{ id, timestamp OPTIONAL, msg, ... }`. Nanoseconds on the realtime clock | None in the schema; libe3's key is expected to be ignored | Native module: an extension after the marker, `{ id, msg, ..., timestamp OPTIONAL }`. Its libe3-compatible codec: libe3's order. Stamps only peers whose setup carried one |

libe3 keeps the timestamp where it is ([#114](https://github.com/wineslab/libe3/issues/114)). The two
placements encode every stamped PDU to different bytes, so OCUDU's native module and libe3 cannot read
each other's stamped PDUs, and the move belongs on OCUDU's side. libe3's layout is the one OAI runs, and
moving it is a wire change that libe3, OAI's E3 agent and the dApp library would have to rebuild for
together. OCUDU's libe3-compatible codec already uses it. Nothing changes in libe3, so there is no
ledger row; the ask is in the OCUDU table below.

Because OCUDU reads the number as milliseconds, a libe3 dApp that asks an OCUDU RAN for 100000
(100 ms in libe3 and Aerial) asks for 100 s, which is out of OCUDU's range and rejected. libe3 does
not convert: it passes the number through, and the caller facing OCUDU passes milliseconds. OCUDU's
libe3-compatible codec does not convert either (it passes the value through and caps it at 1000).
Converting on the OCUDU side is the agreed direction ([#98](https://github.com/wineslab/libe3/issues/98)).

OCUDU decodes `subscriptionTime` and never acts on it. That is not a conflict with libe3 or Aerial, which
only differ from it in what happens next, but a dApp must not rely on expiry against an OCUDU RAN: a
subscription it asked to last 60 s lasts until the dApp deletes it or is released. Until OCUDU
changes (see its table below), a dApp that needs the subscription to end has to end it itself, and
one that talks to all three can send the time and also delete the subscription when it is done
([#95](https://github.com/wineslab/libe3/issues/95)).

Neither libe3 nor Aerial tells the dApp when a subscription runs out. The only message that could,
`E3-ReleaseMessage`, carries just a `dAppIdentifier`, so it ends the whole session and cannot name one
subscription; Aerial's agent sends it only to a dApp that has been idle for 1800 s with no subscription.
A dApp that sent a time knows when the RAN will let go. libe3's dApp keeps its own deadline, counted from
the response, so it is never earlier than the RAN's and a dApp that renews the moment it passes
always reaches the RAN after the old subscription is gone. Aerial's manager counts from the request
instead. A libe3 RAN answers a delete for a subscription that has run out `negative`, as it does
for any unknown subscription.

## RAN function ids

A function entry in the setup response carries `ranFunctionIdentifier`, the telemetry and control id
lists and an opaque `ranFunctionData`. It has no name and no version, so on the wire the id is the only
thing that says which service model a function is. libe3 does not interpret it.

| Id | Service model | Served by |
|---|---|---|
| 1 | Spectrum | OAI |
| 2 | NVIDIA L1 KPM | Aerial's agent, which accepts no other id, and OAI, whose L1 KPM is built to be wire-compatible with it and advertises a subset of the telemetry ids (1, 4, 5 and 6 of Aerial's 25) |
| 10, 11 | Telemetry, management (`OCUDU-TELEMETRY-SM`) | OCUDU |

Id 2 is one service model on both stacks, not two that happen to share a number, so a dApp written for
Aerial connects to OAI unchanged. The agreement lives in the two codebases, not in the protocol:
nothing on the wire says "NVIDIA L1 KPM, version 1", and libe3 cannot check it. OCUDU's entry has an
optional `ranFunctionName` and `ranFunctionVersion` for this. libe3's has neither, and
`RanFunctionDefinition::sm_name` and `sm_version` are declared but never sent. Carrying a name and
version would let a dApp verify the id, and tell apart two services that pick the same number later.
It is not done; [#87](https://github.com/wineslab/libe3/issues/87) records it. Until then a dApp should
treat an id as an agreement with the RAN's vendor, not as something it can verify.

All rows are *Source*, not tested: OCUDU at dApp platform `38fc00bb526c`, Aerial and OAI at the commits
cited on [#87](https://github.com/wineslab/libe3/issues/87). The telemetry ids OAI advertises are as
stated there.

## Conformance fixtures

What CI checks against messages or bytes that did not come from the code under test. Added in
[#102](https://github.com/wineslab/libe3/issues/102).

**Aerial, JSON.** `tests/fixtures/aerial/e3_message_examples.json` is NVIDIA's own example file,
copied unchanged from aerial-sample-apps `99b10eb` (`tests/fixtures/README.md` says how to update it).
`tests/test_interop_aerial.cpp` decodes all 17 messages in it and re-encodes each. They cover nine of
libe3's twelve message types; Aerial publishes no `dAppReport`, `xAppControlAction` or `controlResponse`.
The differences the test pins, so neither side can change them silently:

- Aerial sends no `timestamp`; libe3 reads it as 0 and writes one.
- Aerial's subscription response names the granted lists `telemetryGrantedList` and
  `controlGrantedList`; libe3 reads them and writes `telemetryIdentifierList` and
  `controlIdentifierList`.
- A negative reply carries a free-text `message`, and a negative setup response has no
  `ranIdentifier`; libe3 reads the text as `detail` and writes `detail`, with a `cause`, never
  `message`, and reads the missing identifier as empty.
- An indication carries a `subscriptionId` and a `messageAck` a `dAppIdentifier`; libe3 reads neither.
- `ranFunctionData` is a JSON array in Aerial's setup response; libe3 keeps it as it came.

**libe3, ASN.1.** `tests/test_asn1_golden.cpp` pins the APER bytes of one PDU per message type, in the
shape OCUDU keeps (id 42, timestamp 1756800000123456789), so OCUDU can re-pin against them. A change to
those bytes is a wire change and needs a ledger row. The same file checks that the 11 PDUs OCUDU
recorded from libe3 0.1.3 are refused with `DECODE_FAILED`. It pins `controlResponse` at index 11 and
`requestAck` on both controls, and every earlier vector, the ack's included, keeps its bytes.

Not covered: anything from OAI, and any live Aerial or OCUDU agent.

## When the RAN goes away

What a libe3 dApp sees, and what each peer does to cause it. Added in
[#120](https://github.com/wineslab/libe3/pull/120), extended for a libe3 RAN and for the dApp's retry
by [#93](https://github.com/wineslab/libe3/issues/93) and [#121](https://github.com/wineslab/libe3/issues/121).

| Peer | What it does | What the libe3 dApp reports |
|---|---|---|
| Aerial | Sends a `releaseMessage` naming the dApp, for example after 1800 s idle | `RELEASED_BY_RAN` |
| OCUDU | Closes the association (idle 30 s, setup timeout, release, bad PDU); sends no release to a dApp | `CONNECTION_LOST` |
| libe3 | Sends a `releaseMessage` naming a dApp that sent nothing for `dapp_idle_timeout_s` (default 1800 s, 0 never), then removes it | `RELEASED_BY_RAN` |
| Any | Socket closed, link failed, ZMQ heartbeat missed | `CONNECTION_LOST` |

A libe3 RAN counts only what the dApp sends. An indication sent to a dApp that is gone looks the same as
one sent to a dApp that is there. A dApp that only listens
sends nothing, so it is released after the idle time like any other, and so is a dApp that is
subscribed. A POSIX RAN (TCP, SCTP, IPC) also removes a dApp at once when its connection closes
without a release, with nothing sent, there is nobody to send to. ZeroMQ cannot tell its peers apart,
so there the idle time is the only way.

A POSIX RAN ties a dApp id to the connection of the setup request it answered with that id. A later
message that names the id on another connection (a release, a subscription delete, a control, a
report) is dropped with a warning and does not count as a message from the dApp, so one dApp can no
longer end or control another. The tie is made at setup, so a dApp that crashes before it sends
anything else is removed as soon as its connection closes, not left to the idle time.

After a release, a lost link, or a setup that no RAN answered or that the RAN refused, a libe3 dApp
registers again by itself every `reconnect_interval_ms` (default 1000, 0 never) until the RAN
answers, as Aerial's manager does. The new session has a new dApp id and none of the old
subscriptions: the application subscribes again, and learns that the dApp is back from the setup
response handler. A dApp that sent its own `release()` gets no event when the RAN then closes and is
not registered again; nor is one that called `stop()`.

Against OCUDU, which closes the association after 30 s without traffic, a dApp that sends nothing is
closed and registers again about every 30 s, and loses its subscriptions each time.

<a id="control-answer"></a>
## The answer to a dApp control

A dApp control and an xApp control carry `requestAck`. A libe3 peer answers one that asks with one
`E3-ControlResponse`, and the RAN answers a control it refuses itself either way. libe3 sends no
`messageAck` any more: the message is deprecated, stays in the grammar at index 10 (`DEPRECATED TO BE
REMOVED`), and a libe3 dApp still reads one that another RAN sends. Aerial's 26.2 agent answers every
control with a negative `messageAck`, its 25.3 agent with an `e3_control_response`, and OCUDU with two
messages. What the answer means differs ([#111](https://github.com/wineslab/libe3/issues/111)).

| RAN | When the answer is sent | A positive answer means | Where the result is |
|---|---|---|---|
| libe3 | The core answers a control it cannot hand to a Service Model with a negative response, whether the control asked or not: no setup (`setupRequired`), unknown function (`unsupportedRanFunction`), unknown control id (`unsupportedIdentifier`), no running Service Model (`notFound`). Otherwise the Service Model answers through `respond_to_control`, once it knows the outcome, and only if the control asked. A Service Model that returns an error gets a negative `unspecified` answer from the core; one that says nothing sends nothing, and the RAN has no timeout of its own. At most 1024 controls wait for an answer, then a new one is refused with `resourceExhausted` | The Service Model applied the control. In OAI's, once it has been applied | `responseCode`, `cause`, `detail` and `controlOutcome` of the `E3-ControlResponse`; its `requestId` is the id of the control, and it names the dApp, the RAN function and the control |
| Aerial, agent 26.2 | Always, negative, as a `messageAck`: "control actions not implemented" ([`handleControlMessage`](https://github.com/NVIDIA/aerial-cuda-accelerated-ran/blob/4f65f97c1d5f701ce911f7dda8f1b1f3f0c7693c/cuPHY-CP/data_lake/e3_agent.cpp#L1627-L1655)), because no Service Model or application uses controls yet ([comment](https://github.com/NVIDIA/aerial-cuda-accelerated-ran/blob/4f65f97c1d5f701ce911f7dda8f1b1f3f0c7693c/cuPHY-CP/data_lake/e3_agent.cpp#L1501)). Aerial's dApp manager ignores acks | Never sent | None. No control is executed. A libe3 dApp reads the `messageAck`, reports it with a warning and hands it to its deprecated handler (`E3_EVENT_MESSAGE_ACK` in libe3py) |
| Aerial, agent 25.3 | An `e3_control_response` with status `success` once it has read the `sfn` and `slot` the control carries, or `error` if the control does not parse ([`handleControlMessage`](https://github.com/NVIDIA/aerial-cuda-accelerated-ran/blob/654c2491461e8073321ef472b68deb116241e8a6/cuPHY-CP/data_lake/e3_agent.cpp#L978-L1022)) | The agent read the control | The `status` of that response, with the dApp id and the transaction id. It is another message than `E3-ControlResponse`, and libe3 does not read it |
| OCUDU, native | At once and positive, as a `messageAck`, before the Service Model runs. An `E3-ControlResponse` follows | Accepted | The `E3-ControlResponse`: response code, optional cause and detail, `controlOutcome` (the service model's payload) and the same `requestId`, fragmented like other payloads |
| OCUDU, libe3-compatible mode | After the Service Model ran, one `messageAck` | Executed | The `responseCode` of that ack. The cause and the detail text are not sent. A libe3 dApp reads it as an ack, through its deprecated handler |

**What libe3 does.** `requestAck` is absent or false when a control does not ask, and then the
Service Model's answer is not sent, positive or negative. A control that asks gets the answer when the
Service Model gives it, so a dApp that asked and whose Service Model never answers waits for ever and
needs a timeout of its own. A positive response says the Service Model applied the control and may
carry what it reports in `controlOutcome`; it does not say the RAN accepted the control first and
executed it later, as OCUDU's two messages do. A response names its dApp, so on a link that
broadcasts a dApp reads only its own, which an answer with no dApp id could not. An xApp control
that asks is answered by the dApp the same way, in the other direction: the application calls
`send_control_response`, the RAN gets it in a `ControlResponseHandler`, and a dApp with no handler for
xApp controls answers negative with `unsupportedRanFunction`. The RAN has no timeout for that either.

**What libe3 does not claim.** libe3 is not interoperable with native OCUDU for this message, and
nothing here says a libe3 dApp and a native OCUDU RAN exchange it. libe3's `E3-ControlResponse` has
OCUDU's fields in OCUDU's order without `fragmentIndex`, `fragmentCount` and `payloadBytes`, and with
`controlOutcome` bounded to 1..32768 bytes where OCUDU's is 1..65536 ([#110](https://github.com/wineslab/libe3/issues/110)).
libe3 takes no position on fragments or the payload limit; adding the fragment fields later is
another wire change. Measured, not run against OCUDU's agent: OCUDU's grammar at `38fc00bb526c`,
with `E3-TensorDescriptor` and `E3-SharedRegion` cut out because asn1c 0.9.29 cannot compile them,
built with the asn1c libe3 pins, and the same message in both, with no timestamp (id 42, request 42,
dApp 7, RAN function 1, control 1):

| Message | libe3 0.5.0 | OCUDU |
|---|---|---|
| positive | `002958002900060000000000` | `002958002900060000000000` |
| positive, outcome `ca fe` | `0029588029000600000000000001cafe` | `00295880290006000000000002cafe` |
| negative, cause `notFound`, detail "no subscription to this RAN function" | `00295b0029000600000000a000246e6f20737562736372697074696f6e20746f20746869732052414e2066756e6374696f6e` | `00295b00290006000000005000246e6f20737562736372697074696f6e20746f20746869732052414e2066756e6374696f6e` |

The choice index is the same, 11: `messageAck` stays at 10 in libe3's PDU as in OCUDU's, so
`controlResponse` is the 12th alternative of both. A response with no outcome and no cause is the same
bytes. The preamble has 3 optional bits in libe3 and 6 in OCUDU, which has the three fragment fields,
and in these three messages the padding hides it; another message may not. The two differ where the
responses differ: the length of the outcome is sent in 15 bits in libe3 and 16 in OCUDU (`01` against
`02`), and `E3-ResponseCode` is closed in libe3 and extensible in OCUDU
([#115](https://github.com/wineslab/libe3/issues/115)), which moves the cause bits (`a0` against
`50`). The envelope differs as well, in the timestamp ([#114](https://github.com/wineslab/libe3/issues/114)),
and `sequenceId` ([#116](https://github.com/wineslab/libe3/issues/116)) and the asn1c versions
([#109](https://github.com/wineslab/libe3/issues/109)) are still open.

**An older libe3 refuses the response.** Measured in a Linux container, with libe3 0.4.3 and 0.5.0
built from this repository: 0.4.3 has no alternative 11, so it refuses 0.5.0's `E3-ControlResponse`
with `DECODE_FAILED` and reads a control that carries `requestAck` as a control that does not (it
skips the extension addition). In the other direction 0.5.0 reads 0.4.3's `E3-MessageAck` as before.
A dApp of 0.4.3 therefore hears nothing from a 0.5.0 RAN where it used to hear an ack, and a 0.5.0
dApp gets no answer from a 0.4.3 RAN to a control that asked, but the old ack for a refusal.

**Still open.** The fragment fields and the payload limit (#110), the timestamp (#114), the response
code (#115), `sequenceId` (#116) and the asn1c versions (#109), which are the rest of what stands
between libe3 and native OCUDU, and whether Aerial's agent and OCUDU's libe3-compatible mode answer
a control with an `E3-ControlResponse`, asked for in the tables below.

## Known differences

All open ones are sub-issues of [#84](https://github.com/wineslab/libe3/issues/84), which also holds
the table of places where Aerial and OCUDU pull in different directions. Read that for the evidence;
the next two sections say which way libe3 goes.

## Found while writing the specifications

Differences between the grammar, the encoders and the code that came up while writing the
specifications in [e3ap](e3ap/README.md), [e2sm-dapp](e2sm-dapp/README.md) and
[e3sm](e3sm/README.md). This page is where they are kept. The specifications do not list them, and
neither do their Word editions, because they change with the code and not with the specification.
Each row is *Source*: read from the code, not run. Open a #84 sub-issue for a row when someone picks
it up.

Differences between peers that already have their own place on this page (units, id ranges, the
version string, the SCTP mapping, the asn1c build, OCUDU's optional additions) are in the tables above
and below, and are not repeated here.

### E3AP

libe3, the grammar and the three encodings. The last column is what the specification expects.

| Where | What differs | Effect | Expected resolution |
|---|---|---|---|
| Setup, dApp side | A negative `E3-SetupResponse` still counts as a finished setup. `wait_for_setup` returns success and the dApp has no id. The response's `requestId` is not compared with the request. A dApp with no id after setup tries again every `reconnect_interval_ms` | The application sends messages the RAN cannot match to a dApp. A late response to an older attempt completes the current one | Report a failed setup on a negative response. Compare the two ids and drop a response that does not match |
| Setup, RAN side | The dApp's `e3apProtocolVersion` is logged and never compared. Aerial accepts only the exact string `1.0.0`, OCUDU any string that starts with `1.` | A version string cannot flag a wire change. A dApp that works with libe3 can fail against Aerial | Accept any string that starts with `1.`. Use the string to flag a wire change |
| Setup, RAN side | A second request from the same dApp registers a new dApp. The RAN has no identity check | A retry after a slow reply leaves an id with no owner. One id leaks until a release names it, or until the idle time removes it | Let the dApp send a release for an id it did not use, or give the dApp an identity in the request |
| Setup, RAN side | A negative reply for "no free dApp id" carries `ranFunctionList`. A negative reply for a bad request does not | Two shapes of "negative" | Send no `ranFunctionList` in any negative reply |
| Setup, RAN side | An encode failure of the setup response ends the process | One bad RAN function can stop the RAN node. Sizes are checked first, other failures are not | Leave the failing function out and answer. libe3 does so for the sizes it checks |
| Subscription | A repeat returns the existing subscription in libe3, with the lists and interval it holds. Aerial's dApp manager refuses it before sending it. Aerial's agent and OCUDU's agent add a second subscription for every request, OCUDU up to a limit per dApp and then `resourceExhausted` ([#97](https://github.com/wineslab/libe3/issues/97)) | A dApp that subscribes twice gets three different results, and a repeat uses up one of its subscriptions on Aerial's agent and on OCUDU | Subscribe once, and delete before subscribing again, which behaves the same on all three. The agents return the existing subscription, as libe3 does |
| `E3-MessageAck` | It has no `dAppIdentifier`, and the RAN sends it to every dApp. The dApp handler has no filter | A dApp can read another dApp's acknowledge | Deprecated: libe3 sends none. The `E3-ControlResponse` has a `dAppIdentifier` and a dApp reads only its own. Compare `requestId` with the messages the dApp sent |
| xApp control | `E3Agent::send_xapp_control` does not refuse `sequence_id` 0. It fails later, in the encoder | The error is far from the call | Refuse the value at the call |
| dApp report | No check of the dApp id, the function or the sequence id on the RAN | A bad report reaches the E2-E3 bridge | Validate the report before it leaves the RAN |
| Release | A libe3 RAN sends an `E3-ReleaseMessage` only to a dApp that sent nothing for `dapp_idle_timeout_s`. A dApp whose connection closed gets none, and OCUDU sends none ever | A dApp learns of the end from a release, or from a lost link | Send a release before the RAN drops a dApp, where the link is still there |
| Release | OCUDU closes the association after 30 s idle and never sends a release. Aerial sends one after 1800 s idle, and only to a dApp with no subscription. libe3 sends one after 1800 s of silence from the dApp, whatever it subscribed to | The reason reported to the dApp differs. A dApp that only listens is kept by Aerial and dropped by libe3 and OCUDU | Agree one rule for idle dApps |
| Release | A release is not authenticated by the protocol. A POSIX libe3 RAN binds a dApp id to the connection of its setup request and drops a message that names it on another. Over ZeroMQ, and in OCUDU and Aerial's agent, any dApp can name another dApp | Over ZeroMQ a dApp can end another dApp | Bind the id to the connection that answered setup, on a link that has connections to bind |
| POSIX framing | SCTP and IPC (a sequenced-packet socket) carry one PDU per message with no prefix. TCP carries a 4-octet length prefix in both directions. One limit, 65536 octets, applies to every transport and direction (chapter 04, "Framing on each link") | A peer that frames another way is read wrongly: TCP waits for a prefix that IPC and SCTP never send | One rule for each transport, in chapter 04 |
| ZMQ framing | A ZMQ message larger than the receive buffer is cut by the library | The receiver may read a short buffer as a whole message | Refuse a message that does not fit |
| POSIX link | No heartbeat and no keepalive. ZMQ has a heartbeat on the dApp side. A libe3 RAN ties a dApp to its connection when it registers and removes it when the connection closes, whether or not it sent anything after its setup request. A ZMQ dApp stays until the idle time | Over ZMQ a dApp that crashes keeps a service model running for up to the idle time. A POSIX peer that keeps its connection open and says nothing is not noticed before then | A heartbeat or a keepalive, if a link needs one |
| `ranFunctionData` | 1..262144 octets in the grammar, 1..32768 in libe3. OCUDU allows 262144 for `ranFunctionData` only; the payload fields stay 1..32768 | A peer's larger value is refused | Raise libe3's limit, or lower the grammar. The grammar is the reference. Keep 32768 for the payload fields |
| Ranges | JSON and Protobuf do not check ranges. APER does | A message that works in JSON can fail to encode in APER | Check the ranges in every encoder |
| Protobuf | A response code that is not `NEGATIVE` reads as `POSITIVE`. APER and JSON read an unknown value as negative | An unknown code is a success in one encoding | Read an unknown code as negative everywhere |
| JSON | `timestamp` is written even when 0. APER and Protobuf leave 0 out. A missing member decodes as 0 or an empty string, not as an error | A receiver that reads the key by name sees 0. A malformed message can look valid | Treat 0 as absent in every encoding. Refuse a message that lacks a mandatory field |
| Build | A build with only Protobuf defaults to the ASN.1 encoding | The default config does not start | Choose the encoding explicitly |
| Config | `connect_timeout_ms`, `recv_timeout_ms`, `send_timeout_ms`, `receive_buffer_size` and `send_buffer_size` are not read anywhere | Setting them does nothing | |

### E3 service models

Spectrum (E3SM-SPEC, RAN function 1) and L1-KPM (E3SM-L1-KPM, RAN function 2).

| Where | What differs | Effect |
|---|---|---|
| Spectrum grammar | The two OAI copies differ. The first lacks the apply-outcome types and `maxSlotIndex`, yet the xApp-side build file says all copies are byte identical. The revisions are in the table below | A codec built from the first copy cannot carry the outcome |
| L1-KPM telemetry ids | The identifiers (1 for the IQ reference, 4 to 6 for time stamp, frame and slot, 2 and 3 unused) are defined by Aerial's dApps, not by the grammar. A comment in the RAN code calls them "bit position plus one", but no code uses a bit mask. The L1-KPM specification does not adopt that convention | The numbers have no owner in this repository. To clear with Aerial: which side defines them, and what 2 and 3 mean |
| Spectrum telemetry ids | The RAN advertises ids 2 to 5. The indication does not depend on them | An id a dApp subscribes to does nothing |
| Spectrum JSON | The report and the xApp control have no JSON codec. The xApp side reads and writes APER only. The specification now requires the JSON form. The dApp control follows the encoding of the run (verified) | A run with JSON delivers binary octets in a field that holds nested JSON. A deployment with an xApp has to use APER for the whole run, until the codecs are added |
| Spectrum JSON | The indication nests `sensing_shm{shm_name, write_idx, n_ranges}`. ASN.1 uses `shmName`, `shmWriteIdx`, `nRanges`. Protobuf uses `shm_name`, `shm_write_idx`, `n_ranges`. `timestamp`, `sfn`, `slot` and `beam` stay at the top level in all three | One field, three spellings. A JSON consumer written from the grammar does not find its keys |
| Spectrum JSON | The cell configuration indication writes every field in lower case with underscores (`ul_carrier_prbs` for `ulCarrierPRBs`) and has no Protobuf form. A JSON indication holds the member `indicationType`, which is not in the grammar. A Protobuf indication is a sensing indication with no wrapper | Nothing in a Protobuf indication tells the two kinds apart |
| Spectrum codecs | `Spectrum-PRBBlockedControl` has an xApp encoder and a dApp decoder, and no other xApp control alternative has one. The apply outcome has a RAN encoder and an xApp decoder, in APER only. The dApp report has an xApp decoder in APER only | The other forms do not exist, in any encoding |
| Spectrum controls | `samplingThreshold` and `validityPeriod` are decoded and never used: the PRB block ignores `validityPeriod`, the sensing policy writes it to a log line. A block or a policy does not expire. APER range-checks `validityPeriod` (1..3600000), JSON and Protobuf do not. PRB index 0..272, PRB list at most 273, symbol mask 0..16383 and mask list 1..160 are checked in all three | A dApp that relies on a block to expire after `validityPeriod` ms is wrong and has to clear the block itself. A value of 5000000 is refused by APER and accepted by JSON and Protobuf. Today the RAN ignores the field, so nothing shows; it will when the RAN starts to use it |
| Spectrum controls | `Spectrum-ConfigControl` (`noiseFloorThreshold`, `averagingFrames`, `enable`) has no encoder, decoder or handler on the RAN, the dApp or the xApp side. The unit of `noiseFloorThreshold` is not defined anywhere | The xApp control alternative exists on the wire only |
| Spectrum controls | A "deactivate" sensing policy still needs one mask value, because `maskPerSlot` is `SIZE (1..160)`. An empty list is rejected in all three encodings | A policy cannot be cleared with an empty mask; a dApp usually sends the list it used before |
| Spectrum fields | `sfn` is 0..65535 in the sensing indication (a frame number has 10 bits; the apply outcome uses 0..1023). `slot` is 0..65535 (at most 159; the apply outcome uses 0..159). `shmWriteIdx` is 0..65535 and the ring has 256 slots. `beam` is 0..3 and optional, and the RAN always sends it; what a RAN does with an index above 3 is not stated, an APER encoder is expected to refuse it, and no test of this case was found. `shmName` has no size limit here, the L1-KPM shared memory reference limits its name to 1..63 octets | Wider fields cost bits in APER and hide errors. They are not wrong |
| Spectrum dead code | The RAN clamps the number of listed PRBs to 275, the size of the largest bandwidth part, and ignores a PRB index of 275 or more when it builds the block mask. The grammar limits the list to 273 entries and the index to 272 | Neither check can trigger |
| Spectrum revisions | The apply outcome needs the E3AP sequence id in the control. A RAN of the first revision does not pass the id to the service model, so it cannot report an apply outcome for the procedure. The pending procedure table and its timeout exist in the later revision only | An xApp that waits for the outcome gets the answer after the timeout, with no timestamps |
| L1-KPM Protobuf | `L1KPMRanFunctionData` exists and is unused. A Protobuf session carries JSON text in `ranFunctionData` | A Protobuf dApp reads the wrong type |
| L1-KPM grammar | `cellId`, `nRxAnt` and `validSymbolMask` are `OPTIONAL`, but the RAN always sends them. `cellId` is always 0 | A peer cannot tell "no cell" from cell 0 |
| L1-KPM `timestamp` | Unconstrained integer in ASN.1, `int64` in Protobuf, signed in the codec | Large values can differ across encodings |
| L1-KPM shared memory | The rows carry no marker, and the header has no FP16 scale. A source comment claims about 64 ms before a row is reused. The code has 32 rows, so the time depends on the uplink slot rate | A dApp cannot detect an overwrite or recover the scale from the region |
| L1-KPM Protobuf | The header comment names a grammar file that does not exist | A reader looks for a missing file |

**Revisions of the Spectrum grammar.** The grammar exists in more than one copy and revision. The
[E3SM-SPEC specification](e3sm/spectrum/README.md) describes the latest grammar without the sensing
reservation, the PUSCH slots and the sampling threshold.

| Item | First RAN revision | Later revision, used by the RAN, the dApp and the xApp side | E3SM-SPEC |
|---|---|---|---|
| Apply outcome (`Spectrum-PRBBlockApplyOutcome`, `Spectrum-ApplyOutcomePayload`, `Spectrum-ApplyOutcomeData`) | absent | present | present |
| constant `maxSlotIndex` | absent | 159 | 159 |
| content of `protocolData` | `Spectrum-SensingIndication` | `Spectrum-Indication`, with the cell configuration indication | `Spectrum-Indication` |
| alternatives of the dApp control | the PRB block in the root, the sensing policy as an extension | as the first revision, plus the sensing reservation and the PUSCH slots as extensions | as the first revision |
| alternatives of the xApp control | `prbBlockedControl` and `configControl`, both in the root | the same | the same |
| optional fields of the PRB block control | `samplingThreshold` and `validityPeriod` | the same | `validityPeriod` only |
| unit and range of `validityPeriod`, in the PRB block and in the sensing policy | seconds, 1..3600 | the same | milliseconds, 1..3600000 |

What that means on the wire, checked by encoding the same values with the grammar of the later revision
and with E3SM-SPEC:

* A dApp report, a sensing indication, a cell configuration indication, an apply outcome and an xApp
  control of either alternative have the same octets in both grammars.
* A PRB block control with neither optional field has the same octets in both. A control with one of them
  does not: E3SM-SPEC has one presence bit where the later revision has two, and its `validityPeriod`
  has a length field. The table below gives examples.
* In Protobuf and JSON, `validity_period` and `validityPeriod` keep their field number and key, and the
  unit changes. A value of 60 from a sender of the later revision means one minute. A receiver of
  E3SM-SPEC reads it as 60 ms.
* In Protobuf, a receiver of E3SM-SPEC skips field 2, `sampling_threshold`, as an unknown field. In JSON
  it ignores the key `samplingThreshold`.
* A sensing policy control without `validityPeriod` has the same octets in both grammars. It is the
  first extension alternative in both. With a `validityPeriod` the field is wider in E3SM-SPEC. The open
  type keeps the error inside the alternative: the later revision reads 60000 ms as 16619, and
  E3SM-SPEC reads 60 s as 60 ms.
* A sensing reservation or PUSCH slots control of the later revision is an extension alternative that
  E3SM-SPEC does not know. A receiver rejects it ([clause 9](e3sm/spectrum/05-semantics-and-constraints.md)).
* The indication of the first revision is a bare `Spectrum-SensingIndication`. A receiver of the later
  revisions decodes `protocolData` as `Spectrum-Indication`, so the two do not interwork.
* A RAN of the first revision cannot send an apply outcome.

| Sent with the grammar of | Optional fields sent | Read by the other grammar as |
|---|---|---|
| the later revision | `samplingThreshold` 5, `validityPeriod` 60 | `validityPeriod` 1 |
| the later revision | `validityPeriod` 60 | no `validityPeriod` |
| the later revision | `samplingThreshold` 5 | a decoding error |
| E3SM-SPEC | `validityPeriod` 60000 | `samplingThreshold` 32, no `validityPeriod` |

Only the third case reports an error. In the others the receiver accepts wrong values.

**Open items for Spectrum.** None of these has an owner yet.

| Item | State |
|---|---|
| What a version bump means. The field `version` is "1" and the ring header has `version` "1". No rule says how a dApp reacts to another value | Not defined |
| What a dApp does with a ring when the RAN stops and starts the service model. The RAN removes the region and creates a new one. A mapping that the dApp holds still points to the old region | Not defined. A dApp should unmap and map again on every new subscription. Not verified |
| The wire values of the telemetry identifiers. A comment says that they follow a convention of the L1-KPM service model, in which the identifier is a bit position plus one | The convention is not a rule of E3SM-SPEC, and no code uses a bit mask. See the L1-KPM telemetry ids row above |
| Unit and range of `noiseFloorThreshold` | Not defined |
| `Spectrum-Indication` has no extension marker. A third kind of indication would change the width of its alternative index | Open. An extension marker added now would change the encoding of every indication once |
| The JSON codecs of the dApp report and of the xApp control | Required by E3SM-SPEC ([encoding](e3sm/spectrum/04-encoding.md#json)) and missing |
| What the decoder reports for an unknown extension alternative in an APER choice. The RAN rejects a control that holds an alternative other than the one for its control identifier | Not tested |
| An end to end test of the sensing indication, the ring and the apply outcome against a second implementation | None found. No golden vectors exist for this service model |

### E2SM-DAPP

[E2SM-DAPP](e2sm-dapp/README.md) carries dApp traffic between an E2 node and an xApp. The
specification names the implementations by letter, and so does this subsection:

* **A**, flexric: the original C implementation of the service model, in an E2 agent and a RIC library.
* **B**, libe3: the C++ and C codec that ships with this repository.
* **C**, OCUDU: a C++ RAN stack with a hand-ported codec.
* **OAI**: the RAN bridge described in reference [i.2] of the README.
* **An xApp framework**: a Python framework that wraps flexric.

File names, line numbers and code roots for each entry are in the
[integrator guide](e2sm-dapp/integrator-guide.md) and [code pointers](e2sm-dapp/code-pointers.md).
Nothing here is a design decision; those are in the specification.

Severity: **abort** means an assertion or equivalent takes the process down in a build with assertions
on. **wrong** means a wrong result with no error. **latent** means only a corner case triggers it.
**doc** means documentation only.

**1. Dead code**

| # | What | Notes |
|---|---|---|
| 1.1 | The plain and FlatBuffers encoders and decoders of flexric | All 31 functions fail with "not implemented". The build allows only the ASN.1 encoding. A build branch for the plain encoding would still build the stubs. A branch for FlatBuffers stops the build. The generic dispatch macros still point to them. **doc** |
| 1.2 | Unused includes of the headers of other service models in flexric's codec | Nothing from them is used. Building the codec alone requires deleting them |
| 1.3 | `Bool-Type ::= BOOLEAN` | Defined and referenced nowhere. The ASN.1 compiler still generates code for it |
| 1.4 | A structure with a pointer to a control outcome in flexric | Used nowhere |
| 1.5 | A RIC Service Update structure in the read union of flexric | Nothing fills or reads it for E2SM-DAPP. The RIC Service Update goes through the generic path |
| 1.6 | The sweep of pending controls in flexric | **Nothing calls it**, in the library or in OAI. The 200 ms safety net for an unanswered deferred control never runs. The only timeout in effect is the 150 ms timeout of OAI. **latent** |
| 1.7 | The function that sends a RIC Control Failure in flexric | Nothing calls it. A control for E2SM-DAPP always ends in a RIC Control Acknowledge. An xApp framework registers failure handlers that a flexric RAN never triggers |
| 1.8 | The handlers of the RIC Service Update in the agent and in the RIC of flexric | They are stubs. The agent comment says that the RIC Service Update is not used for this service model, yet the RAN sends one. The stub is the hook for *receiving* one. Sending does not go through it. **doc** |
| 1.9 | Commented-out code in an xApp framework | Dead branches left in place |

**2. Where the grammar and the implementations disagree**

| # | What | Effect |
|---|---|---|
| 2.1 | **`data-size` duplicates the octet string length** in `IndicationMessage-Format1` and `ControlMessage-Format1`. flexric writes its own `data_size` into both the ASN.1 field and the octet string length. On decode it takes the size from the octet string and ignores the ASN.1 field. libe3 and OCUDU keep the ASN.1 field as its own value on encode and decode. They check neither against the octet count | A sender that writes different values is read as the octet length by flexric and as the field by libe3 and OCUDU. **wrong**. Always send equal values |
| 2.2 | The grammar allows **0..256** dApps and **1..64** functions per dApp. flexric accepts fewer. Its decoder, its encoder for the report and control item lists, and its list helpers assert on 256 dApps and on 64 functions. The generator of the reference test vectors documents this | A list of 256 dApps, a list of 0 items in a definition, or a dApp with 64 functions **aborts** flexric. The indication message format 2 path tolerates an empty list. It does not tolerate 256 items on decode. **abort** |
| 2.3 | The definition item lists. flexric checks `0 < n < 64`. The grammar allows 1 or 2 report styles and 1 control style | flexric accepts what the grammar forbids and leaves the rejection to the ASN.1 runtime |
| 2.4 | `ranFunction-Instance` is in the grammar and unusable in flexric. Its encoder asserts that the instance is absent. Its decoder never reads it | A definition with an instance is lost on decode and cannot be encoded. libe3 and OCUDU support it |
| 2.5 | `RIC-Style-Type` and `RIC-Format-Type` are unbounded `INTEGER`. flexric stores 32 bits | A style above `4294967295` or a negative style cannot be represented. libe3 and OCUDU hold 64 bits. **latent** |
| 2.6 | `Timestamp` and `SequenceId` are unbounded `INTEGER`. Every implementation holds 64 bits | A value that needs more than 8 octets cannot be decoded. OCUDU rejects it. libe3 checks that it fits |
| 2.7 | `node-plmn-id` is `OCTET STRING (SIZE(3))`, not the 3GPP `PLMN-Identity`. OAI writes the digits as three shifted values. This drops a digit of a three-digit MNC and is not BCD | A consumer cannot read the PLMN without knowing the packing of the RAN. The grammar does not define a packing |
| 2.8 | The dApp E3 subscription list may be empty (`SIZE(0..256)`). A **subscription item** needs at least one function. The report and control item path of flexric needs at least one item | OAI omits the field when no dApp has a function. An empty list in indication message format 2 is legal and works |
| 2.9 | E2SM-DAPP ids are `0..4294967295`. E3AP ids are `1..65535` | Values that E3 cannot carry are legal in E2SM-DAPP. The reference test vectors include them |
| 2.10 | `SequenceId` is unbounded. `E3-SequenceID` is `1..4294967295`. The control path has no range check. flexric converts a signed value to an unsigned key. OAI truncates it to 32 bits | An id above `2^32 - 1` or a negative id makes the parked key and the completion key differ. The relay never completes the control. The 150 ms timeout of OAI retires it. **latent** |
| 2.11 | The OID text. The grammar and the string use `e2sm(255)`. One comment uses `e2sm(2)` | Stale comment. **doc** |
| 2.12 | The grammar file in flexric has a `v0` in its name and its module is version 1 | The `v0` does not mean a version 0 grammar. The grammar files of flexric and libe3 are byte-identical. **doc** |
| 2.13 | The control header carries no style number | This works while there is one control style. A second control style needs a new header format |
| 2.14 | The control outcome carries no `ran-function-id` | The xApp shall remember the E3 RAN function per `sequence-id` to decode the payload. See [02-ie-mapping.md](e2sm-dapp/02-ie-mapping.md#73-e2sm-dapp-controloutcome) |
| 2.15 | An absent `timestamp` or `sequence-id` is not an error in the grammar. In flexric, `0` means absent | flexric cannot send a present `0`. See [05-semantics-and-constraints.md](e2sm-dapp/05-semantics-and-constraints.md#4-optional-fields-absent-is-not-an-error) |

**3. Aborts and error handling in flexric**

| # | What |
|---|---|
| 3.1 | Every decoder asserts a successful decode. A malformed or truncated PDU aborts the process |
| 3.2 | The encoders of the event trigger, action definition, control header, control message, control outcome and definition assert instead of returning an error. Only the encoders of the indication header and message return an empty buffer |
| 3.3 | A control request without "ack requested" aborts the E2 agent |
| 3.4 | A subscription with more or fewer than one action, or with an insert or policy action, aborts the agent |
| 3.5 | An unknown report style. OAI returns an empty answer, then an assertion of the agent aborts |
| 3.6 | A deferred control with key `0` aborts the agent. The service model defers on whatever `sequence-id` the header holds, and the table asserts that it is not `0`. OAI avoids this by answering at once when the id is `0` |
| 3.7 | The inner decode on the RIC side asserts. A format 1 indication whose `ran-function-id` has no decoder aborts the xApp. Only id `1` (Spectrum) is known. An unknown indication format aborts too |
| 3.8 | The control path on the RIC side aborts when it cannot encode the inner control payload. It returns an **empty** request without error when the E3 type is "none" or the message already carries data |
| 3.9 | The RIC Service Update handler on the RIC side does not decode the new definition |
| 3.10 | The first RIC Service Update repeats the revision of the E2 Setup. The trigger also finds the service model by the literal `255`. **wrong** |
| 3.11 | A comment says that unanswered controls are acknowledged "negatively". The sweep sends a RIC Control Acknowledge with no outcome. **doc** |
| 3.12 | Two parked controls with the same key. A completion releases one. The other waits for a sweep that nothing runs (see 1.6) |
| 3.13 | A fixed encode buffer means that large elements abort. The sizes are 512 KiB for the event trigger, 1 KiB for the action definition, 1 KiB for the indication header, 128 KiB for the indication message, 16 KiB for the control header, control message and control outcome, and 16 KiB for the definition. A control message above about 16 KiB aborts although the grammar allows 32768. The reference test vector for a large control message stops at 16000 octets. A definition with the map for 256 dApps of 64 functions each (about 83 KiB per copy, listed twice) does not fit either |
| 3.14 | The clock helper returns `-1` on error and OAI uses it unchecked |
| 3.15 | OAI does not clamp the dApp list to the grammar. More than 256 dApps, or a dApp with more than 64 functions, would reach flexric's encoder |
| 3.16 | The equality helper of the event trigger compares the format of the first argument only. **latent** (there is one format) |

**4. Consumers**

| # | What | Effect |
|---|---|---|
| 4.1 | The wrapper of the control outcome in an xApp framework has an open "to do" and **no free binding** for the outcome. The likely reason is that the library hides symbols by default and the free function for the outcome is not exported, unlike the free functions of the indication header, indication message and definition | The `e3-control-outcome` buffer leaks at every acknowledge. **latent** |
| 4.2 | An xApp framework mirrors flexric's C structures field by field | The layouts of the library are an **ABI**. A changed field order or type reads the wrong numbers without an error. The decoders return structures by value |
| 4.3 | The mirror of the event trigger definition has two fields. The structure of flexric is empty | Reading a field of a decoded definition that has the event trigger section reads outside the allocation. OAI omits the section, so it does not trigger today. **latent** |
| 4.4 | An xApp framework finds the control style by the name `DAPP-CONTROL-STYLE-1` | A RAN that names it differently is refused. The name is free text in the grammar |
| 4.5 | An xApp framework sends a call process id of `1` in every control request | flexric ignores it. A conforming sender omits it |
| 4.6 | An xApp framework hard-codes RAN function id `255` | |
| 4.7 | A decision engine caches the decoded definition per gNB for its lifetime | The cache is stale after a RIC Service Update. The engine takes the `dapp-id` from reports, so this does not matter today |

**5. Differences between the implementations.** The behavior below differs between flexric (A), libe3
(B) and OCUDU (C) for the same input. The reference test vectors do not detect these differences,
because every vector is a valid message and none sets a present `0` or disagreeing sizes.

| # | Input | flexric (A) | libe3 (B) | OCUDU (C) |
|---|---|---|---|---|
| 5.1 | present `timestamp` or `sequence-id` equal to `0` | encoded as absent | encoded as present `0` | encoded as present `0` |
| 5.2 | `data-size` different from the octet count | encoded as the stored value, decoded as the octet length | encoded and decoded as the field | encoded and decoded as the field |
| 5.3 | extension additions on a `SEQUENCE` | skipped by the ASN.1 runtime | skipped by the ASN.1 runtime | skipped as opaque open types |
| 5.4 | extension bit on a structure to be packed | not applicable | the member is never read when packing. The bit is always `0` | a set member makes `pack` fail |
| 5.5 | a `PrintableString` with a character outside the alphabet | encoded, no check | rejected on encode and decode | rejected on encode, accepted on decode |
| 5.6 | an `INTEGER` of exactly `2^(8k-1)`, for example a `SequenceId` of 128 | correct (two octets `00 80`) | correct | handled by a local routine. The generic helpers of its runtime size it wrongly |
| 5.7 | an empty subscription list in a definition | **abort** | allowed | allowed |
| 5.8 | a dApp item with 0 functions | the ASN.1 encoder rejects it | rejected on encode and decode | rejected on encode and decode |
| 5.9 | the unbounded octet string above 16383 octets | not reachable (16 KiB buffer) | handed to the ASN.1 runtime, untested | rejected on encode |
| 5.10 | malformed input | **abort** | decoding failure code | decoding failure code |
| 5.11 | the format number | enumeration from `0` | `to_number()` from `1` | `to_number()` from `1` |

OCUDU's E2 module against flexric and OAI:

| # | Item | OCUDU | flexric and OAI |
|---|---|---|---|
| 5.12 | RAN function revision in the E2 Setup Request | `0`, although its service model class holds `1` | `1` |
| 5.13 | dApp E3 subscription map in the definition | never present ("dynamic, so not part of the definition") | present in report style 1 and control style 1 when any dApp has a function |
| 5.14 | RIC Service Update for E2SM-DAPP | never sent | sent when the dApp set changes |
| 5.15 | Style names | `E3 Data Report`, `E3 Subscription Map`, `dApp Control` | `DAPP-E3-DATA-REPORT`, `DAPP-E3-SUBSCRIPTION-MAP`, `DAPP-CONTROL-STYLE-1` (the name an xApp framework requires) |
| 5.16 | Event trigger section in the definition | present | absent in OAI |
| 5.17 | `timestamp` of the control outcome | echoes the **control header's** timestamp | the time the outcome was built, on the gNB clock |
| 5.18 | Outcome on failure | RIC Control Failure with the cause of the sink, outcome included | RIC Control Acknowledge only. No failure path exists |
| 5.19 | Control without "ack requested" | applied, not answered | flexric's agent aborts |
| 5.20 | `data-size` not equal to the octet count | an indication is dropped. A control is refused with `ctrl_msg_invalid` | not checked. The ASN.1 field is ignored on decode |
| 5.21 | Reporting rate | one indication per subscription per 10 ms poll | event driven, one indication per report |
| 5.22 | Size caps | indication header plus message at most 16319 octets. `e3-control-outcome` at most 16287 octets. The E2AP codec cannot carry 16384 octets or more | the fixed buffers of flexric (section 3) |
| 5.23 | Where it is enabled | the DU-high only, by a configuration flag | not applicable |
| 5.24 | An unknown report style | not admitted in the RIC Subscription Response | aborts (3.5) |

Limits of the shared ASN.1 helpers of OCUDU, found while its authors hand-ported the grammar. They
affect any other generated E2 service model there and are listed for whoever maintains that runtime.
The E2SM-DAPP codec does not depend on them.

| # | Helper | Problem | What the E2SM-DAPP codec does |
|---|---|---|---|
| 5.25 | the packer for an unconstrained integer | It sizes the values `2^(8k-1)` wrongly. `128` goes out as the single octet `0x80`, which a conforming peer reads as `-128`. Decode zero-extends, so negative values come back wrong | A local two's complement encoder and decoder. `128` encodes as `02 00 80` |
| 5.26 | the packer for an 8-bit integer with upper bound 255 | It treats the field as unbounded and writes nothing | `node-type` is held in a 16-bit integer while packing |
| 5.27 | the integer unpacker, extension path | It can reach an assertion on a long length read from the wire | Out-of-root values are rejected before that path |
| 5.28 | the string unpacker | It ignores the result of its length decode, so an out-of-range length is accepted. There is no alphabet check | A wrapper checks the size on decode. It checks the size and the PrintableString alphabet on encode |
| 5.29 | the unbounded octet string, length packer and unpacker | They cannot produce or parse OCTET STRINGs of 16384 octets or more | The codec refuses to emit such payloads (see 5.22) |

**6. What the reference test vectors prove and what they do not**

* They prove that libe3 and OCUDU **encode** every listed input to the exact bytes of flexric and
  **decode** those bytes back (see the
  [integrator guide](e2sm-dapp/integrator-guide.md#the-contract-the-golden-vectors)).
* flexric produces them, so they inherit its limits. There is no present `0`, no
  `ranFunction-Instance`, no control message above 16000 octets, no `e3-control-outcome` longer than 10
  octets and no extension bits.
* They say nothing about invalid input. Section 5 lists the places where the three codecs differ on it.
* The expected behavior on extension bits, trailing octets and outcome payloads above 127 octets
  (two-octet length form) comes from ITU-T X.691 and the ASN.1 runtime. It does not come from a vector.

**7. Open items**

| # | Item | Owner | Status |
|---|---|---|---|
| 7.1 | The real E3 link and the Spectrum codec are **out of scope** of the codec work of libe3 and OCUDU. The E2SM-DAPP codec does not touch E3 | | not scheduled |
| 7.2 | Decide the `data-size` rule: equal and enforced, or ignored | the specification owners | open. Recommendation: send equal values |
| 7.3 | Define the `node-plmn-id` packing and the `node-type` values | the specification owners | open |
| 7.4 | Fix the limits of flexric (2.2, 3.13) or document them as the limits of that implementation | maintainers of flexric | open |
| 7.5 | Wire the sweep of pending controls into a RAN, or remove it | maintainers of flexric and of RAN stacks | open |
| 7.6 | Add a free binding for the control outcome to the xApp framework | maintainers of the xApp framework | open |
| 7.7 | Extend the reference test vectors with cases that flexric cannot produce (present zero, instance, long payload outcome), from a second source | | not scheduled |
| 7.8 | The E3 side of OCUDU's E2SM-DAPP module: the indication source and the control sink that carry the dApp traffic. Only the no-op versions exist | maintainers of OCUDU | not in the module |

## Aerial: what to change to align

Every statement about Aerial is *Source* (dapps v1.1.0 `99b10eb`, agent `4f65f97`), and the rows are
asks, not claims about what Aerial will do. The conformance test decodes Aerial's own examples, so
the differences in the first two rows are pinned.

| Where Aerial differs | What to change | Why |
|---|---|---|
| The granted lists in a subscription response are `telemetryGrantedList` and `controlGrantedList` | Use `telemetryIdentifierList` and `controlIdentifierList` | libe3 and OCUDU's grammar use those names; libe3 reads Aerial's as aliases only |
| No `timestamp` in the schema; `subscriptionId` on an indication and `dAppIdentifier` on a message ack, which neither libe3 nor OCUDU has | Add the optional `timestamp`. Drop the two extra keys, or propose them for E3AP here | One schema that all three read without special cases |
| Sends the reason of a refusal as one string, `message`, on a negative setup response, subscription response and message ack ([#100](https://github.com/wineslab/libe3/issues/100)) | Send `cause` and `detail` as OCUDU's grammar has them: `detail` carries the text that `message` carries now, and `cause` is one of the `E3-Cause` names (in JSON its camel case name, for example `unsupportedRanFunction`, `unsupportedIdentifier`, `invalidRequest`). Keep `message` in the schema as deprecated. For a control, see the next row | libe3 now writes `cause` and `detail` and reads `message` as `detail`, so nothing breaks while Aerial keeps `message`, and a refusal then reads the same on all three RANs. Aerial's checks of the function, the empty list and the identifier ([#96](https://github.com/wineslab/libe3/issues/96)) already match libe3's and OCUDU's; only the spelling of the reason moves |
| Answers a control with a `messageAck`: always negative, "control actions not implemented", because the agent implements no controls ([#111](https://github.com/wineslab/libe3/issues/111)). The 25.3 agent answered with an `e3_control_response` | Answer a control that carries `requestAck` with an `E3-ControlResponse` as libe3's grammar has it, with `cause` and `detail`: `unsupportedRanFunction` today and `unsupportedIdentifier` for an unknown control id once it has controls. The `messageAck` is deprecated in libe3 and will go | A libe3 dApp reads the `messageAck` as before, and a libe3 RAN sends a response, to a dApp that asked and for a refusal either way |
| Accepts exactly the version `1.0.0` ([#89](https://github.com/wineslab/libe3/issues/89)) | Accept any `1.` prefix, as OCUDU does | A version string can then flag a wire change without failing Aerial |
| No `dAppReport` or `xAppControlAction`, and no `sequenceId` ([#116](https://github.com/wineslab/libe3/issues/116)) | Add both messages with the mandatory `sequenceId` libe3 0.2.0 uses | The E2-E3 loop needs them; OCUDU has the messages and lacks the id |
| The dApp manager refuses a second subscription before it sends it ("limit one subscription per agent for now"), and the agent adds a subscription for every request, with no check for an existing one ([#97](https://github.com/wineslab/libe3/issues/97)) | Return the existing subscription in the agent on a repeat, as libe3 does | libe3 proposes the idempotent form. An agent that adds one per request gives a dApp that retries subscriptions it did not ask for, with ids it cannot tell apart |
| Keeps a quiet subscribed dApp alive and sends a `releaseMessage` when it drops one ([#90](https://github.com/wineslab/libe3/issues/90), [#91](https://github.com/wineslab/libe3/issues/91), [#93](https://github.com/wineslab/libe3/issues/93)) | Agree one idle rule with OCUDU. libe3 now sends a `releaseMessage` before it drops a dApp for 1800 s of silence, and counts only what the dApp sends. Drop a quiet dApp that has subscriptions too | A dApp that crashed with a subscription stays in Aerial's agent, with its service model running, until a release arrives. A dApp that only watches for a release never notices OCUDU closing the association |
| ZMQ and JSON only ([#85](https://github.com/wineslab/libe3/issues/85)) | Nothing in libe3 or in Aerial. A RAN that serves Aerial and OCUDU runs one agent for each | Each peer talks to an agent configured for its link and encoding |

## OCUDU: what to change to align

Every statement about OCUDU is *Source* (dApp platform `38fc00bb526c`), and the rows are asks. The
libe3-compatible mode is the part that has to follow libe3's grammar; OCUDU's native module can keep
what it has where the row says so.

| Where OCUDU differs | What to change | Why |
|---|---|---|
| `periodicity` is milliseconds, `0..60000` ([#98](https://github.com/wineslab/libe3/issues/98)) | Read microseconds, `0..60000000`, as libe3 and Aerial do | One field cannot have two units; decided on #98 |
| dApp ids `1..100` are reserved for libe3-compatible peers, handed out by adapters ([#99](https://github.com/wineslab/libe3/issues/99)) | Accept `1..65535` from libe3-compatible peers and drop the partition and the adapters | libe3 now hands out ids up to 65535, lowest free first, and echoes any non-zero message id |
| The libe3-compatible mode keeps libe3's `UTF8String` setup strings and clamps `subscriptionTime` to 3600 ([#108](https://github.com/wineslab/libe3/issues/108), [#113](https://github.com/wineslab/libe3/issues/113)) | Adopt `OCTET STRING` names and versions and the new bounds; the clamp goes | libe3 now has OCUDU's own types and bounds, so the two grammars agree on these fields |
| The native module puts the timestamp after the extension marker, `{ id, msg, ..., timestamp }` ([#114](https://github.com/wineslab/libe3/issues/114)) | Put it in the root, `{ id, timestamp OPTIONAL, msg }`, as libe3 and its own libe3-compatible codec do; libe3 keeps its order | That is the layout OAI already runs. Measured on a stamped PDU in a Release build, the extension placement adds 2 bytes (14 to 16 for a `releaseMessage`, 274 to 276 for a 256 byte indication) and about 70 ns to encode and 50 ns to decode, which is small, so the reason is the deployed layout, not speed |
| `E3-ResponseCode` is extensible, `{ positive, negative, ... }` ([#115](https://github.com/wineslab/libe3/issues/115)) | Close it, as libe3 and Aerial's schema have it. If OCUDU needs a third code, propose it here: libe3 is open to adding the extension marker together with the code, as one wire change, and to reading an unknown value as a refusal | With asn1c an unknown extension value decodes as `positive`, so a peer that does not know a new code would read a refusal as success. That is why libe3's type is closed until a code exists. Neither side has a third code today |
| The wire guide describes libe3's `E3-PDU` with the timestamp before `msg` and its `E3-ResponseCode` as "deliberately closed", and libe3's limits as 0.1.3's | Update the libe3 rows to 0.2.x | It is the reference an implementer reads |
| Compiles libe3's grammar with a different asn1c, so Setup encodes differently and the two cannot share a process ([#109](https://github.com/wineslab/libe3/issues/109)) | Build libe3's grammar with the asn1c commit `build_libe3` pins, then compare Setup again. With `OCTET STRING` setup strings the `UTF8String` part of the difference should be gone | Same bytes from the same grammar |
| One SCTP association per dApp, one PDU per SCTP message; payload protocol id, streams and buffers differ ([#104](https://github.com/wineslab/libe3/issues/104) to [#107](https://github.com/wineslab/libe3/issues/107)) | libe3's POSIX link over SCTP now does this: one association per dApp, one PDU per message with no prefix, payload protocol id `0x00E3A001`, indications from the RAN on stream 1 and everything else on stream 0, two streams requested, 2 MiB buffers and delayed SACK off on both peers, IPv4 and IPv6. Its default port stays 9990, so a libe3 dApp reaches OCUDU's default port with `setup_port` set to 36423 | Open until it is run against OCUDU's agent: libe3's own tests read the payload protocol id and the streams off the association with a proxy, and nothing here was run against OCUDU. The patch that OCUDU carries against libe3 (a separate link layer) is expected to be unnecessary for libe3's dApp role |
| `E3-DAppReport`, `E3-XAppControlAction` and `E3-DAppControlAction` have no `sequenceId` ([#116](https://github.com/wineslab/libe3/issues/116)) | Add `sequenceId` to the three messages, as libe3 0.2.0 has it: mandatory on the report and the xApp control, optional on the dApp control | It joins a report, the xApp control it provoked and the control the dApp re-issued into one E2-E3 procedure, which is what end-to-end loop tracing needs. Today neither side can decode the other's version of these messages: a libe3 report carries a field OCUDU's grammar does not know, and an OCUDU report lacks one libe3 requires. OCUDU's libe3-compatible mode is the 0.1.3 grammar, which predates the field ([#89](https://github.com/wineslab/libe3/issues/89)) |
| Fragments payloads up to 16 MiB ([#110](https://github.com/wineslab/libe3/issues/110)), and has bulk-data and instance fields ([#112](https://github.com/wineslab/libe3/issues/112)) | Keep these optional and negotiated at setup, so a peer without them still interoperates | Aerial and libe3 send none of these and must not fail on their absence |
| Answers a control with a `messageAck` for "accepted" and then an `E3-ControlResponse`, in compatible mode with one `messageAck` after the Service Model ran ([#111](https://github.com/wineslab/libe3/issues/111), see [the answer to a dApp control](#control-answer)) | In compatible mode, re-vendor libe3's grammar and answer a control that carries `requestAck` with one `E3-ControlResponse`, sent when the Service Model has the outcome. Natively, keep the two messages. Agree the fragment fields (#110) and the response code (#115) | The two ends then mean the same by a positive answer: executed. The choice index is the same in both grammars; the response is not OCUDU's byte for byte (outcome length, response code), measured above |
| Closes the association after 30 s idle and never sends a `releaseMessage` to a dApp ([#90](https://github.com/wineslab/libe3/issues/90), [#93](https://github.com/wineslab/libe3/issues/93)) | Send a `releaseMessage` before closing | A libe3 dApp then reports `RELEASED_BY_RAN`, not `CONNECTION_LOST`, as it does with Aerial |
| Decodes `subscriptionTime` and never acts on it ([#95](https://github.com/wineslab/libe3/issues/95)) | End the subscription when the time is up (0 is no limit), without a message to the dApp, as libe3 and Aerial's agent now do | A dApp that sends a time gets the same result from all three RANs and need not delete the subscription itself. Until then it must, against OCUDU |
| Adds a new subscription for every repeat, up to a limit per dApp, then refuses with `resourceExhausted` ([#97](https://github.com/wineslab/libe3/issues/97)). This is a defect: the dApp asked for the same thing again, not for another subscription | Return the existing subscription on a repeat | libe3 returns the existing one, with what it holds. A dApp that retries or subscribes twice uses up its limit, and the refusal that follows does not say what went wrong. Aerial's agent adds one per request too, with no limit. A libe3 RAN now refuses an unknown function or id with a `cause`, as OCUDU does ([#96](https://github.com/wineslab/libe3/issues/96)) |
| Refuses a subscription that names any control id, with `invalidRequest`, because only the telemetry function accepts subscriptions ([#96](https://github.com/wineslab/libe3/issues/96)) | Refuse a control id that the RAN function does not advertise (`unsupportedIdentifier`) and a request that names no identifier (`invalidRequest`); accept a control id that the function advertises | A RAN function may have controls, and its setup response lists them: libe3's Service Models have them, and Aerial ignores the list in a subscription. A libe3 dApp that subscribes to a function with its control ids must not be refused for naming them. OCUDU's telemetry function has no controls, so no dApp meets the refusal until a function with controls is added |
| `cause` and `detail` are the first extension additions of a native `E3-SubscriptionResponse`, followed by `sharedRegion`, and libe3's libe3-compatible mode is the 0.1.3 grammar, which has neither ([#100](https://github.com/wineslab/libe3/issues/100)) | Re-vendor libe3's grammar: it has `cause` and `detail` first on the setup response, the subscription response and the message ack, as OCUDU's native grammar has them, then `ranFunctionIdentifier` and `periodicity` in the subscription response. The native module keeps its own layout | The two modules do not decode each other. After the re-vendor a libe3 dApp reads OCUDU's refusals with their cause and OCUDU reads libe3's |

## Change ledger

One row for every change that alters the wire format or something a peer can observe. Add a row in
the same PR as the change. Newest first.

**Status** is `unmerged` until the PR lands on `main` and `unreleased` until a release carries it.
**Who has to act** names the components that must be rebuilt together or that will notice.

| Change | PR | Kind | Who has to act | Status |
|---|---|---|---|---|
| The POSIX link serves a dApp on one connection to one endpoint, as OCUDU does, on every transport. SCTP and IPC (now a sequenced-packet socket) carry one PDU per message with no length prefix; TCP prefixes every PDU with its length in both directions. SCTP stamps payload protocol id `0x00E3A001`, the RAN sends indications on stream 1 and everything else on stream 0, two streams are requested, send and receive buffers are set to 2 MiB and delayed SACK is off. A dApp is tied to its connection when it registers, and the RAN sends each PDU on the connection of the dApp it is for instead of to all. The IPC default is `/tmp/dapps/e3`; the port stays 9990. TCP and SCTP take IPv6 and a host in the endpoint, a list of peers and a cap on connections. New `E3Config` fields: `socket_buffer_bytes`, `sctp_delayed_sack`, `allowed_peers`, `maximum_associations` ([#104](https://github.com/wineslab/libe3/issues/104), [#105](https://github.com/wineslab/libe3/issues/105), [#106](https://github.com/wineslab/libe3/issues/106), [#107](https://github.com/wineslab/libe3/issues/107)) | | POSIX wire | Every POSIX peer, rebuilt together: libe3 RAN and dApp, OAI's E3 agent and the dApp library through their twin pull requests. Nothing of the old layout is kept: an old dApp meets a new RAN with a setup that times out or does not decode (the RAN logs why), and the reverse; an old IPC path no longer exists. `E3Connector` gains `send_to`, `last_setup_peer_token` and `PduClass`, and `create_connector` a `PosixOptions` argument. The patch OCUDU carries against libe3 is expected to be unnecessary for libe3's dApp role; this was not run against OCUDU's agent | unmerged |
| `E3-ControlResponse` is the answer to a dApp control and to an xApp control. `E3-MessageAck` stays at index 10 of the PDU choice, marked DEPRECATED TO BE REMOVED, and libe3 sends none. Both controls get an optional `requestAck`, an extension addition written only when true. The response has OCUDU's fields and order without `fragmentIndex`, `fragmentCount` and `payloadBytes`, and `controlOutcome` of 1..32768 bytes. It takes index 11 of the PDU choice, as in OCUDU's grammar. The RAN answers a control it refuses itself (no setup, unknown function, unknown control id, no running Service Model) whether it asked or not, and a Service Model through `respond_to_control` only when the control asked. An xApp control that asks is answered by the dApp. A dApp reads only the responses that name it. JSON: type `controlResponse`, `controlOutcome` nested, `requestAck` only when true. Protobuf: `ControlResponse`, case 14. API: `respond_to_control`, `send_control_response`, `set_control_response_handler`, `e3_service_model_emit_control_response`, `e3_agent_set_control_response_handler`, `request_ack` on the two sends and `out_message_id` on `send_xapp_control` are new; the ack's helpers and handlers stay, deprecated. VERSION 0.5.0 ([#111](https://github.com/wineslab/libe3/issues/111)) | [#149](https://github.com/wineslab/libe3/pull/149) | ASN.1 wire, JSON and Protobuf, C++ API, C API, libe3py, behavior | libe3, OAI's E3 agent and the dApp library rebuilt together. Measured in a Linux container: libe3 0.4.3 refuses 0.5.0's response with `DECODE_FAILED`, so an unmatched dApp hears nothing where it heard an ack, and every earlier vector keeps its bytes. A Service Model that answered a control with the ack helpers should answer with `respond_to_control` or `e3_service_model_emit_control_response`: a message ack still goes out, and a libe3 dApp still reads it, but it is deprecated and no new code should send it. A dApp that sent controls and read nothing back sees negative responses for refusals now, as events, and no positive answer unless it sets `requestAck`. OCUDU's libe3-compatible mode re-vendors the grammar. Native OCUDU is unaffected and still not byte-compatible with libe3 for this message (outcome length, response code; measured, not run against its agent) | unmerged |
| The interop page says that a libe3 RAN agent serves one link and one encoding, that agents with different configurations can run at the same time, and that a RAN with Aerial and OCUDU peers runs one for each ([#85](https://github.com/wineslab/libe3/issues/85)). It says that OCUDU can propose a third `E3-ResponseCode` on the page, and that libe3 is open to adding the extension marker with it ([#115](https://github.com/wineslab/libe3/issues/115)) | | Documentation | Nobody. libe3 does not change. A RAN application that serves both stacks creates the agents | unmerged |
| The `periodicity` of a libe3 RAN's subscription response is the least time its Service Model keeps between two indications, in microseconds, and is present only when the SM reports it through the new `ServiceModel::granted_periodicity()`. A value of 0, or above 60000000, is left out. It was the periodicity of the request, whatever the SM did with it ([#119](https://github.com/wineslab/libe3/issues/119)) | | Behavior, C++ API | A dApp that read the response's periodicity as the one it asked for sees none now (`E3Event.periodicity` is -1 in libe3py), until the SM reports one. An SM that keeps an interval overrides the hook. OAI's SMs go through the C API, which has no hook yet, so OAI dApps see none. A C++ Service Model built against an older header is rebuilt. ASN.1, JSON and Protobuf are unchanged and so is the C API: the bytes of a response lose a field that is optional | unmerged |
| A libe3 RAN answers a repeated subscription (same dApp, same RAN function) with the subscription it holds: the same identifier and the telemetry and control lists and interval it stores. It answered with the lists and interval of the repeat, which it had dropped ([#97](https://github.com/wineslab/libe3/issues/97)) | | Behavior | A dApp that subscribed again to change lists or the interval and read them from the response sees the old ones now, and has to delete the subscription and subscribe again. Nothing on the wire, in the C API or in libe3py, and nothing to rebuild in OAI's E3 agent | unmerged |
| After a release, a lost link, or a setup that no RAN answered or that the RAN refused, a libe3 dApp registers again by itself every `reconnect_interval_ms` (default 1000, `0` never), with a new dApp id and none of its old subscriptions, as Aerial's manager does. `release()` and `stop()` end it. The setup response handler runs on every setup, so the application learns it is back there and subscribes again ([#121](https://github.com/wineslab/libe3/issues/121)) | | Behavior | A dApp that waited for the program to call `stop()` then `start()` now registers by itself; one that does so too races with the library, so set `reconnect_interval_ms` to 0 or leave it to the library. The Python dApp framework has to subscribe again on the setup response event. OAI's E3 agent and the dApp library rebuilt together with the new `E3Config`. Nothing on the wire | unmerged |
| A POSIX libe3 RAN (TCP, SCTP, IPC) removes a dApp whose connection closes without a release, at once and with nothing sent, if the dApp had sent a message that names it on that connection. It also binds a dApp id to the connection that first named it and drops a later message that names it on another connection, so one dApp can no longer release or control another ([#93](https://github.com/wineslab/libe3/issues/93)) | | Behavior | A dApp that relied on naming another dApp's id from its own connection on POSIX is now dropped. A dApp that closes before its first message, and every ZeroMQ dApp, are still left to the idle time. OAI's E3 agent and the dApp library rebuilt together with the new `E3Connector`, which gains two virtual functions. Nothing on the wire | unmerged |
| A libe3 RAN removes a dApp that has vanished. After `dapp_idle_timeout_s` (default 1800 s, `0` never) without a message from the dApp it sends a `releaseMessage` naming it, then drops its registration and subscriptions, stops a RAN function it was the last subscriber of and calls the dApp status handler. Only what the dApp sends resets the time, never what the RAN sends it. A RAN built through the C API sets the time with `set_dapp_idle_timeout` and `dapp_idle_timeout_s` in `e3_config_t` ([#93](https://github.com/wineslab/libe3/issues/93)) | | Behavior | A dApp that only listens, or is subscribed and never sends, is released after 30 minutes and has to be started again by the program (`stop()` then `start()`), or the RAN sets `dapp_idle_timeout_s` to 0 for the old behavior. A libe3-compatible peer must handle a release that names it: OCUDU's does not send one and Aerial's agent does. OAI's E3 agent and the dApp library rebuilt together with the new `E3Config` and `e3_config_t`, which grows by two fields. Nothing on the wire, the release is the existing message | unmerged |
| A negative setup response, subscription response and answer to a control carry a `cause` and a `detail` ([#100](https://github.com/wineslab/libe3/issues/100)), and a RAN refuses a subscription to an unknown RAN function, with no identifier or with an identifier the function does not advertise, and a control for an unknown function, an unknown control id or a function with no subscription, where it answered positive or not at all ([#96](https://github.com/wineslab/libe3/issues/96)). `cause` is OCUDU's `E3-Cause`, 15 values, extensible. `detail` is UTF-8 text of up to 512 bytes. JSON writes `cause` as its camel case name and `detail`, and reads Aerial's `message` as `detail`. ASN.1: `cause` and `detail` are the first extension additions of the setup response and the subscription response, as in OCUDU's grammar | | ASN.1 wire, JSON and Protobuf extension, behavior | libe3, OAI's E3 agent and the dApp library rebuilt together: in the subscription response `cause` and `detail` now come before `ranFunctionIdentifier` and `periodicity`, so every subscription response that carries those has other bytes, and an older libe3 reads `cause` as the RAN function. Setup response bytes change only when a reason is present. A dApp that subscribes to an id its RAN function does not advertise is refused now, so one that relied on the positive answer has to name the advertised ids. OCUDU's libe3-compatible mode has to re-vendor the grammar. Aerial sends `message`, which libe3 reads | unmerged |
| A libe3 RAN ends a subscription when its `subscriptionTime` runs out, as Aerial's agent does (it decoded the field and dropped it, so a subscription asked for 60 s ran until the dApp deleted it or was released). `0` or no time still means no limit. The RAN sends nothing to the dApp, stops the Service Model if that was its last subscriber and calls the dApp status handler as for a delete; a libe3 dApp forgets it too, counted from the response, so never before the RAN ([#95](https://github.com/wineslab/libe3/issues/95)) | [#129](https://github.com/wineslab/libe3/pull/129) | Behavior | A dApp that relied on a subscription running forever after sending a non-zero `subscriptionTime` will see it end: send 0, or leave the field out, or subscribe again when it ends. Against an OCUDU RAN nothing changes, it still never expires one. Nothing on the wire, on any encoding | released in 0.2.10 |
| ASN.1 bounds match OCUDU's: telemetry and control id lists 0..256 entries (were 0..1000), `ranFunctionData` 1..262144 bytes (was 1..32768), at most 64 functions in a setup response (was unbounded), `subscriptionTime` 0..86400 seconds (was 0..3600). A RAN refuses to register a 65th function or one with over 256 ids, and a setup response omits such an entry instead of failing to encode ([#113](https://github.com/wineslab/libe3/issues/113)) | [#123](https://github.com/wineslab/libe3/pull/123) | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together, tracked on duranta#495: a different range changes the bits APER uses, so old and new cannot exchange these PDUs. OCUDU's libe3-compatible codec must take the new bounds. libe3 still carries at most 32768 bytes of `ranFunctionData` and about 60 KB per setup response, because the encode buffer, the receive buffers and the POSIX frame cap are unchanged. JSON and Protobuf unchanged | released in 0.2.9 |
| Tests pin what peers send and what libe3 writes (Aerial's 17 JSON examples, the ASN.1 bytes of one PDU per type). ASN.1 `decode` reports a malformed PDU as `DECODE_FAILED`; it returned `ENCODE_FAILED` ([#102](https://github.com/wineslab/libe3/issues/102)) | [#123](https://github.com/wineslab/libe3/pull/123) | Behavior | Nothing on the wire. A caller that matched `ENCODE_FAILED` on an ASN.1 decode now sees `DECODE_FAILED`, as JSON and Protobuf already returned; libe3 itself does not distinguish them | released in 0.2.9 |
| ASN.1 `dAppName`, `vendor`, `ranIdentifier` and the two version fields change from `UTF8String` to `OCTET STRING`, sizes 1..64 and 1..32 bytes (a version was 1..11 characters). Each string's length was a whole octet and is now 6 bits (names) or 5 bits (versions). An empty name, a name over 64 bytes or a version over 32 bytes now fails to encode, where it went out unchecked before, so over ASN.1 `init()` returns `INVALID_PARAM` for a config string outside those sizes instead of failing the first setup. A dApp with no `vendor` and a RAN with no `ranIdentifier` send `unknown`, on every encoding ([#108](https://github.com/wineslab/libe3/issues/108)) | [#123](https://github.com/wineslab/libe3/pull/123) | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together, tracked on duranta#495: old and new cannot exchange `SetupRequest` or `SetupResponse`. OCUDU's libe3-compatible mode must switch to `OCTET STRING`; its native grammar already uses these types and sizes. JSON and Protobuf wire unchanged, apart from `unknown` where the config left a name empty | released in 0.2.9 |
| The RAN agent hands out dApp ids and subscription ids from 1..65535, lowest free first (was 1..100, next free after the last, wrapping and overwriting a live subscription id), and echoes any non-zero request id of a peer's setup request (was 1..1000 only). Python `DAppSession.subscribe` returns `int64` (was `int`, so an id above 2^31 read as an error code) ([#99](https://github.com/wineslab/libe3/issues/99)) | [#126](https://github.com/wineslab/libe3/pull/126) | Behavior | A libe3 RAN with more than 100 dApps connected at once gives the 101st an id above 100, which OCUDU's libe3-compatible peers reject; fewer than that, ids stay low however often dApps reconnect. Aerial's large message ids now reach the setup response unchanged. No wire change | released in 0.2.4 |
| ASN.1 `periodicity` widened from 0..1000 to 0..60000000, on the request and the granted value in the response. The unit is microseconds ([#98](https://github.com/wineslab/libe3/issues/98)) | [#122](https://github.com/wineslab/libe3/pull/122) | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together: APER sizes the field from the constraint, so old and new cannot exchange subscription requests or responses. JSON and Protobuf unchanged. OCUDU must convert from microseconds | released in 0.2.8 |
| dApp reports `CONNECTION_LOST` and `RELEASED_BY_RAN` and acts on a release for itself; `stop()` then `start()` works; setup retry over ZMQ opens a fresh socket ([#90](https://github.com/wineslab/libe3/issues/90), [#91](https://github.com/wineslab/libe3/issues/91), [#92](https://github.com/wineslab/libe3/issues/92)) | [#120](https://github.com/wineslab/libe3/pull/120) | Behavior | The Python dApp framework needs a handler for the new event. Aerial's `e3_backends.md` lists "no release or disconnect callback" as a gap of the libe3 backend, which this closes | released in 0.2.7 |
| JSON payloads nested like Aerial's; subscription response reports granted ids, function id and periodicity ([#86](https://github.com/wineslab/libe3/issues/86), [#94](https://github.com/wineslab/libe3/issues/94), [#88](https://github.com/wineslab/libe3/issues/88)) | [#118](https://github.com/wineslab/libe3/pull/118) | JSON wire, ASN.1 and Protobuf extension | JSON peers: both ends need this version for `ranFunctionData`, `actionData`, `reportData` and `protocolData`, because older libe3 writes hex and cannot read the nested form. ASN.1: `ranFunctionIdentifier` and `periodicity` in the response are extension additions after `...`, so older decoders skip them. Aerial's `e3_backends.md` lists "no granted telemetry or periodicity" as a gap, which this closes | released in 0.2.6 |
| `sequenceId` on three messages; dApp, function, telemetry and control ids widened to 1..65535, message id to 1..4294967295, identifier lists to 1000 ([commit 3be38cd](https://github.com/wineslab/libe3/commit/3be38cdfac4dd1e26d8b7d04339d22ae3fca8275)) | | ASN.1 wire | libe3, OAI and the dApp library rebuilt together. OCUDU's libe3-compatible mode is the 0.1.3 grammar and decodes none of this ([#89](https://github.com/wineslab/libe3/issues/89)) | released in 0.2.0 |
