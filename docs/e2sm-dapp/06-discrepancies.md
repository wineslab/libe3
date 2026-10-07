# E2SM-DAPP: discrepancies, TODOs and dead code

Everything found while writing this specification where code and grammar disagree,
where an implementation is incomplete, or where code is dead. Each entry says what
was read, with `file:line`, and what it means for someone implementing or consuming
E2SM-DAPP. Nothing here is a design decision; those are in the other sections.

This is section 6 of the specification. The index is [README.md](README.md). Code
roots are in [04-encoding.md](04-encoding.md): `flexric/` is flexric with the
`dapp_sm` service model at `71631466`, `oai/` is OpenAirInterface, `xdevsm/` is
`xDevSM-dApp`, `ocudu/` is OCUDU with the E2SM-DAPP port.

Severity: **abort** means an `assert()` or equivalent takes the process down in a build
with assertions on, **wrong** means a wrong result with no error, **latent** means
only a corner case triggers it, **doc** means documentation only.

## 1. Dead code

| # | What | Where | Notes |
|---|---|---|---|
| 1.1 | Plain and FlatBuffers encoders and decoders | `flexric/src/sm/dapp_sm/enc/dapp_enc_plain.c`, `dec/dapp_dec_plain.c`, `enc/dapp_enc_fb.c`, `dec/dapp_dec_fb.c` | All 31 functions are `assert(0 != 0 && "Not implemented")`. `SM_ENCODING_DAPP` allows only `ASN` (`flexric/CMakeLists.txt:317-319`), but `src/sm/dapp_sm/CMakeLists.txt` still has a `PLAIN` branch that would build the stubs, and a `FLATBUFFERS` branch that is `FATAL_ERROR`. The `_Generic` macros in `enc/dapp_enc_generic.h` and `dec/dapp_dec_generic.h` still dispatch to them. **doc** |
| 1.2 | Unused includes of other service models' headers | `flexric/src/sm/dapp_sm/enc/dapp_enc_asn.c:8-9` (`enc_cell_global_id.h`, `enc_ue_id.h`), `dec/dapp_dec_asn.c:10-11` (`dec_ue_id.h`, `dec_cell_global_id.h`) | nothing from them is used. `libe3/tools/e2sm_dapp_golden/regen.sh` has to delete them to build the codec alone. |
| 1.3 | `Bool-Type ::= BOOLEAN` | grammar line 9 | defined, referenced nowhere. asn1c still generates `Bool-Type.c` (it is in `libe3/messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.cmake`). |
| 1.4 | `dapp_ctrl_out_data_t` | `flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:368-370` | a struct with a pointer to a control outcome, used nowhere in `src/` |
| 1.5 | `dapp_ric_service_update_t` | `dapp_data_ie.h:384-386`, a member of the read union in `flexric/src/sm/agent_if/read/sm_ag_if_rd.h:164` | nothing fills or reads it for E2SM-DAPP; the RIC Service Update goes through the generic path |
| 1.6 | `sweep_pending_control_agent_api` and `e2_agent_sweep_pending_control` | `flexric/src/agent/e2_agent_api.c:161-166`, `e2_agent.c:674-695` | **nothing calls them**, in flexric or in `oai/openair2` outside the vendored copy. The 200 ms safety net for an unanswered deferred control (`pending_ctrl.h:53`) never runs. The only timeout in effect is OAI's 150 ms (`oai/openair2/E3AP/e3_pending_ctrl.c:24`). **latent** |
| 1.7 | `e2_send_control_failure` | `flexric/src/agent/e2_agent.c:847` | never called. A control for E2SM-DAPP always ends in a RIC Control Acknowledge. `xdevsm` registers failure handlers (`control.py`, `register_control_ack_fail_callback`) that a flexric RAN never triggers. |
| 1.8 | `on_ric_service_update_dapp_sm_ag` and `on_ric_service_update_dapp_sm_ric` | `flexric/src/sm/dapp_sm/dapp_sm_agent.c:283-287`, `dapp_sm_ric.c:259-265` | stubs. The agent comment says "RIC Service Update is not used for this service model", yet the RAN sends one (`e2_agent_api.c:168-208`). The stub is the hook for *receiving* one; sending does not go through it. **doc** |
| 1.9 | `xdevsm` commented-out code | `xdevsm/.../dApp/report/dapp_report.py:105-109`, `DAppControlOut.py:52-54`, `DAppControlHdr.py:42-49`, `DAppControlMsg.py:38-45` | dead branches left in place |

