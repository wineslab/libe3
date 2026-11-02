# E2SM-DAPP: code pointers and evidence

This file belongs to the [integrator guide](integrator-guide.md). It is not part of the
specification. The specification chapters say what an implementation does. This file
says where each claim comes from: the code, the line, the repository and the
implementation. Each section answers one chapter of the specification.

Roots and revisions are in the first section. Line numbers drift when the repositories
change. Function names are the stable reference.

## Chapter README: Artifacts and code roots

Original text of the chapter before it was made repository neutral. Everything here carries the code references and named implementations that the specification text no longer has. Paste what you need into the integrator guide.


E2SM-DAPP is the E2 service model that carries dApp traffic between an E2 node (a
RAN) and an xApp. The RAN relays each dApp report to the xApp as a RIC Indication,
and relays each xApp decision to the dApp as a RIC Control. Its acknowledge reports
what applying the decision did on the air.

Until now the model existed only inside flexric. This directory is the standard:
the grammar, what the fields mean, and the encoding, so that a RAN or an xApp
written without flexric (OCUDU, a future stack) can interoperate with it. libe3
ships the grammar and a tested codec for it.

| Item | Value |
|---|---|
| E2 RAN function ID | `255` |
| RAN function revision | `1` (flexric, OAI; OCUDU currently announces `0`, see [06](06-discrepancies.md)) |
| Short name | `E2SM-DAPP` |
| OID | `1.3.6.1.4.1.53148.1.1.255.3` |
| Grammar | [`messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`](../../messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn), module `E2SM-DAPP-IEs`, APER (aligned) |
| Report styles | 1 = E3 data report, 2 = E3 subscription map |
| Control style | 1 |
| Not used | call process ID, INSERT, POLICY, QUERY |

Everything of substance is in the numbered files. This page only says where.

### Where each part is specified

| Section | What it covers | File |
|---|---|---|
| 1. Identity and registration | RAN function id, name, OID, revision, what the definition contains, how it is announced in E2 Setup and RIC Service Update | [01-identity-and-registration.md](01-identity-and-registration.md) |
| 2. E2AP IE mapping | every information element with fields, types, ranges, optionality, formats and styles; which service uses which format; the elements that are not used; the control outcome payload and the Spectrum worked example | [02-ie-mapping.md](02-ie-mapping.md) |
| 3. Procedure flow | discovery of dApp ids, subscription, indication delivery, control relay, the deferred acknowledge, the `sequence-id` chain, with the code of each step | [03-procedure-flow.md](03-procedure-flow.md) |
| 4. Encoding | APER rules, byte-level worked examples checked against the golden vectors, size limits, the type map across flexric, libe3 and OCUDU, build switches | [04-encoding.md](04-encoding.md) |
| 5. Semantics and constraints | format and style numbers, units and clocks, absent versus zero, validation, error paths, unknown ids and formats, what is mandatory | [05-semantics-and-constraints.md](05-semantics-and-constraints.md) |
| 6. Discrepancies, TODOs, dead code | where the code and the grammar disagree, flexric limits and aborts, dead code, differences between the implementations | [06-discrepancies.md](06-discrepancies.md) |

For an engineer adding E2SM-DAPP to a RAN stack there is a separate guide:
[integrator-guide.md](integrator-guide.md). It is not part of the specification.

### Where the artifacts are

| Artifact | Location |
|---|---|
| Grammar | `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`, installed as `share/libe3/e2sm_dapp/e2sm_dapp-1.0.0.asn` |
| C++ API (the OCUDU-compatible structs and `pack` / `unpack`) | `include/libe3/e2sm_dapp.hpp` |
| C API | `include/libe3/e2sm_dapp_c.h` |
| Codec sources | `src/e2sm_dapp/` |
| Golden vectors (the byte contract) | `tests/e2sm_dapp_golden.hpp` |
| Inputs of the vectors, shared byte for byte with OCUDU | `tests/e2sm_dapp_fixtures.inc` |
| Generator and regeneration script | `tools/e2sm_dapp_golden/` (`gen_golden.c`, `gen_header.py`, `regen.sh`, `README.md`) |
| Tests | `tests/test_e2sm_dapp_encode.cpp`, `test_e2sm_dapp_decode.cpp`, `test_e2sm_dapp_roundtrip.cpp`, `test_e2sm_dapp_c_api.cpp`, `test_e2sm_dapp_link_coexist.cpp`; out-of-tree consumer checks in `tests/consume/` |
| Build option | `LIBE3_ENABLE_E2SM_DAPP` (needs `LIBE3_ENABLE_ASN1`) |

### Related documents

* [path-b-e2-e3-loop.md](../control-loops/path-b-e2-e3-loop.md) and [path-a-e3-loop.md](../control-loops/path-a-e3-loop.md):
  the E2-E3 loop and its box numbers, which [03](03-procedure-flow.md) uses.
* [latrec.md](../utilities/latrec.md): the latency recorder, whose monotonic clock must not be
  mixed with the realtime timestamps of E2SM-DAPP.
* `messages/asn1/V1/e3ap-1.0.0.asn1`: E3AP, where `E3-SequenceID` is the same value
  as E2SM-DAPP's `sequence-id`.

### Where the code references point

Code references in these files are `path:line` or a function name that was opened
while writing. Roots and the revisions read:

| Short name | What it is | Revision read |
|---|---|---|
| `flexric/` | flexric with the `dapp_sm` service model | commit `71631466`, the one the golden vectors were generated from |
| `oai/` | OpenAirInterface (OAI) with its E2 agent bridge to E2SM-DAPP | `dc3d67360c`. Its flexric submodule is recorded at `71631466` |
| `xdevsm/` | `xDevSM-dApp` | `a6a4b00` |
| `spectranet-xapps/` | `spectranet-xapps` | `main`, as checked out |
| `libe3/` | this repository | the branch that introduced these files |
| `ocudu/` | OCUDU with the E2SM-DAPP port | the branch that introduced the port |

Line numbers drift when those repositories change. Function names are the stable
reference.

Claims that could not be verified from code are marked as such in the text.

## Chapter 1: Identity and registration (code evidence)

Roots are those of the README "Where the code references point" table: `flexric/`, `oai/`, `xdevsm/`, `libe3/`, `ocudu/`.

### Identity values and where they are defined

| Item | Value | Where it comes from |
|---|---|---|
| E2 RAN function ID | `255` | `flexric/src/sm/dapp_sm/dapp_sm_id.h:6` (`SM_DAPP_ID`). The E2AP range is 0..4095. |
| RAN function revision | `1` | `dapp_sm_id.h:8` (`SM_DAPP_REV`) |
| RAN function short name | `E2SM-DAPP` | `dapp_sm_id.h:10` (`SM_DAPP_SHORT_NAME`) |
| RAN function description | `DAPP Service Model for E2/E3 bridge` | `dapp_sm_id.h:19` (`SM_DAPP_DESCRIPTION`) |
| E2SM OID | `1.3.6.1.4.1.53148.1.1.255.3` | `dapp_sm_id.h:17` (`SM_DAPP_OID`), and the module identifier of the grammar |
| ASN.1 module | `E2SM-DAPP-IEs`, version 1 of the grammar | `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`, line 1 |
| Instance (`ranFunction-Instance`) | not used | the field exists in the grammar, nothing sets it |

### The OID comment

A comment in `flexric/src/sm/dapp_sm/dapp_sm_id.h:11-15` spells the same OID with `e2sm(2)`. It is a stale comment; the string constant on line 17 and the grammar both use `255`. See [06-discrepancies.md](06-discrepancies.md).

### Two kinds of "RAN function ID"

Typical values of the E3 RAN function ID in OAI: `1` for the Spectrum service model, `2` for the L1-KPM service model (`oai/openair2/E3AP/config/e3_config.h:8-9`).

### Where the identity is written down (section 1.3 of the old text)

| Place | What it holds | Notes |
|---|---|---|
| `flexric/src/sm/dapp_sm/dapp_sm_id.h` | id, revision, short name, OID, description | the only place flexric defines them |
| `flexric/src/agent/e2_agent_api.c:177` | the literal `255` | `trigger_ric_service_update_api` looks the SM up by the number, not by `SM_DAPP_ID` |
| `oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:47-51` | its own copy of the short name, OID and description (`E2SM-DAPP for xApp-dApp synchronization`) | **never reaches the wire**: flexric's agent replaces them (`dapp_sm_agent.c:258-259`) |
| `xdevsm/src/xdevsm/decorators/dApp/report/dapp_report.py:48` and `.../control/dapp_prb_mask_control.py:32` | the literal `255` | `xdevsm` finds the RAN function by this id, not by OID |
| `libe3/include/libe3/e2sm_dapp.hpp`, file comment | id `255` and the OID, as text | no constant is exported by libe3 |
| `ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.cpp:17-22` | `short_name`, `oid`, `func_description`, `ran_func_id` (255) and `revision` (1) as static members of `e2sm_dapp_asn1_packer`; the header comment of `ocudu/include/ocudu/asn1/e2sm/e2sm_dapp.h` repeats id and OID as text | the revision constant is **not** what OCUDU announces: see 3.1 |

### What a real RAN sends (old section 2.1)

The OAI RAN builds this definition (`ran_func_dapp.c:183-208`). The agent overwrites OAI's own name strings.

| Part | OAI value |
|---|---|
| event trigger | absent |
| report style 1 | type `1`, name `DAPP-E3-DATA-REPORT`, header format `1`, message format `1`, dApp E3 subscription map if any dApp has a subscribed function |
| report style 2 | type `2`, name `DAPP-E3-SUBSCRIPTION-MAP`, header format `2`, message format `2`, no map |
| control style 1 | type `1`, name `DAPP-CONTROL-STYLE-1`, header, message and outcome format `1`, the same map as report style 1 |

`flexric/test/rnd/fill_rnd_data_dapp.c` and `libe3/tests/e2sm_dapp_fixtures.inc` also build definitions, but they are **test data**, not what a RAN advertises. The random generator in flexric fills random names and random dApp lists only to exercise the codec (`fill_rnd_data_dapp.c:104-135,205-260`). The OAI copy of that file, `oai/openair2/E2AP/flexric/test/rnd/fill_rnd_data_dapp.c`, is the same kind of file. flexric's agent obtains the real definition from the RAN through `read_setup` (`dapp_sm_agent.c:244-275`), so the content is the RAN's responsibility.

