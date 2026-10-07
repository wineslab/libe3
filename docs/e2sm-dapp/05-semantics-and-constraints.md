# E2SM-DAPP: semantics and constraints

What the numbers mean, the units and clocks, which rules a message must satisfy,
what the three implementations check, and what each one does when the input is bad
or unknown.

This is section 5 of the specification. The index is [README.md](README.md). Where
the code and the grammar disagree, the disagreement is listed in
[06-discrepancies.md](06-discrepancies.md) and only referenced here.

## 1. Numbering: formats and styles

There are three different numberings. They share the digits `1` and `2` and mean
different things.

| Numbering | Values | Where it appears | Notes |
|---|---|---|---|
| **Format number** on the wire | the `CHOICE` alternative: header and message format `1` or `2`, event trigger, action definition, control header, control message and control outcome format `1` | the APER choice index | 1-based. A reader that sees format 3 gets a decode failure (see 6). |
| **`RIC-Format-Type`** | `1` or `2`, in use | the RAN function definition items: `ric-IndicationHeaderFormat-Type`, `ric-IndicationMessageFormat-Type`, `ric-ControlHeaderFormat-Type`, `ric-ControlMessageFormat-Type`, `ric-ControlOutcomeFormat-Type` | unbounded `INTEGER`. It is the advertised format number and equals the wire number. |
| **Style number** (`RIC-Style-Type`) | report style `1` = E3 data report, report style `2` = E3 subscription map, control style `1` | `ric-Style-Type` in the action definition, and the definition items | unbounded `INTEGER`. Report and control styles are separate number spaces. |
| **flexric's in-memory format enum** | `FORMAT_1_...` = `0`, `FORMAT_2_...` = `1` | the C struct `format` member | **0-based.** Not the wire number and not `RIC-Format-Type`. `flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:119-275`, mirrored in `xdevsm/.../dapp/enums.py`. |

| Style | Name in the standard text | Format of header and message | Receives |
|---|---|---|---|
| report 1 (`DAPP_RIC_STYLE_E3_DATA_REPORT`) | E3 data report | 1 and 1 | one indication per dApp report |
| report 2 (`DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP`) | E3 subscription map | 2 and 2 | one indication per change of the dApp set |
| control 1 | dApp control | header 1, message 1, outcome 1 | the control |

Style names are free text. The names in use are in
[02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format).

## 2. Identifiers

| Identifier | Meaning | Range in E2SM-DAPP | Range in E3AP | Notes |
|---|---|---|---|---|
| `ran-function-id` | the **E3** RAN function, which names the service model of the payload | `0..4294967295` | `1..65535` | not E2's RAN function id (255). See [01-identity-and-registration.md](01-identity-and-registration.md#12-two-things-named-ran-function-id). |
| `dapp-id` | the dApp | `0..4294967295` | `1..65535` | |
| `SubscribedE3RANFunction-ID` | an E3 RAN function a dApp has subscribed to | `0..4294967295` | `1..65535` | |
| `sequence-id` | one detection-to-mitigation procedure | unbounded, optional | `1..4294967295` (`E3-SequenceID`) | **`E3-SequenceID` equals E2SM-DAPP `sequence-id`**: same value, assigned by the dApp, never reassigned. |
| `node-type`, `node-plmn-id`, `node-nb-id`, `node-cu-du-id` | identity of the reporting node | see 02 | | the grammar defines no enumeration and no packing. See 02. |

**A sender should keep the E2SM-DAPP ids inside the E3 ranges.** E2SM-DAPP can
carry `0` and values above `65535`, but a RAN that relays a control over E3 cannot
deliver them (`dAppIdentifier` and `ranFunctionIdentifier` are `1..65535` on E3).
The golden vectors deliberately include such values (`ih1_bounds` with
`ran-function-id` 0 and `dapp-id` 4294967295) to test the codec, not because a RAN
would send them.

The id `0` is reserved by convention in two other fields: flexric treats a
`sequence-id` of `0` and a timestamp of `0` as absent (section 4).

## 3. Clocks and units