## 2. Where the grammar and the code disagree

| # | What | Evidence | Effect |
|---|---|---|---|
| 2.1 | **`data-size` duplicates the octet string length** in `IndicationMessage-Format1` and `ControlMessage-Format1` | flexric encodes the IR `data_size` into **both** the ASN field and the octet string length (`dapp_enc_asn.c:283-285`, `421-423`) and on decode **takes the size from the octet string** and ignores the ASN field (`dapp_dec_asn.c:261`, `349`). libe3 and OCUDU keep the ASN field as its own value on both encode and decode and check neither against `data.size()` (`libe3/src/e2sm_dapp/e2sm_dapp.cpp`, `build_data` and `get_data`; `ocudu/lib/asn1/e2sm/e2sm_dapp.cpp:970-986`). | A sender that writes different values is read as the octet length by flexric and as the field by libe3 and OCUDU. **wrong**. Always send equal values. |
| 2.2 | The grammar allows **0..256** dApps and **1..64** functions per dApp, flexric accepts fewer | decoder `assert(n < 256)` (`dapp_dec_asn.c:65`) and `assert(nrf < 64)` (`87`); encoder `assert(0 < n < 256)` for the report and control item lists (`540`); `free_dapp_e3_subscription_list` and `cp_dapp_e3_subscription_list` assert `0 < n < 256` (`ie/ir/dapp_e3_subscription_list.c:8`, `40`); `cp_dapp_e3_subscription_item` asserts `< 64` (`dapp_e3_subscription_item.c:45`). The vector generator documents this (`libe3/tools/e2sm_dapp_golden/gen_golden.c`, the comment before `im2_255`). | A list of 256 dApps, a list of 0 items in a definition, or a dApp with 64 functions **aborts** flexric. The indication message format 2 path is tolerant of an empty list (it has its own free and copy, `ie/ir/e2sm_dapp_ind_msg_frmt_2.c`) but not of 256 items on decode. **abort** |
| 2.3 | The definition item lists: the IR check is `0 < n < 64`, the grammar allows 1..2 report styles and 1 control style | `ie/ir/ran_func_def_report.c:8,38`, `ran_func_def_ctrl.c:7,22,42` | the IR accepts what the grammar forbids and leaves the rejection to asn1c. |
| 2.4 | `ranFunction-Instance` is in the grammar and unusable in flexric | encoder `assert(src->instance == NULL && "not implemented")` (`dapp_enc_asn.c:525`), decoder never reads it (`dapp_dec_asn.c:428-441`) | a definition with an instance is lost on decode, and cannot be encoded. libe3 and OCUDU support it (`ran_function_instance_present`). |
| 2.5 | `RIC-Style-Type` and `RIC-Format-Type` are unbounded `INTEGER`, flexric stores `uint32_t` | `dapp_data_ie.h:151`; `ie/ir/seq_report_sty.h`, `seq_ctrl_style.h` | a style above `4294967295` or a negative one cannot be represented. libe3 and OCUDU hold `int64_t`. **latent** |
| 2.6 | `Timestamp` and `SequenceId` are unbounded `INTEGER`, every implementation holds 64 bits | | a value that needs more than 8 octets cannot be decoded: OCUDU rejects it (`unpack_i64`), libe3 checks `fits_long` |
| 2.7 | `node-plmn-id` is `OCTET STRING (SIZE(3))`, not the 3GPP `PLMN-Identity` | grammar lines 132 and 144. OAI writes `mcc>>8`, `mcc & 0xff`, `mnc & 0xff` (`oai/.../ran_func_dapp.c:277-279`, `406-408`), which drops a digit of a three-digit MNC and is not BCD. | a consumer cannot read the PLMN without knowing the RAN's packing. The grammar does not define one. |
| 2.8 | The dApp E3 subscription list may be empty (`SIZE(0..256)`) but a **subscription item** needs at least one function and flexric's report and control item path needs at least one item | grammar line 259 and 263; `dapp_enc_asn.c:540` | the OAI RAN omits the field when no dApp has a function (`ran_func_dapp.c:142-145`). An empty list in indication message format 2 is legal and works. |
| 2.9 | `E2SM-DAPP` ids are `0..4294967295`, E3AP ids are `1..65535` | `libe3/messages/asn1/V1/e3ap-1.0.0.asn1` | values E3 cannot carry are legal in E2SM-DAPP. The golden vectors include them. |
| 2.10 | `SequenceId` is unbounded, `E3-SequenceID` is `1..4294967295`, and the control path has no range check | `dapp_sm_agent.c:189` converts an `int64_t` to the `uint64_t` key; OAI truncates it to `uint32_t` (`ran_func_dapp.c:519`) | an id above `2^32 - 1` or a negative one makes the parked key and the completion key differ. The control is never completed by the relay and is retired by OAI's 150 ms timeout. **latent** |
| 2.11 | The OID text: `e2sm(255)` in the grammar and the string, `e2sm(2)` in a comment | `dapp_sm_id.h:11-15` against line 17 and the grammar | stale comment. **doc** |
| 2.12 | The grammar file is called `e2sm_dapp_v0_standard.asn` and its module is version 1 | flexric: `ie/asn/e2sm_dapp_v0_standard.asn`; libe3: `e2sm_dapp-1.0.0.asn`; module `version1(1)` | the `v0` in flexric's name does not mean a version-0 grammar. The two files are byte-identical (`diff`). **doc** |
| 2.13 | The control header carries no style number | grammar `E2SM-DAPP-ControlHeader-Format1` | works while there is one control style. A second one needs a new header format. |
| 2.14 | The control outcome carries no `ran-function-id` | grammar `E2SM-DAPP-ControlOutcome-Format1` | the xApp must remember the E3 RAN function per `sequence-id` to decode the payload. See [02-ie-mapping.md](02-ie-mapping.md#73-e2sm-dapp-controloutcome). |
| 2.15 | An absent `timestamp` or `sequence-id` is not an error in the grammar, and `0` means absent in flexric's IR | grammar comments lines 15-29 | a present `0` cannot be sent by flexric. See [05-semantics-and-constraints.md](05-semantics-and-constraints.md#4-optional-fields-absent-is-not-an-error). |

## 3. Aborts and error handling in flexric

| # | What | Where |
|---|---|---|
| 3.1 | Every decoder asserts `RC_OK`: a malformed or truncated PDU aborts the process | `dapp_dec_asn.c:117,149,229,288,329,375,415,544` |
| 3.2 | The event trigger, action definition, control header, control message, control outcome and definition **encoders** assert instead of returning an error. Only the indication header and message encoders return an empty buffer | `dapp_enc_asn.c:87,122,406,446,507,663` against `232-236,350-354` |
| 3.3 | A control request without "ack requested" aborts the E2 agent | `flexric/src/agent/msg_handler_agent.c:401` |
| 3.4 | A subscription with more or fewer than one action, or with an insert or policy action, aborts the agent | `msg_handler_agent.c:232-233` |
| 3.5 | An unknown report style: OAI's `AssertError` returns an empty answer, then flexric's `assert(subs.type == SUBS_OUTCOME_SM_AG_IF_ANS_V0)` aborts | `oai/.../ran_func_dapp.c:478-479`; `dapp_sm_agent.c:74` |
| 3.6 | A deferred control with key `0` aborts the agent: the SM defers on whatever `sequence-id` the header holds, and the table asserts it is not `0` | `dapp_sm_agent.c:187-191`; `flexric/src/agent/pending_ctrl.c:49`. OAI avoids it by answering at once when the id is `0` (`ran_func_dapp.c:527-531`). |
| 3.7 | The RIC-side inner decode asserts: a format 1 indication whose `ran-function-id` has no decoder (only id `1`, Spectrum, is known) aborts the xApp. An unknown indication format aborts too | `flexric/src/sm/dapp_sm/dapp_sm_ric.c:128-131`, `140`; `e3/dapp_dec_e3.c:15` |
| 3.8 | The RIC-side control path aborts when the inner control payload cannot be encoded, and returns an **empty** request without error when `e3.type` is `DAPP_E3_SM_NONE` or the message already carries data | `dapp_sm_ric.c:189-212` |
| 3.9 | The RIC-side RIC Service Update handler does not decode the new definition | `dapp_sm_ric.c:259-265` |
| 3.10 | The first RIC Service Update repeats the revision of the E2 Setup | `e2_agent_api.c:175,198` against `dapp_sm_id.h:8`. The trigger also finds the SM by the literal `255` (`e2_agent_api.c:177`). **wrong** |
| 3.11 | The pending-control comment says unanswered controls are acknowledged "negatively", the sweep sends an outcome-less RIC Control Acknowledge | `pending_ctrl.h:78-82` against `e2_agent.c:674-695`. **doc** |
| 3.12 | Two parked controls with the same key: a completion releases one, the other waits for a sweep that nothing runs | `pending_ctrl.c:69-90`, with 1.6 |
| 3.13 | A fixed encode buffer means large elements abort | event trigger 512 KiB (`dapp_enc_asn.c:84`), action definition 1 KiB (119), indication header 1 KiB (228), indication message 128 KiB (344), control header 16 KiB (403), control message 16 KiB (443), control outcome 16 KiB (504), definition 16 KiB (660). A control message above about 16 KiB aborts although the grammar allows 32768 (the golden vector `cm1_large` stops at 16000). A definition with the map for 256 dApps of 64 functions each (about 83 KiB per copy, listed twice) would not fit either. |
| 3.14 | `time_now_ns()` returns `-1` on error and the OAI code uses it unchecked | `flexric/src/util/time_now_us.c:49-57`; `ran_func_dapp.c:271,397` |
| 3.15 | The OAI bridge does not clamp the dApp list to the grammar: more than 256 dApps, or a dApp with more than 64 functions, would reach flexric's encoder | `oai/.../ran_func_dapp.c:131-171` |
| 3.16 | `eq_e2sm_dapp_event_trigger` compares only the first argument's format | `flexric/src/sm/dapp_sm/ie/dapp_data_ie.c`, `eq_e2sm_dapp_event_trigger` | **latent** (there is one format) |

## 4. Consumers

| # | What | Where | Effect |
|---|---|---|---|
| 4.1 | `DAppControlOut.py` carries `# TODO need fixing` and **no free binding** for `free_e2sm_dapp_ctrl_out` ("TODO free function missing in library") | `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:37-42,52-54` | the `e3-control-outcome` buffer is leaked at every acknowledge. The likely reason: the dApp SM library is built with `-fvisibility=hidden` (`flexric/src/sm/dapp_sm/CMakeLists.txt`) and `free_e2sm_dapp_ctrl_out` (`dapp_data_ie.h:283`) has no `visibility("default")` attribute, unlike `free_e2sm_dapp_ind_hdr` (184), `free_e2sm_dapp_ind_msg` (211) and `free_e2sm_dapp_func_def` (313). **latent** |
| 4.2 | `xdevsm` mirrors flexric's C structs with `ctypes` field by field | `DAppControlOut.py:12-22`, `DAppE3IndPayload.py`, `DAppFunctionDef.py`, `DAppIndicationHdr.py` | the layouts of `libdapp_sm.so` are an **ABI**: a changed field order or type reads the wrong numbers without an error. The decoders return structs by value. |
| 4.3 | The `xdevsm` mirror of `ran_func_def_ev_trig_dapp_sm_t` has two fields; flexric's struct is empty | `xdevsm/.../dapp/report/DAppFunctionDef.py:22-34` against `flexric/.../ie/ir/ran_func_def_ev_trig.h:7-8` | reading `sz_seq_ev_trg_style` of a decoded definition that has the event trigger section reads outside the allocation. OAI omits the section, so it does not trigger today. **latent** |
| 4.4 | `xdevsm` finds the control style by the name `DAPP-CONTROL-STYLE-1` | `dapp_prb_mask_control.py:35`, `control.py` `send()` | a RAN that names it differently is refused. The name is free text in the grammar. |
| 4.5 | `xdevsm` sends a call process id of `1` in every control request | `control.py`, `send_control_request_rmr` | ignored by flexric. A conforming sender omits it. |
| 4.6 | `xdevsm` hard-codes RAN function id `255` | `dapp_report.py:48`, `dapp_prb_mask_control.py:32` | |
| 4.7 | The decision engine caches the decoded definition per gNB for its lifetime | `spectranet-xapps/decision_engine_xapp/decision_engine_xapp.py:868-880` | stale after a RIC Service Update. The engine takes the `dapp-id` from reports, so this does not matter today. |

## 5. Differences between the implementations

Behavior that differs between flexric, libe3 and OCUDU for the same input. The
golden vectors do not detect these, because every vector is a valid message and
none sets a present `0` or disagreeing sizes.

| # | Input | flexric | libe3 | OCUDU |
|---|---|---|---|---|
| 5.1 | present `timestamp` or `sequence-id` equal to `0` | encoded as absent | encoded as present `0` | encoded as present `0` |
| 5.2 | `data-size` different from the octet count | encoded as the IR value, decoded as the octet length | encoded and decoded as the field | encoded and decoded as the field |
| 5.3 | extension additions on a `SEQUENCE` | skipped by asn1c | skipped by asn1c | skipped as opaque open types (`skip_ext_additions`, `ocudu/lib/asn1/e2sm/e2sm_dapp.cpp`) |
| 5.4 | extension bit on a struct to be packed | not applicable | the `ext` member is never read when packing (`src/e2sm_dapp/e2sm_dapp.cpp` builds an asn1c struct with no extension additions); the bit is always `0` | `ext == true` makes `pack` fail |
| 5.5 | a `PrintableString` with a character outside the alphabet | encoded, no check | rejected on encode and decode | rejected on encode, accepted on decode |
| 5.6 | an `INTEGER` of exactly `2^(8k-1)`, for example a `SequenceId` of 128 | asn1c: correct (two octets `00 80`) | asn1c: correct | handled by `pack_i64`; the comment in the source says the generic srsRAN helpers mis-size it |
| 5.7 | an empty subscription list in a definition | **abort** | allowed | allowed |
| 5.8 | a dApp item with 0 functions | the asn1c encoder rejects it | rejected on encode and decode | rejected on encode and decode |
| 5.9 | the unbounded octet string above 16383 octets | not reachable (16 KiB buffer) | handed to asn1c, untested | rejected on encode |
| 5.10 | malformed input | **abort** | `ASN_ERROR_DECODE_FAIL` / `E3_DECODE_FAILED` | `OCUDUASN_ERROR_DECODE_FAIL` |
| 5.11 | the format number | enum from `0` | `to_number()` from `1` | `to_number()` from `1` |

### 5.1 OCUDU's E2 module against flexric and OAI

What the OCUDU service model (`ocudu/lib/e2/e2sm/e2sm_dapp/`) does differently from the
flexric and OAI pair, read in the code:

| # | Item | OCUDU | flexric and OAI |
|---|---|---|---|
| 5.12 | RAN function revision in the E2 Setup Request | `0` (`ocudu/lib/e2/common/e2ap_asn1_helpers.h:37`), though `e2sm_dapp_asn1_packer::revision` is `1` | `1` |
| 5.13 | dApp E3 subscription map in the definition | never present ("dynamic, so not part of the definition") | present in report style 1 and control style 1 when any dApp has a function |
| 5.14 | RIC Service Update for E2SM-DAPP | never sent | sent when the dApp set changes |
| 5.15 | Style names | `E3 Data Report`, `E3 Subscription Map`, `dApp Control` | OAI: `DAPP-E3-DATA-REPORT`, `DAPP-E3-SUBSCRIPTION-MAP`, `DAPP-CONTROL-STYLE-1` (the one `xdevsm` requires) |
| 5.16 | Event trigger section in the definition | present | absent in OAI |
| 5.17 | `timestamp` of the control outcome | echoes the **control header's** timestamp (`e2sm_dapp_control_service_impl.cpp`, `make_response`) | OAI: the time the outcome was built, on the gNB clock |
| 5.18 | Outcome on failure | RIC Control Failure with the sink's cause, outcome included | RIC Control Acknowledge only; no failure path exists |
| 5.19 | Control without "ack requested" | applied, not answered | flexric's agent aborts (`msg_handler_agent.c:401`) |
| 5.20 | `data-size` not equal to the octet count | an indication is dropped, a control is refused with `ctrl_msg_invalid` | not checked, ASN field ignored on decode |
| 5.21 | Reporting rate | one indication per subscription per 10 ms poll (`e2sm_dapp_report_poll_period_ms`) | event driven, one indication per report |
| 5.22 | Size caps | indication header plus message at most 16319 octets; `e3-control-outcome` at most 16287 octets (the E2AP codec cannot carry 16384 octets or more) | flexric's own buffers (section 3) |
| 5.23 | Where it is enabled | DU-high only, by `e2sm_dapp_enabled` | n/a |
| 5.24 | An unknown report style | not admitted in the RIC Subscription Response | aborts (3.5) |

### 5.2 OCUDU runtime helpers the port works around

These are limits of OCUDU's shared ASN.1 helpers (`include/ocudu/asn1/asn1_utils.h`),
found while hand-porting the grammar. They affect any other generated E2SM there, so
they are listed for whoever maintains that runtime. The DAPP codec does not depend on
them.

| # | Helper | Problem | What the DAPP codec does |
|---|---|---|---|
| 5.25 | `pack_unconstrained_integer` | Mis-sizes the values `2^(8k-1)`: `128` goes out as the single octet `0x80`, which a conforming peer reads as `-128`. Decode zero-extends, so negative values come back wrong | A local two's-complement encoder and decoder. `128` encodes as `02 00 80` |
| 5.26 | `pack_integer<uint8_t>` with upper bound 255 | Treats the field as unbounded and writes nothing | `node-type` is held in a `uint16_t` while packing |
| 5.27 | `unpack_integer`, extension path | Can reach `ocudu_assert` on a long length read from the wire | Out-of-root values are rejected before that path |
| 5.28 | `asn_string::unpack` | Ignores the result of its length decode, so an out-of-range length is accepted, and there is no alphabet check | A wrapper checks the size on decode, and the size and the PrintableString alphabet on encode |
| 5.29 | `unbounded_octstring`, `pack_length` / `unpack_length` | Cannot produce or parse OCTET STRINGs of 16384 octets or more | The codec refuses to emit such payloads (see 5.22) |

## 6. What the golden vectors prove and what they do not

* They prove that libe3 and OCUDU **encode** every listed input to flexric's exact
  bytes and **decode** those bytes back (see
  [integrator-guide.md](integrator-guide.md#the-contract-the-golden-vectors)).
* They are produced by flexric, so they inherit flexric's limits: no present `0`, no
  `ranFunction-Instance`, nothing above 16000 octets of control message, no
  `e3-control-outcome` longer than 10 octets, no extension bits.
* They say nothing about invalid input. Section 5 above is the list of places where
  the three codecs differ on it.
* The expected behavior on extension bits, trailing octets and outcome payloads above
  127 octets (two-octet length form) is derived from X.691 and the asn1c runtime, not
  from a vector.

## 7. Open items

| # | Item | Owner | Status |
|---|---|---|---|
| 7.1 | The real E3 link and the Spectrum codec are **out of scope** of the libe3 and OCUDU work: the E2SM-DAPP codec does not touch E3 | | not scheduled |
| 7.2 | Decide the `data-size` rule: equal and enforced, or ignored | the specification owners | open. Recommendation in the integrator guide: send equal values. |
| 7.3 | Define the `node-plmn-id` packing and the `node-type` values | the specification owners | open |
| 7.4 | Fix flexric's limits (2.2, 3.13) or document them as the limits of the C implementation | flexric maintainers | open |
| 7.5 | Wire `sweep_pending_control_agent_api` into a RAN, or remove it | flexric / RAN stacks | open |
| 7.6 | Add a free binding for the control outcome to `xdevsm` | `xdevsm` maintainers | open |
| 7.7 | Extend the golden vectors with cases flexric cannot produce (present zero, instance, long payload outcome), from a second source | | not scheduled |
| 7.8 | OCUDU's E3 side: the `e2sm_dapp_indication_source` and `e2sm_dapp_control_sink` that carry the dApp traffic (`ocudu/include/ocudu/e2/e2sm/e2sm_dapp.h`). Only the no-op versions exist. | OCUDU | not in the module |
