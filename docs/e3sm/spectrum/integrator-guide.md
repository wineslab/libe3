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
| `oai/` | OpenAirInterface with the Spectrum service model and the E2-E3 bridge, later revision (with the cell configuration indication, the apply outcome and the sequence identifier) | `20bb1a934d`. Its flexric submodule holds the E2 side. |
| `oai-first/` | the first revision of the same service model, in the OpenAirInterface branch of the dApp library | branch `ff-check`, as checked out |
| `xapp/` | the xApp side codec of the Spectrum service model | `src/sm/dapp_sm/e3/service_models/spectrum_sm/` of the flexric submodule of `oai/`, at `71631466bb` |
| `dapp/` | the dApp side codec of the Spectrum service model (`dapps/common/e3_manager/spectrum_report_sm/`) | `cf96e38` of the dApp repository |
| `dapp-app/` | a dApp that uses the service model (`dapps/applications/adaptive_cpu/`) | the same revision as `dapp/` |

The OpenAirInterface implementation is also the subject of
https://github.com/duranta-project/openairinterface5g/pull/729.

## Artifacts

| Artifact | Location |
|---|---|
| Grammar, later copy (with the cell configuration indication and the apply outcome) | `oai/openair2/E3AP/service_models/spectrum_sm/MESSAGES/ASN1/V1/e3sm_spectrum.asn` and `dapp/e3sm_spectrum.asn` |
| Grammar, xApp copy (with the apply outcome, without the cell configuration indication) | `xapp/asn/defs/e3sm_spectrum.asn` |
| Grammar, first copy | `oai-first/openair2/E3AP/service_models/spectrum_sm/MESSAGES/ASN1/V1/e3sm_spectrum.asn` |
| Protocol Buffers file | `oai/openair2/E3AP/service_models/spectrum_sm/MESSAGES/PROTO/V1/e3sm_spectrum.proto` |
| RAN service model | `oai/openair2/E3AP/service_models/spectrum_sm/spectrum_sm.c` |
| RAN encoders | `spectrum_enc.c` in the same directory |
| RAN decoders | `spectrum_dec.c` in the same directory |
| Sensing ring | `spectrum_sensing_ring.c` and `.h` in the same directory |
| Design notes | `SPECTRUM_SM_ARCHITECTURE.md`, `SENSING_RAN_DESIGN.md`, `PRB_BLOCK_LIMITATIONS.md` in the same directory |
| Telemetry workers | `oai/openair2/E3AP/service_models/e3_sm_worker.c` |
| Shared memory helper | `oai/openair2/E3AP/service_models/e3_shm_region.c` |
| MAC side of the PRB block | `oai/openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_prb_block.c` |
| MAC side of the sensing policy, the sensing reservation, the PUSCH slots and the cell configuration | `oai/openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_ul_sensing.c` |
| Pending control records and the apply outcome relay | `oai/openair2/E3AP/e3_pending_ctrl.c`, `oai/openair2/E3AP/e3_ctrl_ack_relay.c` |

## The RAN side

### Register the service model

`create_spectrum_sm_model` (`spectrum_sm.c:441`) builds the descriptor. It encodes the RAN function
data first, with `spectrum_encode_ran_function_data` (`spectrum_enc.c:154`), and returns NULL if that
fails. The description text is `SPECTRUM_RAN_FUNCTION_DESCRIPTION` (`spectrum_enc.c:26`). The agent
registers the service model only if its id is in the `enabled_sms` list, or if the list is empty
(`oai/openair2/E3AP/e3_agent.c:195`). The descriptor holds the name `spectrum_sm`, the version 1, the
telemetry identifiers 1 to 5 and the control identifiers 1 to 4 (`spectrum_sm.c:71-127`).

The specification defines the control identifiers 1 and 2. Identifiers 3 (sensing reservation) and 4
(PUSCH slots) exist only in this RAN. A dApp written from the specification does not send them.

### Telemetry

The service model runs two workers on the shared driver `e3_sm_worker.c`. `spectrum_sm_start`
(`spectrum_sm.c:489`) starts both.

* **Sensing indication.** `telemetry_wait_and_fetch` (`spectrum_sm.c:367`) waits for a MAC sensing
  publish with `nr_mac_wait_for_sensing_publish` (`gNB_scheduler_ul_sensing.c:465`), reads the ranges
  with `nr_mac_get_sensing_ranges` (line 381), and writes them to the ring with
  `spectrum_sensing_ring_write` (`spectrum_sensing_ring.c:85`). It runs for every publish.
  `emit_batch` (`spectrum_sm.c:282`) encodes one indication with `spectrum_encode_indication`
  (`spectrum_enc.c:410`) and sends it to every subscriber of RAN function 1. If the ring is not
  available, `emit_batch` skips the indication.
