# E2SM-DAPP: E2AP information element mapping

This chapter defines every information element of E2SM-DAPP, in the shape of an
O-RAN E2SM specification. It shows where each element sits in E2AP. It gives the
fields with types, ranges and presence. It lists the formats and styles. It says
which E2 service uses which format.

The index is [README.md](../e2sm-dapp/README.md). The bytes of each element are in
[04-encoding.md](04-encoding.md). The message order is in
[03-procedure-flow.md](03-procedure-flow.md). The meaning of the values and the
checks on them are in
[05-semantics-and-constraints.md](05-semantics-and-constraints.md).

How to read the field tables:

* **Range** is the ASN.1 constraint. A `...` after a range is an extension marker
  on that integer.
* **Presence** is `optional` for an `OPTIONAL` field. Otherwise it is `mandatory`.
* Every `SEQUENCE` and `CHOICE` has an extension marker (`...`). No extension
  additions exist yet.

## 1. The elements at a glance

Table: E2SM-DAPP elements and the E2AP fields that carry them

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

All payload IEs are APER encodings of the ASN.1 type. E2AP tunnels them in an
`OCTET STRING`. The ASN.1 grammar is in
[04-encoding.md](04-encoding.md).

<a id="e2-service-formats"></a>

## 2. Which E2 service uses which format

E2SM-DAPP implements two of the E2 services: **REPORT** and **CONTROL**. The table
lists the report styles and the control style that the RAN advertises. It gives the
format that each IE uses under each style.

Table: Styles and the formats of their IEs

| E2 service | Style type | Style name | Event trigger | Action definition | Indication header | Indication message | Control header | Control message | Control outcome |
|---|---|---|---|---|---|---|---|---|---|
| REPORT | 1 | E3 data report | format 1 | format 1 | format 1 | format 1 | n/a | n/a | n/a |
| REPORT | 2 | E3 subscription map | format 1 | format 1 | format 2 | format 2 | n/a | n/a | n/a |
| CONTROL | 1 | dApp control | n/a | n/a | n/a | n/a | format 1 | format 1 | format 1 |
| INSERT | none | | not used | not used | not used | not used | not used | not used | not used |
| POLICY | none | | not used | not used | not used | not used | not used | not used | not used |
| QUERY | none | | not used | not used | not used | not used | not used | not used | not used |

Rules for styles and formats:

* **Style numbers.** Report style 1 is the E3 data report. Report style 2 is the E3
  subscription map. The subscriber chooses the report style with `ric-Style-Type` in
  the action definition.
* **Style to format.** The RAN function definition carries the style-to-format
  mapping. It uses `ric-IndicationHeaderFormat-Type` and
  `ric-IndicationMessageFormat-Type`. Style 1 uses `1` and `1`. Style 2 uses `2`
  and `2`. The RAN that builds the definition shall fill these in. The definition
  that a RAN actually sends is authoritative. Test data does not define the
  standard.
* **Control style number.** The control style is `1`. The numbering spaces for
  report styles and control styles are separate in E2SM. The two `1` values do not
  collide. The control header does **not** carry a style number. A stack with a
  second control style needs a new control header format.
* **Style names are free text.** `RIC-Style-Name` is any `PrintableString` of 1 to
  150 characters. The names in use differ by implementation:

  Table: Style names in use

  | Implementation | Report style 1 | Report style 2 | Control style 1 |
  |---|---|---|---|
  | implementation A (an OpenAirInterface RAN) | `DAPP-E3-DATA-REPORT` | `DAPP-E3-SUBSCRIPTION-MAP` | `DAPP-CONTROL-STYLE-1` |
  | implementation B (test data and a second RAN) | `E3 Data Report` | `E3 Subscription Map` | `dApp Control` |

* **Selection by name.** Some xApps find the control style **by name**. They look for
  the string `DAPP-CONTROL-STYLE-1` in the definition. They refuse to send a control
  if the string is not there. A RAN that works with such xApps shall use that name.

### 2.1 Why INSERT, POLICY and QUERY are "not used"

E2SM-DAPP defines none of them.

Table: Why the other E2 services are not used