| Quantity | Unit | Clock | Notes |
|---|---|---|---|
| `Timestamp` (every field of that type) | nanoseconds since the Unix epoch, 1970-01-01 00:00:00 UTC | `CLOCK_REALTIME` | the grammar comment says "from the realtime clock (`time_now_ns`)". flexric: `flexric/src/util/time_now_us.c:49-57`. |
| `sfn` (Spectrum payload) | radio frame number | | `0..1023` |
| `slot` (Spectrum payload) | slot in the frame | | `0..159` (`maxSlotIndex`); 20 at numerology 1 |
| `data-size` | octets | | |
| `node-nb-id`, `node-cu-du-id`, ids | dimensionless | | |

### Whose clock is each timestamp on

| Timestamp | Set by | On whose clock | Subtract it from |
|---|---|---|---|
| report timestamp, inside the payload (`Spectrum-DAppReportData.timestamp`) | the dApp when it builds the report | the dApp's host, which is co-located with the gNB | `installedTimestamp`, `onAirTimestamp` |
| indication header `timestamp` | the RAN when it builds the indication (OAI `time_now_ns()`, `ran_func_dapp.c:397`) | the gNB | the report timestamp (same host), not an xApp stamp without allowing for clock offset |
| control header `timestamp` | the xApp when it builds the control (xDevSM `time.time_ns()`) | the xApp's host | only other stamps from the same host |
| control outcome `timestamp` | the RAN when it builds the outcome (`built_ts_ns`) | the gNB | |
| `installedTimestamp`, `onAirTimestamp` (Spectrum payload) | the RAN at B17 and B18 | the gNB | the report timestamp |

This is why the xApp can compute detection-to-mitigation by subtraction **without a
clock of its own**: the report timestamp, the install instant and the on-air instant
are all `CLOCK_REALTIME` nanoseconds on the gNB's host. The xApp only carries the
numbers. A figure that mixes in a stamp from the xApp's host carries the offset
between the two hosts, and the Spectranet decision engine records such figures
separately (`decide_ns` is xApp-clock minus xApp-clock and is exact; `visible_ns`
crosses the boundary and is an estimate). See
`spectranet-xapps/decision_engine_xapp/data.py:204-250`.

**The payload timestamps are deliberately not latency-recorder records.** The latency
recorder (`docs/latrec.md`) stamps `CLOCK_MONOTONIC` and its rule is that a payload
timestamp and a latrec `t_ns` must not be subtracted
([latrec.md](../latrec.md), the section on clocks; lines 103-112). The Spectrum
grammar says the same in its comment on the apply outcome. The latency recorder
measures the same interval independently, so the two figures cross-check each other.

Other things to know:

* **`time_now_ns()` returns `-1` if the clock call fails** (`time_now_us.c:52-54`),
  and the OAI code uses the value without checking. A `-1` timestamp is a valid
  `INTEGER` and would be sent. In practice `clock_gettime(CLOCK_REALTIME)` does not fail.
* `Spectrum-SensingIndication.timestamp` is `CLOCK_MONOTONIC`, not realtime. It never
  leaves the host. Do not compare it with a `Timestamp` of E2SM-DAPP.
* All three implementations hold a timestamp in a signed 64-bit integer. The largest
  value is `2^63 - 1` (in 2262), which the vectors cover.

## 4. Optional fields: absent is not an error

`timestamp` and `sequence-id` are `OPTIONAL` in every message that has them, and so is
`node-cu-du-id`, `ranFunction-Instance`, `e3-control-outcome` and the
`dappE3Subscriptions` of the definition items. **An absent `timestamp` or
`sequence-id` is a normal message.** A decoder must not reject it, and a consumer
must treat it as "the producer did not stamp" and not as an error.

How "absent" is represented in memory differs, and that is the one place the
implementations can disagree about a value:

| Implementation | Absent `timestamp` / `sequence-id` | Can a present value be `0`? | Can a present value be negative? |
|---|---|---|---|
| flexric (C IR) | the field is `0`. The encoder omits the field when it is `0` (`dapp_enc_asn.c:149,157`, `190,198`, `372,380`, `461,469`). The decoder returns `0` for absent. | **No.** `0` is encoded as absent, so a present `0` cannot be sent and cannot be read. | yes |
| libe3 C++ and C | explicit `timestamp_present` / `sequence_id_present` (C++) or `has_timestamp` / `has_sequence_id` (C) | yes | yes |
| OCUDU | explicit `timestamp_present` / `sequence_id_present` | yes | yes |

