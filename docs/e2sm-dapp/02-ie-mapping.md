# E2SM-DAPP: E2AP information element mapping

Every information element E2SM-DAPP defines, in the shape of an O-RAN E2SM
specification: where it sits in E2AP, its fields with types, ranges and
optionality, which formats and styles exist, and which E2 service uses which
format.

This is section 2 of the specification. The index is [README.md](README.md). The
bytes of each element are in [04-encoding.md](04-encoding.md), the message order
in [03-procedure-flow.md](03-procedure-flow.md), what the values mean and what is
checked in [05-semantics-and-constraints.md](05-semantics-and-constraints.md).

Reading the field tables. **Range** is the ASN.1 constraint. `...` after a range
is an extension marker on that integer. **Opt** is `yes` for `OPTIONAL`. Every
`SEQUENCE` and `CHOICE` has an extension marker (`...`) and no extension
additions exist yet. Code references use the roots listed in
[04-encoding.md](04-encoding.md).

## 1. The elements at a glance

| E2SM element | Carried in E2AP message | E2AP field (an `OCTET STRING`) | ASN.1 type | Formats |
|---|---|---|---|---|
| RAN function definition | E2 Setup Request, RIC Service Update | RAN Function Item: RAN Function Definition | `E2SM-DAPP-RANFunctionDefinition` | one |
| Event trigger definition | RIC Subscription Request | RIC Event Trigger Definition | `E2SM-DAPP-EventTrigger` | 1 (empty) |
| Action definition | RIC Subscription Request | RIC Action Definition (action type `report`) | `E2SM-DAPP-ActionDefinition` | 1 (empty, plus a style) |
| Indication header | RIC Indication | RIC Indication Header | `E2SM-DAPP-IndicationHeader` | 1, 2 |
| Indication message | RIC Indication | RIC Indication Message | `E2SM-DAPP-IndicationMessage` | 1, 2 |
| Call process ID | RIC Indication, RIC Control Request | RIC Call Process ID | **not defined** | none |
| Control header | RIC Control Request | RIC Control Header | `E2SM-DAPP-ControlHeader` | 1 |
| Control message | RIC Control Request | RIC Control Message | `E2SM-DAPP-ControlMessage` | 1 |
| Control outcome | RIC Control Acknowledge | RIC Control Outcome | `E2SM-DAPP-ControlOutcome` | 1 |
| Query definition, query header, query outcome | (E2AP query procedures) | | **not defined** | none |

All payload IEs are APER encodings of the ASN.1 type, tunnelled in the E2AP
`OCTET STRING`. The ASN.1 grammar is
`messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`.

## 2. Which E2 service uses which format

E2SM-DAPP implements two of the E2 services: **REPORT** and **CONTROL**. The rows
below list the report styles and the control style the RAN advertises, and the
format each IE uses under each.

| E2 service | Style type | Style name | Event trigger | Action definition | Indication header | Indication message | Control header | Control message | Control outcome |
|---|---|---|---|---|---|---|---|---|---|
| REPORT | 1 (`DAPP_RIC_STYLE_E3_DATA_REPORT`) | E3 data report | format 1 | format 1 | format 1 | format 1 | n/a | n/a | n/a |
| REPORT | 2 (`DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP`) | E3 subscription map | format 1 | format 1 | format 2 | format 2 | n/a | n/a | n/a |
| CONTROL | 1 | dApp control | n/a | n/a | n/a | n/a | format 1 | format 1 | format 1 |
| INSERT | none | | not used | not used | not used | not used | not used | not used | not used |
| POLICY | none | | not used | not used | not used | not used | not used | not used | not used |
| QUERY | none | | not used | not used | not used | not used | not used | not used | not used |

Where each fact comes from:

* **Style numbers.** `DAPP_RIC_STYLE_E3_DATA_REPORT = 1` and
  `DAPP_RIC_STYLE_E3_SUBSCRIPTION_MAP = 2` are in
  `flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:293-296`. The report style a
  subscriber gets is chosen by `ric-Style-Type` in the action definition
  (documented in `flexric/src/sm/dapp_sm/dapp_sm_agent.c:35-38`).
* **Style to format.** The style-to-format mapping is carried in the RAN function
  definition by `ric-IndicationHeaderFormat-Type` and
  `ric-IndicationMessageFormat-Type` (style 1: `1` and `1`; style 2: `2` and `2`).
  The RAN that builds the definition fills these in. flexric's own `dapp_sm_agent.c`
  does not (it asks the RAN through `read_setup`), and its random test data
  (`flexric/test/rnd/fill_rnd_data_dapp.c:205-210`) does not define the
  standard: it only sets `ind_hdr_type = ind_msg_type = report_type`. The
  authoritative definition is the one a RAN actually sends. The OAI RAN sends the
  values above (`oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:76-86`).
* **Control style number.** The control style is `1`. OAI writes it as
  `DAPP_RIC_STYLE_E3_DATA_REPORT` (`ran_func_dapp.c:107`), which is the value `1`
  with the name of a report style. The numbering spaces for report styles and
  control styles are separate in E2SM, so the two `1`s do not collide. The control
  header does **not** carry a style number, so a stack with a second control style
  would need a new control header format.
