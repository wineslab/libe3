# E2SM-DAPP

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

## Where each part is specified

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

## Where the artifacts are

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

## Related documents

* [path-b-e2-e3-loop.md](../path-b-e2-e3-loop.md) and [path-a-e3-loop.md](../path-a-e3-loop.md):
  the E2-E3 loop and its box numbers, which [03](03-procedure-flow.md) uses.
* [latrec.md](../latrec.md): the latency recorder, whose monotonic clock must not be
  mixed with the realtime timestamps of E2SM-DAPP.
* `messages/asn1/V1/e3ap-1.0.0.asn1`: E3AP, where `E3-SequenceID` is the same value
  as E2SM-DAPP's `sequence-id`.

## Where the code references point

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