OCUDU builds a fixed definition (`e2sm_dapp_asn1_packer::pack_ran_function_description`): the name block, an **event trigger section**, two report styles named `E3 Data Report` (formats 1 and 1) and `E3 Subscription Map` (formats 2 and 2), and one control style named `dApp Control` (formats 1, 1, 1). It has **no dApp E3 subscription map** in the definition. These are the names of the golden vectors, not OAI's: an xApp that selects the control style by the name `DAPP-CONTROL-STYLE-1` (`xdevsm`) does not find it on OCUDU.

### What flexric's agent adds or replaces (old section 2.2)

`on_e2_setup_dapp_sm_ag` (`dapp_sm_agent.c:244-275`) takes the definition the RAN returns and **replaces the name block** (`fill_ran_func_name`, lines 215-229) with `SM_DAPP_SHORT_NAME`, `SM_DAPP_OID` and `SM_DAPP_DESCRIPTION`, whatever the RAN put there. It asserts the short name is not empty (lines 261-262). Everything else, the styles, formats and map, is the RAN's. An emulator agent without a RAN callback gets a definition with only the name (`dapp_sm_agent.c:251-256`).

### E2 Setup Request (old section 3.1)

| E2AP RAN function item field | Value | Code |
|---|---|---|
| RAN Function ID | `255` | `flexric/src/agent/gen_msg_agent.c:56` (E2AP v1), `97` (v2, v3) |
| RAN Function Definition (`OCTET STRING`) | the APER encoding of `E2SM-DAPP-RANFunctionDefinition` | `gen_msg_agent.c:53-54`, `94-95` |
| RAN Function Revision | `1` in flexric and OAI. **OCUDU announces `0`** | `gen_msg_agent.c:57`, `98` (`sm->info.rev()`); OCUDU: `ocudu/lib/e2/common/e2ap_asn1_helpers.h:37`, a helper shared by all its service models, which ignores `e2sm_dapp_asn1_packer::revision` |
| RAN Function OID | `1.3.6.1.4.1.53148.1.1.255.3` | `gen_msg_agent.c:58-60`, `99` |

The definition is rebuilt each time the flexric agent builds an E2 Setup Request, so a retried setup carries the dApp map as it is at that moment. Both the E2AP OID field and the `ranFunction-E2SM-OID` string inside the definition are filled from the same constant (`SM_DAPP_OID`), so they cannot differ in flexric.

OCUDU lists E2SM-DAPP in the E2 Setup Request only when `e2sm_dapp_enabled` is set (`e2ap_asn1_helpers.h:138-148`), with RAN function id 255 and the OID of `e2sm_dapp_asn1_packer`, and with the fixed definition described above.

### RIC Service Update (old section 3.2)

| Item | Value | Code |
|---|---|---|
| Function list | **modified**, exactly one entry, RAN function 255 | `flexric/src/agent/e2_agent_api.c:185-205` |
| Definition | freshly built, as for E2 Setup | `e2_agent_api.c:183`, `on_e2_setup` |
| OID | `SM_DAPP_OID` | `e2_agent_api.c:202-204` |
| Revision | a counter that starts at `1` and increases by one per update. The first update therefore carries revision `1`, which the E2 Setup already used | `e2_agent_api.c:175,198` |
| Transaction id | the same counter modulo 256 | `e2_agent_api.c:186` |
| Trigger | the RAN calls `trigger_ric_service_update_api()`. OAI does so from `notify_dapp_status_changed`, only after the RIC has been heard from | `oai/.../ran_func_dapp.c:329-342` |

OCUDU sends no RIC Service Update for E2SM-DAPP: its definition is fixed and carries no dApp map.

| RIC | Behavior |
|---|---|
| flexric RIC | `on_ric_service_update_dapp_sm_ric` (`dapp_sm_ric.c:259-265`) is a placeholder. It returns an empty result and does **not** decode the new definition. |
| O-RAN SC RIC | the RIC's own E2 manager updates its stored node information. xApps that read the definition again (`xdevsm`: `get_ran_function_description`) see the new one; the dApp E3 subscription map in it is current. |

The agent-side callback `on_ric_service_update_dapp_sm_ag` (`dapp_sm_agent.c:283-287`) is a stub that returns nothing: it is the hook for RIC Service Update messages *received* from the RIC, which E2SM-DAPP does not use.

### Checklist notes (old section 4)

* The control style name `DAPP-CONTROL-STYLE-1` is what `xdevsm` xApps look for when they control the RAN.
* flexric's encoder cannot encode an empty list in the definition, and its decoder cannot decode more than 255 items or 63 functions; see [06-discrepancies.md](06-discrepancies.md).
* The flexric agent does not increase the revision for the first RIC Service Update.

## Chapter 2: E2AP information element mapping (code evidence)

Roots: `flexric/`, `oai/`, `xdevsm/`, `spectranet-xapps/`, `libe3/`, `ocudu/` as in the README table of roots.

### Style numbers, formats and names (old section 2, "Where each fact comes from")

* **Style numbers.** `DAPP_RIC_STYLE_E3_DATA_REPORT = 1` and `DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP = 2` are in `flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:293-296`. The report style a subscriber gets is chosen by `ric-Style-Type` in the action definition (documented in `flexric/src/sm/dapp_sm/dapp_sm_agent.c:35-38`). The style column of the table named the C constants `DAPP_RIC_STYLE_E3_DATA_REPORT` (report style 1) and `DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP` (report style 2).
* **Style to format.** The style-to-format mapping is carried in the RAN function definition by `ric-IndicationHeaderFormat-Type` and `ric-IndicationMessageFormat-Type` (style 1: `1` and `1`; style 2: `2` and `2`). The RAN that builds the definition fills these in. flexric's own `dapp_sm_agent.c` does not (it asks the RAN through `read_setup`), and its random test data (`flexric/test/rnd/fill_rnd_data_dapp.c:205-210`) does not define the standard: it only sets `ind_hdr_type = ind_msg_type = report_type`. The authoritative definition is the one a RAN actually sends. The OAI RAN sends the values above (`oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:76-86`).
* **Control style number.** OAI writes the control style number as `DAPP_RIC_STYLE_E3_DATA_REPORT` (`ran_func_dapp.c:107`), which is the value `1` with the name of a report style.
* **Style names in use**

  | Source | Report style 1 | Report style 2 | Control style 1 |
  |---|---|---|---|
  | OAI RAN (`ran_func_dapp.c:77,83,108`) | `DAPP-E3-DATA-REPORT` | `DAPP-E3-SUBSCRIPTION-MAP` | `DAPP-CONTROL-STYLE-1` |
  | flexric golden-vector generator and test data (`libe3/tests/e2sm_dapp_fixtures.inc`) | `E3 Data Report` | `E3 Subscription Map` | `dApp Control` |
  | OCUDU RAN (`ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.cpp`, `pack_ran_function_description`) | `E3 Data Report` | `E3 Subscription Map` | `dApp Control` |

  `xdevsm` finds the control style **by this name**: it looks for the string `DAPP-CONTROL-STYLE-1` in the definition (`xdevsm/src/xdevsm/decorators/dApp/control/dapp_prb_mask_control.py:35` and the loop in `xdevsm/src/xdevsm/decorators/control.py`, `send()`), and refuses to send if it is not there. A RAN that wants to work with `xdevsm` has to use that name.

### Why INSERT, POLICY and QUERY are "not used"

| Claim | Evidence |
|---|---|
| The RAN function definition has no Insert, Policy or Query section | The grammar's `E2SM-DAPP-RANFunctionDefinition` (lines 269-275) lists only name, event trigger, report and control |
| flexric's E2 agent accepts only a report action | `flexric/src/agent/msg_handler_agent.c:229-235`: `assert(sr->len_action == 1 ...)` and `assert(sr->action->type == RIC_ACT_REPORT ...)`. A subscription with an insert or policy action aborts the agent. |
| flexric's E2AP model has no query action type | `flexric/src/lib/e2ap/v3_01/e2ap_types/common/ric_action.h:31-35` lists `RIC_ACT_REPORT`, `RIC_ACT_INSERT`, `RIC_ACT_POLICY` and nothing else |
| The xApp framework only builds report actions | `xdevsm/src/xdevsm/decorators/dApp/report/dapp_report.py:155-158`: `action_type="report"` |
| Every indication is a REPORT indication | `flexric/src/agent/e2_agent.c:48-52` and `76-80`: `.type = RIC_IND_REPORT` |
| The event trigger needs no timer | the dApp SM answers every subscription with an aperiodic outcome (`dapp_sm_agent.c:75`, `ran_func_dapp.c:482-486`), so reports are driven by the RAN, not a period |

### Why there is no call process ID

| Claim | Evidence |
|---|---|
| The agent's indication path never sets it | `dapp_sm_agent.c:133-139` builds `exp_ind_data_t ret = {.has_value = true}` and fills only header and message. `flexric/src/agent/e2_agent.c:58-63` copies a call process ID only when the SM provides one. |
| The agent never reads it on a control request | `flexric/src/agent/msg_handler_agent.c:393-411` reads only the header, the message and the RIC ID. A search for `call_process_id` in that file finds nothing. |
| The agent writes none on the control acknowledge | `msg_handler_agent.c:436` and `flexric/src/agent/e2_agent.c:456`: `.call_process_id = NULL` |
| A sender may still put one | `xdevsm` sends the string `1` in every control request (`xdevsm/src/xdevsm/decorators/control.py`, parameter `call_process_id: bytes=b"1"` of `send_control_request_rmr`). It is ignored. A conforming sender omits it. |

### Common types

| ASN.1 type | Remark |
|---|---|
| `RIC-Style-Type` | flexric stores a `uint32_t`. |
| `RIC-Format-Type` | flexric stores a `uint32_t`. |

The E3AP grammar is `libe3/messages/asn1/V1/e3ap-1.0.0.asn1` (`E3-DAppID ::= INTEGER (1..65535)` and `ranFunctionIdentifier INTEGER (1..65535)`).

### Event trigger and action definition

* The agent decodes the empty event trigger and ignores it (`dapp_sm_agent.c:55-56`).
* Style 1 receives indication header and message format 1 only. Style 2 receives format 2 only. Both flexric and OAI behave this way (`dapp_sm_agent.c:35-38`; `oai/.../ran_func_dapp.c:463-480`).
* The action definition is optional in E2AP. The flexric agent copes with its absence (`dapp_sm_agent.c:59`), but the OAI RAN requires it and aborts without one (`ran_func_dapp.c:461`, `assert(... != NULL && "Action definition required")`). A subscription with a style number other than 1 or 2 makes OAI's `AssertError` (`ran_func_dapp.c:478-479`) log and return an empty answer, and flexric's agent then fails `assert(subs.type == SUBS_OUTCOME_SM_AG_IF_ANS_V0)` at `dapp_sm_agent.c:74`, because the empty answer has type `0` (`CTRL_OUTCOME_SM_AG_IF_ANS_V0`, `flexric/src/sm/agent_if/ans/sm_ag_if_ans.h:38`). In a build with assertions on, the gNB aborts.

