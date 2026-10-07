# E2SM-DAPP: identity and registration

The numbers and strings that identify E2SM-DAPP, what the RAN function definition
contains, and how an E2 node announces the service model to the RIC.

This is section 1 of the specification. The index is [README.md](README.md). The
field-by-field description of the definition is in
[02-ie-mapping.md](02-ie-mapping.md#5-ran-function-definition), its bytes in
[04-encoding.md](04-encoding.md). Code references use the roots listed in
[04-encoding.md](04-encoding.md).

## 1. Identity

| Item | Value | Where it comes from |
|---|---|---|
| **RAN function ID** (E2) | `255` | `flexric/src/sm/dapp_sm/dapp_sm_id.h:6` (`SM_DAPP_ID`). The E2AP range is 0..4095. |
| **RAN function revision** | `1` | `dapp_sm_id.h:8` (`SM_DAPP_REV`) |
| **RAN function short name** | `E2SM-DAPP` | `dapp_sm_id.h:10` (`SM_DAPP_SHORT_NAME`) |
| **RAN function description** | `DAPP Service Model for E2/E3 bridge` | `dapp_sm_id.h:19` (`SM_DAPP_DESCRIPTION`) |
| **E2SM OID** | `1.3.6.1.4.1.53148.1.1.255.3` | `dapp_sm_id.h:17` (`SM_DAPP_OID`), and the module identifier of the grammar |
| **ASN.1 module** | `E2SM-DAPP-IEs`, version 1 of the grammar | `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`, line 1 |
| **Style numbers** | report styles `1` and `2`, control style `1` | [02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format) |
| **Instance** (`ranFunction-Instance`) | not used | the field exists in the grammar, nothing sets it |

### 1.1 How the OID is built

The module identifier in the grammar is

```text
E2SM-DAPP-IEs {iso(1) identified-organization(3) dod(6) internet(1) private(4)
               enterprise(1) oran(53148) e2(1) version1(1) e2sm(255) e2sm-DAPP-IEs (3)}
```

which reads as `1.3.6.1.4.1.53148.1.1.255.3`. The arc `53148` is the O-RAN
enterprise number, the same arc the O-RAN service models use. The arc `255` is the
service model number, and it is deliberately the same number as the E2 RAN function
ID. The final `3` is the module number inside that service model.

A comment in `flexric/src/sm/dapp_sm/dapp_sm_id.h:11-15` spells the same OID with
`e2sm(2)`. It is a stale comment; the string constant on line 17 and the grammar
both use `255`. See [06-discrepancies.md](06-discrepancies.md).

### 1.2 Two things named "RAN function ID"

E2SM-DAPP touches two identifier spaces that both carry the words "RAN function ID".
They are different and must not be mixed up.

| Name | Space | Used where | Typical value |
|---|---|---|---|
| E2 RAN function ID | E2AP, `RANfunctionID (0..4095)` | E2 Setup, RIC Subscription, RIC Indication, RIC Control (E2AP fields) | `255` for E2SM-DAPP itself |
| E3 RAN function ID | E3AP, `ranFunctionIdentifier (1..65535)` | inside E2SM-DAPP: `ran-function-id` in the indication header and the control header, and `SubscribedE3RANFunction-ID` in the subscription map | `1` for the Spectrum service model, `2` for the L1-KPM service model (OAI: `oai/openair2/E3AP/config/e3_config.h:8-9`) |

The golden vectors use `255` in the `ran-function-id` fields of the header. That is
arbitrary test data, chosen to be easy to recognize. It does not mean that the E3
function id is 255.

### 1.3 Where the identity is written down

| Place | What it holds | Notes |
|---|---|---|
| `flexric/src/sm/dapp_sm/dapp_sm_id.h` | id, revision, short name, OID, description | the only place flexric defines them |
| `flexric/src/agent/e2_agent_api.c:177` | the literal `255` | `trigger_ric_service_update_api` looks the SM up by the number, not by `SM_DAPP_ID` |
| `oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:47-51` | its own copy of the short name, OID and description (`E2SM-DAPP for xApp-dApp synchronization`) | **never reaches the wire**: flexric's agent replaces them (`dapp_sm_agent.c:258-259`) |
| `xdevsm/src/xdevsm/decorators/dApp/report/dapp_report.py:48` and `.../control/dapp_prb_mask_control.py:32` | the literal `255` | `xdevsm` finds the RAN function by this id, not by OID |
| `libe3/include/libe3/e2sm_dapp.hpp`, file comment | id `255` and the OID, as text | no constant is exported by libe3 |
| `ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.cpp:17-22` | `short_name`, `oid`, `func_description`, `ran_func_id` (255) and `revision` (1) as static members of `e2sm_dapp_asn1_packer`; the header comment of `ocudu/include/ocudu/asn1/e2sm/e2sm_dapp.h` repeats id and OID as text | the revision constant is **not** what OCUDU announces: see 3.1 |

## 2. RAN function definition: what it contains

The definition is one `E2SM-DAPP-RANFunctionDefinition`, APER encoded. The fields are
specified in [02-ie-mapping.md](02-ie-mapping.md#5-ran-function-definition). In
summary:

| Part | Content | Required |
|---|---|---|
| `ranFunction-Name` | short name, OID, description, optional instance | yes |
| `ranFunctionDefinition-EventTrigger` | empty marker | optional |
| `ranFunctionDefinition-Report` | 1 or 2 report styles. Each: style number, style name, indication header format, indication message format, optional dApp E3 subscription map | optional |
| `ranFunctionDefinition-Control` | exactly 1 control style. It carries style number, style name, control header format, control message format, control outcome format, optional dApp E3 subscription map | optional |

A definition with only the name (`fd_min`, 78 octets) is valid but advertises nothing.
A useful definition has at least a report section or a control section.

### 2.1 What a real RAN sends

The OAI RAN builds this definition (`ran_func_dapp.c:183-208`):

| Part | Value |
|---|---|
| name | `E2SM-DAPP`, the OID, `DAPP Service Model for E2/E3 bridge` (the agent overwrites OAI's own strings) |
| event trigger | absent |
| report style 1 | type `1`, name `DAPP-E3-DATA-REPORT`, header format `1`, message format `1`, **dApp E3 subscription map** if any dApp has a subscribed function |
| report style 2 | type `2`, name `DAPP-E3-SUBSCRIPTION-MAP`, header format `2`, message format `2`, no map |
| control style 1 | type `1`, name `DAPP-CONTROL-STYLE-1`, header format `1`, message format `1`, outcome format `1`, the same map as report style 1 |

The map holds one item per dApp that has at least one subscribed E3 RAN function:
the `dapp-id` and the list of E3 RAN function ids. How an xApp uses it is in
[03-procedure-flow.md](03-procedure-flow.md#3-discovery-how-an-xapp-learns-the-dapp-ids).

`flexric/test/rnd/fill_rnd_data_dapp.c` and `libe3/tests/e2sm_dapp_fixtures.inc`
also build definitions, but they are **test data**, not what a RAN advertises. The
random generator in flexric fills random names and random dApp lists only to
exercise the codec (`fill_rnd_data_dapp.c:104-135,205-260`). The OAI copy of that
file, `oai/openair2/E2AP/flexric/test/rnd/fill_rnd_data_dapp.c`, is the same kind of
file. flexric's agent obtains the real definition from the RAN through `read_setup`
(`dapp_sm_agent.c:244-275`), so the content is the RAN's responsibility.

OCUDU builds a fixed definition (`e2sm_dapp_asn1_packer::pack_ran_function_description`):
the name block, an **event trigger section**, two report styles named `E3 Data Report`
(formats 1 and 1) and `E3 Subscription Map` (formats 2 and 2), and one control style named
`dApp Control` (formats 1, 1, 1). It has **no dApp E3 subscription map** in the
definition. These are the names of the golden vectors, not OAI's: an xApp that selects the
control style by the name `DAPP-CONTROL-STYLE-1` (`xdevsm`) does not find it on OCUDU.

### 2.2 What flexric's agent adds or replaces

`on_e2_setup_dapp_sm_ag` (`dapp_sm_agent.c:244-275`) takes the definition the RAN
returns and **replaces the name block** (`fill_ran_func_name`, lines 215-229) with
`SM_DAPP_SHORT_NAME`, `SM_DAPP_OID` and `SM_DAPP_DESCRIPTION`, whatever the RAN put
there. It asserts the short name is not empty (lines 261-262). Everything else, the
styles, formats and map, is the RAN's. An emulator agent without a RAN callback gets
a definition with only the name (`dapp_sm_agent.c:251-256`).

## 3. Announcement in E2 Setup and RIC Service Update

### 3.1 E2 Setup Request

The E2 node lists E2SM-DAPP as one **RAN function item** in the E2 Setup Request.

| E2AP RAN function item field | Value for E2SM-DAPP | Code |
|---|---|---|
| RAN Function ID | `255` | `flexric/src/agent/gen_msg_agent.c:56` (E2AP v1), `97` (v2, v3) |
| RAN Function Definition (`OCTET STRING`) | the APER encoding of `E2SM-DAPP-RANFunctionDefinition` | `gen_msg_agent.c:53-54`, `94-95` |
| RAN Function Revision | `1` in flexric and OAI. **OCUDU announces `0`** | `gen_msg_agent.c:57`, `98` (`sm->info.rev()`); OCUDU: `ocudu/lib/e2/common/e2ap_asn1_helpers.h:37`, a helper shared by all its service models, which ignores `e2sm_dapp_asn1_packer::revision` |
| RAN Function OID | `1.3.6.1.4.1.53148.1.1.255.3` | `gen_msg_agent.c:58-60`, `99` |

The definition is rebuilt each time the agent builds an E2 Setup Request, so a
retried setup carries the dApp map as it is at that moment. There is nothing
E2SM-DAPP-specific in the E2 Setup Response.

Both the E2AP OID field and the `ranFunction-E2SM-OID` string inside the definition
are filled from the same constant (`SM_DAPP_OID`), so they cannot differ in flexric.

OCUDU lists E2SM-DAPP in the E2 Setup Request only when `e2sm_dapp_enabled` is set
(`e2ap_asn1_helpers.h:138-148`), with RAN function id 255 and the OID of
`e2sm_dapp_asn1_packer`, and with the fixed definition described in 2.1.

### 3.2 RIC Service Update

The RAN sends a RIC Service Update when the content of the definition changes, which
for E2SM-DAPP means when a dApp connects or disconnects and the dApp E3 subscription
map changes.

| Item | Value | Code |
|---|---|---|
| Function list | **modified**, exactly one entry, RAN function 255. Nothing is added or deleted | `flexric/src/agent/e2_agent_api.c:185-205` |
| Definition | freshly built, as for E2 Setup | `e2_agent_api.c:183`, `on_e2_setup` |
| OID | `SM_DAPP_OID` | `e2_agent_api.c:202-204` |
| Revision | a counter that starts at `1` and increases by one per update. The first update therefore carries revision `1`, which the E2 Setup already used | `e2_agent_api.c:175,198` |
| Transaction id | the same counter modulo 256 | `e2_agent_api.c:186` |
| Trigger | the RAN calls `trigger_ric_service_update_api()`. OAI does so from `notify_dapp_status_changed`, only after the RIC has been heard from | `oai/.../ran_func_dapp.c:329-342` |

OCUDU sends no RIC Service Update for E2SM-DAPP: its definition is fixed and carries no
dApp map.

What the RIC side does with a RIC Service Update for E2SM-DAPP:

| RIC | Behavior |
|---|---|
| flexric RIC | `on_ric_service_update_dapp_sm_ric` (`dapp_sm_ric.c:259-265`) is a placeholder. It returns an empty result and does **not** decode the new definition. |
| O-RAN SC RIC | the RIC's own E2 manager updates its stored node information. xApps that read the definition again (`xdevsm`: `get_ran_function_description`) see the new one; the dApp E3 subscription map in it is current. |

The agent-side callback `on_ric_service_update_dapp_sm_ag` (`dapp_sm_agent.c:283-287`)
is a stub that returns nothing: it is the hook for RIC Service Update messages
*received* from the RIC, which E2SM-DAPP does not use.

## 4. What must be true for a RAN to announce E2SM-DAPP

A checklist for a RAN stack implementing the E2 side.

1. Register a RAN function with E2 RAN function ID `255`, revision `1` and OID
   `1.3.6.1.4.1.53148.1.1.255.3`.
2. Put an APER `E2SM-DAPP-RANFunctionDefinition` in the RAN function definition. It
   must contain at least the name. For xApps to use it, it should also contain the
   report section (styles 1 and 2) and the control section (style 1), with the
   formats listed in [02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format).
3. Use the control style name `DAPP-CONTROL-STYLE-1` if `xdevsm` xApps are to control
   the RAN.
4. Keep every `DAppE3Subscription-Item` to **at least one and at most 64**
   subscribed E3 RAN functions, and the list to **at most 256** items. Omit the
   `dappE3Subscriptions` field when no dApp has a subscribed function (flexric's
   encoder cannot encode an empty list in the definition, and its decoder cannot
   decode more than 255 items or 63 functions; see
   [06-discrepancies.md](06-discrepancies.md)).
5. Send a RIC Service Update with the new definition when the dApp set changes.
   Increase the revision each time (the flexric agent does not, for the first one).
6. Accept RIC Subscription Requests with exactly one report action, event trigger
   format 1 and action definition style 1 or 2. Accept RIC Control Requests with an
   acknowledge request.

## 5. Related

* The definition's bytes: [04-encoding.md](04-encoding.md#310-fd_min-the-smallest-ran-function-definition).
* What the RAN does at each step: [03-procedure-flow.md](03-procedure-flow.md).
* Building a definition in code: [integrator-guide.md](integrator-guide.md).
