# E2SM-DAPP: identity and registration

This chapter gives the numbers and strings that identify E2SM-DAPP. It also says
what the RAN function definition contains and how an E2 node announces the service
model to the RIC.

The index is [README.md](../e2sm-dapp/README.md). The field-by-field description of the
definition is in [02-ie-mapping.md](02-ie-mapping.md#5-ran-function-definition).
Its bytes are in [04-encoding.md](04-encoding.md).

## 1. Identity

Table: Identity of E2SM-DAPP

| Item | Value | Remark |
|---|---|---|
| **RAN function ID** (E2) | `255` | The E2AP range is 0..4095. |
| **RAN function revision** | `1` | One implementation announces `0`. |
| **RAN function short name** | `E2SM-DAPP` | |
| **RAN function description** | `DAPP Service Model for E2/E3 bridge` | |
| **E2SM OID** | `1.3.6.1.4.1.53148.1.1.255.3` | It is also the module identifier of the grammar. |
| **ASN.1 module** | `E2SM-DAPP-IEs`, version 1 of the grammar | |
| **Style numbers** | report styles `1` and `2`, control style `1` | See [02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format). |
| **Instance** (`ranFunction-Instance`) | not used | The field exists in the grammar. No sender sets it. |

### 1.1 How the OID is built

The module identifier in the grammar is:

```text
E2SM-DAPP-IEs {iso(1) identified-organization(3) dod(6) internet(1) private(4)
               enterprise(1) oran(53148) e2(1) version1(1) e2sm(255) e2sm-DAPP-IEs (3)}
```

It reads as `1.3.6.1.4.1.53148.1.1.255.3`. The arc `53148` is the O-RAN enterprise
number. The O-RAN service models use the same arc. The arc `255` is the service
model number. It is deliberately the same number as the E2 RAN function ID. The
final `3` is the module number inside that service model.

One implementation carries a stale source comment that spells the same OID with
`e2sm(2)`. The OID string and the grammar both use `255`.

### 1.2 Two things named "RAN function ID"

E2SM-DAPP touches two identifier spaces. Both carry the words "RAN function ID".
They are different. Do not mix them up.

Table: The two RAN function ID spaces

| Name | Space | Used where | Typical value |
|---|---|---|---|
| E2 RAN function ID | E2AP, `RANfunctionID (0..4095)` | E2 Setup, RIC Subscription, RIC Indication, RIC Control (E2AP fields) | `255` for E2SM-DAPP itself |
| E3 RAN function ID | E3AP, `ranFunctionIdentifier (1..65535)` | inside E2SM-DAPP: `ran-function-id` in the indication header and the control header, and `SubscribedE3RANFunction-ID` in the subscription map | `1` for the Spectrum service model, `2` for the L1-KPM service model |

The golden test vectors use `255` in the `ran-function-id` fields of the header.
That is arbitrary test data, chosen to be easy to recognize. It does not mean that
the E3 function id is 255.

## 2. RAN function definition: what it contains

The definition is one `E2SM-DAPP-RANFunctionDefinition`, APER encoded. The fields
are specified in [02-ie-mapping.md](02-ie-mapping.md#5-ran-function-definition).
In summary:

Table: Parts of the RAN function definition

| Part | Content | Required |
|---|---|---|
| `ranFunction-Name` | short name, OID, description, optional instance | yes |
| `ranFunctionDefinition-EventTrigger` | empty marker | optional |
| `ranFunctionDefinition-Report` | 1 or 2 report styles. Each has a style number, a style name, an indication header format, an indication message format and an optional dApp E3 subscription map. | optional |
| `ranFunctionDefinition-Control` | exactly 1 control style. It has a style number, a style name, a control header format, a control message format, a control outcome format and an optional dApp E3 subscription map. | optional |

A definition with only the name (78 octets in the smallest test vector) is valid.
It advertises nothing. A useful definition has at least a report section or a
control section.

### 2.1 What a RAN sends

A RAN that serves dApps and xApps builds the definition as follows.

Table: Definition content of a RAN

| Part | Value |
|---|---|
| name | `E2SM-DAPP`, the OID, `DAPP Service Model for E2/E3 bridge` |
| event trigger | absent |
| report style 1 | type `1`, name `DAPP-E3-DATA-REPORT`, header format `1`, message format `1`. It carries the **dApp E3 subscription map** if any dApp has a subscribed function. |
| report style 2 | type `2`, name `DAPP-E3-SUBSCRIPTION-MAP`, header format `2`, message format `2`, no map |
| control style 1 | type `1`, name `DAPP-CONTROL-STYLE-1`, header, message and outcome format `1`. It carries the same map as report style 1. |

The map holds one item per dApp that has at least one subscribed E3 RAN function.
An item holds the `dapp-id` and the list of E3 RAN function ids. How an xApp uses
the map is in
[03-procedure-flow.md](03-procedure-flow.md#discovery-how-an-xapp-learns-the-dapp-ids).

Another implementation builds a fixed definition. It has an event trigger section.
Its style names are `E3 Data Report`, `E3 Subscription Map` and `dApp Control`.
It has no dApp E3 subscription map. An xApp that selects the control style by the
name `DAPP-CONTROL-STYLE-1` does not find that style there. The test data in the
repository also uses these names. Test data is not what a RAN advertises. The
content of the definition is the responsibility of the RAN.

### 2.2 Who sets the name block

An E2 agent may replace the name block of the definition. It then writes its own
short name, OID and description, whatever the RAN supplied. The short name shall
not be empty. All other parts of the definition, which are the styles, the formats
and the map, are the responsibility of the RAN. An agent that has no RAN callback
produces a definition with only the name.

## 3. Announcement in E2 Setup and RIC Service Update

### 3.1 E2 Setup Request

The E2 node lists E2SM-DAPP as one **RAN function item** in the E2 Setup Request.

Table: RAN function item for E2SM-DAPP

| E2AP RAN function item field | Value for E2SM-DAPP |
|---|---|
| RAN Function ID | `255` |
| RAN Function Definition (`OCTET STRING`) | the APER encoding of `E2SM-DAPP-RANFunctionDefinition` |
| RAN Function Revision | `1`. One implementation announces `0`. |
| RAN Function OID | `1.3.6.1.4.1.53148.1.1.255.3` |

The E2 node shall rebuild the definition each time it builds an E2 Setup Request.
A retried setup then carries the dApp map as it is at that moment. The E2AP OID
field and the `ranFunction-E2SM-OID` string inside the definition shall have the
same value. The E2 Setup Response has nothing that is specific to E2SM-DAPP.

An E2 node that does not enable E2SM-DAPP does not list it in the E2 Setup Request.

### 3.2 RIC Service Update

The RAN sends a RIC Service Update when the content of the definition changes.
For E2SM-DAPP this happens when a dApp connects or disconnects and the dApp E3
subscription map changes.

Table: RIC Service Update for E2SM-DAPP

| Item | Value |
|---|---|
| Function list | **modified**, exactly one entry, RAN function 255. Nothing is added or deleted. |
| Definition | freshly built, as for E2 Setup |
| OID | `1.3.6.1.4.1.53148.1.1.255.3` |
| Revision | a counter that starts at `1` and increases by one per update. The first update therefore carries revision `1`, which the E2 Setup already used. The revision should increase on every update, the first one included. |
| Transaction id | the same counter modulo 256 |
| Trigger | the RAN triggers the update when the dApp set changes. One implementation does this only after the RIC has been heard from. |

An E2 node whose definition is fixed and carries no dApp map sends no RIC Service
Update for E2SM-DAPP.

Table: What the RIC side does with a RIC Service Update for E2SM-DAPP

| RIC | Behavior |
|---|---|
| a RIC whose service model plug-in is a placeholder | It returns an empty result. It does **not** decode the new definition. |
| a RIC with its own E2 manager | The E2 manager updates its stored node information. An xApp that reads the definition again sees the new one. The dApp E3 subscription map in it is current. |

An E2 agent has a hook for RIC Service Update messages that it **receives** from
the RIC. E2SM-DAPP does not use that hook.

## 4. Requirements for a RAN that announces E2SM-DAPP

A RAN stack that implements the E2 side shall meet these requirements.

1. Register a RAN function with E2 RAN function ID `255`, revision `1` and OID
   `1.3.6.1.4.1.53148.1.1.255.3`.
2. Put an APER `E2SM-DAPP-RANFunctionDefinition` in the RAN function definition.
   It shall contain at least the name. For xApps to use it, it should also contain
   the report section (styles 1 and 2) and the control section (style 1). The
   formats are listed in
   [02-ie-mapping.md](02-ie-mapping.md#2-which-e2-service-uses-which-format).
3. Use the control style name `DAPP-CONTROL-STYLE-1`, if xApps that select the
   control style by name are to control the RAN.
4. Keep every `DAppE3Subscription-Item` to **at least one and at most 64**
   subscribed E3 RAN functions. Keep the list to **at most 256** items. Omit the
   `dappE3Subscriptions` field when no dApp has a subscribed function. Some
   implementations cannot encode an empty list in the definition. They also cannot
   decode more than 255 items or 63 functions.
5. Send a RIC Service Update with the new definition when the dApp set changes.
   Increase the revision each time.
6. Accept RIC Subscription Requests with exactly one report action, event trigger
   format 1 and action definition style 1 or 2. Accept RIC Control Requests with
   an acknowledge request.
