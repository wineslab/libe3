# E3SM-SPEC: example message exchange

A complete exchange of the Spectrum service model in APER and in JSON, from the setup to the PRB block that an xApp asked for.

This is Annex A of the specification. It is informative.

## General

This annex gives examples of the use of the service model. Each E3AP message and each service model message was encoded in APER with the grammar of [1] and of clause 8, decoded back and compared with the input. The JSON forms follow the JSON encoding of [1] and clause 8. The identifiers, names and values are examples.

Hexadecimal dumps show the offset of the first octet of each line, then 16 octets.

## PRB block loop with an xApp

A dApp on the host of the RAN subscribes to RAN function 1. It receives the configuration of the uplink carrier and the sensing indications, finds four PRBs occupied and reports them. An xApp asks to block them, and the dApp sends the PRB block control. Table B-1 lists the messages.

Table: Messages of the example

| Step | Direction | E3AP message | APER octets | Service model message |
|---|---|---|---|---|
| 1 | dApp to RAN | E3 SETUP REQUEST | 56 | none |
| 2 | RAN to dApp | E3 SETUP RESPONSE | 160 | RAN function data, 104 octets |
| 3 | dApp to RAN | E3 SUBSCRIPTION REQUEST | 32 | none |
| 4 | RAN to dApp | E3 SUBSCRIPTION RESPONSE | 40 | none |
| 5 | RAN to dApp | E3 INDICATION MESSAGE, cell configuration | 49 | indication, 31 octets |
| 6 | RAN to dApp | E3 INDICATION MESSAGE, sensing | 50 | indication, 32 octets |
| 7 | dApp to RAN | E3 dApp REPORT | 41 | dApp report, 21 octets |
| 8 | RAN to dApp | E3 xApp CONTROL ACTION | 41 | xApp control, 21 octets |
| 9 | dApp to RAN | E3 dApp CONTROL ACTION | 33 | dApp control, 11 octets |
| 10 | RAN to dApp | E3 MESSAGE ACK | 14 | none |

### Step 1: E3 SETUP REQUEST

The dApp registers. The message has no service model payload.

The message in JSON:

```json
{
  "type": "setupRequest",
  "id": 11,
  "timestamp": 1762080000000000000,
  "e3apProtocolVersion": "1.0.0",
  "dAppName": "spectrum-sensing",
  "dAppVersion": "0.1.0",
  "vendor": "example-vendor"
}
```

The message in APER has 56 octets:

```
0000  40 0a 08 18 74 2a 6c 91 d2 00 00 00 80 31 2e 30
0010  2e 30 3c 73 70 65 63 74 72 75 6d 2d 73 65 6e 73
0020  69 6e 67 20 30 2e 31 2e 30 34 65 78 61 6d 70 6c
0030  65 2d 76 65 6e 64 6f 72
```

### Step 2: E3 SETUP RESPONSE

The RAN lists RAN function 1 with the telemetry identifiers 1 to 5 and the control identifiers 1 and 2.

The RAN function data in APER has 104 octets:

```
0000  50 73 70 65 63 74 72 75 6d 5f 73 6d 00 01 b0 53
0010  70 65 63 74 72 75 6d 20 73 65 72 76 69 63 65 20
0020  6d 6f 64 65 6c 20 66 6f 72 20 73 65 6e 73 69 6e
0030  67 20 72 61 6e 67 65 20 69 6e 64 69 63 61 74 69
0040  6f 6e 2c 20 50 52 42 20 62 6c 6f 63 6b 20 61 6e
0050  64 20 73 65 6e 73 69 6e 67 20 70 6f 6c 69 63 79
0060  20 63 6f 6e 74 72 6f 6c
```

The message in JSON:

```json
{
  "type": "setupResponse",
  "id": 51,
  "timestamp": 1762080000000250000,
  "requestId": 11,
  "responseCode": "positive",
  "ranIdentifier": "example-ran",
  "e3apProtocolVersion": "1.0.0",
  "dAppIdentifier": 3,
  "ranFunctionList": [
    {
      "ranFunctionIdentifier": 1,
      "telemetryIdentifierList": [1, 2, 3, 4, 5],
      "controlIdentifierList": [1, 2],
      "ranFunctionData": {
        "name": "spectrum_sm",
        "version": 1,
        "description": "Spectrum service model for sensing range indication, PRB block and sensing policy control"
      }
    }
  ]
}
```

The message in APER has 160 octets:

```
0000  40 32 08 18 74 2a 6c 91 d5 d0 90 0b 80 0a 14 65
0010  78 61 6d 70 6c 65 2d 72 61 6e 20 31 2e 30 2e 30
0020  00 02 02 00 00 00 05 00 00 00 01 00 02 00 03 00
0030  04 00 02 00 00 00 01 68 50 73 70 65 63 74 72 75
0040  6d 5f 73 6d 00 01 b0 53 70 65 63 74 72 75 6d 20
0050  73 65 72 76 69 63 65 20 6d 6f 64 65 6c 20 66 6f
0060  72 20 73 65 6e 73 69 6e 67 20 72 61 6e 67 65 20
0070  69 6e 64 69 63 61 74 69 6f 6e 2c 20 50 52 42 20
0080  62 6c 6f 63 6b 20 61 6e 64 20 73 65 6e 73 69 6e
0090  67 20 70 6f 6c 69 63 79 20 63 6f 6e 74 72 6f 6c
```