* **Cell configuration indication.** `cell_config_wait_and_fetch` (`spectrum_sm.c:240`) waits for the
  period `SPECTRUM_CELL_CONFIG_PERIOD_US`, 1 s (line 155), then reads the carrier with
  `nr_mac_get_cell_config` (`gNB_scheduler_ul_sensing.c:504`). It stamps the indication with
  `pub_channel_now_ns()`, the monotonic clock. `emit_cell_config` (`spectrum_sm.c:196`) encodes it with
  `spectrum_encode_cell_config_indication` (`spectrum_enc.c:280`) and sends it to every subscriber of
  RAN function 1. The worker emits whether the MAC publishes or not. It skips a period while the MAC
  has no cell.

`nr_mac_get_cell_config` takes the downlink point A when the uplink has none
(`gNB_scheduler_ul_sensing.c:552-557`), and writes one letter for each slot of the TDD period
(lines 539-545). The cell configuration encoder has an APER and a JSON form. With Protocol Buffers it
logs an error and sends nothing (`spectrum_enc.c:291-293`).

The period of the sensing indication comes from `min_subscription_period_us` (`e3_agent.c:63`): the
smallest periodicity of the subscribers, in microseconds, with "0" for any subscriber that wants every
event. `on_dapp_status_changed` (`e3_agent.c:81`) recomputes it and sets it with
`spectrum_telemetry_set_period_us` (`e3_agent.c:94`). The period of the cell configuration indication
is fixed.

The ring is created by `spectrum_sensing_ring_init` (`spectrum_sensing_ring.c:42`) through
`e3_shm_region_create` (`e3_shm_region.c:20`). That removes any old region and creates a new one with
the mode 0644. The ring init then zeroes it and writes the header. The layout is in
`spectrum_sensing_ring.h`.

### Controls

`spectrum_sm_process_control` (`spectrum_sm.c:861`) is the dispatcher. It checks, in this order, the
RAN function id, that the service model runs, and that the payload is not empty (lines 877-900). Then
it switches on the control identifier (line 903).

* Identifier 1: `spectrum_process_prb_block` (`spectrum_sm.c:631`). It decodes with
  `spectrum_decode_prb_control` (`spectrum_dec.c:415`), builds a per-PRB mask of `0x3FFF` (line 665),
  and installs it in both directions with one call to `set_prb_block_masks` (line 687). An empty list
  clears both directions (line 684). The MAC function is `set_prb_block_masks` in
  `gNB_scheduler_prb_block.c:180`.
* Identifier 2: `spectrum_process_sensing_policy` (`spectrum_sm.c:804`). It decodes with
  `spectrum_decode_sensing_policy` (`spectrum_dec.c:595`). A `deactivate` of "TRUE" clears the policy
  (`spectrum_sm.c:835-836`). Otherwise it calls `set_sensing_policy` (`gNB_scheduler_ul_sensing.c:735`),
  which checks the number of masks against the slots in a frame (line 744).
* Identifiers 3 and 4: `spectrum_process_sensing_reservation` (`spectrum_sm.c:719`) and
  `spectrum_process_sensing_pusch_slots` (`spectrum_sm.c:762`), with `set_sensing_reservation` and
  `set_sensing_pusch_slots` (`gNB_scheduler_ul_sensing.c:198`, `274`). They are not in the
  specification.

The decoders pick the codec with `e3_get_encoding` (`e3_agent.c:28`). The JSON envelope check is
`spectrum_json_envelope_extract` (`spectrum_dec.c:179`). The ASN.1 range checks are in
`spectrum_extract_prb_control` (`spectrum_dec.c:41`) and `spectrum_extract_sensing_policy`
(`spectrum_dec.c:104`).

`spectrum_sm_stop` (`spectrum_sm.c:519`) stops both workers, which removes the ring (lines 531-532).
Then it clears the sensing policy (line 544), restores the configured sensing reservation and PUSCH
slots (lines 550-551), and clears the PRB block in both directions with one call (line 552). In the
first revision it clears the two directions one after the other (`oai-first` `spectrum_sm.c:421-422`).

### The report and the xApp control

`e2_e3_bridge` (`e3_agent.c:33`) is the dApp report handler. It calls
`generate_e2_indication_from_e3_dapp_report` (line 49) when the build has an E2 agent. Without an E2
agent it does nothing. `e3_send_xapp_control` (`e3_agent.c:288`) sends an xApp control to a dApp. The
E2 side that calls it is in `oai/openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_dapp.c:533-539`. It first
opens a pending record with `e3_pending_ctrl_open` (`e3_pending_ctrl.c:60`).

### The apply outcome

The record table is `oai/openair2/E3AP/e3_pending_ctrl.c`. It holds 64 records (`E3_PENDING_CTRL_MAX`,
`e3_pending_ctrl.h:32`). The timeout is `E3_PENDING_CTRL_TIMEOUT_NS` (`e3_pending_ctrl.c:24`), 150 ms. The install
is recorded at `spectrum_sm.c:705`. The MAC calls `prb_block_on_air_hook` (`e3_ctrl_ack_relay.c:48`)
on the first tick that uses the block (`gNB_scheduler_prb_block.c:598`), and
`prb_block_superseded_hook` when a later block replaces it (`gNB_scheduler_prb_block.c:165,196`). The
relay thread turns each finished record into an outcome with `send_deferred_ctrl_ack`
(`e3_ctrl_ack_relay.c:64`), which encodes it with `spectrum_encode_apply_outcome`
(`spectrum_enc.c:443`).

