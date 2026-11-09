# E3SM-L1-KPM: integrator guide

A practical guide for an engineer who adds E3SM-L1-KPM to a RAN stack or reads it in a
dApp. It is not part of the specification. Where it and the specification differ, the
specification wins. Code pointers are allowed here and only here.

The index of the specification is [README.md](../l1-kpm/README.md).

The implementation described here predates the present draft of the specification.
[Annex C](annex-c-open-items.md) lists where it differs from the draft, and the last two
clauses of this guide give the evidence in the code.

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

1. Build a service model descriptor with RAN function ID `2`, the telemetry identifiers `1`, `4`, `5`, `6`, no control identifiers and no `sm_process_control` (`l1_kpm_sm.c`, `create_l1_kpm_sm_model`). libe3 refuses a control for it with a negative CONTROL RESPONSE, because the RAN function lists no control identifier.
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

1. Subscribe with `ranFunctionIdentifier` `2` and an empty control list. Set `periodicity` if you want a slower rate. Remember the RAN sends the fastest rate of all dApps to everyone (Annex C, item C.17).
2. Decode the indication with the codec of your session. It has the flat form of Annex C, item C.18, and the RAN function data the form of item C.27. Take the reference from `iqSamplesRef`.
3. Open the region once with `shm_open(name, O_RDONLY, 0)` and `mmap` with `PROT_READ` and `MAP_SHARED`. Map the whole file. Check `version` in the header. The region of this implementation has version `1` and a header of 64 octets (Annex C, item C.25).
4. Compute the row and sample addresses with the formulas of [Annex A.5](annex-a-shared-memory.md#location-of-the-data) and [Annex A.6.1](annex-a-shared-memory.md#iq-samples), with the header of 64 octets in place of the header of Annex A.3. The row size is `num_fh_samples * 2` bytes, and the buffer size is in the header.
5. Copy the samples you need, convert from binary16, divide by the `beta` of the RAN if you need the FFT scale (Annex C, item C.13), then run the overwrite rules of [Annex A.4](annex-a-shared-memory.md#rows-buffers-and-reuse). One slot fills one row here, so the finer check of Annex A.4 applies.
6. Retry `shm_open` for a short time after the subscription. The RAN creates the object at the first uplink slot.
7. In a container, share `/dev/shm` with the RAN container.

## Evidence for the items of Annex C

| Item of [Annex C](annex-c-open-items.md) | Evidence |
|---|---|
| C.19, C.20 | `l1_kpm_enc.c`, ASN.1 branch of `l1_kpm_enc_indication`: `cellId`, `nRxAnt`, `validSymbolMask` always set. `e3_ran_buffers.c`: `.cell_id = 0` in the snapshot (line 381) |
| C.22 | `l1_kpm_enc.c`: `pdu.timestamp = (long)slot->timestamp_ns`. `.proto`: `int64 timestamp` |
| C.3 | comment on `L1KPM-Indication` in the grammar file: a subscription grants only the four core identifiers, so `cellId` and `nRxAnt` are optional |
| C.4 | `l1_kpm_enc.c:160-220`: the `else` branch of `l1_kpm_enc_ran_function_data` writes JSON for every encoding that is not ASN.1 |
| C.23 | `l1_kpm_enc.c:207`: `snprintf` with `%s` |
| C.13 | `e3_ran_buffers_header_t` in `e3_ran_buffers.h`, `reserved[7]` |
| C.26 | `e3_ran_buffers.c:316-350`: the loop runs over `ants` only. The comment above `E3_RB_N_ANTS` says "zero-fills the rest" |
| C.16 | `e3_ran_buffers.c:207-213` and `:300-306`: init is retried at each call |

## Code notes

Findings in the code that do not change what the specification says. Severity as in
Annex C.

| # | Note | Evidence |
|---|---|---|
| N.1 | The Protocol Buffers file says that it mirrors a grammar file with a different name from the file that exists. A reader looks for a file that is not there. Severity: doc. | header comment of `e3sm_l1_kpm.proto` |
| N.2 | A comment says that the depth of the buffers gives about 64 ms before the RAN reuses a row. The code has 32 rows, and the time depends on the rate of uplink slots. The 64 ms could not be reproduced from the code. Severity: doc. | comment above `E3_RB_NUM_FH_ROWS` in `e3_ran_buffers.c` |
| N.3 | The producer keeps indices and sizes for the PUSCH and channel estimate areas (`pusch_buffer_index`, `hest_buffer_index`, `hest_data_size`). They are always `0` and are not encoded. | `e3_ran_buffers_slot_info_t` fields written in `e3_ran_buffers_push_rxdataF` and read nowhere else |
| N.4 | The `valid` flag of the snapshot is set and never read. | as N.3 |
| N.5 | The header fields `num_pusch_rows`, `num_hest_rows` and `max_hest_samples_per_row` are constants. No code reads them. | not recorded |
| N.6 | A comment says that the header layout matches an external one byte for byte. It could not be tied to code. | a comment in the region code |
| N.7 | The symbol mask uses the slot number as the index into the TDD table. Whether that is right for a TDD period shorter than a frame could not be checked. | `e3_ran_buffers.c:352-363` |
| N.8 | The cadence of an absent `periodicity`: the RAN reads it as `0` through a helper that was not opened. | `min_subscription_period_us` in `e3_agent.c` |
| N.9 | A failed encoding or a failed send records one warning, and the later ones are silenced. A dApp that a send misses is counted as skipped for that indication. | not recorded |