| Claim | Reason |
|---|---|
| The RAN function definition has no Insert, Policy or Query section. | The grammar of `E2SM-DAPP-RANFunctionDefinition` lists only name, event trigger, report and control. |
| An E2 agent accepts only a report action. | A subscription with an insert or policy action aborts such an agent. |
| The E2AP model of an agent has no query action type. | The action types are `report`, `insert` and `policy`. |
| An xApp framework builds only report actions. | The action type is `report`. |
| Every indication is a REPORT indication. | The indication type is always `report`. |
| The event trigger needs no timer. | The service model answers every subscription with an aperiodic outcome. The RAN drives the reports, not a period. |

The reasons were checked in the implementations. The evidence is in the
integrator guide.

### 2.2 Why there is no call process ID

E2AP carries an optional Call Process ID on an indication and on a control request.
E2SM-DAPP does not use it.

Table: Why the call process ID is not used

| Claim | Reason |
|---|---|
| The agent never sets it on an indication. | The indication path fills only the header and the message. |
| The agent never reads it on a control request. | The agent reads only the header, the message and the RIC ID. |
| The agent writes none on the control acknowledge. | The field is absent. |
| A sender may still put one. | One xApp framework sends the string `1` in every control request. The receiver ignores it. A conforming sender omits it. |

## 3. Common types

Several elements use these types.

Table: Common ASN.1 types

| ASN.1 type | Definition | Meaning |
|---|---|---|
| `RIC-Style-Type` | `INTEGER` (unbounded) | style number. 1 and 2 for reports, 1 for control. An implementation may store it in 32 bits. |
| `RIC-Format-Type` | `INTEGER` (unbounded) | format number in the RAN function definition. 1 or 2. An implementation may store it in 32 bits. |
| `RIC-Style-Name` | `PrintableString (SIZE(1..150, ...))` | free text |
| `Timestamp` | `INTEGER` (unbounded) | nanoseconds since the Unix epoch on the realtime clock. In every message it is `OPTIONAL`. Absent means that the producer did not stamp. That is a normal message. |
| `SequenceId` | `INTEGER` (unbounded) | correlation id of one detection-to-mitigation procedure. `OPTIONAL` everywhere. It has the same value as `E3-SequenceID` of E3AP (`INTEGER (1..4294967295)`). |
| `SubscribedE3RANFunction-ID` | `INTEGER (0..4294967295, ...)` | an E3 RAN function id |
| `Bool-Type` | `BOOLEAN` | defined and **never used** in the grammar |
| `RANfunction-Name` | `SEQUENCE` | see the RAN function definition below |

The standard has two identifier spaces besides the identifiers of E2. The first is
the **E3 RAN function id**. It says which E3 service model a report or a control
belongs to, for example Spectrum. The second is the **dApp id**. It says which dApp.
Both come from the E3 interface. They are not E2 identifiers. The E2 RAN function
id of E2SM-DAPP is 255. See
[01-identity-and-registration.md](01-identity-and-registration.md).

The E2SM-DAPP ids relate to the E3AP ids as follows. In E2SM-DAPP, `ran-function-id`
and `dapp-id` are `INTEGER (0..4294967295, ...)`. In E3AP, `E3-DAppID` is
`INTEGER (1..65535)` and `ranFunctionIdentifier` is `INTEGER (1..65535)`. Every E3
value is valid in E2SM-DAPP. The reverse is not true. E2SM-DAPP can carry `0` and
values above `65535`. The E3 link cannot deliver them. See
[05-semantics-and-constraints.md](05-semantics-and-constraints.md).

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

