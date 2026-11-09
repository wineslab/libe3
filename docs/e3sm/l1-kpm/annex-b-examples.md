# Example message exchange

## General

This annex gives examples of the use of the service model. Each E3AP message and each service model IE was encoded in APER with the grammar of [1] and of clause 8.4, decoded back and compared with the input. The JSON forms follow clause 8.3 of [1] and clause 8.6. The identifiers, names and values are examples.

Hexadecimal dumps show the offset of the first octet of each line, then 16 octets.

## IQ samples of one cell

A dApp asks for the IQ samples of each PUSCH slot, with the time, the frame and slot numbers, the cell, the number of antennas and the uplink symbols: identifiers 1, 4, 5, 6, 7, 8 and 1000, with `periodicity` `0`.

The SUBSCRIPTION REQUEST message carries no service model payload. Its JSON form is:

```json
{
  "type": "subscriptionRequest",
  "id": 3,
  "timestamp": 1762080000000500000,
  "dAppIdentifier": 1,
  "ranFunctionIdentifier": 2,
  "telemetryIdentifierList": [1, 4, 5, 6, 7, 8, 1000],
  "controlIdentifierList": [],
  "periodicity": 0
}
```

Its APER encoding has 36 octets:

```
0000  40 02 08 18 74 2a 6c 91 d9 a1 20 11 00 00 00 01
0010  00 07 00 00 00 03 00 04 00 05 00 06 00 07 03 e7
0020  00 00 00 00
```

Each PUSCH slot gives one INDICATION MESSAGE message. The INDICATION DATA IE in JSON is:

```json
{
  "timestamp": 7078771123456789,
  "sfn": 512,
  "slot": 9,
  "cells": [
    {
      "cell_id": 1,
      "iq_samples": {
        "shm_name": "/e3_ran_buffers",
        "fh_buffer_index": 0,
        "fh_write_index": 42
      },
      "n_rx_ant": 4,
      "valid_symbol_mask": 16383
    }
  ]
}
```

In APER the INDICATION DATA IE has 39 octets:

```
0000  5a 07 19 26 1a d6 42 8b 15 02 00 09 05 20 08 00
0010  01 0e 2f 65 33 5f 72 61 6e 5f 62 75 66 66 65 72
0020  73 00 00 2a 0c 3f ff
```

The INDICATION MESSAGE message has 57 octets. The octets 18 to 56 are the INDICATION DATA IE above, unchanged:

```
0000  40 27 08 18 74 2a 6c 99 2e 75 e0 28 00 00 00 01
0010  00 26 5a 07 19 26 1a d6 42 8b 15 02 00 09 05 20
0020  08 00 01 0e 2f 65 33 5f 72 61 6e 5f 62 75 66 66
0030  65 72 73 00 00 2a 0c 3f ff
```

The dApp locates the row with the header of the region (Annex A). With `num_fh_samples` `366912` (4 antennas) and `num_fh_rows` `120`, `fh_buffer_size` is `88058880`.

Table: Position of the IQ data of the example

| Item | Computation | Octet offset in the region |
|---|---|---|
| Buffer 0 of ID 1 | `200 + 0 x 88058880` | `200` |
| Row 42 | `200 + 42 x 2 x 366912` | `30820808` |
| In-phase word of antenna 2, symbol 3, subcarrier 100 | row + `2 x ((2 x 14 + 3) x 3276 + 100) x 2` | `31227432` |

The mask `16383` says that all 14 symbols are uplink symbols.

## UE metrics with a UE filter

A dApp asks for PUSCH metrics of each UE every 10 ms, for 60 s. It then keeps one UE with the Set Subscription Data control action. Table B-2 lists the messages.

Table: Messages of the UE metrics example

| Step | Direction | E3AP message | APER octets | Service model IE |
|---|---|---|---|---|
| 1 | dApp to E3 Agent | SETUP REQUEST | 50 | none |
| 2 | E3 Agent to dApp | SETUP RESPONSE | 231 | RAN FUNCTION DATA IE, 150 octets |
| 3 | dApp to E3 Agent | SUBSCRIPTION REQUEST | 55 | none |
| 4 | E3 Agent to dApp | SUBSCRIPTION RESPONSE | 60 | none |
| 5 | E3 Agent to dApp | INDICATION MESSAGE | 85 | INDICATION DATA IE, 67 octets |
| 6 | dApp to E3 Agent | dApp CONTROL ACTION | 29 | CONTROL DATA IE, 6 octets |
| 7 | E3 Agent to dApp | CONTROL RESPONSE | 21 | none |
| 8 | E3 Agent to dApp | INDICATION MESSAGE | 61 | INDICATION DATA IE, 43 octets |