The grammar comment that justifies flexric's convention says "0 is never assigned by
any producer in this codebase, so it is safe to use as the IR struct's own 'absent'
sentinel". It holds for the producers in this codebase. A producer outside it that
sends a present `0` is read as absent by flexric: the information "present, zero" is
lost, nothing else breaks. E3AP's `E3-SequenceID` starts at 1, so a real sequence id
is never `0`.

The same convention applies to the payload:

| Payload field | Absent means | Present means |
|---|---|---|
| `e3-control-outcome` | the RAN only acknowledges, with nothing to say about applying | the service model's account |
| Spectrum `installedTimestamp` | the control never installed (it was rejected) | the mask was written into the MAC |
| Spectrum `onAirTimestamp`, `sfn`, `slot` | the control installed but never reached a scheduler tick (superseded or timed out) | the first tick that put it on the air |

flexric's `e3_control_outcome` uses a pointer and a size: **`NULL` and `0` mean
absent**, and an empty octet string (present, length 0) cannot be sent through the C
IR (`dapp_enc_asn.c:478`, `dapp_dec_asn.c:395`).

## 5. What the code treats as mandatory

The grammar says what must be present. The table adds what the code requires on top
of it, so a sender knows what will break at the receiver.

| Rule | Where it is enforced | Effect when broken |
|---|---|---|
| A RIC Control Request carries **ack requested** | `flexric/src/agent/msg_handler_agent.c:401` (`assert`) | the RAN aborts |
| A RIC Subscription Request has exactly **one** action and it is a **report** | `msg_handler_agent.c:232-233` (`assert`) | the RAN aborts |
| A RIC Subscription Request carries an **action definition** with style 1 or 2 | OAI `ran_func_dapp.c:461` (`assert`), `463-480` | the RAN aborts, or answers with an empty subscription outcome that flexric's own `assert` at `dapp_sm_agent.c:74` then rejects |
| A control carries a **`sequence-id` that is not `0`** | OAI `ran_func_dapp.c:527-531`. flexric's agent parks a deferred control under that id and asserts it is not `0` (`pending_ctrl.c:49`). | OAI: the control is acknowledged at once and **not applied**. A RAN that defers a control whose id is `0` aborts the flexric agent. |
| An indication or control payload has **at least one octet** | grammar `SIZE(1..32768)`; flexric: `dapp_enc_asn.c:277-278` (indication, via `valid_ind_msg` at 300), `dapp_enc_asn.c:415-416` (control, `assert`) | flexric drops an empty indication, aborts on an empty control message. The OAI bridge ignores a report with no data (`ran_func_dapp.c:361-363`). |
| A subscription **item** has 1 to 64 functions | grammar | flexric's decoder aborts at 64 (see [06-discrepancies.md](06-discrepancies.md)); libe3 and OCUDU reject 0 and more than 64 |
| The report style list has 1 or 2 items, the control style list exactly 1 | grammar | libe3 rejects other sizes on encode and on decode. flexric's IR check is `0 < n < 64` for both (`ran_func_def_report.c:8`, `ran_func_def_ctrl.c:7`) and leaves the grammar limit to asn1c. |
| The definition has the three-string name | grammar | |
| The control's `ran-function-id` names a service model **the RAN can decode** | OAI: Spectrum service model | an unknown id: the E3 relay still sends the control, the dApp decides |

Not mandatory, though it looks like it: `node-cu-du-id`, `timestamp`, `sequence-id` on
a report; `timestamp` on a control; the event trigger section and the report and
control sections of the definition.

## 6. What each implementation checks

### 6.1 On encode