* **Style names are free text.** `RIC-Style-Name` is any `PrintableString` of 1 to
  150 characters. The names in use differ by source:

  | Source | Report style 1 | Report style 2 | Control style 1 |
  |---|---|---|---|
  | OAI RAN (`ran_func_dapp.c:77,83,108`) | `DAPP-E3-DATA-REPORT` | `DAPP-E3-SUBSCRIPTION-MAP` | `DAPP-CONTROL-STYLE-1` |
  | flexric golden-vector generator and test data (`libe3/tests/e2sm_dapp_fixtures.inc`) | `E3 Data Report` | `E3 Subscription Map` | `dApp Control` |
  | OCUDU RAN (`ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.cpp`, `pack_ran_function_description`) | `E3 Data Report` | `E3 Subscription Map` | `dApp Control` |

  `xdevsm` finds the control style **by this name**: it looks for the string
  `DAPP-CONTROL-STYLE-1` in the definition
  (`xdevsm/src/xdevsm/decorators/dApp/control/dapp_prb_mask_control.py:35` and the
  loop in `xdevsm/src/xdevsm/decorators/control.py`, `send()`), and refuses to send
  if it is not there. A RAN that wants to work with `xdevsm` has to use that name.

### 2.1 Why INSERT, POLICY and QUERY are "not used"

E2SM-DAPP defines none of them. The code agrees in every place it was checked:

| Claim | Evidence |
|---|---|
| The RAN function definition has no Insert, Policy or Query section | The grammar's `E2SM-DAPP-RANFunctionDefinition` (lines 269-275) lists only name, event trigger, report and control |
| flexric's E2 agent accepts only a report action | `flexric/src/agent/msg_handler_agent.c:229-235`: `assert(sr->len_action == 1 ...)` and `assert(sr->action->type == RIC_ACT_REPORT ...)`. A subscription with an insert or policy action aborts the agent. |
| flexric's E2AP model has no query action type | `flexric/src/lib/e2ap/v3_01/e2ap_types/common/ric_action.h:31-35` lists `RIC_ACT_REPORT`, `RIC_ACT_INSERT`, `RIC_ACT_POLICY` and nothing else |
| The xApp framework only builds report actions | `xdevsm/src/xdevsm/decorators/dApp/report/dapp_report.py:155-158`: `action_type="report"` |
| Every indication is a REPORT indication | `flexric/src/agent/e2_agent.c:48-52` and `76-80`: `.type = RIC_IND_REPORT` |
| The event trigger needs no timer | the dApp SM answers every subscription with an aperiodic outcome (`dapp_sm_agent.c:75`, `ran_func_dapp.c:482-486`), so reports are driven by the RAN, not a period |

### 2.2 Why there is no call process ID

E2AP carries an optional Call Process ID on an indication and on a control request.
E2SM-DAPP does not use it:

| Claim | Evidence |
|---|---|
| The agent's indication path never sets it | `dapp_sm_agent.c:133-139` builds `exp_ind_data_t ret = {.has_value = true}` and fills only header and message. `flexric/src/agent/e2_agent.c:58-63` copies a call process ID only when the SM provides one. |
| The agent never reads it on a control request | `flexric/src/agent/msg_handler_agent.c:393-411` reads only the header, the message and the RIC ID. A search for `call_process_id` in that file finds nothing. |
| The agent writes none on the control acknowledge | `msg_handler_agent.c:436` and `flexric/src/agent/e2_agent.c:456`: `.call_process_id = NULL` |
| A sender may still put one | `xdevsm` sends the string `1` in every control request (`xdevsm/src/xdevsm/decorators/control.py`, parameter `call_process_id: bytes=b"1"` of `send_control_request_rmr`). It is ignored. A conforming sender omits it. |

## 3. Common types

These are used by several elements.

| ASN.1 type | Definition | Meaning |
|---|---|---|
| `RIC-Style-Type` | `INTEGER` (unbounded) | style number. 1 and 2 for reports, 1 for control. flexric stores a `uint32_t`. |
| `RIC-Format-Type` | `INTEGER` (unbounded) | format number in the RAN function definition. 1 or 2. flexric stores a `uint32_t`. |
| `RIC-Style-Name` | `PrintableString (SIZE(1..150, ...))` | free text |
| `Timestamp` | `INTEGER` (unbounded) | nanoseconds since the Unix epoch, `CLOCK_REALTIME`. In every message it is `OPTIONAL`: absent means "the producer did not stamp", which is a normal message. |
| `SequenceId` | `INTEGER` (unbounded) | correlation id of one detection-to-mitigation procedure. `OPTIONAL` everywhere. Equal to E3AP's `E3-SequenceID` (`INTEGER (1..4294967295)`). |
| `SubscribedE3RANFunction-ID` | `INTEGER (0..4294967295, ...)` | an E3 RAN function id |
| `Bool-Type` | `BOOLEAN` | defined and **never used** in the grammar |
| `RANfunction-Name` | `SEQUENCE`, see 5.1 | |

