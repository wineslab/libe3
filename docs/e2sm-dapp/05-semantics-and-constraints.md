# E2SM-DAPP: semantics and constraints

This clause defines what the numbers mean, the units and clocks, the rules a message
shall satisfy, and what a sender and a receiver shall do when the input is wrong or
unknown.

This is section 5 of the specification. The index is [README.md](../e2sm-dapp/README.md).

## 1. Numbering: formats and styles

Three different numberings exist. They share the digits `1` and `2` and mean different
things.

Table: Numberings that share the digits 1 and 2

| Numbering | Values | Where it appears | Notes |
|---|---|---|---|
| **Format number** on the wire | the `CHOICE` alternative: header and message format `1` or `2`, event trigger, action definition, control header, control message and control outcome format `1` | the APER choice index | 1-based. A receiver that sees format 3 shall report a decoding failure (see 6). |
| **`RIC-Format-Type`** | `1` or `2`, in use | the RAN function definition items: `ric-IndicationHeaderFormat-Type`, `ric-IndicationMessageFormat-Type`, `ric-ControlHeaderFormat-Type`, `ric-ControlMessageFormat-Type`, `ric-ControlOutcomeFormat-Type` | unbounded `INTEGER`. It is the advertised format number and equals the wire number. |
| **Style number** (`RIC-Style-Type`) | report style `1` = E3 data report, report style `2` = E3 subscription map, control style `1` | `ric-Style-Type` in the action definition, and the definition items | unbounded `INTEGER`. Report and control styles are separate number spaces. |
| **In-memory format enumeration** | an implementation may number the formats from `0` | a local data structure | **0-based in at least one implementation.** It is not the wire number and not `RIC-Format-Type`. An implementation shall convert it at the codec boundary. |

Table: Report styles and the control style

| Style | Name in the standard text | Format of header and message | Receives |
|---|---|---|---|
| report 1 | E3 data report | 1 and 1 | one indication per dApp report |
| report 2 | E3 subscription map | 2 and 2 | one indication per change of the dApp set |
| control 1 | dApp control | header 1, message 1, outcome 1 | the control |

Style names are free text. The names in use are in
[02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format).

## 2. Identifiers

Table: Identifiers and their ranges