### Step 1: SETUP REQUEST

The dApp registers. The message has no service model payload.

The message in JSON:

```json
{
  "type": "setupRequest",
  "id": 7,
  "timestamp": 1762080000000000000,
  "e3apProtocolVersion": "1.0.0",
  "dAppName": "ue-metrics",
  "dAppVersion": "0.1.0",
  "vendor": "example-vendor"
}
```

The message in APER has 50 octets:

```
0000  40 06 08 18 74 2a 6c 91 d2 00 00 00 80 31 2e 30
0010  2e 30 24 75 65 2d 6d 65 74 72 69 63 73 20 30 2e
0020  31 2e 30 34 65 78 61 6d 70 6c 65 2d 76 65 6e 64
0030  6f 72
```

### Step 2: SETUP RESPONSE

The E3 Agent lists RAN function 2 with the identifiers that it can produce, and control identifier 1. The RAN FUNCTION DATA IE is cut to three stream descriptors here. A complete one has a descriptor for each identifier of clause 7.3. ID 36 is "unavailable", so it is not in `telemetryIdentifierList`.

The RAN FUNCTION DATA IE in APER has 150 octets:

```
0000  14 4c 31 2d 4b 50 4d 00 01 56 4c 31 2d 4b 50 4d
0010  3a 20 70 6f 73 74 2d 46 46 54 20 49 51 2c 20 50
0020  55 53 43 48 20 61 6e 64 20 53 52 53 20 74 65 6c
0030  65 6d 65 74 72 79 04 40 00 00 24 69 71 5f 73 61
0040  6d 70 6c 65 73 58 61 72 72 61 79 28 69 6e 74 31
0050  36 29 03 80 2f 65 33 5f 72 61 6e 5f 62 75 66 66
0060  65 72 73 c0 05 3f ac 00 00 77 20 00 11 0c 72 73
0070  72 70 30 66 6c 6f 61 74 33 32 20 c0 72 73 72 70
0080  00 00 23 24 75 65 5f 67 72 70 5f 69 64 78 28 75
0090  69 6e 74 31 36 80
```

The message in JSON:

```json
{
  "type": "setupResponse",
  "id": 29,
  "timestamp": 1762080000000250000,
  "requestId": 7,
  "responseCode": "positive",
  "ranIdentifier": "example-ran",
  "e3apProtocolVersion": "1.0.0",
  "dAppIdentifier": 2,
  "ranFunctionList": [
    {
      "ranFunctionIdentifier": 2,
      "telemetryIdentifierList": [1, 4, 5, 6, 7, 8, 10, 16, 18, 21, 23, 25, 26, 33, 34, 39, 41, 1000],
      "controlIdentifierList": [1],
      "ranFunctionData": {
        "name": "L1-KPM",
        "version": 1,
        "description": "L1-KPM: post-FFT IQ, PUSCH and SRS telemetry",
        "streams": [
          {
            "telemetryIdentifier": 1,
            "stream_id": "iq_samples",
            "data_type": "array(int16)",
            "status": "available",
            "delivery_method": {
              "transport_type": "shared_memory",
              "shared_memory_info": {
                "memory_key": "/e3_ran_buffers",
                "memory_size_bytes": 88058880,
                "num_rows": 120
              }
            }
          },
          {
            "telemetryIdentifier": 18,
            "stream_id": "rsrp",
            "data_type": "float32",
            "status": "available",
            "delivery_method": {
              "transport_type": "protocolData",
              "keyword": "rsrp"
            }
          },
          {
            "telemetryIdentifier": 36,
            "stream_id": "ue_grp_idx",
            "data_type": "uint16",
            "status": "unavailable"
          }
        ]
      }
    }
  ]
}
```

The message in APER has 231 octets:

```
0000  40 1c 08 18 74 2a 6c 91 d5 d0 90 0b 80 06 14 65
0010  78 61 6d 70 6c 65 2d 72 61 6e 20 31 2e 30 2e 30
0020  00 01 02 00 01 00 12 00 00 00 03 00 04 00 05 00
0030  06 00 07 00 09 00 0f 00 11 00 14 00 16 00 18 00
0040  19 00 20 00 21 00 26 00 28 03 e7 00 01 00 00 80
0050  96 14 4c 31 2d 4b 50 4d 00 01 56 4c 31 2d 4b 50
0060  4d 3a 20 70 6f 73 74 2d 46 46 54 20 49 51 2c 20
0070  50 55 53 43 48 20 61 6e 64 20 53 52 53 20 74 65
0080  6c 65 6d 65 74 72 79 04 40 00 00 24 69 71 5f 73
0090  61 6d 70 6c 65 73 58 61 72 72 61 79 28 69 6e 74
00a0  31 36 29 03 80 2f 65 33 5f 72 61 6e 5f 62 75 66
00b0  66 65 72 73 c0 05 3f ac 00 00 77 20 00 11 0c 72
00c0  73 72 70 30 66 6c 6f 61 74 33 32 20 c0 72 73 72
00d0  70 00 00 23 24 75 65 5f 67 72 70 5f 69 64 78 28
00e0  75 69 6e 74 31 36 80
```

The octets 81 to 230 of the message are the RAN FUNCTION DATA IE, unchanged.

### Step 3: SUBSCRIPTION REQUEST

The dApp asks for identifiers 4, 5, 6, 7, 16, 18, 23, 25, 26, 33, 34, 36, 39 and 41, and control identifier 1, every 10 ms for 60 s. All the identifiers are shared or of the PUSCH path, so only the PUSCH path fires (Table 6-1).

The message in JSON:

```json
{
  "type": "subscriptionRequest",
  "id": 8,
  "timestamp": 1762080000001000000,
  "dAppIdentifier": 2,
  "ranFunctionIdentifier": 2,
  "telemetryIdentifierList": [4, 5, 6, 7, 16, 18, 23, 25, 26, 33, 34, 36, 39, 41],
  "controlIdentifierList": [1],
  "subscriptionTime": 60,
  "periodicity": 10000
}
```

The message in APER has 55 octets:

```
0000  40 07 08 18 74 2a 6c 91 e1 42 40 13 00 01 00 01
0010  00 0e 00 03 00 04 00 05 00 06 00 0f 00 11 00 16
0020  00 18 00 19 00 20 00 21 00 23 00 26 00 28 00 01
0030  00 00 00 3c 40 27 10
```

### Step 4: SUBSCRIPTION RESPONSE

The E3 Agent grants the requested identifiers that are available. ID 36 is not granted.

The message in JSON:

```json
{
  "type": "subscriptionResponse",
  "id": 30,
  "timestamp": 1762080000001250000,
  "requestId": 8,
  "dAppIdentifier": 2,
  "responseCode": "positive",
  "subscriptionId": 4,
  "telemetryIdentifierList": [4, 5, 6, 7, 16, 18, 23, 25, 26, 33, 34, 39, 41],
  "controlIdentifierList": [1],
  "ranFunctionIdentifier": 2,
  "periodicity": 10000
}
```

The message in APER has 60 octets:

```
0000  40 1d 08 18 74 2a 6c 91 e5 12 d0 27 80 07 00 01
0010  00 00 03 00 0d 00 03 00 04 00 05 00 06 00 0f 00
0020  11 00 16 00 18 00 19 00 20 00 21 00 26 00 28 00
0030  01 00 00 03 80 02 00 01 03 40 27 10
```

### Step 5: INDICATION MESSAGE

A PUSCH slot with two UEs in cell 1. Only the granted fields are present. The `timestamp` of the INDICATION DATA IE (ID 4) is the time of the slot data, on the monotonic clock of the E3 Agent host. The `timestamp` of the E3AP message is the time at which the E3 Agent created the message, on the realtime clock (UTC) [1]. The two are not compared.

The INDICATION DATA IE in APER has 67 octets:

```
0000  5a 07 19 26 1a d6 db 21 95 02 01 09 04 08 10 00
0010  01 04 0b 80 82 02 90 00 00 00 00 46 01 00 00 00
0020  32 a0 60 cd cc a2 c2 cd cc 94 41 70 10 40 52 00
0030  00 00 00 46 02 00 33 00 17 4c 00 66 66 c0 c2 33
0040  33 83 40
```

The message in JSON:

```json
{
  "type": "indicationMessage",
  "id": 31,
  "timestamp": 1762080000133500000,
  "dAppIdentifier": 2,
  "ranFunctionIdentifier": 2,
  "protocolData": {
    "timestamp": 7078771133456789,
    "sfn": 513,
    "slot": 9,
    "cells": [
      {
        "cell_id": 1,
        "n_ue": 2,
        "ues": [
          {
            "rnti": 17921,
            "rb_start": 0,
            "rb_size": 51,
            "mcs_index": 20,
            "tb_crc_fail": 0,
            "harq_process_id": 3,
            "rsrp": -81.4,
            "sinr": 18.6
          },
          {
            "rnti": 17922,
            "rb_start": 51,
            "rb_size": 24,
            "mcs_index": 9,
            "tb_crc_fail": 1,
            "harq_process_id": 0,
            "rsrp": -96.2,
            "sinr": 4.1
          }
        ]
      }
    ]
  }
}
```