The standard has only two identifier spaces besides E2's own: the **E3 RAN function
id** (which E3 service model a report or a control belongs to, for example
Spectrum) and the **dApp id** (which dApp). Both come from the E3 interface. They
are not E2 identifiers. E2's own RAN function id for E2SM-DAPP is 255
([01-identity-and-registration.md](01-identity-and-registration.md)).

The relation between the E2SM-DAPP ids and E3AP's: E2SM-DAPP `ran-function-id`
and `dapp-id` are `INTEGER (0..4294967295, ...)`; E3AP's are
`E3-DAppID ::= INTEGER (1..65535)` and `ranFunctionIdentifier INTEGER (1..65535)`
(`libe3/messages/asn1/V1/e3ap-1.0.0.asn1`). Every E3 value is valid in E2SM-DAPP.
The reverse is not true. E2SM-DAPP can carry `0` and values above `65535`, which
the E3 link cannot deliver. See [05-semantics-and-constraints.md](05-semantics-and-constraints.md).

## 4. Event trigger and action definition

### 4.1 `E2SM-DAPP-EventTrigger`

```text
E2SM-DAPP-EventTrigger ::= SEQUENCE {
    ric-eventTrigger-formats CHOICE {
        eventTrigger-Format1  E2SM-DAPP-EventTrigger-Format1,
        ...
    },
    ...
}
E2SM-DAPP-EventTrigger-Format1 ::= SEQUENCE { }
```

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ric-eventTrigger-formats` | `CHOICE` | one alternative: `eventTrigger-Format1` | no | |
| `eventTrigger-Format1` | empty `SEQUENCE` | no fields | no | The subscription has no trigger condition. Indications are sent when the RAN has something to report. |

A subscriber always sends the empty trigger. The agent decodes it and ignores it
(`dapp_sm_agent.c:55-56`).

### 4.2 `E2SM-DAPP-ActionDefinition`

```text
E2SM-DAPP-ActionDefinition ::= SEQUENCE {
    ric-Style-Type             RIC-Style-Type,
    actionDefinition-formats   CHOICE {
        actionDefinition-Format1  E2SM-DAPP-ActionDefinition-Format1,
        ...
    },
    ...
}
E2SM-DAPP-ActionDefinition-Format1 ::= SEQUENCE { }
```

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ric-Style-Type` | `RIC-Style-Type` | unbounded integer; in use: 1, 2 | no | selects the report style, therefore which indication formats the subscriber receives (see section 2) |
| `actionDefinition-formats` | `CHOICE` | one alternative: `actionDefinition-Format1` | no | |
| `actionDefinition-Format1` | empty `SEQUENCE` | no fields | no | |

Style 1 receives indication header and message format 1 only. Style 2 receives
format 2 only. To get both, subscribe twice (two RIC Subscription Requests, with
different RIC request ids). Both flexric and OAI behave this way
(`dapp_sm_agent.c:35-38`; `oai/.../ran_func_dapp.c:463-480`).

The action definition is optional in E2AP. The flexric agent copes with its
absence (`dapp_sm_agent.c:59`), but the OAI RAN requires it and aborts without
one (`ran_func_dapp.c:461`, `assert(... != NULL && "Action definition required")`).
A subscription with a style number other than 1 or 2 makes OAI's `AssertError`
(`ran_func_dapp.c:478-479`) log and return an empty answer, and flexric's agent then
fails `assert(subs.type == SUBS_OUTCOME_SM_AG_IF_ANS_V0)` at
`dapp_sm_agent.c:74`, because the empty answer has type `0`
(`CTRL_OUTCOME_SM_AG_IF_ANS_V0`, `flexric/src/sm/agent_if/ans/sm_ag_if_ans.h:38`).
In a build with assertions on, the gNB aborts. A subscriber must send an action
definition with style 1 or 2.

## 5. RAN function definition

The RAN function definition is the E2SM element the RAN sends in E2 Setup and in
RIC Service Update. It advertises the styles and, for dApp discovery, the current
**dApp E3 subscription map**.

```text
E2SM-DAPP-RANFunctionDefinition ::= SEQUENCE {
    ranFunction-Name                     RANfunction-Name,
    ranFunctionDefinition-EventTrigger   RANFunctionDefinition-EventTrigger  OPTIONAL,
    ranFunctionDefinition-Report         RANFunctionDefinition-Report        OPTIONAL,
    ranFunctionDefinition-Control        RANFunctionDefinition-Control       OPTIONAL,
    ...
}
```

| Field | Type | Opt | Notes |
|---|---|---|---|
| `ranFunction-Name` | `RANfunction-Name` | no | see 5.1 |
| `ranFunctionDefinition-EventTrigger` | `RANFunctionDefinition-EventTrigger` | yes | empty `SEQUENCE { ... }`. Its presence says the function has an event trigger. OAI omits it (`ran_func_dapp.c:189`: `def.ev_trig = NULL`). |
| `ranFunctionDefinition-Report` | `RANFunctionDefinition-Report` | yes | see 5.2 |
| `ranFunctionDefinition-Control` | `RANFunctionDefinition-Control` | yes | see 5.3 |