Table: Fields of the event trigger

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ric-eventTrigger-formats` | `CHOICE` | one alternative: `eventTrigger-Format1` | mandatory | |
| `eventTrigger-Format1` | empty `SEQUENCE` | no fields | mandatory | The subscription has no trigger condition. The RAN sends indications when it has something to report. |

A subscriber shall always send the empty trigger. The agent decodes it and ignores
it.

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

Table: Fields of the action definition

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ric-Style-Type` | `RIC-Style-Type` | unbounded integer; in use: 1, 2 | mandatory | It selects the report style. It therefore selects the indication formats that the subscriber receives. See [the style table](#e2-service-formats). |
| `actionDefinition-formats` | `CHOICE` | one alternative: `actionDefinition-Format1` | mandatory | |
| `actionDefinition-Format1` | empty `SEQUENCE` | no fields | mandatory | |

Style 1 receives indication header and message format 1 only. Style 2 receives
format 2 only. To get both, the subscriber sends two RIC Subscription Requests with
different RIC request ids. Both implementations behave this way.

The action definition is optional in E2AP. Some agents cope with its absence. Other
agents require it and abort without one. A subscription with a style number other
than 1 or 2 can abort the E2 node: the RAN answers with an empty result, and an
agent with assertions on rejects that result. A subscriber shall send an action
definition with style 1 or 2.

## 5. RAN function definition

The RAN sends the RAN function definition in E2 Setup and in RIC Service Update. It
advertises the styles. For dApp discovery, it also advertises the current **dApp E3
subscription map**.

```text
E2SM-DAPP-RANFunctionDefinition ::= SEQUENCE {
    ranFunction-Name                     RANfunction-Name,
    ranFunctionDefinition-EventTrigger   RANFunctionDefinition-EventTrigger  OPTIONAL,
    ranFunctionDefinition-Report         RANFunctionDefinition-Report        OPTIONAL,
    ranFunctionDefinition-Control        RANFunctionDefinition-Control       OPTIONAL,
    ...
}
```

Table: Fields of the RAN function definition

| Field | Type | Presence | Notes |
|---|---|---|---|
| `ranFunction-Name` | `RANfunction-Name` | mandatory | see the name table below |
| `ranFunctionDefinition-EventTrigger` | `RANFunctionDefinition-EventTrigger` | optional | empty `SEQUENCE { ... }`. Its presence says that the function has an event trigger. One RAN omits it. |
| `ranFunctionDefinition-Report` | `RANFunctionDefinition-Report` | optional | see 5.2 |
| `ranFunctionDefinition-Control` | `RANFunctionDefinition-Control` | optional | see 5.3 |

### 5.1 `RANfunction-Name`

Table: Fields of RANfunction-Name

| Field | Type | Range | Presence | Value in use |
|---|---|---|---|---|
| `ranFunction-ShortName` | `PrintableString` | `SIZE(1..150, ...)` | mandatory | `E2SM-DAPP` |
| `ranFunction-E2SM-OID` | `PrintableString` | `SIZE(1..1000, ...)` | mandatory | `1.3.6.1.4.1.53148.1.1.255.3` |
| `ranFunction-Description` | `PrintableString` | `SIZE(1..150, ...)` | mandatory | `DAPP Service Model for E2/E3 bridge` |
| `ranFunction-Instance` | `INTEGER` | unbounded | optional | not used. A sender shall not set it. One encoder refuses to encode it, and one decoder never reads it. |

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

Table: Fields of the report section

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ric-ReportStyle-List` | `SEQUENCE OF` | `SIZE(1..2)` (`maxnoofDAPPReportStyles`) | mandatory | |
| `ric-ReportStyle-Type` | `RIC-Style-Type` | 1 or 2 in use | mandatory | |
| `ric-ReportStyle-Name` | `RIC-Style-Name` | 1..150 chars | mandatory | |
| `ric-IndicationHeaderFormat-Type` | `RIC-Format-Type` | 1 or 2 in use | mandatory | the indication header format that the style produces |
| `ric-IndicationMessageFormat-Type` | `RIC-Format-Type` | 1 or 2 in use | mandatory | the indication message format that the style produces |
| `dappE3Subscriptions` | `DAppE3Subscription-List` | 0..256 items | optional | the dApp E3 subscription map at the time the RAN built the definition. A RAN should attach it to report style 1 only. It should attach it only when at least one dApp has at least one subscribed function. |

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

Table: Fields of the control section

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ric-ControlStyle-List` | `SEQUENCE OF` | `SIZE(1..1)` (`maxnoofDAPPControlStyles`) | mandatory | exactly one control style |
| `ric-ControlStyle-Type` | `RIC-Style-Type` | 1 | mandatory | |
| `ric-ControlStyle-Name` | `RIC-Style-Name` | 1..150 chars | mandatory | See the style name table in [section 2](#e2-service-formats). |
| `ric-ControlHeaderFormat-Type` | `RIC-Format-Type` | 1 | mandatory | |
| `ric-ControlMessageFormat-Type` | `RIC-Format-Type` | 1 | mandatory | |
| `ric-ControlOutcomeFormat-Type` | `RIC-Format-Type` | 1 | mandatory | |
| `dappE3Subscriptions` | `DAppE3Subscription-List` | 0..256 items | optional | the same map as in the report item. A RAN should copy it from report style 1. |

### 5.4 dApp E3 subscription list

```text
DAppE3Subscription-Item ::= SEQUENCE {
    dapp-id                      INTEGER (0..4294967295, ...),
    subscribed-e3-ran-functions  SEQUENCE (SIZE(1..maxnoofSubscribedE3RANFunctions)) OF SubscribedE3RANFunction-ID,
    ...
}
DAppE3Subscription-List ::= SEQUENCE (SIZE(0..maxnoofDApps)) OF DAppE3Subscription-Item
```

Table: Fields of the dApp E3 subscription list

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| (list) | `DAppE3Subscription-List` | `SIZE(0..256)` (`maxnoofDApps`) | | The list can be empty. |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the dApp |
| `subscribed-e3-ran-functions` | `SEQUENCE OF SubscribedE3RANFunction-ID` | `SIZE(1..64)` (`maxnoofSubscribedE3RANFunctions`) | mandatory | the E3 RAN functions that the dApp has subscribed to. An item shall have at least one. |
| element | `SubscribedE3RANFunction-ID` | `0..4294967295, ...` | | |

The same list type appears in three places: the report item, the control item and
indication message format 2. The list means the same in all three: "these dApps are
connected, and each has subscribed to these E3 RAN functions". How an xApp uses it
is in
[03-procedure-flow.md](03-procedure-flow.md#discovery-how-an-xapp-learns-the-dapp-ids).

A RAN shall build the list from the subscription map of its E3 agent. It shall drop
every dApp with zero subscribed functions. This keeps the one-or-more rule of the
grammar.

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

Table: Fields of indication header format 1

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ran-function-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the **E3 RAN function id** of the report. It identifies the service model that produced the report, for example Spectrum. It is not the E2 RAN function id 255. It selects the decoder for the payload in indication message format 1. The RAN copies it from the E3 report. |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the dApp that sent the report |
| `node-type` | `INTEGER` | `0..255, ...` | mandatory | the type of the reporting node. The grammar does not enumerate values. One RAN fills it with its own RRC node-type enumeration. |
| `node-plmn-id` | `OCTET STRING` | `SIZE(3)` | mandatory | three octets. The grammar does not say how they are packed. One RAN writes the high octet of the MCC, the low octet of the MCC and the low octet of the MNC. That is **not** the 3GPP BCD `PLMN-Identity` encoding. |
| `node-nb-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the gNB id of the node. One RAN uses the NR cell id shifted right by 14 bits. |
| `node-cu-du-id` | `INTEGER` | `0..4294967295, ...` | optional | the CU or DU id inside the node. One RAN uses the gNB-DU id. It omits the field when the node identity is unknown. |
| `timestamp` | `Timestamp` | | optional | the time at which the RAN built this indication, in nanoseconds on the realtime clock. It is **not** the report timestamp of the dApp. That timestamp travels inside the payload. |
| `sequence-id` | `SequenceId` | | optional | the correlation id of the dApp for this report. The RAN copies it from `E3-DAppReport.sequenceId`. The xApp echoes it on its control header. |

When the node identity is unavailable, one RAN sets `node-type`, `node-plmn-id` and
`node-nb-id` to zero and omits `node-cu-du-id`. The grammar has no "unknown" value.
This is a convention of that RAN. It is not part of the standard.

**Format 2** is the header of a style-2 indication: the dApp subscription map.

Table: Fields of indication header format 2

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `node-type` | `INTEGER` | `0..255, ...` | mandatory | as in format 1 |
| `node-plmn-id` | `OCTET STRING` | `SIZE(3)` | mandatory | as in format 1 |
| `node-nb-id` | `INTEGER` | `0..4294967295, ...` | mandatory | as in format 1 |
| `node-cu-du-id` | `INTEGER` | `0..4294967295, ...` | optional | as in format 1 |
| `timestamp` | `Timestamp` | | optional | the time at which the RAN built the indication. A RAN should set it. |
| `sequence-id` | `SequenceId` | | optional | No procedure is behind a subscription map. A RAN should leave the field absent. |

Format 2 has no `ran-function-id` and no `dapp-id`. The map names the dApps itself.

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

**Format 1** pairs with header format 1.

Table: Fields of indication message format 1

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `data-size` | `INTEGER` | `0..maxE2SMDAPPIndMsgSize` (32768), no extension | mandatory | the number of octets in `data`. It repeats the length of the octet string. |
| `data` | `OCTET STRING` | `SIZE(1..maxE2SMDAPPIndMsgSize)` (1..32768) | mandatory | the encoding of the dApp report by the service model. The RAN copies it unchanged from the E3 `reportData`. E2SM-DAPP does not interpret it. It has at least one octet. |

The service model that the header field `ran-function-id` names decodes `data`. For
Spectrum, that is `Spectrum-DAppReportData`. It holds a timestamp (the own timestamp
of the dApp, in nanoseconds on the realtime clock) and the PRB list.

**Format 2** pairs with header format 2.

Table: Fields of indication message format 2

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `dappE3Subscriptions` | `DAppE3Subscription-List` | `SIZE(0..256)` | mandatory | the current dApp E3 subscription map. It can be empty when no dApp is connected. |

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

Table: Fields of control header format 1

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `ran-function-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the E3 RAN function id that the control is for. The RAN forwards the payload to the dApp under this id. The service model with this id decodes the control message and builds the control outcome. |
| `dapp-id` | `INTEGER` | `0..4294967295, ...` | mandatory | the dApp that is the target of the control |
| `timestamp` | `Timestamp` | | optional | the time at which the xApp built the control, in nanoseconds on the realtime clock of the host of the xApp. Unlike the RAN-side timestamps, it is **not** on the clock of the gNB. |
| `sequence-id` | `SequenceId` | | optional | the id of the procedure. The xApp echoes it from the indication header that prompted this control. **If it is absent or `0`, the RAN cannot defer the acknowledge.** The RAN then answers immediately and applies nothing. |

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

Table: Fields of control message format 1

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `data-size` | `INTEGER` | `0..maxE2SMDAPPCtrlMsgSize` (32768), no extension | mandatory | the number of octets in `data` |
| `data` | `OCTET STRING` | `SIZE(1..maxE2SMDAPPCtrlMsgSize)` (1..32768) | mandatory | the encoding of the xApp control by the service model. The RAN delivers it unchanged to the dApp as the `xAppControlData` of an `E3-XAppControlAction`. E2SM-DAPP does not interpret it. |

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

Table: Fields of control outcome format 1

| Field | Type | Range | Presence | Notes |
|---|---|---|---|---|
| `timestamp` | `Timestamp` | | optional | the time at which the RAN built the outcome. It is not the time at which anything was applied. The RAN takes the time once and uses it here and in the inner payload. |
| `sequence-id` | `SequenceId` | | optional | the procedure that this outcome answers. The RAN copies it from the control header. It lets an xApp match the acknowledge to the control that it sent. |
| `e3-control-outcome` | `OCTET STRING` | unbounded | optional | the account of the service model on applying the control. E2SM-DAPP does not interpret it. It is absent when the RAN only acknowledges. See [section 8](#e3-control-outcome-payload). |

The outcome has **no `ran-function-id`**. The xApp shall remember, for each
`sequence-id`, the E3 RAN function to which it sent the control. That tells the xApp
which service model decodes `e3-control-outcome`. The Spectrum example below
assumes an xApp that talks only to Spectrum.

<a id="e3-control-outcome-payload"></a>

## 8. The `e3-control-outcome` payload

`e3-control-outcome` is the payload of the control outcome. E2SM-DAPP defines the
envelope (the three fields above). It defines nothing about the content. The same
tunnelling is used for the control itself (`ControlMessage-Format1.data`) and for the
report (`IndicationMessage-Format1.data`).

### 8.1 Who owns what

Table: Ownership of the layers of the control outcome

| Layer | Owner | Defines |
|---|---|---|
| `E2SM-DAPP-ControlOutcome-Format1` | this standard | `timestamp`, `sequence-id`, and that an opaque payload may follow |
| the bytes in `e3-control-outcome` | the **service model named by the `ran-function-id` of the control** | the PDU, its encoding, its units, and what "applied" means for that service model |

"Applied" has a different meaning for each service model. For a spectrum control, a
PRB block is in effect only after the scheduler puts the mask on the air. That
instant is later than the instant at which the control handler returns. For another
service model, "applied" can be a single instant, a verdict or a set of counters.
E2SM-DAPP cannot say which. It does not try.

### 8.2 Worked example: the Spectrum service model

The Spectrum service model owns the apply outcome. It does not belong to this
standard. The grammar is in the Spectrum specification
([E3SM Spectrum](../e3sm/spectrum/README.md)). Do not implement the outcome from
this document. This section shows what an outcome can contain.

Spectrum defines an apply-outcome PDU, `Spectrum-ApplyOutcomeData`. It is a
`SEQUENCE` with no extension marker. It has an optional `timestamp` and an
`applyOutcomePayload`. The payload is a `CHOICE` with an extension marker. It has
one alternative today, `prbBlockApplyOutcome`. That alternative is a `SEQUENCE` with
an extension marker and four optional fields. Both timestamps are `INTEGER` values
in nanoseconds on the realtime clock of the gNB. The xApp can therefore subtract
them from the timestamp of the Spectrum dApp report, which is on the same clock.
They are payload fields. They are not latency recorder records, because the latency
recorder uses the monotonic clock. Never subtract one from the other. The outcome
`timestamp` is the time at which the outcome was built. The applied instants are in
the payload.

`maxSlotIndex` is 159. It is `maxSlotsFrame` (160) minus 1.

Table: Fields of the Spectrum apply outcome

| Spectrum field | Type and range | Meaning | Absent means |
|---|---|---|---|
| `Spectrum-ApplyOutcomeData.timestamp` | `INTEGER` | when the RAN built the outcome | not stamped |
| `installedTimestamp` | `INTEGER` | the mask was written into the MAC (box B17 in [03-procedure-flow.md](03-procedure-flow.md)) | the control was rejected: it never installed |
| `onAirTimestamp` | `INTEGER` | the first scheduler tick that put the mask on the air (box B18). It is later than `installedTimestamp` by up to one slot, and by a variable amount. | the control installed, but a later install replaced it before any tick ran (it was *superseded*), or it timed out |
| `sfn` | `INTEGER (0..1023)` | frame of that tick | no tick |
| `slot` | `INTEGER (0..159)` | slot of that tick | no tick |

How the RAN fills the outcome:

* `installedTimestamp` is present when it is not zero.
* `onAirTimestamp`, `sfn` and `slot` are present together when `onAirTimestamp` is
  not zero.
* The `timestamp` is present when it is not zero.
* The RAN omits the whole payload when neither the installed time nor the on-air
  time is set. A timed-out control with nothing known is then acknowledged with an
  outcome that has `timestamp` and `sequence-id` and no payload.

**A nested example (hand-derived).** An outcome was built at `1700000000200000000`
ns. The mask was installed at `1700000000100000000`. It was on the air at
`1700000000103000000`, in frame 512, slot 7. The procedure is 42.

Table: Bytes of the nested example

| Layer | Bytes |
|---|---|
| inner `Spectrum-ApplyOutcomeData` (32 octets) | `80 08 17979cfe4215c200 3c 08 17979cfe3c1fe100 08 17979cfe3c4da7c0 02 00 07` |
| outer `E2SM-DAPP-ControlOutcome` (45 octets) | `1c 08 17979cfe4215c200 01 2a 20` followed by the 32 inner octets |

The inner value reads as follows:

* Bit `1`: timestamp present.
* The timestamp (`08` and 8 octets).
* Choice extension bit `0`.
* `Spectrum-PRBBlockApplyOutcome` extension bit `0`.
* Presence bits `1111`.
* Two pad bits, so the octet is `3c`.
* `installedTimestamp`.
* `onAirTimestamp`.
* `sfn` 512 as a 2-octet aligned integer (range 0..1023).
* `slot` 7 as an 8-bit field (range 0..159).

A superseded control was installed and never on the air. It has presence bits
`1000` and ends after `installedTimestamp`:
`80 08 17979cfe4215c200 20 08 17979cfe3c1fe100`.

The bytes were derived by hand with the rules in [04-encoding.md](04-encoding.md).
They are **not** golden vectors. No Spectrum codec was run on them.

### 8.3 How a consumer reads it

A consumer decodes in two steps. The first step belongs to E2SM-DAPP. The second
belongs to the service model.

**Step 1, E2SM-DAPP.** The consumer decodes `E2SM-DAPP-ControlOutcome`. It reads
four values: the timestamp, the sequence id, the payload and the payload size. A
decoder that stores "absent" as `0` gives the consumer a `0`. The consumer shall map
that `0` back to "absent" for the timestamp and the sequence id. When the E2AP
outcome field is empty, or the decode fails, the consumer shall treat all four
values as absent.

**Step 2, the service model.** The consumer passes the payload to the decoder of the
service model to which it sent the control. For Spectrum, the decoder returns the
installed time, the on-air time, the frame and the slot. Each value can be absent.
The frame and the slot are absent unless an on-air time is present.

A decision engine can then compute two latencies from the dApp report timestamp
`origin`:

* the mitigation latency: the on-air time minus `origin`;
* the install latency: the installed time minus `origin`.

Rules for any consumer:

1. Treat an absent payload, an empty payload and an undecodable payload as "no
   values". Never raise an error because of the payload. The acknowledge itself is
   real.
2. Use `sequence-id` from the envelope to find the control that the outcome answers.
3. Decode with the service model that matches the E3 RAN function id **that you put
   in the control header**. The outcome does not say.
4. Free the payload buffer, if the decoder allocates one.

### 8.4 How another service model adds its own outcome

A service model with E3 RAN function id `N` needs no change to E2SM-DAPP. It does
the following.

1. It defines an outcome PDU in its own grammar. It copies the pattern: a `SEQUENCE`
   with an optional build `timestamp` and a `CHOICE` of outcome payloads. The
   `CHOICE` has an extension marker. Alternatives can then be added without a change
   to the encoding width of the existing one. The Spectrum grammar explains this in
   its comment on `Spectrum-DAppControlPayload`.
2. It makes optional every field that the RAN may not know. "Not known" is then
   absence, not `0`.
3. In the RAN, when the procedure with `sequence-id` S ends, it encodes the PDU. It
   passes the PDU as `e3-control-outcome` of an `E2SM-DAPP-ControlOutcome` with the
   same `sequence-id` S. If there is nothing to say, it omits the payload.
4. In the xApp, when the xApp sends a control to E3 RAN function `N`, it remembers
   `(node, S) -> N`. On the acknowledge, it decodes `e3-control-outcome` with the
   decoder of `N`.
5. It states its clock and units in its own grammar. If it wants the xApp to compute
   a latency against a dApp report timestamp, both timestamps shall be in
   nanoseconds on the realtime clock of one host, as the Spectrum ones are.

E2SM-DAPP has no registry of E3 RAN function ids. The RAN stack assigns them. A
RIC-side decoder can hard-code the id of a service model. A new service model
therefore needs its id agreed between the RAN stack, the dApp and the xApp.

## 9. Where the elements are produced and consumed

Table: Producers and consumers of the elements

| Element | Produced by | Consumed by |
|---|---|---|
| RAN function definition | the RAN | the RIC and the xApp |
| Event trigger, action definition | the xApp | the RAN |
| Indication header and message | the RAN | the xApp |
| Control header and message | the xApp | the RAN |
| Control outcome | the RAN | the xApp |

The step-by-step account is in [03-procedure-flow.md](03-procedure-flow.md).

## 10. Other representations of the elements

Other implementations mirror the elements one to one in their own types. The names
and the differences are in [04-encoding.md](04-encoding.md) and in the
[integrator guide](integrator-guide.md).
