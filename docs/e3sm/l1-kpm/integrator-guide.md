# E3SM-L1KPM: integrator guide

A practical guide for an engineer who adds E3SM-L1KPM to a RAN stack or reads it in a
dApp. It is not part of the specification. Where it and the specification differ, the
specification wins. Code pointers are allowed here and only here.

The index of the specification is [README.md](../l1-kpm/README.md).

## Code roots

| Short name | What it is | Revision read |
|---|---|---|
| `oai/` | the OpenAirInterface implementation of E3AP and of this service model, https://github.com/duranta-project/openairinterface5g/pull/729 | commit `77e45217b9` of the checkout used for this text |
| `libe3/` | this repository: the E3AP library that carries the indications | the branch that introduced these files |

Line numbers drift. Function names are the stable reference. Paths under `oai/` start at
`openair2/E3AP/` unless a full path is given.

## Where the pieces are

| Piece | Location |
|---|---|
| Service model descriptor, worker hooks, indication building | `oai/openair2/E3AP/service_models/l1_kpm_sm/l1_kpm_sm.c` (`create_l1_kpm_sm_model`, `emit_batch`, `l1_wait_and_fetch`) |
| Public interface of the service model | `l1_kpm_sm/l1_kpm_sm.h` (`L1_KPM_TID_*`, `l1_kpm_sm_has_subscribers`, `l1_kpm_sm_set_period_us`) |
| Region writer, header, FP16 conversion, publish | `l1_kpm_sm/e3_ran_buffers.c` and `.h` (`e3_ran_buffers_push_rxdataF`, `e3_ran_buffers_init_from_phy`) |
| Payload codecs | `l1_kpm_sm/l1_kpm_enc.c` (`l1_kpm_enc_indication`, `l1_kpm_enc_ran_function_data`) |
| Grammar and Protocol Buffers file | `l1_kpm_sm/MESSAGES/ASN1/V1/e3sm_l1_kpm.asn`, `l1_kpm_sm/MESSAGES/PROTO/V1/e3sm_l1_kpm.proto` |
| Worker thread (wait, cadence, emit) | `service_models/e3_sm_worker.c` |
| Shared memory creation and the row cursor | `service_models/e3_shm_region.c`, `e3_shm_region.h` |
| Publish and wait primitive | `service_models/pub_channel.h` |
| Call from the PHY | `oai/openair1/SCHED_NR/phy_procedures_nr_gNB.c:1267-1272` |
| Cadence rule | `oai/openair2/E3AP/e3_agent.c:56-72` (`min_subscription_period_us`), applied in `on_dapp_status_changed` |
| Registration | `e3_agent.c:214-240` |
| Configuration of the scale factor | `config/e3_config.c` (`fp16_beta`), default `E3_FP16_BETA_DEFAULT` in `config/e3_config.h` |

## For a RAN

### Steps

1. Build a service model descriptor with RAN function ID `2`, the telemetry identifiers `1`, `4`, `5`, `6`, no control identifiers and no `sm_process_control` (`l1_kpm_sm.c`, `create_l1_kpm_sm_model`). libe3 answers a control action with a negative acknowledgment when the callback is not set.
2. Encode the RAN function data at registration, once (`l1_kpm_enc_ran_function_data`). If the encoder fails, do not register the service model. An empty `ranFunctionData` fails the APER encoding of the setup response.
3. Create the region lazily, at the first row. `e3_ran_buffers_init_from_phy` removes a stale object, opens a new one with mode `0644`, sets the size and maps it. It then writes the header (`e3_ran_buffers.c:196-270`).
4. Write the row. The loop in `e3_ran_buffers_push_rxdataF` (`e3_ran_buffers.c:287-392`) reads the `rxdataF` of the slot, shifts the carrier to start at subcarrier 0, converts to FP16 and zero fills above the PRBs of the cell. It takes the row from the cursor.
5. Compute the symbol mask (`e3_ran_buffers.c:352-363`). For TDD, bit `s` is set when the TDD table entry for the slot and symbol has value `1`. For FDD all 14 bits are set.
6. Take the time with `CLOCK_MONOTONIC`, then lock, store the snapshot, advance the cursor, publish and wake the worker (`e3_ran_buffers.c:365-391`). The snapshot is copied under the lock. The row itself is written before the lock.
7. Call the writer only for uplink and mixed slots and only when `l1_kpm_sm_has_subscribers` is true (`phy_procedures_nr_gNB.c:1267-1272`). It is true when the worker runs, that is from the first subscription to the last release.
8. In the worker, encode once and send to each subscriber with `e3_sm_worker_emit_to_dapp` (`l1_kpm_sm.c`, `emit_batch`). `emit_batch` gets the list of subscribers once for the batch.
9. Set the cadence from the subscriptions (`min_subscription_period_us`). `0` means one indication for each publish. In the periodic mode the worker keeps the latest snapshot and sends it when the period has passed (`e3_sm_worker.c`, `e3_sm_worker_thread`).