### 5.1 `RANfunction-Name`

| Field | Type | Range | Opt | Value in use |
|---|---|---|---|---|
| `ranFunction-ShortName` | `PrintableString` | `SIZE(1..150, ...)` | no | `E2SM-DAPP` |
| `ranFunction-E2SM-OID` | `PrintableString` | `SIZE(1..1000, ...)` | no | `1.3.6.1.4.1.53148.1.1.255.3` |
| `ranFunction-Description` | `PrintableString` | `SIZE(1..150, ...)` | no | `DAPP Service Model for E2/E3 bridge` |
| `ranFunction-Instance` | `INTEGER` | unbounded | yes | not used. flexric's encoder asserts it is `NULL` (`dapp_enc_asn.c:525`) and its decoder never reads it (`dapp_dec_asn.c:428-441`). |

### 5.2 `RANFunctionDefinition-Report`

```text
RANFunctionDefinition-Report ::= SEQUENCE {
    ric-ReportStyle-List  SEQUENCE (SIZE(1..maxnoofDAPPReportStyles)) OF RANFunctionDefinition-Report-Item,
    ...
}
RANFunctionDefinition-Report-Item ::= SEQUENCE {
    ric-ReportStyle-Type              RIC-Style-Type,
    ric-ReportStyle-Name              RIC-Style-Name,
    ric-IndicationHeaderFormat-Type   RIC-Format-Type,
    ric-IndicationMessageFormat-Type  RIC-Format-Type,
    dappE3Subscriptions               DAppE3Subscription-List  OPTIONAL,
    ...
}
```

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ric-ReportStyle-List` | `SEQUENCE OF` | `SIZE(1..2)` (`maxnoofDAPPReportStyles`) | no | |
| `ric-ReportStyle-Type` | `RIC-Style-Type` | 1 or 2 in use | no | |
| `ric-ReportStyle-Name` | `RIC-Style-Name` | 1..150 chars | no | |
| `ric-IndicationHeaderFormat-Type` | `RIC-Format-Type` | 1 or 2 in use | no | the indication header format the style produces |
| `ric-IndicationMessageFormat-Type` | `RIC-Format-Type` | 1 or 2 in use | no | the indication message format the style produces |
| `dappE3Subscriptions` | `DAppE3Subscription-List` | 0..256 items | yes | the dApp E3 subscription map at the time the definition was built. OAI attaches it to report style 1 only, and only when at least one dApp has at least one subscribed function (`ran_func_dapp.c:195-199`). |

### 5.3 `RANFunctionDefinition-Control`

```text
RANFunctionDefinition-Control ::= SEQUENCE {
    ric-ControlStyle-List  SEQUENCE (SIZE(1..maxnoofDAPPControlStyles)) OF RANFunctionDefinition-Control-Item,
    ...
}
RANFunctionDefinition-Control-Item ::= SEQUENCE {
    ric-ControlStyle-Type          RIC-Style-Type,
    ric-ControlStyle-Name          RIC-Style-Name,
    ric-ControlHeaderFormat-Type   RIC-Format-Type,
    ric-ControlMessageFormat-Type  RIC-Format-Type,
    ric-ControlOutcomeFormat-Type  RIC-Format-Type,
    dappE3Subscriptions            DAppE3Subscription-List  OPTIONAL,
    ...
}
```

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ric-ControlStyle-List` | `SEQUENCE OF` | `SIZE(1..1)` (`maxnoofDAPPControlStyles`) | no | exactly one control style |
| `ric-ControlStyle-Type` | `RIC-Style-Type` | 1 | no | |
| `ric-ControlStyle-Name` | `RIC-Style-Name` | 1..150 chars | no | see the name table in section 2 |
| `ric-ControlHeaderFormat-Type` | `RIC-Format-Type` | 1 | no | |
| `ric-ControlMessageFormat-Type` | `RIC-Format-Type` | 1 | no | |
| `ric-ControlOutcomeFormat-Type` | `RIC-Format-Type` | 1 | no | |
| `dappE3Subscriptions` | `DAppE3Subscription-List` | 0..256 items | yes | the same map as in the report item. OAI copies it from report style 1 (`ran_func_dapp.c:201-204`). |

### 5.4 dApp E3 subscription list

```text
DAppE3Subscription-Item ::= SEQUENCE {
    dapp-id                      INTEGER (0..4294967295, ...),
    subscribed-e3-ran-functions  SEQUENCE (SIZE(1..maxnoofSubscribedE3RANFunctions)) OF SubscribedE3RANFunction-ID,
    ...
}
DAppE3Subscription-List ::= SEQUENCE (SIZE(0..maxnoofDApps)) OF DAppE3Subscription-Item
```

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| (list) | `DAppE3Subscription-List` | `SIZE(0..256)` (`maxnoofDApps`) | | the list can be empty |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | no | the dApp |
| `subscribed-e3-ran-functions` | `SEQUENCE OF SubscribedE3RANFunction-ID` | `SIZE(1..64)` (`maxnoofSubscribedE3RANFunctions`) | no | the E3 RAN functions the dApp has subscribed to. An item must have at least one. |
| element | `SubscribedE3RANFunction-ID` | `0..4294967295, ...` | | |