The octets 56 to 159 of the message are the RAN function data, unchanged.

### Step 3: E3 SUBSCRIPTION REQUEST

The dApp asks for the five telemetry identifiers and for the PRB block control. It sets no `periodicity`, so it receives one sensing indication for each sensing publish.

The message in JSON:

```json
{
  "type": "subscriptionRequest",
  "id": 12,
  "timestamp": 1762080000001000000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "telemetryIdentifierList": [1, 2, 3, 4, 5],
  "controlIdentifierList": [1]
}
```

The message in APER has 32 octets:

```
0000  40 0b 08 18 74 2a 6c 91 e1 42 40 10 00 02 00 00
0010  00 05 00 00 00 01 00 02 00 03 00 04 00 01 00 00
```

### Step 4: E3 SUBSCRIPTION RESPONSE

The RAN grants the request. The service model starts and creates the sensing ring.

The message in JSON:

```json
{
  "type": "subscriptionResponse",
  "id": 52,
  "timestamp": 1762080000001250000,
  "requestId": 12,
  "dAppIdentifier": 3,
  "responseCode": "positive",
  "subscriptionId": 1,
  "telemetryIdentifierList": [1, 2, 3, 4, 5],
  "controlIdentifierList": [1],
  "ranFunctionIdentifier": 1
}
```

The message in APER has 40 octets:

```
0000  40 33 08 18 74 2a 6c 91 e5 12 d0 27 80 0b 00 02
0010  00 00 00 00 05 00 00 00 01 00 02 00 03 00 04 00
0020  01 00 00 03 00 02 00 00
```

### Step 5: E3 INDICATION MESSAGE, cell configuration

The uplink carrier has 273 PRBs at 30 kHz, starts at point A, and point A is NR-ARFCN 620000, that is 3300 MHz. The TDD pattern has a period of 10 slots, so a frame of 20 slots holds it twice. The RAN sends this indication about once a second.

The indication in APER has 31 octets:

```
0000  b0 07 19 26 1a cf 05 42 80 01 10 00 00 60 09 75
0010  e0 13 09 44 44 44 44 44 44 44 53 55 55 00 01
```

The message in JSON:

```json
{
  "type": "indicationMessage",
  "id": 53,
  "timestamp": 1762080000002050000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "protocolData": {
    "indicationType": "cellConfig",
    "cellConfigIndication": {
      "timestamp": 7078771002000000,
      "ul_carrier_prbs": 273,
      "ul_offset_to_carrier": 0,
      "ul_subcarrier_spacing": 1,
      "ul_arfcn_point_a": 620000,
      "slots_per_frame": 20,
      "tdd_slot_pattern": "DDDDDDDSUU",
      "nr_pci": 1
    }
  }
}
```

The message in APER has 49 octets:

```
0000  40 34 08 18 74 2a 6c 91 f1 47 d0 28 00 02 00 00
0010  00 1e b0 07 19 26 1a cf 05 42 80 01 10 00 00 60
0020  09 75 e0 13 09 44 44 44 44 44 44 44 53 55 55 00
0030  01
```

The octets 18 to 48 of the message are the indication, unchanged.

### Step 6: E3 INDICATION MESSAGE, sensing

The scheduler published three sensing ranges for slot 8 of frame 513, an uplink slot, and the RAN wrote them into ring slot 17. The `timestamp` of the indication is on the monotonic clock of the RAN host. The `timestamp` of the E3AP message is on the realtime clock (UTC) [1]. The two are not compared.

The indication in APER has 32 octets:

```
0000  40 07 19 26 1a d6 db 21 95 02 01 00 08 00 0e 2f
0010  65 33 5f 6c 32 5f 73 65 6e 73 69 6e 67 00 11 03
```

The message in JSON:

```json
{
  "type": "indicationMessage",
  "id": 54,
  "timestamp": 1762080000133500000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "protocolData": {
    "indicationType": "sensing",
    "sensingIndication": {
      "sensing_shm": {
        "shm_name": "/e3_l2_sensing",
        "write_idx": 17,
        "n_ranges": 3
      },
      "timestamp": 7078771133456789,
      "sfn": 513,
      "slot": 8,
      "beam": 0
    }
  }
}
```

The message in APER has 50 octets:

```
0000  40 35 08 18 74 2a 6c 99 c7 0c 60 28 00 02 00 00
0010  00 1f 40 07 19 26 1a d6 db 21 95 02 01 00 08 00
0020  0e 2f 65 33 5f 6c 32 5f 73 65 6e 73 69 6e 67 00
0030  11 03
```

The octets 18 to 49 of the message are the indication, unchanged.