### RAN function definition

| Field | Evidence |
|---|---|
| `ranFunctionDefinition-EventTrigger` | OAI omits it (`ran_func_dapp.c:189`: `def.ev_trig = NULL`). |
| `ranFunction-Instance` | flexric's encoder asserts it is `NULL` (`dapp_enc_asn.c:525`) and its decoder never reads it (`dapp_dec_asn.c:428-441`). |
| report `dappE3Subscriptions` | OAI attaches it to report style 1 only, and only when at least one dApp has at least one subscribed function (`ran_func_dapp.c:195-199`). |
| control `dappE3Subscriptions` | OAI copies it from report style 1 (`ran_func_dapp.c:201-204`). |

OAI builds the list from the E3 agent's subscription map and drops every dApp with zero subscribed functions (`ran_func_dapp.c:131-171`), which keeps the one-or-more rule of the grammar.

### Indication header

| Field | Evidence |
|---|---|
| `ran-function-id` | OAI copies it from the E3 report (`ran_func_dapp.c:393-395`). |
| `node-type` | OAI fills it with its own RRC node-type enum (`ran_func_dapp.c:403`). |
| `node-plmn-id` | OAI writes the high octet of the MCC, the low octet of the MCC, and the low octet of the MNC (`ran_func_dapp.c:406-408`). It is not the 3GPP BCD `PLMN-Identity` encoding. |
| `node-nb-id` | OAI uses the NR cell id shifted right by 14 (`ran_func_dapp.c:404`). |
| `node-cu-du-id` | OAI uses the gNB-DU id and omits it when the node identity is unknown (`ran_func_dapp.c:410-411`). |
| `timestamp` (format 1) | OAI: `time_now_ns()`, `ran_func_dapp.c:397`. |
| `sequence-id` (format 1) | copied from the E3 `E3-DAppReport.sequenceId` (`ran_func_dapp.c:400`). |
| unknown node identity | OAI leaves `node-type`, `node-plmn-id` and `node-nb-id` at zero and omits `node-cu-du-id` (`ran_func_dapp.c:381-412`). |
| `timestamp` (format 2) | OAI sets it (`ran_func_dapp.c:271`). |
| `sequence-id` (format 2) | OAI leaves it absent. |

### Control header and outcome

| Field | Evidence |
|---|---|
| `timestamp` | `xdevsm` sets `time.time_ns()` (`xdevsm/.../dapp/control/DAppControlReq.py:56`). |
| `sequence-id` absent or `0` | the OAI RAN answers immediately without applying anything (`oai/.../ran_func_dapp.c:527-531`). |
| outcome `timestamp` | OAI: `built_ts_ns`, taken once and used both here and in the inner payload (`oai/openair2/E3AP/e3_ctrl_ack_relay.c:67,76`). |

### The Spectrum outcome worked example (old section 8.2)

The text below was quoted in the chapter from the Spectrum service model's grammar, `flexric/src/sm/dapp_sm/e3/service_models/spectrum_sm/asn/defs/e3sm_spectrum.asn` lines 175-217 (the "APPLY OUTCOME STRUCTURES" part). It is owned by the Spectrum service model and licensed by its owners as `LicenseRef-CSSL-1.0` (Copyright (c) 2026 Institute for Intelligent Networked Systems (INSI) at Northeastern University). The chapter now keeps a field table and the hand-derived bytes, and leaves the grammar text out, because the Spectrum grammar goes into the Spectrum specification later. Paste this block into the integrator guide only if you decide the license allows it.

```text
-- ============================================================================
-- APPLY OUTCOME STRUCTURES
-- ============================================================================
--
-- What the RAN reports back about a control it applied. This is Service Model
-- territory, not E3AP or E2SM-DAPP territory: "applied" means something
-- different for every control this SM defines, and a PRB block is only "in
-- effect" once the scheduler has actually put the mask on the air, which is a
-- later instant than the handler returning. The E2SM-DAPP control outcome
-- carries this as an opaque octet string, the same way it carries a control.
--
-- Both timestamps are CLOCK_REALTIME nanoseconds at the gNB, chosen so the xApp
-- can difference them against Spectrum-DAppReportData.timestamp, which is on the
-- same clock. They are payload fields, deliberately not latrec records: latrec
-- is CLOCK_MONOTONIC and the two must never be differenced against each other.

Spectrum-PRBBlockApplyOutcome ::= SEQUENCE {
    -- Mask written into the MAC, i.e. the control handler's install returning.
    installedTimestamp      INTEGER OPTIONAL,
    -- First scheduler tick that put the mask on the air. Later than
    -- installedTimestamp by up to one slot, and by a stochastic amount: the
    -- install runs on the control thread, the tick on the MAC thread.
    onAirTimestamp          INTEGER OPTIONAL,
    -- The frame and slot of that tick, so a reader can place it in the radio
    -- frame rather than only on the wall clock.
    sfn                     INTEGER (0..1023) OPTIONAL,
    slot                    INTEGER (0..maxSlotIndex) OPTIONAL,
    ...
}

Spectrum-ApplyOutcomePayload ::= CHOICE {
    prbBlockApplyOutcome    Spectrum-PRBBlockApplyOutcome,
    ...
}

-- Top-level apply-outcome PDU, carried by the E2SM-DAPP control outcome.
--
-- timestamp is when the outcome itself was built, which is not when anything was
-- applied; the applied instants are in the payload.
Spectrum-ApplyOutcomeData ::= SEQUENCE {
    timestamp               INTEGER OPTIONAL,
    applyOutcomePayload     Spectrum-ApplyOutcomePayload
}
```

`maxSlotIndex` is 159 (`maxSlotsFrame` 160 minus 1, same file, lines 226-227).

How the RAN fills it (OAI): `oai/openair2/E3AP/service_models/spectrum_sm/spectrum_enc.c:280-336`, `spectrum_encode_apply_outcome`. `installedTimestamp` is present when it is not zero. `onAirTimestamp`, `sfn` and `slot` are present together when `onAirTimestamp` is not zero. The timestamp is present when it is not zero. The caller in `oai/openair2/E3AP/e3_ctrl_ack_relay.c:64-102` omits the whole payload when neither `installed_ts_ns` nor `on_air_ts_ns` is set, so a timed-out control with nothing known is acknowledged with an outcome that has `timestamp` and `sequence-id` and no payload.

### How a consumer reads the outcome (old section 8.3)

**Step 1, E2SM-DAPP.** Decode `E2SM-DAPP-ControlOutcome`. In flexric that is `dapp_dec_ctrl_out_asn()` (`dapp_dec_asn.c:405-426`), which returns an `e2sm_dapp_ctrl_out_t` by value. The fields the consumer reads are `timestamp_ns`, `sequence_id`, `e3_control_outcome` (pointer) and `e3_control_outcome_size`.