## The dApp side

1. Send an E3 Setup Request and look for `ranFunctionIdentifier` 1 in the response.
2. Decode `ranFunctionData` in the encoding of the run.
3. Subscribe to RAN function 1. Set `periodicity` to the interval that you can process, or leave it
   unset for every publish.
4. Map `/e3_l2_sensing` read only. Compare the three layout fields of the header with your own.
5. On each indication, look at the alternative. A cell configuration indication arrives about once a
   second. Keep its carrier: the PRB indices of the sensing ranges refer to it.
6. On a sensing indication, read the ring slot `shmWriteIdx`. Compare `sfn` and `slot`. Drop on a
   mismatch.
7. To block PRBs, send the complete list. Send an empty list to clear.
8. To keep symbols free for sensing, send a sensing policy with one mask for each slot of the frame.
   To clear it, send `deactivate` "TRUE" with at least one mask.
9. For an xApp control, decode `Spectrum-XAppControlData`, then send the PRB block control with the same
   `sequenceId` in the E3 dApp Control Action.

The dApp side codec is in `dapp/spectrum_control_codec.c`. It decodes the xApp control with APER only
(`spectrum_sm_dec_blocked_control`, line 20). The comment at the top of its header says that the dApp
sends the PRB block control as JSON, because the RAN decodes with the encoding of its build. The
application builds that JSON in `BuildPrbBlockControlJson` (`dapp-app/subcarrier_power_app.cpp:1080`)
and reads the cell configuration in `HandleCellConfig` (line 2301).

## The xApp side

The xApp codec is in `xapp/asn/`. `spectrum_sm_enc_control` (`encoder.c:13`) builds a
`Spectrum-XAppControlData` with a `prbBlockedControl`. It refuses more than 273 PRBs or an index above
272. A timestamp of "0" is sent as absent. `spectrum_sm_dec_report` (`decoder.c:11`) decodes the dApp
report. `spectrum_sm_dec_apply_outcome` (`decoder.c:80`) decodes the apply outcome and returns the two
realtime timestamps, the frame and the slot. An xApp has to remember which RAN function it sent the
control to, because the E2 control outcome has no RAN function identifier.

The xApp-side build of the grammar compiles only the report and xApp control chain, and the apply
outcome. The other types are generated and not used (`xapp/asn/defs/CMakeLists.txt`). Its copy of the
grammar has no cell configuration indication. The xApp does not decode indications, so this does not
matter today.

## Evidence for the discrepancies

The discrepancies are listed in [interop.md](../../interop.md#e3-service-models). This is where each one comes from.

Table: Where each discrepancy comes from

| Discrepancy | Evidence |
|---|---|
| Grammar copies differ | the diff of the first and the later `e3sm_spectrum.asn`: the cell configuration indication (line 40), the apply outcome (lines 206 to 248) and the constant `maxSlotIndex` (line 258) exist only in the later copy. The differences between the later copy and the present grammar are in [interop.md](../../interop.md#e3-service-models). |
| JSON indication keys | `spectrum_enc.c:396-405` (`indicationType`, `sensing_shm`, `write_idx`) and `spectrum_enc.c:260-275` (the cell configuration in lower case with underscores) against the grammar and the proto file |
| Cell configuration has no Protocol Buffers form | `spectrum_enc.c:291-293` |
| `validityPeriod` unused | `spectrum_process_prb_block` reads only the list (`spectrum_sm.c:631-714`). `spectrum_process_sensing_policy` logs `validity_period` only (`spectrum_sm.c:840-843`). |
| Strictness differs | `validityPeriod` of the PRB block: JSON path `spectrum_dec.c:336-337`, protobuf path `spectrum_dec.c:400-401`, with no range check. ASN.1 path `spectrum_dec.c:89-91`, which gets its range check from the generated decoder. |
| `ConfigControl` has no codec | no encoder in `xapp/asn/encoder.c`, no decoder in `dapp/spectrum_control_codec.c`, no handler in `spectrum_sm.c` |
| Dead clamp and range check | `spectrum_sm.c:654-657` and `spectrum_sm.c:664` against the 273 and 272 limits of the grammar |
| Beam always sent | `spectrum_enc.c:314-316` (APER), `361-362` (Protocol Buffers) and `404` (JSON) set it whatever the value |
| Empty mask rejected | `spectrum_dec.c:107-108` (ASN.1), `spectrum_dec.c:490-498` (JSON), `spectrum_dec.c:548` (protobuf) |
| First revision has no sequence identifier in the control handler | `oai-first` `spectrum_sm.c:92-100` has no `sequence_id` parameter |

## Tests

No test or golden vector for this service model was found in the E3AP library tree or in `oai/`. The
CI test script of the OpenAirInterface tree runs the service model against a radio simulator, and this
guide did not read its result. [Annex A](07-examples.md) gives one exchange generated from the grammar.
It was not run against an implementation. Treat the encodings as untested against a second
implementation.