The message in APER has 85 octets:

```
0000  40 1e 08 18 74 2a 6c 99 c7 0c 60 28 00 01 00 01
0010  00 42 5a 07 19 26 1a d6 db 21 95 02 01 09 04 08
0020  10 00 01 04 0b 80 82 02 90 00 00 00 00 46 01 00
0030  00 00 32 a0 60 cd cc a2 c2 cd cc 94 41 70 10 40
0040  52 00 00 00 00 46 02 00 33 00 17 4c 00 66 66 c0
0050  c2 33 33 83 40
```

The octets 18 to 84 of the message are the INDICATION DATA IE, unchanged.

### Step 6: dApp CONTROL ACTION

The dApp keeps only the UE with RNTI 17921 in cell 1.

The CONTROL DATA IE in APER has 6 octets:

```
0000  10 10 00 01 46 01
```

The message in JSON:

```json
{
  "type": "dAppControlAction",
  "id": 9,
  "timestamp": 1762080000140000000,
  "dAppIdentifier": 2,
  "ranFunctionIdentifier": 2,
  "controlIdentifier": 1,
  "requestAck": true,
  "actionData": {
    "set_subscription_data": {
      "ue_filter": [
        {
          "cell_id": 1,
          "rnti": 17921
        }
      ]
    }
  }
}
```

The message in APER has 29 octets:

```
0000  40 08 08 18 74 2a 6c 9a 2a 3b 00 34 00 01 00 01
0010  00 00 00 05 10 10 00 01 46 01 01 01 80
```

The octets 20 to 25 of the message are the CONTROL DATA IE, unchanged. The octets 26 to 28 are the extension addition `requestAck`: the extension bitmap, the length of the addition and the value "true".

### Step 7: CONTROL RESPONSE

The E3 Agent applies the subscription data. The `requestId` is the `id` of the control in step 6.

The message in JSON:

```json
{
  "type": "controlResponse",
  "id": 32,
  "timestamp": 1762080000140200000,
  "requestId": 9,
  "dAppIdentifier": 2,
  "ranFunctionIdentifier": 2,
  "controlIdentifier": 1,
  "responseCode": "positive"
}
```

The message in APER has 21 octets:

```
0000  40 1f 08 18 74 2a 6c 9a 2d 48 40 58 00 08 00 01
0010  00 01 00 00 00
```

### Step 8: INDICATION MESSAGE

10 ms later only UE 17921 is present, and `nUe` counts the entries that the indication carries.

The INDICATION DATA IE in APER has 43 octets:

```
0000  5a 07 19 26 1a d7 73 b8 15 02 02 09 04 08 10 00
0010  01 02 03 80 82 02 90 00 00 00 00 46 01 00 00 00
0020  32 a8 80 cd cc a1 c2 00 00 98 41
```

The message in JSON:

```json
{
  "type": "indicationMessage",
  "id": 33,
  "timestamp": 1762080000143500000,
  "dAppIdentifier": 2,
  "ranFunctionIdentifier": 2,
  "protocolData": {
    "timestamp": 7078771143456789,
    "sfn": 514,
    "slot": 9,
    "cells": [
      {
        "cell_id": 1,
        "n_ue": 1,
        "ues": [
          {
            "rnti": 17921,
            "rb_start": 0,
            "rb_size": 51,
            "mcs_index": 21,
            "tb_crc_fail": 0,
            "harq_process_id": 4,
            "rsrp": -80.9,
            "sinr": 19.0
          }
        ]
      }
    ]
  }
}
```

The message in APER has 61 octets:

```
0000  40 20 08 18 74 2a 6c 9a 5f a2 e0 28 00 01 00 01
0010  00 2a 5a 07 19 26 1a d7 73 b8 15 02 02 09 04 08
0020  10 00 01 02 03 80 82 02 90 00 00 00 00 46 01 00
0030  00 00 32 a8 80 cd cc a1 c2 00 00 98 41
```

The octets 18 to 60 of the message are the INDICATION DATA IE, unchanged.

The Float32 values are stored least significant octet first. For example, `rsrp` -81.4 is the octets `cd cc a2 c2` and `sinr` 18.6 is the octets `cd cc 94 41`. In JSON they are the numbers -81.4 and 18.6.