The same list type appears in three places: the report item, the control item, and
indication message format 2. The list means the same thing in all three: "these
dApps are connected, and each has subscribed to these E3 RAN functions". How an
xApp uses it is in [03-procedure-flow.md](03-procedure-flow.md#3-discovery-how-an-xapp-learns-the-dapp-ids).

OAI builds the list from the E3 agent's subscription map and drops every dApp with
zero subscribed functions (`ran_func_dapp.c:131-171`), which keeps the one-or-more
rule of the grammar.

## 6. Indication header and message

### 6.1 `E2SM-DAPP-IndicationHeader`

```text
E2SM-DAPP-IndicationHeader ::= SEQUENCE {
    ric-indicationHeader-formats CHOICE {
        indicationHeader-Format1  E2SM-DAPP-IndicationHeader-Format1,
        indicationHeader-Format2  E2SM-DAPP-IndicationHeader-Format2,
        ...
    },
    ...
}
```

**Format 1** is the header of a style-1 indication: one dApp report.

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ran-function-id` | `INTEGER` | `0..4294967295, ...` | no | the **E3 RAN function id** of the report (the service model that produced it, for example Spectrum). It is not E2's RAN function id 255. It selects the decoder for the payload in indication message format 1. OAI copies it from the E3 report (`ran_func_dapp.c:393-395`). |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | no | the dApp that sent the report |
| `node-type` | `INTEGER` | `0..255, ...` | no | the type of the reporting node. The grammar does not enumerate values. OAI fills it with its own RRC node-type enum (`ran_func_dapp.c:403`). |
| `node-plmn-id` | `OCTET STRING` | `SIZE(3)` | no | three octets. The grammar does not say how they are packed. OAI writes the high octet of the MCC, the low octet of the MCC, and the low octet of the MNC (`ran_func_dapp.c:406-408`). It is **not** the 3GPP BCD `PLMN-Identity` encoding. |
| `node-nb-id` | `INTEGER` | `0..4294967295, ...` | no | the node's gNB id. OAI uses the NR cell id shifted right by 14 (`ran_func_dapp.c:404`). |
| `node-cu-du-id` | `INTEGER` | `0..4294967295, ...` | yes | the CU or DU id inside the node. OAI uses the gNB-DU id and omits it when the node identity is unknown (`ran_func_dapp.c:410-411`). |
| `timestamp` | `Timestamp` | | yes | when the RAN built this indication, `CLOCK_REALTIME` ns (OAI: `time_now_ns()`, `ran_func_dapp.c:397`). It is **not** the dApp's report timestamp, which travels inside the payload. |
| `sequence-id` | `SequenceId` | | yes | the dApp's correlation id for this report, copied from the E3 `E3-DAppReport.sequenceId` (`ran_func_dapp.c:400`). The xApp echoes it on its control header. |

When the node identity is unavailable, OAI leaves `node-type`, `node-plmn-id` and
`node-nb-id` at zero and omits `node-cu-du-id` (`ran_func_dapp.c:381-412`). The
grammar has no "unknown" value, so this is a convention of that RAN, not of the
standard.

**Format 2** is the header of a style-2 indication: the dApp subscription map.

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `node-type` | `INTEGER` | `0..255, ...` | no | as in format 1 |
| `node-plmn-id` | `OCTET STRING` | `SIZE(3)` | no | as in format 1 |
| `node-nb-id` | `INTEGER` | `0..4294967295, ...` | no | as in format 1 |
| `node-cu-du-id` | `INTEGER` | `0..4294967295, ...` | yes | as in format 1 |
| `timestamp` | `Timestamp` | | yes | when the RAN built the indication. OAI sets it (`ran_func_dapp.c:271`). |
| `sequence-id` | `SequenceId` | | yes | no procedure is behind a subscription map, so OAI leaves it absent |

Format 2 has no `ran-function-id` and no `dapp-id`: the map names the dApps itself.

### 6.2 `E2SM-DAPP-IndicationMessage`

```text
E2SM-DAPP-IndicationMessage ::= SEQUENCE {
    ric-indicationMessage-formats CHOICE {
        indicationMessage-Format1  E2SM-DAPP-IndicationMessage-Format1,
        indicationMessage-Format2  E2SM-DAPP-IndicationMessage-Format2,
        ...
    },
    ...
}
```

**Format 1**, paired with header format 1.

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `data-size` | `INTEGER` | `0..maxE2SMDAPPIndMsgSize` (32768), no extension | no | the number of octets in `data`. It repeats the length of the octet string. See [06-discrepancies.md](06-discrepancies.md). |
| `data` | `OCTET STRING` | `SIZE(1..maxE2SMDAPPIndMsgSize)` (1..32768) | no | the service model's own encoding of the dApp report, copied as is from the E3 `reportData`. Opaque to E2SM-DAPP. At least one octet. |

The `data` is decoded by the service model named by the header's `ran-function-id`.
For Spectrum that is `Spectrum-DAppReportData`, which holds a timestamp (the dApp's
own, `CLOCK_REALTIME` ns) and the PRB list.

**Format 2**, paired with header format 2.

| Field | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `dappE3Subscriptions` | `DAppE3Subscription-List` | `SIZE(0..256)` | no | the current dApp E3 subscription map. Can be empty (no dApp connected). |

## 7. Control header, message, outcome

### 7.1 `E2SM-DAPP-ControlHeader`

```text
E2SM-DAPP-ControlHeader ::= SEQUENCE {
    ric-controlHeader-formats CHOICE {
        controlHeader-Format1  E2SM-DAPP-ControlHeader-Format1,
        ...
    },
    ...
}
```

| Field (format 1) | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `ran-function-id` | `INTEGER` | `0..4294967295, ...` | no | the E3 RAN function id the control is for. The RAN forwards the payload to the dApp under this id, and the service model with this id decodes the control message and builds the control outcome. |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | no | the dApp that is the target of the control |
| `timestamp` | `Timestamp` | | yes | when the xApp built the control, `CLOCK_REALTIME` ns of the xApp's host (`xdevsm` sets `time.time_ns()`, `xdevsm/.../dapp/control/DAppControlReq.py:56`). Unlike the RAN-side timestamps, this one is **not** on the gNB's clock. |
| `sequence-id` | `SequenceId` | | yes | the id of the procedure, echoed from the indication header that prompted this control. **If absent or `0`, the RAN cannot defer the acknowledge** and answers immediately without applying anything (`oai/.../ran_func_dapp.c:527-531`). |

### 7.2 `E2SM-DAPP-ControlMessage`

```text
E2SM-DAPP-ControlMessage ::= SEQUENCE {
    ric-controlMessage-formats CHOICE {
        controlMessage-Format1  E2SM-DAPP-ControlMessage-Format1,
        ...
    },
    ...
}
```

| Field (format 1) | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `data-size` | `INTEGER` | `0..maxE2SMDAPPCtrlMsgSize` (32768), no extension | no | the number of octets in `data` |
| `data` | `OCTET STRING` | `SIZE(1..maxE2SMDAPPCtrlMsgSize)` (1..32768) | no | the service model's own encoding of the xApp control, delivered unchanged to the dApp as the `xAppControlData` of an `E3-XAppControlAction`. Opaque to E2SM-DAPP. |

### 7.3 `E2SM-DAPP-ControlOutcome`

```text
E2SM-DAPP-ControlOutcome ::= SEQUENCE {
    ric-controlOutcome-formats CHOICE {
        controlOutcome-Format1  E2SM-DAPP-ControlOutcome-Format1,
        ...
    },
    ...
}
E2SM-DAPP-ControlOutcome-Format1 ::= SEQUENCE {
    timestamp           Timestamp  OPTIONAL,
    sequence-id         SequenceId OPTIONAL,
    e3-control-outcome  OCTET STRING OPTIONAL,
    ...
}
```

| Field (format 1) | Type | Range | Opt | Notes |
|---|---|---|---|---|
| `timestamp` | `Timestamp` | | yes | when the RAN built the outcome (not when anything was applied). OAI: `built_ts_ns`, taken once and used both here and in the inner payload (`oai/openair2/E3AP/e3_ctrl_ack_relay.c:67,76`). |
| `sequence-id` | `SequenceId` | | yes | the procedure this outcome answers, copied from the control header. It is what lets an xApp match the acknowledge to the control it sent. |
| `e3-control-outcome` | `OCTET STRING` | unbounded | yes | the service model's account of applying the control. Opaque to E2SM-DAPP. Absent when the RAN only acknowledges. See section 8. |

The outcome has **no `ran-function-id`**. The xApp must remember, per
`sequence-id`, which E3 RAN function it sent the control to, to know which service
model decodes `e3-control-outcome`. The Spectrum example below is decoded by an
xApp that knows it only talks Spectrum.

## 8. The `e3-control-outcome` payload

`e3-control-outcome` is the payload of the control outcome. E2SM-DAPP defines the
envelope (the three fields above) and nothing about what is inside. This is the
same tunnelling `ControlMessage-Format1.data` does for the control itself and
`IndicationMessage-Format1.data` does for the report.

### 8.1 Who owns what

| Layer | Owner | Defines |
|---|---|---|
| `E2SM-DAPP-ControlOutcome-Format1` | this standard | `timestamp`, `sequence-id`, and that there may be an opaque payload |
| the bytes in `e3-control-outcome` | the **service model named by the control's `ran-function-id`** | the PDU, its encoding, its units, and what "applied" means for that service model |

"Applied" means something different for every service model. For a spectrum
control, a PRB block is in effect only once the scheduler has put the mask on the
air, which is a later instant than the control handler returning. For a different
service model it may be a single instant, a verdict, or a set of counters. E2SM-DAPP
cannot say, so it does not.

### 8.2 Worked example: the Spectrum service model

The text below is quoted from the Spectrum service model's grammar,
`flexric/src/sm/dapp_sm/e3/service_models/spectrum_sm/asn/defs/e3sm_spectrum.asn`
lines 175-217 (the "APPLY OUTCOME STRUCTURES" part). **It is owned by the Spectrum
service model, not by this standard**, and it is licensed by its owners as
`LicenseRef-CSSL-1.0` (Copyright (c) 2026 Institute for Intelligent Networked
Systems (INSI) at Northeastern University). It is quoted here to show what an
outcome can contain. Do not implement it from this document; take the grammar
file.

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
`Spectrum-ApplyOutcomeData` has no extension marker; the inner types do.

| Spectrum field | Meaning | Absent means |
|---|---|---|
| `Spectrum-ApplyOutcomeData.timestamp` | when the RAN built the outcome | not stamped |
| `installedTimestamp` | the mask was written into the MAC (B17 in [03](03-procedure-flow.md)) | the control was rejected: it never installed |
| `onAirTimestamp` | the first scheduler tick that put the mask on the air (B18) | the control installed but a later install replaced it before any tick ran (it was *superseded*), or it timed out |
| `sfn` (0..1023), `slot` (0..159) | frame and slot of that tick | no tick |

How the RAN fills it (OAI): `oai/openair2/E3AP/service_models/spectrum_sm/spectrum_enc.c:280-336`,
`spectrum_encode_apply_outcome`. `installedTimestamp` is present when it is not
zero. `onAirTimestamp`, `sfn` and `slot` are present together when
`onAirTimestamp` is not zero. The timestamp is present when it is not zero. The
caller in `oai/openair2/E3AP/e3_ctrl_ack_relay.c:64-102` omits the whole payload
when neither `installed_ts_ns` nor `on_air_ts_ns` is set, so a timed-out control
with nothing known is acknowledged with an outcome that has `timestamp` and
`sequence-id` and no payload.

**A nested example (hand-derived).** An outcome built at
`1700000000200000000` ns, installed at `1700000000100000000`, on the air at
`1700000000103000000` in frame 512, slot 7, for procedure 42:

| Layer | Bytes |
|---|---|
| inner `Spectrum-ApplyOutcomeData` (32 octets) | `80 08 17979cfe4215c200 3c 08 17979cfe3c1fe100 08 17979cfe3c4da7c0 02 00 07` |
| outer `E2SM-DAPP-ControlOutcome` (45 octets) | `1c 08 17979cfe4215c200 01 2a 20` followed by the 32 inner octets |

The inner value reads: bit `1` timestamp present; timestamp (`08` + 8 octets);
choice extension bit `0`; `Spectrum-PRBBlockApplyOutcome` extension bit `0`;
presence bits `1111`; two pad bits (so the octet is `3c`); `installedTimestamp`;
`onAirTimestamp`; `sfn` 512 as a 2-octet aligned integer (range 0..1023); `slot` 7
as an 8-bit field (range 0..159). A superseded control (installed, never on air)
has presence bits `1000` and ends after `installedTimestamp`:
`80 08 17979cfe4215c200 20 08 17979cfe3c1fe100`. These bytes were derived by hand
with the rules in [04-encoding.md](04-encoding.md). They are **not** golden
vectors, and no Spectrum codec was run on them.

### 8.3 How a consumer reads it

There are two decode steps. The first belongs to E2SM-DAPP and the second to the
service model.

**Step 1, E2SM-DAPP.** Decode `E2SM-DAPP-ControlOutcome`. In flexric that is
`dapp_dec_ctrl_out_asn()` (`dapp_dec_asn.c:405-426`), which returns an
`e2sm_dapp_ctrl_out_t` by value. The fields the consumer reads are `timestamp_ns`,
`sequence_id`, `e3_control_outcome` (pointer) and `e3_control_outcome_size`.

* **xDevSM** mirrors the C struct with `ctypes`:
  `e2sm_dapp_ctrl_out_frmt_1_t` in
  `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:12-22`,
  fields `timestamp_ns` (`c_int64`), `sequence_id` (`c_int64`),
  `e3_control_outcome` (`POINTER(c_uint8)`), `e3_control_outcome_size`
  (`c_uint32`), in that order. `DAppControlOutWrapper.decode()` calls
  `dapp_dec_ctrl_out_asn`. The file carries `# TODO need fixing` (line 37) and has
  no free binding: the `free_e2sm_dapp_ctrl_out` wrapper is commented out with
  `# TODO free function missing in library` (lines 41-42), so each decode leaks the
  payload buffer.
* `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlAck.py`
  (`decode_control_ack`, lines 63-102) peels the E2AP envelope, then the E2SM
  outcome, and returns a `DAppControlAck` with `timestamp_ns`, `sequence_id` and
  `apply_outcome` (raw `bytes`, or `None`). It converts `0` to `None` for the
  timestamp and the sequence id (flexric's "0 means absent"), and returns an ack
  with all of these `None` when the E2AP outcome field is empty or the decode fails.

**Step 2, the service model.** The consumer hands `apply_outcome` to the decoder of
the service model it sent the control to.

* `xdevsm/src/xdevsm/sm_framework/py_oran/dapp/e3/SpectrumApplyOutcome.py`,
  `decode_apply_outcome(payload)`: calls `spectrum_sm_dec_apply_outcome` in
  `libdapp_sm.so` (`flexric/.../spectrum_sm/asn/decoder.c:80-123`) and returns
  `(installed_ts_ns, on_air_ts_ns, sfn, slot)`. Missing values are `None`. `sfn` and
  `slot` are `None` unless an on-air timestamp is present.
* `spectranet-xapps/decision_engine_xapp/apply_outcome.py` wraps that helper and
  returns the same four-tuple. It returns `(None, None, None, None)` for an empty
  payload, when the helper is not installed, and when decoding raises
  (lines 35-44). It does not import the Spectrum grammar: it imports the helper from
  `xdevsm`, on the grounds that no Python code is generated for that grammar.
* The decision engine then does
  `mitigation_latency_ns = on_air_ts_ns - origin_ts_ns` and
  `install_latency_ns = installed_ts_ns - origin_ts_ns`, where `origin_ts_ns` is the
  dApp's report timestamp
  (`spectranet-xapps/decision_engine_xapp/data.py:242-245`, in `submit_control`).

**Rules for any consumer:**

1. Treat an absent payload, an empty payload, and an undecodable payload as "no
   values". Never raise because of the payload. The acknowledge itself is real.
2. Use `sequence-id` from the envelope to find the control the outcome answers.
3. Decode with the service model that matches the E3 RAN function id **you put in
   the control header**. The outcome does not say.
4. Free the payload buffer (flexric: `free_e2sm_dapp_ctrl_out`).

### 8.4 How another service model adds its own outcome

No change to E2SM-DAPP is needed. A service model with E3 RAN function id `N`:

1. Defines an outcome PDU in its own grammar. Copy the pattern: a `SEQUENCE` with an
   optional build `timestamp` and a `CHOICE` of outcome payloads that has an
   extension marker, so alternatives can be added without changing the encoding
   width of the existing one (the Spectrum grammar explains why in its comment on
   `Spectrum-DAppControlPayload`).
2. Makes every field `OPTIONAL` that the RAN may not know, so "not known" is
   expressed by absence and not by `0`.
3. In the RAN, when the procedure with `sequence-id` S ends, encodes the PDU and
   passes it as `e3-control-outcome` of an `E2SM-DAPP-ControlOutcome` with the same
   `sequence-id` S. If there is nothing to say, omits the payload.
4. In the xApp, when it sends a control to E3 RAN function `N`, remembers
   `(node, S) -> N`. On the acknowledge, decodes `e3-control-outcome` with the
   decoder of `N`.
5. States its clock and units in its own grammar. If it wants the xApp to compute a
   latency against a dApp report timestamp, both must be `CLOCK_REALTIME` ns on the
   same host's clock, as the Spectrum ones are.

There is no registry of E3 RAN function ids in E2SM-DAPP. The RAN stack assigns
them (OAI: `E3_SM_ID_SPECTRUM 1`, `E3_SM_ID_KPM 2` in
`oai/openair2/E3AP/config/e3_config.h:8-9`), and flexric's RIC-side inner decoder
hard-codes `case 1` for Spectrum
(`flexric/src/sm/dapp_sm/e3/dapp_dec_e3.c:15`). A new service model therefore also
needs its id agreed between the RAN stack, the dApp and the xApp.

## 9. Where the elements are produced and consumed

| Element | Produced by | Consumed by |
|---|---|---|
| RAN function definition | RAN (OAI: `fill_dapp_ran_def`, `ran_func_dapp.c:183`) | RIC and xApp (flexric: `ric_on_e2_setup_dapp_sm_ric`; xDevSM: `DAppFunctionDefWrapper.decode`) |
| Event trigger, action definition | xApp (flexric: `on_subscription_dapp_sm_ric`; xDevSM: `DAppReport.subscribe`) | RAN (`on_subscription_dapp_sm_ag`) |
| Indication header and message | RAN (`on_indication_dapp_sm_ag`) | xApp (`on_indication_dapp_sm_ric`; xDevSM: `DAppReport.decode_message`) |
| Control header and message | xApp (`ric_on_control_req_dapp_sm_ric`; xDevSM: `DAppControlReqWrapper`) | RAN (`on_control_dapp_sm_ag`) |
| Control outcome | RAN (OAI: `send_deferred_ctrl_ack`) | xApp (`ric_on_control_out_dapp_sm_ric`; xDevSM: `decode_control_ack`) |

The per-step account, with code, is in [03-procedure-flow.md](03-procedure-flow.md).

## 10. The same elements in libe3 and OCUDU

The libe3 and OCUDU types mirror this section one to one. The names and the
differences are tabulated in [04-encoding.md](04-encoding.md#5-the-type-map-across-implementations)
and [integrator-guide.md](integrator-guide.md#the-ocudu-to-libe3-c-interface).
