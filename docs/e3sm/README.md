# E3 service models

An E3 service model (E3SM) gives meaning to the payloads that E3AP carries for one
RAN function. E3AP moves the payloads as octet strings and does not read them. The
service model defines what each payload contains, how it is encoded and how the RAN
function and the dApp use it.

Each service model has its own folder with the same six chapters as the
[E2SM-DAPP specification](../e2sm-dapp/README.md) and the [E3AP specification](../e3ap/README.md).
The L1-KPM draft follows the clause layout of the E2 service models instead: services,
RAN function description, information elements with ASN.1 and JSON, error handling, and
annexes.

| Service model | RAN function ID | What it does | Folder | Word edition |
|---|---|---|---|---|
| E3SM Spectrum | 1 | Sensing telemetry, PRB blacklist control, sensing policy control, dApp reports and xApp controls for spectrum sharing | [spectrum/](spectrum/README.md) | `O-RAN.XXX.TS.E3SM-SPEC-R000-v00.01.docx` |
| E3SM L1-KPM (draft) | 2 | Telemetry from the uplink layer 1: the PUSCH and SRS data of each uplink slot (79 telemetry identifiers and the valid symbol mask), selection of cells and UEs, and a shared memory layout for the large arrays | [l1-kpm/](l1-kpm/README.md) | `O-RAN.XXX.TS.E3SM-L1-KPM-R000-v00.01.docx` |

## What the service models share

* The RAN function identifier is the number that E3AP carries in `ranFunctionIdentifier`. It has no link to the E2 RAN function identifier. See [E2SM-DAPP](../e2sm-dapp/01-identity-and-registration.md) for the difference.
* A payload travels in one octet string field of one E3AP message: `ranFunctionData` in the Setup response, `protocolData` in an indication, `actionData` in a dApp control action, `reportData` in a dApp report and `xAppControlData` in an xApp control action. See [E3AP information elements](../e3ap/02-ie-mapping.md).
* The encoding of a payload follows the encoding of E3AP on the same link: APER, JSON or Protobuf. See [E3AP encoding](../e3ap/04-encoding.md).
* A payload that crosses hosts carries a realtime timestamp. A payload that stays on one host and points into shared memory carries a monotonic timestamp. The two clocks shall never be subtracted from each other. See [latrec](../utilities/latrec.md#clock-model).

## How a new service model joins

A new service model needs a folder next to the two above with the same chapters, a
RAN function identifier that no other service model uses, and a Word edition built
with `tools/oran_docx` (see its `README.md`).