| Check | flexric | libe3 (`pack`, `libe3_e2sm_dapp_encode_*`) | OCUDU (`pack`) |
|---|---|---|---|
| ids and `node-nb-id` in `0..4294967295` | IR types are 32-bit, except `node_cu_du_id` (64-bit, cast to `long`, not checked) | rejected with `ASN_ERROR_ENCODE_FAIL` (`set_u32`) | rejected with `OCUDUASN_ERROR_ENCODE_FAIL` (`pack_u32`) |
| `node-type` in `0..255` | `uint8_t` | `uint8_t` | `uint8_t` |
| extension bit | always written as `0` | always `0` (asn1c) | `pack_unsupported_ext_flag`: a struct with `ext == true` is rejected |
| payload `1..32768` octets | indication: dropped (empty buffer) when empty or larger than 32768. Control: `assert(data_size > 0)` and the 16 KiB buffer assert. | rejected (`build_data`) | the octet string type rejects other sizes |
| `data-size` equals the octet count | **written from the IR `data_size`**, which is also the octet count the encoder copies (`dapp_enc_asn.c:283-285`, `421-423`) | **not checked**: `data_size` and `data` are encoded as given (`build_data`) | **not checked**: `data_size` is packed as an integer `0..32768` and `data` separately |
| `PrintableString` alphabet and length | name length only: `assert(len > 0 && len < 151)` in `seq_report_sty.c:8`; the alphabet is not checked | alphabet and length (`valid_printable`) | alphabet (`pack_printable`); length by the string type |
| subscription list of 0 items | indication message: allowed. **Definition: aborts** (`dapp_enc_asn.c:540`). | allowed in the indication message; in a definition the field is omitted or empty as the caller set it | allowed |
| subscription item with 0 functions | encodes (the asn1c encoder then fails the size constraint) | rejected | rejected |
| list of 256 items | indication message: encodes. Definition: **aborts** (`assert(... < 256)`). | allowed | allowed |
| report styles other than 1 or 2 items, control styles other than 1 | no check in flexric itself (asn1c) | rejected | rejected (`pack_dyn_seq_of` with bounds, `pack_constrained_whole_number`) |
| a `PLMN` other than 3 octets | not representable (`uint8_t[3]`) | not representable (`std::array<uint8_t, 3>`) | not representable (`fixed_octstring<3>`) |
| unknown format enum | indication header and message: dropped with a log line (`dapp_enc_asn.c:215-218`, `319`). Others: `assert` | `ASN_ERROR_ENCODE_FAIL` | `OCUDUASN_ERROR_ENCODE_FAIL` (`log_invalid_choice_id`) |
| timestamp or sequence id out of `long` | not a case (`int64_t`) | rejected on platforms where `long` is narrower (`fits_long`) | not a case |
| unbounded `OCTET STRING` above 16383 octets | not applicable (16 KiB buffer) | handed to asn1c; no test covers it | rejected (`pack_ctrl_outcome_octets`: "needs fragmentation, which is not supported") |

### 6.2 On decode

| Check | flexric | libe3 (`unpack`, `libe3_e2sm_dapp_decode_*`) | OCUDU (`unpack`) |
|---|---|---|---|
| malformed APER | `assert(ret.code == RC_OK)`: **aborts** (lines 117, 149, 229, 288, 329, 375, 415, 544 of `dapp_dec_asn.c`) | `ASN_ERROR_DECODE_FAIL`. The half-built asn1c structure is freed. | `OCUDUASN_ERROR_DECODE_FAIL` |
| empty input | `assert(len != 0)` in every decoder except the control header's, which has no length check | `ASN_ERROR_DECODE_FAIL` | `OCUDUASN_ERROR_DECODE_FAIL`: the bit reader reports that the buffer limit was reached (`cbit_ref::unpack`) |
| unknown choice alternative (a format other than the defined ones) | asn1c fails the decode, and the `assert` on it aborts | `ASN_ERROR_DECODE_FAIL` | `OCUDUASN_ERROR_DECODE_FAIL` |
| extension bit `1` on a `SEQUENCE` | asn1c skips the additions | asn1c skips the additions | the additions are read as opaque open types and dropped (`skip_ext_additions`) |
| extension bit `1` on `INTEGER (0..4294967295, ...)` or `(0..255, ...)` (a value outside the root range) | asn1c may decode a value outside the root | a value outside the root range is rejected (`get_u32`, `get_node_type`) | rejected (`unpack_u32`, `unpack_u8`) |
| ids above `4294967295` | cast to `uint32_t`, the high bits are lost | rejected | rejected |
| `node-plmn-id` not 3 octets | `assert(size == 3)` | rejected | cannot occur (fixed size) |
| `data-size` against the octet count | **the ASN `data-size` is ignored**; the IR's `data_size` is set from the octet string length (`dapp_dec_asn.c:261`, `349`) | the decoded `data_size` is the ASN field and may differ from `data.size()`; no error | the decoded `data_size` is the ASN field; no error |
| payload length 1..32768 | by asn1c | rejected | by the octet string type |
| `PrintableString` alphabet | not checked | rejected | not checked on decode (`unpack_printable` checks the length only) |
| subscription list above 255 items or an item with 64 functions | **aborts** (`assert(n < 256)` at `dapp_dec_asn.c:65`, `assert(nrf < 64)` at 87) | accepted up to 256 and 64 | accepted up to 256 and 64 |
| trailing octets after the value | ignored | ignored | ignored (`unpack_e2sm_dapp_ie` checks only the return code) |
| `e3-control-outcome` | copied; empty or absent is read as absent | `e3_ctrl_outcome_present` and the bytes | `e3_ctrl_outcome_present` and the bytes |