| Identifier | Meaning | Range in E2SM-DAPP | Range in E3AP | Notes |
|---|---|---|---|---|
| `ran-function-id` | the **E3** RAN function, which names the service model of the payload | `0..4294967295` | `1..65535` | not the E2 RAN function id (255). See [01-identity-and-registration.md](01-identity-and-registration.md#12-two-things-named-ran-function-id). |
| `dapp-id` | the dApp | `0..4294967295` | `1..65535` | |
| `SubscribedE3RANFunction-ID` | an E3 RAN function a dApp has subscribed to | `0..4294967295` | `1..65535` | |
| `sequence-id` | one detection-to-mitigation procedure | unbounded, optional | `1..4294967295` (`E3-SequenceID`) | **`E3-SequenceID` equals E2SM-DAPP `sequence-id`**: same value, assigned by the dApp, never reassigned. |
| `node-type`, `node-plmn-id`, `node-nb-id`, `node-cu-du-id` | identity of the reporting node | see [02](02-ie-mapping.md) | | the grammar defines no enumeration and no packing. See [02](02-ie-mapping.md). |

**A sender should keep the E2SM-DAPP ids inside the E3 ranges.** E2SM-DAPP can carry
`0` and values above `65535`. A RAN that relays a control over E3 cannot deliver them,
because `dAppIdentifier` and `ranFunctionIdentifier` are `1..65535` on E3. The
reference test vectors include such values on purpose (an indication header with
`ran-function-id` 0 and `dapp-id` 4294967295). They test the codec. A RAN does not send
them.

By convention, `0` is reserved in two other fields. One implementation treats a
`sequence-id` of `0` and a timestamp of `0` as absent (section 4).

## 3. Clocks and units

Table: Quantities, units and clocks

| Quantity | Unit | Clock | Notes |
|---|---|---|---|
| `Timestamp` (every field of that type) | nanoseconds since the Unix epoch, 1970-01-01 00:00:00 UTC | `CLOCK_REALTIME` | the grammar comment says "from the realtime clock". |
| `sfn` (Spectrum payload) | radio frame number | | `0..1023` |
| `slot` (Spectrum payload) | slot in the frame | | `0..159` (`maxSlotIndex`); 20 at numerology 1 |
| `data-size` | octets | | |
| `node-nb-id`, `node-cu-du-id`, ids | dimensionless | | |

### Whose clock is each timestamp on

Table: Timestamps, their source and their clock

| Timestamp | Set by | On whose clock | Subtract it from |
|---|---|---|---|
| report timestamp, inside the payload (`Spectrum-DAppReportData.timestamp`) | the dApp when it builds the report | the dApp's host, which is co-located with the gNB | `installedTimestamp`, `onAirTimestamp` |
| indication header `timestamp` | the RAN when it builds the indication | the gNB | the report timestamp (same host). Do not subtract an xApp stamp without allowing for the clock offset. |
| control header `timestamp` | the xApp when it builds the control | the xApp's host | only other stamps from the same host |
| control outcome `timestamp` | the RAN when it builds the outcome | the gNB | |
| `installedTimestamp`, `onAirTimestamp` (Spectrum payload) | the RAN at B17 and B18 | the gNB | the report timestamp |

The xApp can compute detection-to-mitigation by subtraction **without a clock of its
own**. The report timestamp, the install instant and the on-air instant are all
`CLOCK_REALTIME` nanoseconds on the host of the gNB. The xApp only carries the numbers.
A figure that mixes in a stamp from the xApp host carries the offset between the two
hosts. An xApp should record such figures separately. A figure from two xApp stamps is
exact. A figure that crosses the host boundary is an estimate.

**The payload timestamps are deliberately not latency recorder records.** The latency
recorder ([latrec.md](../utilities/latrec.md), the section on clocks) stamps
`CLOCK_MONOTONIC`. Its rule is that a payload timestamp and a recorder `t_ns` shall not
be subtracted from each other. The Spectrum grammar says the same in its comment on the
apply outcome. The latency recorder measures the same interval independently, so the
two figures cross-check each other.

Other points:

* A clock read can fail and return `-1`. A sender shall check the result before it
  uses the value as a timestamp. A `-1` timestamp is a valid `INTEGER` and would be
  sent. In practice `clock_gettime(CLOCK_REALTIME)` does not fail.
* `Spectrum-SensingIndication.timestamp` is `CLOCK_MONOTONIC`, not realtime. It never
  leaves the host. A receiver shall not compare it with a `Timestamp` of E2SM-DAPP.
* All three known implementations hold a timestamp in a signed 64-bit integer. The
  largest value is `2^63 - 1` (in 2262). The reference test vectors cover it.

## 4. Optional fields: absent is not an error

`timestamp` and `sequence-id` are `OPTIONAL` in every message that has them. So are
`node-cu-du-id`, `ranFunction-Instance`, `e3-control-outcome` and the
`dappE3Subscriptions` of the definition items. **An absent `timestamp` or
`sequence-id` is a normal message.** A decoder shall not reject it. A consumer shall
treat it as "the producer did not stamp" and not as an error.

How "absent" is represented in memory differs. It is the one place where
implementations can disagree about a value.

Table: Representation of an absent timestamp or sequence id

| Implementation | Absent `timestamp` / `sequence-id` | Can a present value be `0`? | Can a present value be negative? |
|---|---|---|---|
| A (C, intermediate representation) | the field is `0`. The encoder omits the field when it is `0`. The decoder returns `0` for absent. | **No.** `0` is encoded as absent, so a present `0` cannot be sent and cannot be read. | yes |
| B (C++ and C) | explicit presence flags | yes | yes |
| C (C++) | explicit presence flags | yes | yes |

The grammar comment that justifies the convention of implementation A says that no
producer assigns `0`, so `0` is safe as the "absent" marker of the intermediate
structure. This holds for the producers of that implementation. A producer outside it
that sends a present `0` is read as absent by implementation A. The information
"present, zero" is lost and nothing else breaks. The E3AP `E3-SequenceID` starts at 1,
so a real sequence id is never `0`.

The same convention applies to the payload.

Table: Presence of the control outcome payload fields

| Payload field | Absent means | Present means |
|---|---|---|
| `e3-control-outcome` | the RAN only acknowledges, with nothing to say about applying | the service model's account |
| Spectrum `installedTimestamp` | the control never installed (it was rejected) | the mask was written into the MAC |
| Spectrum `onAirTimestamp`, `sfn`, `slot` | the control installed but never reached a scheduler tick (superseded or timed out) | the first tick that put it on the air |

In implementation A, the `e3_control_outcome` is a pointer and a size. **`NULL` and `0`
mean absent**. An empty octet string (present, length 0) cannot be sent through that
representation.

## 5. What is mandatory

The grammar defines what shall be present. The table adds what implementations require
on top of it, so a sender knows what breaks at the receiver.

Table: Rules that implementations enforce in addition to the grammar

| Rule | Effect when broken |
|---|---|
| A RIC Control Request shall carry **ack requested** | at least one implementation aborts the RAN; another applies the control and does not answer |
| A RIC Subscription Request shall have exactly **one** action and it shall be a **report** | at least one implementation aborts the RAN |
| A RIC Subscription Request shall carry an **action definition** with style 1 or 2 | the RAN aborts, or answers with an empty subscription outcome that a second check then rejects |
| A control shall carry a **`sequence-id` that is not `0`** | the OpenAirInterface implementation acknowledges the control at once and **does not apply it**. A RAN that defers a control whose id is `0` aborts the E2 agent of implementation A. |
| An indication or control payload shall have **at least one octet** | the grammar says `SIZE(1..32768)`. One implementation drops an empty indication and aborts on an empty control message. The OpenAirInterface implementation ignores a report with no data. |
| A subscription **item** shall have 1 to 64 functions | implementation A aborts its decoder at 64; the others reject 0 and more than 64 |
| The report style list shall have 1 or 2 items and the control style list exactly 1 | implementation B rejects other sizes on encode and decode. Implementation A leaves the grammar limit to the ASN.1 runtime. |
| The definition shall have the three-string name | |
| The control's `ran-function-id` shall name a service model **the RAN can decode** | an unknown id: the E3 relay still sends the control and the dApp decides |

These fields are not mandatory, although they look so: `node-cu-du-id`, `timestamp` and
`sequence-id` on a report, `timestamp` on a control, the event trigger section, and the
report and control sections of the definition.

## 6. Validation and error handling

### 6.1 On encode

A sender shall satisfy these rules. The right column says how the known implementations
treat a breach. Implementation A encodes some of them without any check.

Table: Checks on encode

| Check | Rule | Known behavior on a breach |
|---|---|---|
| ids and `node-nb-id` | shall be in `0..4294967295` | B and C reject the value. A holds 32-bit fields and does not check `node-cu-du-id`. |
| `node-type` | shall be in `0..255` | all hold one octet |
| extension bit | shall be `0` | A and B always write `0`. C rejects a structure that sets it. |
| payload length | shall be `1..32768` octets | A drops an empty or oversized indication and aborts on a control. B and C reject. |
| `data-size` | shall equal the octet count of `data` | A writes the octet count. B and C encode `data-size` and `data` as given and do not compare them. |
| `PrintableString` alphabet and length | shall follow the grammar | A checks the name length only. B checks alphabet and length. C checks the alphabet and relies on the string type for the length. |
| subscription list of 0 items | allowed in an indication message; in a definition, omit the field | A aborts on an empty list in a definition. B and C allow it. |
| subscription item with 0 functions | shall not be sent | A leaves it to the ASN.1 runtime. B and C reject. |
| list of 256 items | allowed in an indication message | A aborts on it in a definition. B and C allow it. |
| report styles other than 1 or 2 items, control styles other than 1 | shall not be sent | A has no check of its own. B and C reject. |
| `PLMN` | shall be 3 octets | all three implementations can only represent 3 octets |
| unknown format number | shall not be sent | A drops an indication header or message with a log line and aborts on the other elements. B and C return an error. |
| timestamp or sequence id outside a signed 64-bit range | shall not be sent | B rejects it on a platform where the native integer is narrower |
| unbounded `OCTET STRING` above 16383 octets | shall not be sent | A has a 16 KiB buffer. B hands it to the ASN.1 runtime without a test. C rejects it. |

### 6.2 On decode

A receiver shall not abort on bad input. It shall return an error to its caller and
shall leave the output untouched or zeroed.

Table: Checks on decode

| Check | Rule | Known behavior |
|---|---|---|
| malformed APER | the receiver shall return a decoding failure | A aborts the process. B and C return an error. B frees the half-built structure. |
| empty input | the receiver shall return a decoding failure | A aborts, except in the control header decoder, which has no length check. B and C return an error. |
| unknown choice alternative | the receiver shall return a decoding failure | A fails in the ASN.1 runtime and then aborts on the failure. B and C return an error. |
| extension bit `1` on a `SEQUENCE` | the receiver shall skip the additions | A and B skip them in the ASN.1 runtime. C reads them as opaque open types and drops them. |
| extension bit `1` on `INTEGER (0..4294967295, ...)` or `(0..255, ...)`, a value outside the root range | the receiver should reject it | A may decode a value outside the root. B and C reject it. |
| ids above `4294967295` | the receiver shall reject them | A casts to 32 bits and loses the high bits. B and C reject. |
| `node-plmn-id` not 3 octets | the receiver shall reject it | A aborts. B rejects. C cannot see it, because the size is fixed. |
| `data-size` against the octet count | the receiver should not rely on `data-size` | A ignores the ASN.1 field and takes the octet string length. B and C keep the field and do not compare it. |
| payload length 1..32768 | the receiver shall reject other lengths | A and C rely on the ASN.1 runtime or the string type. B rejects. |
| `PrintableString` alphabet | the receiver should reject other characters | A does not check. B rejects. C checks the length only. |
| subscription list above 255 items, or an item with 64 functions | the receiver shall accept up to 256 items and up to 64 functions | A aborts. B and C accept. |
| trailing octets after the value | the receiver shall ignore them | all three ignore them |
| `e3-control-outcome` | the receiver shall read an empty or absent value as absent | A copies it and reads empty as absent. B and C keep a presence flag and the bytes. |

### 6.3 Error paths in summary

Table: How each implementation reports a failure

| Implementation | How a failure is reported | What a caller shall do |
|---|---|---|
| A | assertions in the codecs and in the helpers. In a build with assertions on, the **process aborts**, which for the E2 agent is the gNB. A few paths return an empty buffer instead: the indication header and message encoders. | validate input before the call. Treat the encoders as unsafe on untrusted data. |
| B (C++) | `pack` and `unpack` return a code: success `0`, decoding failure `-1`, encoding failure `-2`. `unpack` leaves the target untouched on failure. A choice accessor throws when the other alternative is held. | check the code |
| B (C) | the functions return an error code for an invalid parameter, an encoding failure, a decoding failure or a memory error, or `0`. On a failed decode the output is zeroed and nothing needs freeing. | check the code |
| C | `pack` and `unpack` return a success code, an encoding failure code or a decoding failure code. A log line says why. | check the code |

The failure codes of implementation B have the same values as those of implementation C.

## 7. Behavior on unknown ids and formats

Table: Behavior on unknown ids and formats

| Situation | Behavior in implementation A (agent in the RAN) | Behavior in implementation A (RIC side) and in an xApp framework | Behavior in the OpenAirInterface RAN |
|---|---|---|---|
| E2 RAN function id other than 255 | not an E2SM-DAPP message; the agent routes it to another service model | | |
| Report style number other than 1 or 2 in an action definition | aborts after the RAN returns an empty answer | | the RAN returns an empty answer |
| Action definition format other than 1 | decoding fails and the agent aborts | | |
| Indication format other than 1 or 2 | | the RIC aborts. The framework decoder aborts in the same library; its callback rejects a message format it does not know. | |
| Indication format 1 with an unknown `ran-function-id` | | the RIC **aborts**, because its inner decoder knows only id 1. The framework returns nothing and the xApp decides. | |
| Control with an unknown `dapp-id` | | | the RAN relays it to E3 without a check. The result is E3's. |
| Control with an unknown `ran-function-id` | the encoder side of the RIC rejects it and then aborts | | the RAN relays it to the dApp |
| Control outcome format other than 1 | decoding fails and the agent aborts | | |
| Style 2 subscription when no dApp has ever connected | | | the RAN accepts it. It sends no indication until the set changes (see [03-procedure-flow.md](03-procedure-flow.md#what-discovery-does-not-do)). |

A new RIC or xApp implementation shall be stricter than implementation A. Decoding
failures shall return errors. An unknown id shall be logged and ignored.

## 8. Control semantics

### 8.1 What "applied" means, and what the acknowledge says

* The acknowledge is a **RIC Control Acknowledge** that carries an
  `E2SM-DAPP-ControlOutcome`. The RAN shall send it when the procedure ends, at B19,
  and not when it received the request, at B7
  ([03-procedure-flow.md](03-procedure-flow.md#the-e2-acknowledge-is-deferred-to-b19)).
* The service model defines "applied", not E2SM-DAPP. The acknowledge always means
  "the procedure ended". The payload says whether the control took effect.

Table: Outcomes and what the xApp sees

| What happened | Outcome the xApp sees |
|---|---|
| installed and on the air | `sequence-id`, `timestamp`, payload with installed and on-air instants |
| installed, replaced before any tick ran (superseded) | payload with the installed instant only |
| rejected by the service model (never installed) | payload with neither instant, or no payload |
| timed out in the RAN (150 ms in the OpenAirInterface implementation) | no payload, or the installed instant if it had happened |
| the control could not be relayed (no `sequence-id`, table full, E3 send failed) | `timestamp` and `sequence-id`, no payload, sent at once |

**A rejected control never installed. A control that installed but never reached a
scheduler tick has no on-air instant. Either way the acknowledge is real.** The
acknowledge never means failure. The agent of implementation A has no code path that
sends a RIC Control Failure for this service model.

### 8.2 What a consumer shall do

1. A consumer shall treat an absent or undecodable payload as "no values", never as an
   error. An example is a decision engine that logs a warning that "the control was
   answered but the mitigation cannot be timed" and continues.
2. A consumer shall match the acknowledge to its control by `sequence-id`. An
   acknowledge with no `sequence-id` cannot be matched. The consumer logs it and
   drops it.
3. A consumer shall not compute a latency from a payload instant that is absent. A
   helper shall report the Spectrum `sfn` and `slot` as "none" unless an on-air
   timestamp is present.
4. A consumer should expect the acknowledge to arrive after a delay of a few
   milliseconds, and up to the timeout of the RAN (150 ms in the OpenAirInterface
   implementation). The consumer keeps its own pending-control timeout.

### 8.3 Ordering and replacement

* An agent cannot tell apart two controls in flight with the same `sequence-id`. Its
  pending table uses `sequence-id` alone as the key. A completion releases the first
  slot with that key. The other slot stays until a timeout sweep retires it. In
  implementation A, nothing runs that sweep.
* A later Spectrum install replaces an earlier mask. This retires the earlier control
  as superseded. The RAN does not queue controls per dApp.
* A re-installed subscription under the same RIC request id replaces the old one.

## 9. Rules that cross elements

Table: Rules between elements

| Rule | Between | Notes |
|---|---|---|
| The `sequence-id` of the control header shall equal the `sequence-id` of the indication header that prompted it | indication header, control header | the xApp echoes it |
| The `sequence-id` of the control outcome shall equal the control header's | control header, control outcome | the RAN copies it |
| `data-size` shall be the octet count of `data` | `data-size`, `data` | not enforced by two implementations and ignored on decode by the third. A sender shall always send equal values. |
| The `ran-function-id` in the control header is the E3 id of the service model of the control message and of the outcome payload | control header, control message, outcome | the outcome does not repeat it |
| The indication message format shall match the indication header format | indication header, indication message | format 1 with format 1, format 2 with format 2. The RIC of implementation A aborts on a mismatch. |
| The formats of a report style shall match the definition | definition, indications | style 1 produces format 1 and 1, style 2 produces 2 and 2 |
| The dApp map of the definition and the format 2 map shall describe the same set | definition, format 2 message | both are built from the same E3 subscription map |