* **xDevSM** mirrors the C struct with `ctypes`: `e2sm_dapp_ctrl_out_frmt_1_t` in `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:12-22`, fields `timestamp_ns` (`c_int64`), `sequence_id` (`c_int64`), `e3_control_outcome` (`POINTER(c_uint8)`), `e3_control_outcome_size` (`c_uint32`), in that order. `DAppControlOutWrapper.decode()` calls `dapp_dec_ctrl_out_asn`. The file carries `# TODO need fixing` (line 37) and has no free binding: the `free_e2sm_dapp_ctrl_out` wrapper is commented out with `# TODO free function missing in library` (lines 41-42), so each decode leaks the payload buffer.
* `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlAck.py` (`decode_control_ack`, lines 63-102) peels the E2AP envelope, then the E2SM outcome, and returns a `DAppControlAck` with `timestamp_ns`, `sequence_id` and `apply_outcome` (raw `bytes`, or `None`). It converts `0` to `None` for the timestamp and the sequence id (flexric's "0 means absent"), and returns an ack with all of these `None` when the E2AP outcome field is empty or the decode fails.

**Step 2, the service model.**

* `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/e3/SpectrumApplyOutcome.py`, `decode_apply_outcome(payload)`: calls `spectrum_sm_dec_apply_outcome` in `libdapp_sm.so` (`flexric/.../spectrum_sm/asn/decoder.c:80-123`) and returns `(installed_ts_ns, on_air_ts_ns, sfn, slot)`. Missing values are `None`. `sfn` and `slot` are `None` unless an on-air timestamp is present.
* `spectranet-xapps/decision_engine_xapp/apply_outcome.py` wraps that helper and returns the same four-tuple. It returns `(None, None, None, None)` for an empty payload, when the helper is not installed, and when decoding raises (lines 35-44). It does not import the Spectrum grammar: it imports the helper from `xdevsm`, on the grounds that no Python code is generated for that grammar.
* The decision engine then does `mitigation_latency_ns = on_air_ts_ns - origin_ts_ns` and `install_latency_ns = installed_ts_ns - origin_ts_ns`, where `origin_ts_ns` is the dApp's report timestamp (`spectranet-xapps/decision_engine_xapp/data.py:242-245`, in `submit_control`).

Rule 4 for consumers named the free function: `free_e2sm_dapp_ctrl_out` (flexric).

### E3 RAN function id registry (old section 8.4)

The RAN stack assigns the ids (OAI: `E3_SM_ID_SPECTRUM 1`, `E3_SM_ID_KPM 2` in `oai/openair2/E3AP/config/e3_config.h:8-9`), and flexric's RIC-side inner decoder hard-codes `case 1` for Spectrum (`flexric/src/sm/dapp_sm/e3/dapp_dec_e3.c:15`).

### Where the elements are produced and consumed (old section 9)

| Element | Produced by | Consumed by |
|---|---|---|
| RAN function definition | RAN (OAI: `fill_dapp_ran_def`, `ran_func_dapp.c:183`) | RIC and xApp (flexric: `ric_on_e2_setup_dapp_sm_ric`; xDevSM: `DAppFunctionDefWrapper.decode`) |
| Event trigger, action definition | xApp (flexric: `on_subscription_dapp_sm_ric`; xDevSM: `DAppReport.subscribe`) | RAN (`on_subscription_dapp_sm_ag`) |
| Indication header and message | RAN (`on_indication_dapp_sm_ag`) | xApp (`on_indication_dapp_sm_ric`; xDevSM: `DAppReport.decode_message`) |
| Control header and message | xApp (`ric_on_control_req_dapp_sm_ric`; xDevSM: `DAppControlReqWrapper`) | RAN (`on_control_dapp_sm_ag`) |
| Control outcome | RAN (OAI: `send_deferred_ctrl_ack`) | xApp (`ric_on_control_out_dapp_sm_ric`; xDevSM: `decode_control_ack`) |

### The same elements in libe3 and OCUDU (old section 10)

The libe3 and OCUDU types mirror the elements one to one. The names and the differences are tabulated in [04-encoding.md](04-encoding.md) and [integrator-guide.md](integrator-guide.md#the-ocudu-to-libe3-c-interface). Grammar: `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`.

## Chapter 3: Procedure flow (code evidence moved out of 03-procedure-flow.md)

References use the roots of the README table "Where the code references point".

### The cast: who is who in the code

| Role | Evidence |
|---|---|
| dApp | an application next to the RAN, for example the Spectrum dApp. Speaks E3AP to the RAN, through libe3 |
| RAN | a gNB (OAI today, OCUDU in progress). It hosts the E3 agent, the E2 agent, and the E2-E3 bridge (the "dApp RAN function") between them |
| RIC | the near-RT RIC. E2AP to the RAN, RMR to xApps |
| xApp | an application on the RIC, for example the Spectranet decision engine. E2SM-DAPP, wrapped in E2AP by the RIC |

### Every message on the flow

| Claim | Code pointer |
|---|---|
| The RAN never sends a RIC Control Failure for E2SM-DAPP. `e2_send_control_failure` exists but nothing calls it. Every control, even one that was refused or that timed out, ends in a RIC Control Acknowledge | `flexric/src/agent/e2_agent.c:847` |

### Discovery: the RAN function definition (E2 Setup and RIC Service Update)

| Step | E2AP message | What happens | Code |
|---|---|---|---|
| RAN builds the definition | (inside S1 or S3) | The agent asks the RAN for the definition through `read_setup`, replaces the name, OID and description with its constants, and encodes it | `flexric/src/sm/dapp_sm/dapp_sm_agent.c`, `on_e2_setup_dapp_sm_ag` (lines 244-275; the RAN callback is called at 254-255, the name replaced at 258-259, the encode at 266) |
| RAN fills styles and map | | OAI: two report styles, one control style, no event trigger, the map copied into report style 1 and control style 1 | `oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c`: `fill_dapp_ran_def` (183-208), `build_dapp_e3_subs_from_map` (131-171), `read_dapp_setup_sm` (218-228) |
| E2 Setup | S1 | The definition goes in the RAN function item | `flexric/src/agent/gen_msg_agent.c:51-60` (E2AP v1), `92-99` (v2, v3) |
| RIC stores it | | flexric RIC: `ric_on_e2_setup_dapp_sm_ric` (`dapp_sm_ric.c:242-253`) decodes it. With xDevSM the xApp reads the definition from the E2 node information the RIC exposes (`json_ran_info["gnb"]["ranFunctions"]`) and decodes it itself | |
| xApp decodes it | | xDevSM: `get_ran_function_description` finds the RAN function with id 255 in the RIC's node information and calls `dapp_dec_func_def_asn` | `xdevsm/src/xdevsm/decorators/report.py:132-155`, `decorators/control.py:82-114`, `sm_framework/py_oran/dapp/report/DAppFunctionDef.py:147-151` |

* The definition is rebuilt on every E2 Setup Request. The OAI comment at `ran_func_dapp.c:331-336` says the agent regenerates the definition, including the current map, on every E2 SETUP REQUEST retry.
* If no dApp has any subscribed function, OAI omits `dappE3Subscriptions` instead of sending an empty list (`ran_func_dapp.c:142-145,195-205`). flexric's encoder cannot encode an empty list in the definition (see [06-discrepancies.md](06-discrepancies.md)).

### When the set of dApps changes

When a dApp connects or disconnects, OAI does two things on the E3 agent's thread (`oai/openair2/E3AP/e3_agent.c:80-98`, `on_dapp_status_changed`, which calls `notify_dapp_status_changed`, `ran_func_dapp.c:329-342`):

| Action | Code |
|---|---|
| RIC Service Update (S3): calls `trigger_ric_service_update_api()`, which rebuilds the definition and sends it as RAN function 255 under "modified" | `flexric/src/agent/e2_agent_api.c:168-208` |
| The update is skipped until the RIC has been heard from: `e2_ric_seen` is set by the first subscription or control | `ran_func_dapp.c:337-340`, `456`, `501` |
| Reason for the skip: the send is synchronous on the E3 thread and blocks while the SCTP association is still being set up | comment at `ran_func_dapp.c:331-336` |
| Format 2 indication (R2) for every style-2 subscription, holding the current map | `generate_e2_indication_dapp_e3_subscriptions`, `ran_func_dapp.c:237-323` |
| The list is built with every dApp that has at least one function, so it can be empty | `ran_func_dapp.c:250-255` |
| RIC Service Update revision starts at `1` and increases by one per update | `e2_agent_api.c:175,198` |
| The E2 Setup Request announced revision `1`, so the first update repeats it | `dapp_sm_id.h:8` |

### Discovery in every report header

| Claim | Code pointer |
|---|---|
| The Spectranet decision engine echoes the `dapp-id` and `ran-function-id` of the report it received. The ingestor stores `dapp_id` with each report | `spectranet-xapps/data_ingestor_xapp/data_ingestor_xapp.py:794,1223-1224` |
| The decision engine reads the `dapp_id` back with the detection and puts it in the control | `spectranet-xapps/decision_engine_xapp/decision_engine_xapp.py:836-887` |

### What discovery does not do

| Claim | Code pointer |
|---|---|
| Subscribing to style 2 does not return the current map: `write_subs_dapp_sm` only records the RIC request id | `ran_func_dapp.c:453-487` |
| The decision engine caches the decoded definition once per gNB. A RIC Service Update after that does not change what it holds | `decision_engine_xapp.py:868-880` |

### Subscription

| Step | E2AP message | E2SM-DAPP content | Code |
|---|---|---|---|
| xApp builds the request | P1 | event trigger: format 1 (empty). One action: type `report`, id 1, action definition `style = 1` or `2`. | flexric: `on_subscription_dapp_sm_ric` (`dapp_sm_ric.c:49-70`). xDevSM: `DAppReport.subscribe` (`dapp_report.py:122-165`), which builds `DAppActionDefWrapper.create_action_def_from_report_style` and sends through the RIC's subscription manager with `action_type="report"` and a "continue" subsequent action |
| RAN decodes it | | event trigger, action definition | `on_subscription_dapp_sm_ag` (`dapp_sm_agent.c:45-81`) |
| RAN registers the subscriber | | style 1 adds the RIC request id to the format-1 subscriber list, style 2 to the format-2 list | `write_subs_dapp_sm` (`ran_func_dapp.c:453-487`): `ric_subs_frmt1_add` or `ric_subs_frmt2_add` |
| Admission | P2 | the single action is always admitted | `msg_handler_agent.c:338-344`, `generate_subscription_response` (line 254) |
| Teardown | P3, P4 | | `e2ap_handle_subscription_delete_request_agent` (`msg_handler_agent.c:346-368`), `free_aperiodic_subscription` (`ran_func_dapp.c:439-442`) |

| Claim | Code pointer |
|---|---|
| The E2 agent insists on exactly one action per request and that it is a report | `msg_handler_agent.c:229-235` |
| A second request under the same RIC request id replaces the first | `msg_handler_agent.c:283-296` |
| The subscription is aperiodic | `dapp_sm_agent.c:75` |
| The RAN does not check that the dApp exists or has subscribed before relaying; `e3_send_xapp_control` reports only whether libe3 accepted the call | `oai/openair2/E3AP/e3_agent.c:288-310` |

### Indication delivery (report up)

| Box | Code |
|---|---|
| `A20`: the E3 report arrives from the dApp and is handed to the bridge | `oai/openair2/E3AP/e3_agent.c:33-54`, `e2_e3_bridge` |
| `B1` build: one indication per style-1 subscriber. Every subscriber gets every report, with no filter on dApp or function | `ran_func_dapp.c:355-431`, `generate_e2_indication_from_e3_dapp_report`; loop at 387-426 |
| `B1` encode: a header or message the encoder declines is dropped, not aborted | `on_indication_dapp_sm_ag` (`dapp_sm_agent.c:102-140`, lines 123-131) |
| `B1` send: RIC Indication, action id from the subscription, no sequence number, no call process ID | `generate_aindication`, `flexric/src/agent/e2_agent.c:42-65` |
| `B3` decode: for format 1 the inner payload is decoded using `ran-function-id` as the key | flexric: `on_indication_dapp_sm_ric` (`dapp_sm_ric.c:96-146`, inner decode at 128). xDevSM: `DAppReport.decode_message` (`dapp_report.py:64-119`) decodes the header and the message and hands both to the xApp callback. The inner decode is left to the xApp: `DAppE3IndPayloadWrapper` (`dapp/e3/DAppE3IndPayload.py`) |
| `B4`: the xApp processes the report | `spectranet-xapps/data_ingestor_xapp/data_ingestor_xapp.py:709-813` stores it; the decision engine reads it later |
| flexric's RIC-side decode asserts on a format 1 message whose inner payload cannot be decoded (for example an unknown `ran-function-id`), and on an unknown format | `dapp_sm_ric.c:130`, `dapp_sm_ric.c:140` |

### Control relay: policy down

| Box | Code |
|---|---|
| `B4` to `B5`: the xApp builds the control header and message | xDevSM: `DAppPrbMaskControl` and `DAppControlReqWrapper.generate_control_req_frmt_1` (`dapp_prb_mask_control.py:68-73`, `DAppControlReq.py:37-61`); flexric RIC: `ric_on_control_req_dapp_sm_ric` (`dapp_sm_ric.c:177-215`) |
| `B6`: RIC Control Request, ack requested | xDevSM: `send_control_request_rmr` (`decorators/control.py`) with `control_ack_request=1` |
| `B7`: the RAN's E2 agent decodes the request and aborts if it does not ask for an acknowledge | `e2ap_handle_control_request_agent`, `flexric/src/agent/msg_handler_agent.c:392-447` (the assert is at line 401) |
| `B8`: E2SM decode of the control header and message | `on_control_dapp_sm_ag` (`dapp_sm_agent.c:156-205`, decode at 172-173) |
| `B9`: the bridge relays the control over E3, or acknowledges at once without applying when the `sequence-id` is absent or `0`, the pending table is full, or the E3 send fails | OAI `write_ctrl_dapp_sm`, `ran_func_dapp.c:498-575` (immediate paths at 527-545 and 563-568; the call to `e3_send_xapp_control` at 539) |
| `B10`, `B11`: E3AP `E3-XAppControlAction` | `libe3/src/core/e3_agent.cpp:226-246`, `E3Agent::send_xapp_control` |

### Control relay: mitigation back to the air

| Box | Code |
|---|---|
| `B17` apply: the Spectrum service model writes the mask into the MAC and records the instant on the realtime clock, per `sequenceId` | `oai/openair2/E3AP/service_models/spectrum_sm/spectrum_sm.c:566-578` (`e3_pending_ctrl_installed`) |
| `B18` live on air: the first scheduler tick that uses the mask records the instant, plus the frame and slot | `oai/openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_prb_block.c:596-599`, calling `prb_block_on_air_hook` (`oai/openair2/E3AP/e3_ctrl_ack_relay.c:48-53`) |
| A mask replaced before any tick ran: the control is retired as superseded | `prb_block_superseded_hook` (`e3_ctrl_ack_relay.c:59-62`) |

### The deferred acknowledge

| Step | What happens | Code |
|---|---|---|
| 1 | `on_control_dapp_sm_ag` calls the RAN's write handler. When the RAN answers `NONE`, the agent returns a deferred result keyed by the control header's `sequence-id` | `flexric/src/sm/dapp_sm/dapp_sm_agent.c:180-191` |
| 2 | The E2 agent parks the RIC id under that key and returns "no message". If the table (64 entries) is full it acknowledges at once | `flexric/src/agent/msg_handler_agent.c:420-431`, `flexric/src/agent/pending_ctrl.h` (`PENDING_CTRL_MAX 64`) |
| 3 | OAI's `write_ctrl_dapp_sm` opened a pending control for the id before forwarding it, and answers `NONE` | `ran_func_dapp.c:527-552` |
| 4 | When the mask is on the air (B18), the relay thread builds the outcome and calls `complete_control_agent_api(sequence_id, bytes, len)` | `oai/openair2/E3AP/e3_ctrl_ack_relay.c:64-102` |
| 5 | `e2_agent_complete_control` claims the parked RIC id and queues the encoded outcome; the agent's own thread sends it as C2, a RIC Control Acknowledge | `flexric/src/agent/e2_agent.c:647-672` (queue), `e2_agent.c:443-471` (send) |
| 6 | The xApp receives C2 and reads `sequence-id` and the payload | xDevSM: `decode_control_ack` (`DAppControlAck.py:63-102`) |

| Timer | Where | Value | Effect |
|---|---|---|---|
| OAI pending-control timeout | `oai/openair2/E3AP/e3_pending_ctrl.c:24` | 150 ms | the control is retired with `ok = false`; the relay still sends a RIC Control Acknowledge, with `installed_ts_ns` only if the install had happened and no on-air instant |
| flexric pending-control timeout | `flexric/src/agent/pending_ctrl.h:53` | 200 ms | would answer with an outcome-less acknowledge, but nothing calls the sweep (`sweep_pending_control_agent_api` has no caller in flexric or in OAI's own sources), so only the OAI timer is in effect |

OAI's timer is shorter than flexric's by design: the comment at `e3_pending_ctrl.c:20-23` says it must stay under flexric's, so the RAN gives up first and the acknowledge still says something.

### The mitigation figure

| Claim | Code pointer |
|---|---|
| The xApp contributes no clock of its own to `mitigation_latency_ns` and `install_latency_ns`. It only moves the numbers and subtracts them | `spectranet-xapps/decision_engine_xapp/apply_outcome.py:8-11`, `data.py:242-245` |
| `control_sent_ts_ns` and `de_read_ts_ns` are the xApp's own clock and are used only against each other | same files |
| `origin_ts_ns` is the dApp report timestamp inside the report payload | `Spectrum-DAppReportData.timestamp` |

### The sequence-id chain

| Hop | Code |
|---|---|
| indication header copied by the RAN bridge | `ran_func_dapp.c:400` |
| xApp control echoed by the xApp | `xdevsm`: `set_sequence_id`, `dapp_prb_mask_control.py:52-55` |
| relayed control copied by the RAN bridge | `ran_func_dapp.c:519,539` |
| outcome set by the RAN | `e3_ctrl_ack_relay.c:77` |
| A `sequence-id` of `0` or absent: OAI acknowledges the control immediately without applying it | `write_ctrl_dapp_sm`, `ran_func_dapp.c:498-575` |

### The same flow in each implementation

| Step | flexric (agent in the RAN, RIC-side code in the RIC) | OAI RAN (on top of flexric) | xDevSM and xApps | OCUDU |
|---|---|---|---|---|
| Definition | `on_e2_setup_dapp_sm_ag`, `ric_on_e2_setup_dapp_sm_ric` | `fill_dapp_ran_def`, `read_dapp_setup_sm` | `DAppFunctionDefWrapper.decode` | `e2sm_dapp_asn1_packer::pack_ran_function_description` |
| Subscribe | `on_subscription_dapp_sm_ric`, `on_subscription_dapp_sm_ag` | `write_subs_dapp_sm` | `DAppReport.subscribe` | `e2sm_dapp_impl::action_supported`, `get_e2sm_report_service` |
| Indicate | `on_indication_dapp_sm_ag`, `on_indication_dapp_sm_ric` | `generate_e2_indication_from_e3_dapp_report`, `generate_e2_indication_dapp_e3_subscriptions` | `DAppReport.decode_message`, then the xApp's callback | `e2sm_dapp_report_service::collect_measurements`, polling `e2sm_dapp_indication_source::next_indication` |
| Control | `ric_on_control_req_dapp_sm_ric`, `on_control_dapp_sm_ag` | `write_ctrl_dapp_sm`, `e3_send_xapp_control` | `DAppPrbMaskControl`, `DAppControlReqWrapper` | `e2sm_dapp_control_service::execute_control_request`, calling `e2sm_dapp_control_sink::apply_control` |
| Acknowledge | `e2_agent_complete_control`, `ric_on_control_out_dapp_sm_ric` | `e3_ctrl_ack_relay.c`, `e3_pending_ctrl.c` | `decode_control_ack`, `decode_apply_outcome` | the E2 agent sends the acknowledge or failure when `apply_control`'s task completes; `e2sm_dapp_asn1_packer::pack_ric_control_response` |

* flexric has two halves. `dapp_sm_agent.c` runs in the gNB, `dapp_sm_ric.c` in a flexric RIC or flexric C xApp. They are in one shared library (`libdapp_sm.so`). `xdevsm` uses the same library for E2SM encode and decode through `ctypes`, with the E2AP half done by the O-RAN SC RIC's own stack (RMR, E2Term, subscription manager). So an O-RAN SC deployment never runs `dapp_sm_ric.c`.
* The RIC-side flexric control path is not symmetric with xDevSM. flexric's `ric_on_control_req_dapp_sm_ric` encodes the inner Service-Model payload itself (through `dapp_enc_e3_control`, Spectrum only, `dapp_sm_ric.c:194`), whereas xDevSM encodes it before calling the E2SM encoder. It also returns an all-zero request, silently, when `e3.type` is `DAPP_E3_SM_NONE` or when the control message already holds data (`dapp_sm_ric.c:189-212`: the `if` has no `else`).
* OCUDU has the E2 side only (`ocudu/include/ocudu/e2/e2sm/e2sm_dapp.h`, `ocudu/lib/e2/e2sm/e2sm_dapp/`). The E3 side is a seam: two interfaces, `e2sm_dapp_indication_source` and `e2sm_dapp_control_sink`, passed to `create_e2_du_agent` as `e2sm_dapp_endpoints`. Without them, no-op implementations advertise the RAN function, admit subscriptions, never report, and refuse every control with the E2AP cause "system not ready".

OCUDU, by step:

| Step | Behavior and pointer |
|---|---|
| Definition | A fixed one (`pack_ran_function_description`): the name block, an event trigger section, two report styles (`E3 Data Report` with formats 1 and 1, `E3 Subscription Map` with 2 and 2) and one control style (`dApp Control`, formats 1, 1, 1). The comment in the source says the dApp map "is dynamic, so it is not part of the definition", so `dappE3Subscriptions` is never present. OCUDU sends no RIC Service Update for E2SM-DAPP (nothing in `lib/e2` builds one for it), and the RAN function revision in its E2 Setup Request is `0`, from the shared helper `fill_ran_function_item` (`ocudu/lib/e2/common/e2ap_asn1_helpers.h:37`), although `e2sm_dapp_asn1_packer::revision` is `1` |
| Subscribe | `action_supported` admits a report action whose action definition has style 1 or 2 and refuses everything else, which the E2 agent then reports as not admitted (no abort). The event trigger gives a fixed 10 ms poll period (`e2sm_dapp_report_poll_period_ms`), because the E2 agent only runs periodic indications. At most one indication is sent per poll and subscription |
| Indicate | The report service asks the source for the next indication of its subscription, drops one whose formats do not match the style, drops a style-1 indication whose `data_size` differs from the octet count of `data`, and drops one whose header and message exceed 16319 octets together |
| Control | The control service rejects a control whose `data-size` differs from the octet count of `data` (cause `ctrl_msg_invalid`), otherwise hands it to the sink and waits for the sink's task. The RIC Control Acknowledge, or the RIC Control Failure with the sink's cause, goes out after that. A control without "ack requested" is applied and not answered (test `control_without_ack_request_is_applied_but_not_answered` in `ocudu/tests/unittests/e2/e2sm_dapp_test.cpp`) |
| Outcome | `make_response` always echoes the control header's `timestamp` and `sequence-id` into the outcome and adds the sink's `e3_ctrl_outcome` when it fits within 16287 octets. Unlike OAI it does not stamp the build time of the outcome, and it also sends the outcome on a failure |

## Chapter 4: Encoding (code evidence moved out of 04-encoding.md)

Code references use these roots, all at the commits listed in the README table "Where the code references point": `flexric/` is flexric with the `dapp_sm` service model, `libe3/` is this repository, `ocudu/` is OCUDU with the E2SM-DAPP port, `oai/` is OpenAirInterface, `xdevsm/` is `xDevSM-dApp`.

### What is on the wire

| Claim | Code pointer |
|---|---|
| The grammar is `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`. It is a byte-identical copy of flexric's grammar (checked with `diff`) | `flexric/src/sm/dapp_sm/ie/asn/e2sm_dapp_v0_standard.asn` |
| Plain and FlatBuffers encodings do not exist. flexric has stub files for them, and all 31 functions in them are `assert(0 != 0 && "Not implemented")` (see [06-discrepancies.md](06-discrepancies.md)) | `enc/dapp_enc_plain.c`, `dec/dapp_dec_plain.c`, `enc/dapp_enc_fb.c`, `dec/dapp_dec_fb.c` |

### Byte order, packing, length prefixes

| Topic | Evidence |
|---|---|
| Struct packing | The in-memory structs are ordinary C structs with natural alignment and no `packed` attribute (flexric `src/sm/dapp_sm/ie/ir/*.h`). |
| Trailing data | APER has no end marker. flexric and libe3 check only that the decode succeeded, not that every octet was consumed (`flexric/src/sm/dapp_sm/dec/dapp_dec_asn.c` asserts `RC_OK` only; libe3's `unpack_pdu` in `libe3/src/e2sm_dapp/e2sm_dapp.cpp` tests `rv.code`). OCUDU's `unpack_e2sm_dapp_ie` (`ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.h:35-39`) checks only the return code, so trailing octets are ignored there too. |

### A receiver that sees extension additions

| Implementation | Extension bit `1` on a `SEQUENCE` |
|---|---|
| flexric (asn1c) | asn1c reads the extension bitmap and skips additions it does not know (`SEQUENCE_decode_aper`, "Skip over overflow extensions", in the generated `constr_SEQUENCE_aper.c`), so the value decodes with the added fields ignored. |
| libe3 (asn1c) | Same, because libe3 uses the same runtime family. |
| OCUDU (hand-written codec) | `skip_ext_additions` (`ocudu/lib/asn1/e2sm/e2sm_dapp.cpp`, called at the end of every `unpack`) reads the additions as opaque open types (X.691 19.7 to 19.9) and drops them. `pack` refuses a struct with `ext == true` (`pack_unsupported_ext_flag`). |

A `CHOICE` alternative beyond the defined ones (a future format 3) is not skipped by any of them: asn1c fails the decode (`CHOICE_decode_aper`), and OCUDU's choice `unpack` returns `OCUDUASN_ERROR_DECODE_FAIL`. An extension bit of `1` on a constrained `INTEGER (0..4294967295, ...)` is rejected by libe3 and OCUDU (see [05-semantics-and-constraints.md](05-semantics-and-constraints.md)).

### Implementation limits tighter than the grammar

flexric encodes into fixed buffers and aborts when an element does not fit:

| Element | flexric buffer | Reference |
|---|---|---|
| Event trigger | 512 KiB | `flexric/src/sm/dapp_sm/enc/dapp_enc_asn.c:84` |
| Action definition | 1 KiB | `dapp_enc_asn.c:119` |
| Indication header | 1 KiB | `dapp_enc_asn.c:228` |
| Indication message | 128 KiB | `dapp_enc_asn.c:344` |
| Control header | 16 KiB | `dapp_enc_asn.c:403` |
| Control message | 16 KiB | `dapp_enc_asn.c:443` |
| Control outcome | 16 KiB | `dapp_enc_asn.c:504` |
| RAN function definition | 16 KiB | `dapp_enc_asn.c:660` |

A control message above about 16 KiB aborts flexric's encoder even though the grammar allows 32768. The vector `cm1_large` stops at 16000 octets for this reason (see `libe3/tests/e2sm_dapp_fixtures.inc:296`). The indication message and header encoders return an empty buffer instead of aborting (`dapp_enc_asn.c:232-236`, `350-354`). The others assert at lines 87, 122, 406, 446, 507 and 663.

### Worked examples: where the bytes come from

All the bytes in the worked examples are from `libe3/tests/e2sm_dapp_golden.hpp`. They were produced by flexric's own encoder at flexric commit `71631466`. Every example was re-derived by hand from the grammar and the rules, and all 31 vectors (27 inline and 4 by length and hash) were also reproduced by an independent bit-level encoder written for this document, which agreed byte for byte.

* Input of `ih1_min`: `libe3/tests/e2sm_dapp_fixtures.inc`, function `ih1_min`.

### Golden vectors (full list)

`libe3/tests/e2sm_dapp_golden.hpp` holds the bytes. `libe3/tests/e2sm_dapp_fixtures.inc` builds each input. Vectors too large to inline carry a length and an FNV-1a 64-bit hash instead (the offset basis is `14695981039346656037`, the prime is `1099511628211`; see `libe3/tools/e2sm_dapp_golden/gen_golden.c`, function `emit`).

### The type map across implementations

The same ASN.1 types appear in four code bases under four spellings. Field names in libe3's C++ and OCUDU are identical, on purpose (see [integrator-guide.md](integrator-guide.md)).

#### Type names

| ASN.1 type | libe3 C++ (`libe3::e2sm_dapp`, `include/libe3/e2sm_dapp.hpp`) | libe3 C (`include/libe3/e2sm_dapp_c.h`) | flexric IR (`src/sm/dapp_sm/ie/`) | OCUDU (`asn1::e2sm_dapp`, `include/ocudu/asn1/e2sm/e2sm_dapp.h`) |
|---|---|---|---|---|
| `E2SM-DAPP-EventTrigger` | `e2sm_dapp_event_trigger_s` | `libe3_e2sm_dapp_event_trigger_t` | `e2sm_dapp_event_trigger_t` | `e2sm_dapp_event_trigger_s` |
| `E2SM-DAPP-EventTrigger-Format1` | `e2sm_dapp_event_trigger_format1_s` | (no fields, folded into the enum) | `e2sm_dapp_ev_trg_frmt_1_t` | `e2sm_dapp_event_trigger_format1_s` |
| `E2SM-DAPP-ActionDefinition` | `e2sm_dapp_action_definition_s` | `libe3_e2sm_dapp_action_definition_t` | `e2sm_dapp_action_def_t` | `e2sm_dapp_action_definition_s` |
| `E2SM-DAPP-ActionDefinition-Format1` | `e2sm_dapp_action_definition_format1_s` | (no fields) | `e2sm_dapp_action_def_frmt_1_t` | `e2sm_dapp_action_definition_format1_s` |
| `E2SM-DAPP-IndicationHeader` | `e2sm_dapp_ind_hdr_s` | `libe3_e2sm_dapp_ind_hdr_t` | `e2sm_dapp_ind_hdr_t` | `e2sm_dapp_ind_hdr_s` |
| `E2SM-DAPP-IndicationHeader-Format1` | `e2sm_dapp_ind_hdr_format1_s` | `libe3_e2sm_dapp_ind_hdr_format1_t` | `e2sm_dapp_ind_hdr_frmt_1_t` | `e2sm_dapp_ind_hdr_format1_s` |
| `E2SM-DAPP-IndicationHeader-Format2` | `e2sm_dapp_ind_hdr_format2_s` | `libe3_e2sm_dapp_ind_hdr_format2_t` | `e2sm_dapp_ind_hdr_frmt_2_t` | `e2sm_dapp_ind_hdr_format2_s` |
| `E2SM-DAPP-IndicationMessage` | `e2sm_dapp_ind_msg_s` | `libe3_e2sm_dapp_ind_msg_t` | `e2sm_dapp_ind_msg_t` | `e2sm_dapp_ind_msg_s` |
| `E2SM-DAPP-IndicationMessage-Format1` | `e2sm_dapp_ind_msg_format1_s` | `libe3_e2sm_dapp_ind_msg_format1_t` | `e2sm_dapp_ind_msg_frmt_1_t` | `e2sm_dapp_ind_msg_format1_s` |
| `E2SM-DAPP-IndicationMessage-Format2` | `e2sm_dapp_ind_msg_format2_s` | `libe3_e2sm_dapp_ind_msg_format2_t` | `e2sm_dapp_ind_msg_frmt_2_t` | `e2sm_dapp_ind_msg_format2_s` |
| `E2SM-DAPP-ControlHeader` | `e2sm_dapp_ctrl_hdr_s` | `libe3_e2sm_dapp_ctrl_hdr_t` (format and fields in one struct) | `e2sm_dapp_ctrl_hdr_t` | `e2sm_dapp_ctrl_hdr_s` |
| `E2SM-DAPP-ControlHeader-Format1` | `e2sm_dapp_ctrl_hdr_format1_s` | (same struct) | `e2sm_dapp_ctrl_hdr_frmt_1_t` | `e2sm_dapp_ctrl_hdr_format1_s` |
| `E2SM-DAPP-ControlMessage` | `e2sm_dapp_ctrl_msg_s` | `libe3_e2sm_dapp_ctrl_msg_t` (format and data in one struct) | `e2sm_dapp_ctrl_msg_t` | `e2sm_dapp_ctrl_msg_s` |
| `E2SM-DAPP-ControlMessage-Format1` | `e2sm_dapp_ctrl_msg_format1_s` | (same struct) | `e2sm_dapp_ctrl_msg_frmt_1_t` | `e2sm_dapp_ctrl_msg_format1_s` |
| `E2SM-DAPP-ControlOutcome` | `e2sm_dapp_ctrl_outcome_s` | `libe3_e2sm_dapp_ctrl_outcome_t` (format and fields in one struct) | `e2sm_dapp_ctrl_out_t` | `e2sm_dapp_ctrl_outcome_s` |
| `E2SM-DAPP-ControlOutcome-Format1` | `e2sm_dapp_ctrl_outcome_format1_s` | (same struct) | `e2sm_dapp_ctrl_out_frmt_1_t` | `e2sm_dapp_ctrl_outcome_format1_s` |
| `DAppE3Subscription-Item` | `dapp_e3_subscription_item_s` | `libe3_e2sm_dapp_sub_item_t` | `dapp_e3_subscription_item_t` | `dapp_e3_subscription_item_s` |
| `DAppE3Subscription-List` | `dapp_e3_subscription_list_l` | `libe3_e2sm_dapp_sub_list_t` | `dapp_e3_subscription_list_t` | `dapp_e3_subscription_list_l` |
| `RANfunction-Name` | `ran_function_name_s` | `libe3_e2sm_dapp_ran_function_name_t` | `ran_function_name_t` (`flexric/src/lib/sm/ie/ran_function_name.h`, shared with other service models) | `ran_function_name_s` |
| `RANFunctionDefinition-Report-Item` | `ran_function_definition_report_item_s` | `libe3_e2sm_dapp_report_style_t` | `seq_report_sty_dapp_sm_t` | `ran_function_definition_report_item_s` |
| `RANFunctionDefinition-Control-Item` | `ran_function_definition_ctrl_item_s` | `libe3_e2sm_dapp_ctrl_style_t` | `seq_ctrl_style_dapp_sm_t` | `ran_function_definition_ctrl_item_s` |
| `RANFunctionDefinition-Report` | `ran_function_definition_report_s` | (array inside the definition struct) | `ran_func_def_report_dapp_sm_t` | `ran_function_definition_report_s` |
| `RANFunctionDefinition-Control` | `ran_function_definition_ctrl_s` | (array inside the definition struct) | `ran_func_def_ctrl_dapp_sm_t` | `ran_function_definition_ctrl_s` |
| `RANFunctionDefinition-EventTrigger` | `ran_function_definition_event_trigger_s` | `has_event_trigger` flag | `ran_func_def_ev_trig_dapp_sm_t` | `ran_function_definition_event_trigger_s` |
| `E2SM-DAPP-RANFunctionDefinition` | `e2sm_dapp_ran_function_definition_s` | `libe3_e2sm_dapp_ran_function_definition_t` | `e2sm_dapp_func_def_t` | `e2sm_dapp_ran_function_definition_s` |

#### How the same field is held

| ASN.1 field | flexric IR | libe3 C++ | libe3 C | OCUDU |
|---|---|---|---|---|
| `ran-function-id`, `dapp-id` (0..4294967295) | `uint32_t` | `uint64_t` | `uint32_t` | `uint64_t` |
| `node-type` (0..255) | `uint8_t` | `uint8_t` | `uint8_t` | `uint8_t` |
| `node-plmn-id` (SIZE(3)) | `uint8_t[3]` | `fixed_octstring<3, true>` (a `std::array<uint8_t, 3>`) | `uint8_t[3]` | `fixed_octstring<3, true>` |
| `node-nb-id` (0..4294967295) | `uint32_t` | `uint64_t` | `uint32_t` | `uint64_t` |
| `node-cu-du-id` OPTIONAL | `bool node_cu_du_id_present` + `uint64_t` | `node_cu_du_id_present` + `uint64_t` | `has_node_cu_du_id` + `uint32_t` | `node_cu_du_id_present` + `uint64_t` |
| `timestamp` OPTIONAL | `int64_t timestamp_ns`, **0 means absent** | `timestamp_present` + `int64_t` | `has_timestamp` + `int64_t` | `timestamp_present` + `int64_t` |
| `sequence-id` OPTIONAL | `int64_t sequence_id`, **0 means absent** | `sequence_id_present` + `int64_t` | `has_sequence_id` + `int64_t` | `sequence_id_present` + `int64_t` |
| `data-size` | `size_t data_size` (indication), `uint32_t data_size` (control) | `uint16_t data_size` | none, it is `data.len` | `uint16_t data_size` |
| `data` | `uint8_t* data` | `std::vector`-like `bounded_octstring<1, 32768, true>` | `libe3_e2sm_dapp_bytes_t data` | `bounded_octstring<1, 32768, true>` |
| `e3-control-outcome` OPTIONAL | `uint8_t*` + `uint32_t e3_control_outcome_size`, **NULL and 0 mean absent** | `e3_ctrl_outcome_present` + `unbounded_octstring<true>` | `has_e3_control_outcome` + bytes | `e3_ctrl_outcome_present` + `unbounded_octstring<true>` |
| `ric-style-type`, format and style types | `uint32_t` | `int64_t` | `int64_t` | `int64_t` |
| `ranFunction-Instance` OPTIONAL | `ran_function_name_t::instance` pointer, **encoder asserts it is NULL** | `ran_function_instance_present` + `int64_t` | `has_instance` + `int64_t` | `ran_function_instance_present` + `int64_t` |
| the format choice | `enum` named `FORMAT_1_...`, values start at **0** | `type()` of an `enumerated`, `to_number()` is the wire format number, starting at **1** | `enum`, values 1, 2, ... | same as libe3 C++ |

Two traps:

* **flexric's format enums start at 0 and the wire's format numbers start at 1.** `FORMAT_1_E2SM_DAPP_IND_HDR` is `0` in memory (`flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:168-173`). Python code that mirrors these enums through `ctypes` has to use the same numbering (`xdevsm/src/xdevsm/sm_framework/py_oran/dapp/enums.py`). The enum value is not the same as the `RIC-Format-Type` carried in the definition, which is `1` and `2`.
* **A zeroed flexric struct is a valid format 1.** Because `FORMAT_1_..._IND_MSG` is `0`, an all-zero `e2sm_dapp_ind_msg_t` reads as "format 1, no data". The encoder guards against this (`dapp_enc_asn.c:295-321`).

#### The struct layout of flexric is an ABI

`xdevsm` does not link against a header. It mirrors flexric's C structs field by field with `ctypes`, and calls `dapp_dec_*_asn` and `dapp_enc_*_asn` in `libdapp_sm.so`. The decoders return the structs by value. A mirror whose field order or type differs from `libdapp_sm.so` reads the wrong offsets and does not complain. Examples: `e2sm_dapp_ctrl_out_frmt_1_t` in `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:12-22`, and `spectrum_sm_report_t` in `.../dapp/e3/DAppE3IndPayload.py`. Treat the IR structs in `flexric/src/sm/dapp_sm/ie/ir/*.h` as frozen between releases of `libdapp_sm.so`.

### Build switches

#### flexric: `SM_ENCODING_DAPP`

| Item | Value | Reference |
|---|---|---|
| CMake variable | `SM_ENCODING_DAPP`, a cache string | `flexric/CMakeLists.txt:317-319` |
| Allowed values | `ASN` only (the property `STRINGS` lists nothing else) | `CMakeLists.txt:318` |
| What `PLAIN` does if forced | `src/sm/dapp_sm/CMakeLists.txt` has a `PLAIN` branch that builds `enc/dapp_enc_plain.c`, and every function in it asserts | `dapp_enc_plain.c` |
| What `FLATBUFFERS` does if forced | `message(FATAL_ERROR "DAPP SM FB not implemented")` | `src/sm/dapp_sm/CMakeLists.txt` |
| asn1c invocation | `/opt/asn1c/bin/asn1c -no-gen-BER -no-gen-UPER -no-gen-OER -no-gen-JER -fcompound-names -no-gen-example -findirect-choice -fno-include-deps` | `src/sm/dapp_sm/ie/asn/CMakeLists.txt` |
| Compile definitions on the SM | `-DASN_DISABLE_OER_SUPPORT -DASN_DISABLE_JER_SUPPORT` and the encoding name (`ASN`) as a public definition | `src/sm/dapp_sm/CMakeLists.txt` |
| Generated sources | written into the source tree at configure time (`execute_process` with `-D ${CMAKE_CURRENT_SOURCE_DIR}`) | `ie/asn/CMakeLists.txt` |
| Agent-side switch | the agent file is built with `ASN`, `FLATBUFFERS` or `PLAIN` defined and falls through to `static_assert(false, ...)` | `dapp_sm_agent.c:16-24` |
| E3 relay | the `E3_AGENT` macro adds the real I/O to the control and setup paths | `dapp_sm_agent.c:72-79`, `179-201`, `251-256` |

#### libe3: `LIBE3_ENABLE_E2SM_DAPP`

| Item | Value | Reference |
|---|---|---|
| CMake option | `LIBE3_ENABLE_E2SM_DAPP`, default `ON` | `libe3/cmake/libe3Options.cmake:26` |
| Dependency | needs `LIBE3_ENABLE_ASN1`; if that is `OFF` the option is forced `OFF` with a status message | `libe3Options.cmake:27-30` |
| Targets | `libe3::e2sm_dapp` (static) and `libe3::e2sm_dapp_shared` | `libe3/cmake/libe3Targets.cmake` |
| Compile definition | `LIBE3_ENABLE_E2SM_DAPP`, public | `libe3Targets.cmake` |
| asn1c invocation | `-pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example` | `libe3/messages/CMakeLists.txt:91` |
| asn1c version | the fork `mouse07410/asn1c`, commit `940dd5fa9f3917913fd487b13dfddfacd0ded06e` | `libe3/build_libe3:195-227` |
| Generated sources | in the build tree: `${PROJECT_BINARY_DIR}/generated/e2sm_dapp` | `messages/CMakeLists.txt:86` |
| asn1c runtime | a private copy, symbols hidden, so a consumer that links its own asn1c (flexric does) does not clash | `messages/CMakeLists.txt:76-82` |

Targets: `libe3_e2sm_dapp` and `libe3_e2sm_dapp_shared` with the aliases above (`cmake/libe3Targets.cmake:190-209`). The install ships the headers `e2sm_dapp.hpp` and `e2sm_dapp_c.h`, the grammar under `share/libe3/e2sm_dapp/` and a `libe3_e2sm_dapp.pc` (`cmake/libe3Install.cmake:98-161`). `find_package(libe3 CONFIG REQUIRED COMPONENTS e2sm_dapp)` finds the component only if the install has the codec (`cmake/libe3Config.cmake.in:30-50`).

#### OCUDU: `e2sm_dapp_enabled`

OCUDU does not use asn1c for E2SM-DAPP. The codec is hand-written in the style of OCUDU's other E2SM headers: `include/ocudu/asn1/e2sm/e2sm_dapp.h` declares the structs and `lib/asn1/e2sm/e2sm_dapp.cpp` implements `pack` and `unpack` on OCUDU's `bit_ref` and `cbit_ref`. It is built into `e2ap_asn1` (`ocudu/lib/asn1/CMakeLists.txt`, the `add_library(e2ap_asn1 ...)` line).

The run-time switch is `e2sm_dapp_enabled`, default `false`:

| Item | Reference |
|---|---|
| `e2ap_configuration::e2sm_dapp_enabled` | `ocudu/include/ocudu/e2/e2ap_configuration.h:27` |
| application config `e2_config::e2sm_dapp_enabled` | `ocudu/apps/helpers/e2/e2_appconfig.h:33` |
| command line `--e2sm_dapp_enabled`, and the YAML key of the same name in the E2 section | `apps/helpers/e2/e2_cli11_schema.cpp:35`, `e2_config_yaml_writer.cpp:20` |
| copied into the E2AP configuration for the DU-high only; the CU-CP and CU-UP translators set it `false` | `apps/units/flexible_o_du/o_du_high/e2/o_du_high_e2_config_translators.cpp:24`, `apps/units/o_cu_cp/e2/o_cu_cp_e2_config_translators.cpp:20`, `apps/units/o_cu_up/e2/o_cu_up_e2_config_translators.cpp:24` |
| effect: `create_e2_du_agent` registers the service model (RAN function 255, the OID) and the E2 Setup Request lists it | `lib/e2/common/e2_du_factory.cpp:90-102`, `lib/e2/common/e2ap_asn1_helpers.h:138-148` |

With the flag on and no E3 link attached, the agent advertises the RAN function, admits subscriptions, never sends an indication, and refuses every control (see [03-procedure-flow.md](03-procedure-flow.md)).

### Reproducing and checking an encoding

* To regenerate the golden vectors from flexric: [integrator-guide.md](integrator-guide.md#reproducing-the-golden-vectors).
* To check a stack: encode every fixture and compare with the hex in `tests/e2sm_dapp_golden.hpp`, then decode every hex and compare with the fixture ([checklist](integrator-guide.md#checklist-for-a-new-stack)).

## Chapter 5: Semantics and constraints

Original text of the chapter before it was made repository neutral. Everything here carries the code references and named implementations that the specification text no longer has. Paste what you need into the integrator guide.


What the numbers mean, the units and clocks, which rules a message must satisfy,
what the three implementations check, and what each one does when the input is bad
or unknown.

This is section 5 of the specification. The index is [README.md](../e2sm-dapp/README.md). Where
the code and the grammar disagree, the disagreement is listed in
[06-discrepancies.md](06-discrepancies.md) and only referenced here.

### 1. Numbering: formats and styles

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

### 2. Identifiers

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

### 3. Clocks and units

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
recorder (`docs/utilities/latrec.md`) stamps `CLOCK_MONOTONIC` and its rule is that a payload
timestamp and a latrec `t_ns` must not be subtracted
([latrec.md](../utilities/latrec.md), the section on clocks; lines 103-112). The Spectrum
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

### 4. Optional fields: absent is not an error

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

### 5. What the code treats as mandatory

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

### 6. What each implementation checks

#### 6.1 On encode

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

#### 6.2 On decode

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

#### 6.3 Error paths in summary

| Implementation | How a failure is reported | What a caller must do |
|---|---|---|
| flexric | `assert()` in the codecs and in the IR helpers. In a build with assertions enabled the **process aborts**, which for the E2 agent is the gNB. A few paths return an empty `byte_array_t` instead: the indication header and message encoders. | validate input before calling; treat the flexric encoders as unsafe on untrusted data. The agent's own comment says why the indication path was changed: aborting "would take the gNB down for one bad report" (`dapp_sm_agent.c:114-122`). |
| libe3 C++ | `pack` and `unpack` return `libe3::e2sm_dapp::asn_code`: `ASN_SUCCESS` (0), `ASN_ERROR_DECODE_FAIL` (-1), `ASN_ERROR_ENCODE_FAIL` (-2). `unpack` leaves the target untouched on failure. Choice getters throw `std::bad_variant_access` when the other alternative is held. | check the code |
| libe3 C | the functions return `e3_error_t`: `E3_INVALID_PARAM`, `E3_ENCODE_FAILED`, `E3_DECODE_FAILED`, `E3_SM_ERROR_MEMORY`, or 0 (`include/libe3/e2sm_dapp_c.h:19-20`). On a failed decode the output is zeroed and nothing needs freeing. | check the code |
| OCUDU | `pack` and `unpack` return `OCUDUASN_CODE`: `OCUDUASN_SUCCESS`, `OCUDUASN_ERROR_ENCODE_FAIL`, `OCUDUASN_ERROR_DECODE_FAIL`. A `log_error` line says why. | check the code |

The libe3 `asn_code` has "the same values as OCUDU's `OCUDUASN_CODE`"
(`include/libe3/e2sm_dapp.hpp:47-52`).

### 7. Behavior on unknown ids and formats

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
| Style 2 subscription when no dApp has ever connected | | | accepted. No indication is sent until the set changes (see [03-procedure-flow.md](03-procedure-flow.md#what-discovery-does-not-do)). |

A consumer written for a new stack should be stricter than flexric: decode
failures return errors, and unknown ids are logged and ignored.

### 8. Control semantics

#### 8.1 What "applied" means, and what the acknowledge says

* The acknowledge is a **RIC Control Acknowledge** that carries an
  `E2SM-DAPP-ControlOutcome`. It is sent when the procedure ends, at B19, and not
  when the RAN received the request, at B7
  ([03-procedure-flow.md](03-procedure-flow.md#the-e2-acknowledge-is-deferred-to-b19)).
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

#### 8.2 What a consumer must do

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

#### 8.3 Ordering and replacement

* Two controls in flight with the same `sequence-id` are not distinguishable by the
  agent: the pending table is keyed by `sequence-id` alone. A completion releases the
  first slot with that key and leaves the other until a timeout sweep retires it, and
  nothing runs that sweep (`flexric/src/agent/pending_ctrl.c:92-114`; see
  [06-discrepancies.md](06-discrepancies.md)).
* A later Spectrum install replaces an earlier mask, which retires the earlier
  control as superseded. Controls are not queued per dApp.
* A re-installed subscription under the same RIC request id replaces the old one
  (`msg_handler_agent.c:283-296`).

### 9. Rules that cross elements

| Rule | Between | Notes |
|---|---|---|
| The `sequence-id` of the control header equals the `sequence-id` of the indication header that prompted it | indication header, control header | the xApp echoes it |
| The `sequence-id` of the control outcome equals the control header's | control header, control outcome | the RAN copies it |
| `data-size` is the octet count of `data` | `data-size`, `data` | not enforced by libe3 or OCUDU, ignored on decode by flexric. A sender should always send equal values. |
| `ran-function-id` in the control header is the E3 id of the service model of the control message and of the outcome payload | control header, control message, outcome | the outcome does not repeat it |
| the indication message format matches the indication header format | indication header, indication message | format 1 with format 1, format 2 with format 2. flexric's RIC aborts on a mismatch (`dapp_sm_ric.c:122`, `137`). |
| a report style's formats match the definition | definition, indications | style 1 produces format 1 and 1, style 2 produces 2 and 2 |
| the definition's dApp map and the format 2 map describe the same set | definition, format 2 message | both are built from the same E3 subscription map |

## Chapter 6: Discrepancies, TODOs and dead code

Original text of the chapter before it was made repository neutral. Everything here carries the code references and named implementations that the specification text no longer has. Paste what you need into the integrator guide.


Everything found while writing this specification where code and grammar disagree,
where an implementation is incomplete, or where code is dead. Each entry says what
was read, with `file:line`, and what it means for someone implementing or consuming
E2SM-DAPP. Nothing here is a design decision; those are in the other sections.

This is section 6 of the specification. The index is [README.md](../e2sm-dapp/README.md). Code
roots are in [04-encoding.md](04-encoding.md): `flexric/` is flexric with the
`dapp_sm` service model at `71631466`, `oai/` is OpenAirInterface, `xdevsm/` is
`xDevSM-dApp`, `ocudu/` is OCUDU with the E2SM-DAPP port.

Severity: **abort** means an `assert()` or equivalent takes the process down in a build
with assertions on, **wrong** means a wrong result with no error, **latent** means
only a corner case triggers it, **doc** means documentation only.

### 1. Dead code

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

### 2. Where the grammar and the code disagree

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

### 3. Aborts and error handling in flexric

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

### 4. Consumers

| # | What | Where | Effect |
|---|---|---|---|
| 4.1 | `DAppControlOut.py` carries `# TODO need fixing` and **no free binding** for `free_e2sm_dapp_ctrl_out` ("TODO free function missing in library") | `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:37-42,52-54` | the `e3-control-outcome` buffer is leaked at every acknowledge. The likely reason: the dApp SM library is built with `-fvisibility=hidden` (`flexric/src/sm/dapp_sm/CMakeLists.txt`) and `free_e2sm_dapp_ctrl_out` (`dapp_data_ie.h:283`) has no `visibility("default")` attribute, unlike `free_e2sm_dapp_ind_hdr` (184), `free_e2sm_dapp_ind_msg` (211) and `free_e2sm_dapp_func_def` (313). **latent** |
| 4.2 | `xdevsm` mirrors flexric's C structs with `ctypes` field by field | `DAppControlOut.py:12-22`, `DAppE3IndPayload.py`, `DAppFunctionDef.py`, `DAppIndicationHdr.py` | the layouts of `libdapp_sm.so` are an **ABI**: a changed field order or type reads the wrong numbers without an error. The decoders return structs by value. |
| 4.3 | The `xdevsm` mirror of `ran_func_def_ev_trig_dapp_sm_t` has two fields; flexric's struct is empty | `xdevsm/.../dapp/report/DAppFunctionDef.py:22-34` against `flexric/.../ie/ir/ran_func_def_ev_trig.h:7-8` | reading `sz_seq_ev_trg_style` of a decoded definition that has the event trigger section reads outside the allocation. OAI omits the section, so it does not trigger today. **latent** |
| 4.4 | `xdevsm` finds the control style by the name `DAPP-CONTROL-STYLE-1` | `dapp_prb_mask_control.py:35`, `control.py` `send()` | a RAN that names it differently is refused. The name is free text in the grammar. |
| 4.5 | `xdevsm` sends a call process id of `1` in every control request | `control.py`, `send_control_request_rmr` | ignored by flexric. A conforming sender omits it. |
| 4.6 | `xdevsm` hard-codes RAN function id `255` | `dapp_report.py:48`, `dapp_prb_mask_control.py:32` | |
| 4.7 | The decision engine caches the decoded definition per gNB for its lifetime | `spectranet-xapps/decision_engine_xapp/decision_engine_xapp.py:868-880` | stale after a RIC Service Update. The engine takes the `dapp-id` from reports, so this does not matter today. |

### 5. Differences between the implementations

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

#### 5.1 OCUDU's E2 module against flexric and OAI

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

#### 5.2 OCUDU runtime helpers the port works around

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

### 6. What the golden vectors prove and what they do not

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

### 7. Open items

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

