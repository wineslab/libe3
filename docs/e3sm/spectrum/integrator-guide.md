# E3SM-SPEC: integrator guide

For an engineer who adds the Spectrum service model to a RAN, a dApp or an xApp. This guide is not
part of the specification. It holds the code pointers that the specification leaves out.

The specification is in [README.md](../spectrum/README.md). Read [02-ie-mapping.md](02-ie-mapping.md) and
[03-procedure-flow.md](03-procedure-flow.md) first.

## Where the code references point

Code references are `path:line` or a function name that was opened while writing. Function names are
the stable reference. Line numbers drift. A path without a root belongs to `oai/` (the later revision).

Table: Roots and the revisions read

| Short name | What it is | Revision read |
|---|---|---|
| `oai/` | OpenAirInterface with the Spectrum service model and the E2-E3 bridge, later revision (with the apply outcome and the sequence identifier) | `dc3d67360c`. Its flexric submodule holds the E2 side. |
| `oai-first/` | the first revision of the same service model, in the OpenAirInterface branch of the dApp library | branch `ff-check`, as checked out |
| `xapp/` | the xApp side codec of the Spectrum service model | `src/sm/dapp_sm/e3/service_models/spectrum_sm/` of the flexric tree, as checked out |
| `dapp/` | the dApp side codec of the Spectrum service model (`dapps/common/e3_manager/spectrum_report_sm/`) | as checked out |

The OpenAirInterface implementation is also the subject of
https://github.com/duranta-project/openairinterface5g/pull/729.

## Artifacts

| Artifact | Location |
|---|---|
| Grammar, later copy (with the apply outcome) | `xapp/asn/defs/e3sm_spectrum.asn` and `oai/openair2/E3AP/service_models/spectrum_sm/MESSAGES/ASN1/V1/e3sm_spectrum.asn` |
| Grammar, first copy | `oai-first/openair2/E3AP/service_models/spectrum_sm/MESSAGES/ASN1/V1/e3sm_spectrum.asn` |
| Protocol Buffers file | `oai/openair2/E3AP/service_models/spectrum_sm/MESSAGES/PROTO/V1/e3sm_spectrum.proto` |
| RAN service model | `oai/openair2/E3AP/service_models/spectrum_sm/spectrum_sm.c` |
| RAN encoders | `spectrum_enc.c` in the same directory |
| RAN decoders | `spectrum_dec.c` in the same directory |
| Sensing ring | `spectrum_sensing_ring.c` and `.h` in the same directory |
| Design notes | `SPECTRUM_SM_ARCHITECTURE.md`, `SENSING_RAN_DESIGN.md`, `PRB_BLOCK_LIMITATIONS.md` in the same directory |
| Telemetry worker | `oai/openair2/E3AP/service_models/e3_sm_worker.c` |
| Shared memory helper | `oai/openair2/E3AP/service_models/e3_shm_region.c` |
| MAC side of the PRB block | `oai/openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_prb_block.c` |
| MAC side of the sensing policy | `oai/openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_ul_sensing.c` |

## The RAN side

### Register the service model

`create_spectrum_sm_model` (`spectrum_sm.c:334`) builds the descriptor. It encodes the RAN function
data first, with `spectrum_encode_ran_function_data` (`spectrum_enc.c:151`), and returns NULL if that
fails. The agent registers the service model only if its id is in the `enabled_sms` list, or if the list
is empty (`oai/openair2/E3AP/e3_agent.c:195`). The descriptor holds the name `spectrum_sm`, the version
1, the control identifiers 1 and 2 and the telemetry identifiers 1 to 5 (`spectrum_sm.c:73-118`).

### Telemetry

The worker is a shared driver. `telemetry_wait_and_fetch` (`spectrum_sm.c:260`) waits for a MAC sensing
publish, reads the ranges, and writes them to the ring with `spectrum_sensing_ring_write`
(`spectrum_sensing_ring.c:85`). It runs for every publish. `emit_batch` (`spectrum_sm.c:175`) encodes
one indication with `spectrum_encode_indication` (`spectrum_enc.c:170`) and sends it to every
subscriber of RAN function 1. If the ring is not available, `emit_batch` skips the indication.

The emission period comes from `min_subscription_period_us` (`e3_agent.c:63`): the smallest
periodicity of the subscribers, in microseconds, with "0" for any subscriber that wants every event.
`on_dapp_status_changed` (`e3_agent.c:81`) recomputes it.

The ring is created by `spectrum_sensing_ring_init` (`spectrum_sensing_ring.c:42`). It removes any old
region, creates a new one with the mode 0644, and zeroes it. The layout is in
`spectrum_sensing_ring.h`.

### Controls

`spectrum_sm_process_control` (`spectrum_sm.c:650`) is the dispatcher. It checks, in this order, the
RAN function id, that the service model runs, and that the payload is not empty. Then it switches on the
control identifier.

* Identifier 1: `spectrum_process_prb_block` (`spectrum_sm.c:503`). It decodes with
  `spectrum_decode_prb_control` (`spectrum_dec.c:412`), builds a per-PRB mask of `0x3FFF`, and installs
  it in both directions with `set_prb_block_masks` (`spectrum_sm.c:556,559`). The MAC function is
  `set_prb_block_masks` in `gNB_scheduler_prb_block.c:180`.
* Identifier 2: `spectrum_process_sensing_policy` (`spectrum_sm.c:593`). It decodes with
  `spectrum_decode_sensing_policy` (`spectrum_dec.c:591`) and calls `set_sensing_policy`
  (`gNB_scheduler_ul_sensing.c:370`), which checks the number of masks against the slots in a frame
  (line 380).