### Things that bite

* The payload buffer is 512 bytes. A new field can overflow it. The encoder then returns an error and the worker silences the warning after the first one.
* The scale factor `beta` is a RAN setting. Tell dApp authors the value. The header does not carry it.
* The region has no row marker. Keep the rows in order and do not skip a position.
* Change the layout only with a new header `version`.
* `e3_ran_buffers_destroy` is called from the destroy hook of the service model, not from the stop hook. The region stays after the last release.

## For a dApp

1. Subscribe with `ranFunctionIdentifier` `2` and an empty control list. Set `periodicity` if you want a slower rate. Remember the RAN sends the fastest rate of all dApps to everyone.
2. Decode the indication with the codec of your session. Take the reference from `iqSamplesRef`.
3. Open the region once with `shm_open(name, O_RDONLY, 0)` and `mmap` with `PROT_READ` and `MAP_SHARED`. Map the whole file. Check `version` in the header.
4. Compute the row and sample addresses with the formulas of [02-ie-mapping.md](02-ie-mapping.md). The row size is `num_fh_samples * 2` bytes, and the buffer size is in the header.
5. Copy the samples you need, convert from binary16, divide by the `beta` of the RAN if you need the FFT scale, then run the overwrite rules of [03-procedure-flow.md](03-procedure-flow.md).
6. Retry `shm_open` for a short time after the subscription. The RAN creates the object at the first uplink slot.
7. In a container, share `/dev/shm` with the RAN container.

## Evidence for the discrepancies

| # in [06](06-discrepancies.md) | Evidence |
|---|---|
| 1.1, 1.2 | `l1_kpm_enc.c`, ASN.1 branch of `l1_kpm_enc_indication`: `cellId`, `nRxAnt`, `validSymbolMask` always set. `e3_ran_buffers.c`: `.cell_id = 0` in the snapshot (line 381) |
| 1.5, 2.6 | `l1_kpm_enc.c`: `pdu.timestamp = (long)slot->timestamp_ns`. `.proto`: `int64 timestamp` |
| 1.6 | comment on `L1KPM-Indication` in the grammar file |
| 2.1 | `l1_kpm_enc.c:160-220`: the `else` branch of `l1_kpm_enc_ran_function_data` writes JSON for every encoding that is not ASN.1 |
| 2.4 | `l1_kpm_enc.c:207`: `snprintf` with `%s` |
| 2.5 | header comment of `e3sm_l1_kpm.proto` |
| 3.2 | `e3_ran_buffers_header_t` in `e3_ran_buffers.h`, `reserved[7]` |
| 3.3 | comment above `E3_RB_NUM_FH_ROWS` in `e3_ran_buffers.c` |
| 3.4 | `e3_ran_buffers.c:316-350`: the loop runs over `ants` only. The comment above `E3_RB_N_ANTS` says "zero-fills the rest" |
| 3.6 | `e3_ran_buffers.c:207-213` and `:300-306`: init is retried at each call |
| 4.1, 4.2 | `e3_ran_buffers_slot_info_t` fields written in `e3_ran_buffers_push_rxdataF` and read nowhere else |