### 6.3 Error paths in summary

| Implementation | How a failure is reported | What a caller must do |
|---|---|---|
| flexric | `assert()` in the codecs and in the IR helpers. In a build with assertions enabled the **process aborts**, which for the E2 agent is the gNB. A few paths return an empty `byte_array_t` instead: the indication header and message encoders. | validate input before calling; treat the flexric encoders as unsafe on untrusted data. The agent's own comment says why the indication path was changed: aborting "would take the gNB down for one bad report" (`dapp_sm_agent.c:114-122`). |
| libe3 C++ | `pack` and `unpack` return `libe3::e2sm_dapp::asn_code`: `ASN_SUCCESS` (0), `ASN_ERROR_DECODE_FAIL` (-1), `ASN_ERROR_ENCODE_FAIL` (-2). `unpack` leaves the target untouched on failure. Choice getters throw `std::bad_variant_access` when the other alternative is held. | check the code |
| libe3 C | the functions return `e3_error_t`: `E3_INVALID_PARAM`, `E3_ENCODE_FAILED`, `E3_DECODE_FAILED`, `E3_SM_ERROR_MEMORY`, or 0 (`include/libe3/e2sm_dapp_c.h:19-20`). On a failed decode the output is zeroed and nothing needs freeing. | check the code |
| OCUDU | `pack` and `unpack` return `OCUDUASN_CODE`: `OCUDUASN_SUCCESS`, `OCUDUASN_ERROR_ENCODE_FAIL`, `OCUDUASN_ERROR_DECODE_FAIL`. A `log_error` line says why. | check the code |

The libe3 `asn_code` has "the same values as OCUDU's `OCUDUASN_CODE`"
(`include/libe3/e2sm_dapp.hpp:47-52`).

## 7. Behavior on unknown ids and formats