The decoders pick the codec with `e3_get_encoding` (`e3_agent.c:28`). The JSON envelope check is
`spectrum_json_envelope_extract` (`spectrum_dec.c:176`). The ASN.1 range checks are in
`spectrum_extract_prb_control` (`spectrum_dec.c:38`) and `spectrum_extract_sensing_policy`
(`spectrum_dec.c:101`).

`spectrum_sm_stop` (`spectrum_sm.c:396`) clears the policy and the block. In the first revision it
clears the two directions one after the other (`oai-first` `spectrum_sm.c:421-422`). In the later
revision it makes one call (`spectrum_sm.c:425`).

### The report and the xApp control

`e2_e3_bridge` (`e3_agent.c:33`) is the dApp report handler. It calls
`generate_e2_indication_from_e3_dapp_report` when the build has an E2 agent. Without an E2 agent it
does nothing. `e3_send_xapp_control` (`e3_agent.c:288`) sends an xApp control to a dApp. The E2 side
that calls it is in `oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:533-539`. It first opens a
pending record with `e3_pending_ctrl_open`.

### The apply outcome

The record table is `oai/openair2/E3AP/e3_pending_ctrl.c`. It holds 64 records. The timeout is
`E3_PENDING_CTRL_TIMEOUT_NS` (line 24), 150 ms. The install is recorded at `spectrum_sm.c:577`. The MAC
calls `prb_block_on_air_hook` (`e3_ctrl_ack_relay.c:48`) on the first tick that uses the block
(`gNB_scheduler_prb_block.c:598`), and `prb_block_superseded_hook` when a later block replaces it
(`gNB_scheduler_prb_block.c:165,196`). The relay thread turns each finished record into an outcome with
`send_deferred_ctrl_ack` (`e3_ctrl_ack_relay.c:64`), which encodes it with
`spectrum_encode_apply_outcome` (`spectrum_enc.c:280`).

## The dApp side

1. Send an E3 Setup Request and look for `ranFunctionIdentifier` 1 in the response.
2. Decode `ranFunctionData` in the encoding of the run.
3. Subscribe to RAN function 1. Set `periodicity` to the interval that you can process, or leave it
   unset for every publish.
4. Map `/e3_l2_sensing` read only. Compare the three layout fields of the header with your own.
5. On each indication, read the ring slot `shmWriteIdx`. Compare `sfn` and `slot`. Drop on a mismatch.
6. To block PRBs, send the complete list. Send an empty list to clear.
7. For an xApp control, decode `Spectrum-XAppControlData`, then send the PRB block control with the same
   `sequenceId` in the E3 dApp Control Action.

The dApp side codec is in `dapp/spectrum_control_codec.c`. It decodes the xApp control with APER only
(`spectrum_sm_dec_blocked_control`). The comment at the top of its header says that the dApp sends the
PRB block control as JSON, because the RAN decodes with the encoding of its build.

## The xApp side

The xApp codec is in `xapp/asn/`. `spectrum_sm_enc_control` (`encoder.c:13`) builds a
`Spectrum-XAppControlData` with a `prbBlockedControl`. It refuses more than 273 PRBs or an index above
272. A timestamp of "0" is sent as absent. `spectrum_sm_dec_report` (`decoder.c:11`) decodes the dApp
report. `spectrum_sm_dec_apply_outcome` (`decoder.c:80`) decodes the apply outcome and returns the two
realtime timestamps, the frame and the slot. An xApp has to remember which RAN function it sent the
control to, because the E2 control outcome has no RAN function identifier.

The xApp-side build of the grammar compiles only the report and xApp control chain, and the apply
outcome. The other types are generated and not used (`xapp/asn/defs/CMakeLists.txt`).

## Evidence for the discrepancies

Table: Where each discrepancy comes from

| Discrepancy | Evidence |
|---|---|
| Grammar copies differ | the diff of the two `e3sm_spectrum.asn` files: lines 176 to 219 and the constant `maxSlotIndex` exist only in the later copy |
| JSON indication keys | `spectrum_enc.c:247-267` (`sensing_shm`, `write_idx`) against the grammar and the proto file |
| `samplingThreshold` and `validityPeriod` unused | `spectrum_process_prb_block` reads neither (`spectrum_sm.c:503-590`). `spectrum_process_sensing_policy` logs `validity_period` only (`spectrum_sm.c:629-634`). |
| Strictness differs | JSON path `spectrum_dec.c:328-335`, protobuf path `spectrum_dec.c:394-399`, ASN.1 path `spectrum_dec.c:82-89`, which gets its range check from the generated decoder |
| `ConfigControl` has no codec | no encoder in `xapp/asn/encoder.c`, no decoder in `dapp/spectrum_control_codec.c`, no handler in `spectrum_sm.c` |
| Dead clamp and range check | `spectrum_sm.c:526-529` and `spectrum_sm.c:536` against the 273 and 272 limits of the grammar |
| Beam always sent | `spectrum_enc.c:191-193` sets the pointer whatever the value |
| Empty mask rejected | `spectrum_dec.c:105-110` (ASN.1), `spectrum_dec.c:486-499` (JSON), `spectrum_dec.c:544-548` (protobuf) |
| First revision has no sequence identifier in the control handler | `oai-first` `spectrum_sm.c:92-100` has no `sequence_id` parameter |

## Tests

No test or golden vector for this service model was found in the E3AP library tree. The CI test script
of the OpenAirInterface tree runs the service model against a radio simulator, and this guide did not
read its result. Treat the encodings as untested against a second implementation.