The dApp reads ring slot 17 at octet offset 64 + 17 x 2072 = 35288 of the region. It checks that the `sfn` and the `slot` of the ring slot are 513 and 8, then reads the first three of its sensing ranges.

### Step 7: E3 dApp REPORT

The dApp found PRBs 120 to 123 occupied and reports them with a new `sequenceId`, 41. The RAN does not decode the report. It relays the octets to the xApp.

The dApp report in APER has 21 octets:

```
0000  80 08 18 74 2a 6c 9a 2a 3b 00 00 00 04 00 78 00
0010  79 00 7a 00 7b
```

The message in JSON:

```json
{
  "type": "dAppReport",
  "id": 13,
  "timestamp": 1762080000140050000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "sequenceId": 41,
  "reportData": {
    "reportType": "prbBlacklist",
    "timestamp": 1762080000140000000,
    "reportPayload": {
      "prbBlacklistReport": {
        "blacklistedPRBs": [120, 121, 122, 123]
      }
    }
  }
}
```

The message in APER has 41 octets:

```
0000  40 0c 08 18 74 2a 6c 9a 2a fe 50 38 00 02 00 00
0010  00 28 00 14 80 08 18 74 2a 6c 9a 2a 3b 00 00 00
0020  04 00 78 00 79 00 7a 00 7b
```

The octets 20 to 40 of the message are the dApp report, unchanged.

### Step 8: E3 xApp CONTROL ACTION

The xApp asks to block the four PRBs. The RAN relays its control with the `sequenceId` 41 of the report. The `timestamp` of the xApp control is on the realtime clock of the xApp host.

The xApp control in APER has 21 octets:

```
0000  80 08 18 74 2a 6c 9a e1 56 00 00 00 04 00 78 00
0010  79 00 7a 00 7b
```

The message in JSON:

```json
{
  "type": "xAppControlAction",
  "id": 55,
  "timestamp": 1762080000152300000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "sequenceId": 41,
  "xAppControlData": {
    "controlType": "prbBlocked",
    "timestamp": 1762080000152000000,
    "controlPayload": {
      "prbBlockedControl": {
        "blockedPRBs": [120, 121, 122, 123]
      }
    }
  }
}
```

The message in APER has 41 octets:

```
0000  40 36 08 18 74 2a 6c 9a e5 e9 e0 40 00 02 00 00
0010  00 28 00 14 80 08 18 74 2a 6c 9a e1 56 00 00 00
0020  04 00 78 00 79 00 7a 00 7b
```

The octets 20 to 40 of the message are the xApp control, unchanged.

### Step 9: E3 dApp CONTROL ACTION

The dApp sends the complete list of PRBs to block, with control identifier 1 and the `sequenceId` 41 of the xApp control.

The dApp control in APER has 11 octets:

```
0000  00 00 04 00 78 00 79 00 7a 00 7b
```

The message in JSON:

```json
{
  "type": "dAppControlAction",
  "id": 14,
  "timestamp": 1762080000152800000,
  "dAppIdentifier": 3,
  "ranFunctionIdentifier": 1,
  "controlIdentifier": 1,
  "sequenceId": 41,
  "actionData": {
    "controlType": "prbBlacklist",
    "controlPayload": {
      "prbBlacklistControl": {
        "blacklistedPRBs": [120, 121, 122, 123]
      }
    }
  }
}
```

The message in APER has 33 octets:

```
0000  40 0d 08 18 74 2a 6c 9a ed 8b 00 32 00 02 00 00
0010  00 00 00 28 00 0a 00 00 04 00 78 00 79 00 7a 00
0020  7b
```

The octets 22 to 32 of the message are the dApp control, unchanged.

### Step 10: E3 MESSAGE ACK

The RAN has installed the block in the scheduler.

The message in JSON:

```json
{
  "type": "messageAck",
  "id": 56,
  "timestamp": 1762080000153500000,
  "requestId": 14,
  "responseCode": "positive"
}
```

The message in APER has 14 octets:

```
0000  40 37 08 18 74 2a 6c 9a f8 39 60 50 0d 00
```

## Apply outcome

The RAN tells the xApp when the block of the procedure with `sequenceId` 41 is in effect. The E2 control outcome carries the apply outcome. It has no E3AP message and no JSON form, so this example shows the APER encoding only.

Table: Values of the apply outcome

| Field | Value |
|---|---|
| `timestamp` | `1762080000160000000` |
| `installedTimestamp` | `1762080000153400000`, 0.6 ms after the dApp control was sent |
| `onAirTimestamp` | `1762080000154000000` |
| `sfn`, `slot` | 515, 9: 41 slots after the sensing indication of step 6 |

The apply outcome in APER has 32 octets:

```
0000  80 08 18 74 2a 6c 9b 5b 68 00 3c 08 18 74 2a 6c
0010  9a f6 b2 c0 08 18 74 2a 6c 9a ff da 80 02 03 09
```

All the timestamps of the outcome are on the realtime clock, so the xApp can subtract the `timestamp` of the dApp report of step 7 from `onAirTimestamp`: the block was on air 14 ms after the dApp reported the PRBs.
