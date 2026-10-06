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
| libe3 | ZMQ or POSIX, over IPC or TCP | JSON, Protobuf | either | CI unit and equivalence tests; the live example runs are manual |
| Aerial | ZMQ | JSON | dApp | CI for the messages: all 17 of Aerial's published examples decode. Source for the rest. Aerial's own dApps can use libe3 as a backend (`e3_backends.md` in `aerial-sample-apps`) |
| OCUDU | SCTP | ASN.1 | dApp | Source. SCTP is off by default in libe3 (`LIBE3_ENABLE_SCTP`) and nothing in CI uses it ([#81](https://github.com/wineslab/libe3/issues/81)). OCUDU's libe3-compatible mode is the 0.1.3 grammar ([#89](https://github.com/wineslab/libe3/issues/89)) |
| OAI | ASN.1 or JSON, chosen at runtime by the `encoding` option of the gNB's E3 configuration (`asn1`, `json` or `protobuf`) | Same | OAI runs the RAN side, libe3 the library underneath | Source. Upstream OAI accepts only `libe3>=0.0.10, <0.2.0`, so it needs a rebuild for anything since 0.2.0 |

A libe3 RAN agent serves one link and one encoding at a time
([#85](https://github.com/wineslab/libe3/issues/85)), so it cannot face Aerial (ZMQ, JSON) and
OCUDU (SCTP, ASN.1) at once.

Defaults when nothing is set: setup, subscriber and publisher ports 9990, 9999 and 9991, and the
IPC endpoints under `/tmp/dapps/` (`E3Config` in `include/libe3/types.hpp`).

## JSON as libe3 speaks it

Written by `src/encoder/json_encoder.cpp`.

**Envelope.** Every message is one flat JSON object with `type`, `id` and `timestamp`, then the
message's own members at the top level. A message with a nested `data` wrapper is refused. `type` is
the PDU name in camel case: `setupRequest`, `setupResponse`, `subscriptionRequest`,
`subscriptionDelete`, `subscriptionResponse`, `indicationMessage`, `dAppControlAction`, `dAppReport`,
`xAppControlAction`, `releaseMessage`, `messageAck`.

**Key names** follow the ASN.1 field names: `dAppIdentifier`, `ranFunctionIdentifier`,
`telemetryIdentifierList`, `controlIdentifierList`, `subscriptionTime`, `periodicity`,
`subscriptionId`, `requestId`, `responseCode` (`"positive"` or `"negative"`), `sequenceId`.

**Opaque payloads are nested JSON, not hex.** `ranFunctionData`, `protocolData`, `actionData` and
`reportData` carry a JSON object or array. Anything else, an empty payload included, fails to encode
with `ENCODE_FAILED`. Before [#118](https://github.com/wineslab/libe3/pull/118) the first,
third and fourth were hex strings; see the ledger.

**Granted values.** A subscription response carries `telemetryIdentifierList`,
`controlIdentifierList`, `ranFunctionIdentifier` and `periodicity` when the RAN reports them.
Absent means not reported; an empty list means none granted. libe3 also reads Aerial's
`telemetryGrantedList` and `controlGrantedList` as aliases.

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
| dApp id and subscription id | 1..65535; the RAN hands out the lowest free one | counts up from 1, no cap | dApp ids 1..100 reserved for libe3-compatible peers, 101..65535 native |
| Message id | 1..4294967295; a peer's id in a setup request is echoed unless it is 0 | one counter from 1, shared by every message type | 1..4294967295 (its libe3-compatible codec: 1..1000) |
| Setup names (`dAppName`, `vendor`, `ranIdentifier`) | `OCTET STRING`, 1..64 bytes over ASN.1; any length over JSON and Protobuf | JSON string | `OCTET STRING`, 1..64 bytes |
| Setup versions (`e3apProtocolVersion`, `dAppVersion`) | `OCTET STRING`, 1..32 bytes over ASN.1; any length over JSON and Protobuf | JSON string | `OCTET STRING`, 1..32 bytes |
| Telemetry and control id lists | 0..256 entries over ASN.1; any length over JSON and Protobuf | JSON array, no limit | 0..256 entries |
| `ranFunctionData` | 1..262144 bytes on the wire over ASN.1; libe3 carries 1..32768 | JSON array or object, no limit | 1..262144 bytes |
| RAN functions in a setup response | 0..64 over ASN.1; the RAN refuses to register a 65th | JSON array, no limit | 0..64 |
| `subscriptionTime` | 0..86400 seconds over ASN.1; accepted and not acted on | no limit | 0..86400 seconds; decoded and not acted on |

Because OCUDU reads the number as milliseconds, a libe3 dApp that asks an OCUDU RAN for 100000
(100 ms in libe3 and Aerial) asks for 100 s, which is out of OCUDU's range and rejected. libe3 does
not convert: it passes the number through, and the caller facing OCUDU passes milliseconds. OCUDU's
libe3-compatible codec does not convert either (it passes the value through and caps it at 1000).
Converting on the OCUDU side is the agreed direction ([#98](https://github.com/wineslab/libe3/issues/98)).

## Conformance fixtures

What CI checks against messages or bytes that did not come from the code under test. Added in
[#102](https://github.com/wineslab/libe3/issues/102).

**Aerial, JSON.** `tests/fixtures/aerial/e3_message_examples.json` is NVIDIA's own example file,
copied unchanged from aerial-sample-apps `99b10eb` (`tests/fixtures/README.md` says how to update it).
`tests/test_interop_aerial.cpp` decodes all 17 messages in it and re-encodes each. They cover nine of
libe3's eleven message types; Aerial publishes no `dAppReport` or `xAppControlAction`. The differences
the test pins, so neither side can change them silently:

- Aerial sends no `timestamp`; libe3 reads it as 0 and writes one.
- Aerial's subscription response names the granted lists `telemetryGrantedList` and
  `controlGrantedList`; libe3 reads them and writes `telemetryIdentifierList` and
  `controlIdentifierList`.
- A negative reply carries a free-text `message`, and a negative setup response has no
  `ranIdentifier`; libe3 has no field for the text and reads the identifier as empty.
- An indication carries a `subscriptionId` and a `messageAck` a `dAppIdentifier`; libe3 reads neither.
- `ranFunctionData` is a JSON array in Aerial's setup response; libe3 keeps it as it came.

**libe3, ASN.1.** `tests/test_asn1_golden.cpp` pins the APER bytes of one PDU per message type, in the
shape OCUDU keeps (id 42, timestamp 1756800000123456789), so OCUDU can re-pin against them. A change to
those bytes is a wire change and needs a ledger row. The same file checks that the 11 PDUs OCUDU
recorded from libe3 0.1.3 are refused with `DECODE_FAILED`.

Not covered: anything from OAI, and any live Aerial or OCUDU agent.

## When the RAN goes away

What a libe3 dApp sees, and what each peer does to cause it. Added in
[#120](https://github.com/wineslab/libe3/pull/120).

| Peer | What it does | What the libe3 dApp reports |
|---|---|---|
| Aerial | Sends a `releaseMessage` naming the dApp, for example after 1800 s idle | `RELEASED_BY_RAN` |
| OCUDU | Closes the association (idle 30 s, setup timeout, release, bad PDU); sends no release to a dApp | `CONNECTION_LOST` |
| Any | Socket closed, link failed, ZMQ heartbeat missed | `CONNECTION_LOST` |

The dApp does not reconnect. The program calls `stop()` then `start()`, or exits
([#121](https://github.com/wineslab/libe3/issues/121)). A dApp that sent its own `release()` gets no
event when the RAN then closes.

## Known differences

All open ones are sub-issues of [#84](https://github.com/wineslab/libe3/issues/84), which also holds
the table of places where Aerial and OCUDU pull in different directions. Read that for the evidence;
the next two sections say which way libe3 goes.

## Aerial: what to change to align

Every statement about Aerial is *Source* (dapps v1.1.0 `99b10eb`, agent `4f65f97`), and the rows are
asks, not claims about what Aerial will do. The conformance test decodes Aerial's own examples, so
the differences in the first two rows are pinned.

| Where Aerial differs | What to change | Why |
|---|---|---|
| The granted lists in a subscription response are `telemetryGrantedList` and `controlGrantedList` | Use `telemetryIdentifierList` and `controlIdentifierList` | libe3 and OCUDU's grammar use those names; libe3 reads Aerial's as aliases only |
| No `timestamp` in the schema; `message` text on a negative reply; `subscriptionId` on an indication and `dAppIdentifier` on a message ack, which neither libe3 nor OCUDU has | Add the optional `timestamp`. Send the reason as OCUDU's `detail`, with its `cause` code, once libe3 and OCUDU agree the shape ([#100](https://github.com/wineslab/libe3/issues/100)). Drop the two extra keys, or propose them for E3AP here | One schema that all three read without special cases |
| Accepts exactly the version `1.0.0` ([#89](https://github.com/wineslab/libe3/issues/89)) | Accept any `1.` prefix, as OCUDU does | A version string can then flag a wire change without failing Aerial |
| No `dAppReport` or `xAppControlAction`, and no `sequenceId` ([#116](https://github.com/wineslab/libe3/issues/116)) | Add both messages with the mandatory `sequenceId` libe3 0.2.0 uses | The E2-E3 loop needs them; OCUDU has the messages and lacks the id |
| Subscribing twice to one RAN function is refused ([#97](https://github.com/wineslab/libe3/issues/97)) | Return the existing subscription, as libe3 does | libe3 proposes the idempotent form; OCUDU's new subscription per repeat should change too |
| Keeps a quiet subscribed dApp alive and sends a `releaseMessage` when it drops one ([#90](https://github.com/wineslab/libe3/issues/90), [#91](https://github.com/wineslab/libe3/issues/91), [#93](https://github.com/wineslab/libe3/issues/93)) | Agree one idle rule with OCUDU. libe3 proposes that a RAN always sends a `releaseMessage` before dropping a dApp | A dApp that only watches for a release never notices OCUDU closing the association |
| ZMQ and JSON only ([#85](https://github.com/wineslab/libe3/issues/85)) | Nothing now. libe3's one-agent-one-link limit is libe3's to fix | One libe3 RAN cannot serve Aerial and OCUDU at once |

## OCUDU: what to change to align

Every statement about OCUDU is *Source* (dApp platform `38fc00bb526c`), and the rows are asks. The
libe3-compatible mode is the part that has to follow libe3's grammar; OCUDU's native module can keep
what it has where the row says so.

| Where OCUDU differs | What to change | Why |
|---|---|---|
| `periodicity` is milliseconds, `0..60000` ([#98](https://github.com/wineslab/libe3/issues/98)) | Read microseconds, `0..60000000`, as libe3 and Aerial do | One field cannot have two units; decided on #98 |
| dApp ids `1..100` are reserved for libe3-compatible peers, handed out by adapters ([#99](https://github.com/wineslab/libe3/issues/99)) | Accept `1..65535` from libe3-compatible peers and drop the partition and the adapters | libe3 now hands out ids up to 65535, lowest free first, and echoes any non-zero message id |
| The libe3-compatible mode keeps libe3's `UTF8String` setup strings and clamps `subscriptionTime` to 3600 ([#108](https://github.com/wineslab/libe3/issues/108), [#113](https://github.com/wineslab/libe3/issues/113)) | Adopt `OCTET STRING` names and versions and the new bounds; the clamp goes | libe3 now has OCUDU's own types and bounds, so the two grammars agree on these fields |
| The native module puts the timestamp after the extension marker, `{ id, msg, ..., timestamp }` ([#114](https://github.com/wineslab/libe3/issues/114)) | Put it in the root, `{ id, timestamp OPTIONAL, msg }`, as libe3 and its own libe3-compatible codec do | That is the layout OAI already runs. Measured on a stamped PDU in a Release build, the extension placement adds 2 bytes (14 to 16 for a `releaseMessage`, 274 to 276 for a 256 byte indication) and about 70 ns to encode and 50 ns to decode, which is small, so the reason is the deployed layout, not speed |
| `E3-ResponseCode` is extensible, `{ positive, negative, ... }` ([#115](https://github.com/wineslab/libe3/issues/115)) | Close it, as libe3 and Aerial's schema have it. If a third code is ever needed, agree it on this page first | With asn1c an unknown extension value decodes as `positive`, so a peer that does not know a new code would read a refusal as success. Neither side has a third code today |
| The wire guide describes libe3's `E3-PDU` with the timestamp before `msg` and its `E3-ResponseCode` as "deliberately closed", and libe3's limits as 0.1.3's | Update the libe3 rows to 0.2.x | It is the reference an implementer reads |
| Compiles libe3's grammar with a different asn1c, so Setup encodes differently and the two cannot share a process ([#109](https://github.com/wineslab/libe3/issues/109)) | Build libe3's grammar with the asn1c commit `build_libe3` pins, then compare Setup again. With `OCTET STRING` setup strings the `UTF8String` part of the difference should be gone | Same bytes from the same grammar |
| One SCTP association per dApp, one PDU per SCTP message; payload protocol id, streams and buffers differ ([#104](https://github.com/wineslab/libe3/issues/104) to [#107](https://github.com/wineslab/libe3/issues/107)) | Open. libe3 uses three associations and a length-prefixed stream, and will propose one mapping on those issues | No change to either side is made here |
| Fragments payloads up to 16 MiB ([#110](https://github.com/wineslab/libe3/issues/110)), answers a control with an `E3-ControlResponse` ([#111](https://github.com/wineslab/libe3/issues/111)), has bulk-data and instance fields ([#112](https://github.com/wineslab/libe3/issues/112)), and has no `sequenceId` ([#116](https://github.com/wineslab/libe3/issues/116)) | Add the mandatory `sequenceId` to the loop messages. Keep the others optional and negotiated at setup, so a peer without them still interoperates | Aerial and libe3 send none of these and must not fail on their absence |
| Closes the association after 30 s idle and never sends a `releaseMessage` to a dApp ([#90](https://github.com/wineslab/libe3/issues/90), [#93](https://github.com/wineslab/libe3/issues/93)) | Send a `releaseMessage` before closing | A libe3 dApp then reports `RELEASED_BY_RAN`, not `CONNECTION_LOST`, as it does with Aerial |
| Rejects an unknown function or id with a `cause` ([#96](https://github.com/wineslab/libe3/issues/96)), and adds a new subscription for every repeat ([#97](https://github.com/wineslab/libe3/issues/97)) | Keep the rejection, and return the existing subscription on a repeat | libe3's RAN accepts unknown ids today and owes the same rejection |

## Change ledger

One row for every change that alters the wire format or something a peer can observe. Add a row in
the same PR as the change. Newest first.

**Status** is `unmerged` until the PR lands on `main` and `unreleased` until a release carries it.
**Who has to act** names the components that must be rebuilt together or that will notice.

| Change | PR | Kind | Who has to act | Status |
|---|---|---|---|---|
| ASN.1 bounds match OCUDU's: telemetry and control id lists 0..256 entries (were 0..1000), `ranFunctionData` 1..262144 bytes (was 1..32768), at most 64 functions in a setup response (was unbounded), `subscriptionTime` 0..86400 seconds (was 0..3600). A RAN refuses to register a 65th function or one with over 256 ids, and a setup response omits such an entry instead of failing to encode ([#113](https://github.com/wineslab/libe3/issues/113)) | | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together, tracked on duranta#495: a different range changes the bits APER uses, so old and new cannot exchange these PDUs. OCUDU's libe3-compatible codec must take the new bounds. libe3 still carries at most 32768 bytes of `ranFunctionData` and about 60 KB per setup response, because the encode buffer, the receive buffers and the POSIX frame cap are unchanged. JSON and Protobuf unchanged | unmerged |
| Tests pin what peers send and what libe3 writes (Aerial's 17 JSON examples, the ASN.1 bytes of one PDU per type). ASN.1 `decode` reports a malformed PDU as `DECODE_FAILED`; it returned `ENCODE_FAILED` ([#102](https://github.com/wineslab/libe3/issues/102)) | | Behavior | Nothing on the wire. A caller that matched `ENCODE_FAILED` on an ASN.1 decode now sees `DECODE_FAILED`, as JSON and Protobuf already returned; libe3 itself does not distinguish them | unmerged |
| ASN.1 `dAppName`, `vendor`, `ranIdentifier` and the two version fields change from `UTF8String` to `OCTET STRING`, sizes 1..64 and 1..32 bytes (a version was 1..11 characters). Each string's length was a whole octet and is now 6 bits (names) or 5 bits (versions). An empty name, a name over 64 bytes or a version over 32 bytes now fails to encode, where it went out unchecked before, so over ASN.1 `init()` returns `INVALID_PARAM` for a config string outside those sizes instead of failing the first setup. A dApp with no `vendor` and a RAN with no `ranIdentifier` send `unknown`, on every encoding ([#108](https://github.com/wineslab/libe3/issues/108)) | | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together, tracked on duranta#495: old and new cannot exchange `SetupRequest` or `SetupResponse`. OCUDU's libe3-compatible mode must switch to `OCTET STRING`; its native grammar already uses these types and sizes. JSON and Protobuf wire unchanged, apart from `unknown` where the config left a name empty | unmerged |
| The RAN agent hands out dApp ids and subscription ids from 1..65535, lowest free first (was 1..100, next free after the last, wrapping and overwriting a live subscription id), and echoes any non-zero request id of a peer's setup request (was 1..1000 only). Python `DAppSession.subscribe` returns `int64` (was `int`, so an id above 2^31 read as an error code) ([#99](https://github.com/wineslab/libe3/issues/99)) | | Behavior | A libe3 RAN with more than 100 dApps connected at once gives the 101st an id above 100, which OCUDU's libe3-compatible peers reject; fewer than that, ids stay low however often dApps reconnect. Aerial's large message ids now reach the setup response unchanged. No wire change | unmerged |
| ASN.1 `periodicity` widened from 0..1000 to 0..60000000, on the request and the granted value in the response. The unit is microseconds ([#98](https://github.com/wineslab/libe3/issues/98)) | [#122](https://github.com/wineslab/libe3/pull/122) | ASN.1 wire | libe3, OAI's E3 agent and the dApp library rebuilt together: APER sizes the field from the constraint, so old and new cannot exchange subscription requests or responses. JSON and Protobuf unchanged. OCUDU must convert from microseconds | unmerged |
| dApp reports `CONNECTION_LOST` and `RELEASED_BY_RAN` and acts on a release for itself; `stop()` then `start()` works; setup retry over ZMQ opens a fresh socket ([#90](https://github.com/wineslab/libe3/issues/90), [#91](https://github.com/wineslab/libe3/issues/91), [#92](https://github.com/wineslab/libe3/issues/92)) | [#120](https://github.com/wineslab/libe3/pull/120) | Behavior | The Python dApp framework needs a handler for the new event. Aerial's `e3_backends.md` lists "no release or disconnect callback" as a gap of the libe3 backend, which this closes | unmerged |
| JSON payloads nested like Aerial's; subscription response reports granted ids, function id and periodicity ([#86](https://github.com/wineslab/libe3/issues/86), [#94](https://github.com/wineslab/libe3/issues/94), [#88](https://github.com/wineslab/libe3/issues/88)) | [#118](https://github.com/wineslab/libe3/pull/118) | JSON wire, ASN.1 and Protobuf extension | JSON peers: both ends need this version for `ranFunctionData`, `actionData`, `reportData` and `protocolData`, because older libe3 writes hex and cannot read the nested form. ASN.1: `ranFunctionIdentifier` and `periodicity` in the response are extension additions after `...`, so older decoders skip them. Aerial's `e3_backends.md` lists "no granted telemetry or periodicity" as a gap, which this closes | unmerged |
| `sequenceId` on three messages; dApp, function, telemetry and control ids widened to 1..65535, message id to 1..4294967295, identifier lists to 1000 ([commit 3be38cd](https://github.com/wineslab/libe3/commit/3be38cdfac4dd1e26d8b7d04339d22ae3fca8275)) | | ASN.1 wire | libe3, OAI and the dApp library rebuilt together. OCUDU's libe3-compatible mode is the 0.1.3 grammar and decodes none of this ([#89](https://github.com/wineslab/libe3/issues/89)) | released in 0.2.0 |