| Situation | flexric (agent in the RAN) | flexric (RIC side) and xDevSM | OAI RAN |
|---|---|---|---|
| E2 RAN function id other than 255 | not an E2SM-DAPP message; routed to another service model | | |
| Report style number other than 1 or 2 in an action definition | aborts through `assert(subs.type == ...)` after OAI's `AssertError` | | `AssertError` returns an empty answer (`ran_func_dapp.c:478-479`) |
| Action definition format other than 1 | decode fails (`assert`, `dapp_dec_asn.c:159`) | | |
| Indication format other than 1 or 2 | | RIC: `assert(0 != 0 && "Unknown indication message format")` (`dapp_sm_ric.c:140`). xDevSM: the header or message decoder aborts in the same flexric library; its callback rejects a message format it does not know (`dapp_report.py:100-102`). | |
| Indication format 1 with an unknown `ran-function-id` | | RIC: **aborts** (`dapp_sm_ric.c:128-131`), because `dapp_dec_e3_indication` knows only id 1 (`dapp_dec_e3.c:15`). xDevSM: the inner decoder returns `None` and the xApp decides. | |
| Control with an unknown `dapp-id` | | | relayed to E3 without a check (`ran_func_dapp.c:539`), the result is E3's |
| Control with an unknown `ran-function-id` | the encoder side (flexric RIC) rejects it: `dapp_enc_e3_control` returns false, then `assert` (`dapp_sm_ric.c:194-197`) | | relayed to the dApp |
| Control outcome format other than 1 | decode fails (`assert`) | | |
| Style 2 subscription when no dApp has ever connected | | | accepted. No indication is sent until the set changes (see [03-procedure-flow.md](03-procedure-flow.md#34-what-discovery-does-not-do)). |

A consumer written for a new stack should be stricter than flexric: decode
failures return errors, and unknown ids are logged and ignored.

## 8. Control semantics

### 8.1 What "applied" means, and what the acknowledge says

* The acknowledge is a **RIC Control Acknowledge** that carries an
  `E2SM-DAPP-ControlOutcome`. It is sent when the procedure ends, at B19, and not
  when the RAN received the request, at B7
  ([03-procedure-flow.md](03-procedure-flow.md#63-the-e2-acknowledge-is-deferred-to-b19-not-sent-at-b7)).
* "Applied" is defined by the service model, not by E2SM-DAPP. The acknowledge
  always means "the procedure ended". Whether the control took effect is in the
  payload.

| What happened | Outcome the xApp sees |
|---|---|
| installed and on the air | `sequence-id`, `timestamp`, payload with installed and on-air instants |
| installed, replaced before any tick ran (superseded) | payload with the installed instant only |
| rejected by the service model (never installed) | payload with neither instant, or no payload |
| timed out in the RAN (150 ms in OAI) | no payload, or the installed instant if it had happened |
| the control could not be relayed (no `sequence-id`, table full, E3 send failed) | `timestamp` and `sequence-id`, no payload, sent at once |

**A rejected control never installed. A control that installed but never reached a
scheduler tick has no on-air instant. Either way the acknowledge is real.** The
acknowledge never means failure: the flexric agent has no code path that sends a RIC
Control Failure for this service model.

### 8.2 What a consumer must do

1. **Treat an absent or undecodable payload as "no values", never as an error.** The
   Spectranet decision engine does: it logs a warning that "the control was answered
   but the mitigation cannot be timed" and continues
   (`decision_engine_xapp.py:662-668`).
2. Match the acknowledge to its control by `sequence-id`. An acknowledge with no
   `sequence-id` cannot be matched; the decision engine logs it and drops it.
3. Do not compute a latency from a payload instant that is absent. Spectrum's
   `sfn` and `slot` are reported by the xDevSM helper as `None` unless an on-air
   timestamp is present (`SpectrumApplyOutcome.py:44-46`).
4. Expect the acknowledge to arrive after a delay of the order of a few
   milliseconds, and up to the RAN's timeout (150 ms in OAI). The decision engine
   keeps its own pending-control timeout.

### 8.3 Ordering and replacement

* Two controls in flight with the same `sequence-id` are not distinguishable by the
  agent: the pending table is keyed by `sequence-id` alone. A completion releases the
  first slot with that key and leaves the other until a timeout sweep retires it, and
  nothing runs that sweep (`flexric/src/agent/pending_ctrl.c:92-114`; see
  [06-discrepancies.md](06-discrepancies.md)).
* A later Spectrum install replaces an earlier mask, which retires the earlier
  control as superseded. Controls are not queued per dApp.
* A re-installed subscription under the same RIC request id replaces the old one
  (`msg_handler_agent.c:283-296`).

## 9. Rules that cross elements

| Rule | Between | Notes |
|---|---|---|
| The `sequence-id` of the control header equals the `sequence-id` of the indication header that prompted it | indication header, control header | the xApp echoes it |
| The `sequence-id` of the control outcome equals the control header's | control header, control outcome | the RAN copies it |
| `data-size` is the octet count of `data` | `data-size`, `data` | not enforced by libe3 or OCUDU, ignored on decode by flexric. A sender should always send equal values. |
| `ran-function-id` in the control header is the E3 id of the service model of the control message and of the outcome payload | control header, control message, outcome | the outcome does not repeat it |
| the indication message format matches the indication header format | indication header, indication message | format 1 with format 1, format 2 with format 2. flexric's RIC aborts on a mismatch (`dapp_sm_ric.c:122`, `137`). |
| a report style's formats match the definition | definition, indications | style 1 produces format 1 and 1, style 2 produces 2 and 2 |
| the definition's dApp map and the format 2 map describe the same set | definition, format 2 message | both are built from the same E3 subscription map |
