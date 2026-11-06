# E3SM-SPEC: discrepancies, dead fields and open items

Where the grammar, the Protocol Buffers file and the codecs disagree. Fields that no code uses. Facts
that this specification could not verify. The text names no product. The code evidence is in
[the integrator guide](integrator-guide.md).

This note is informative.

## Revisions of the grammar

The grammar exists in more than one copy and in more than one revision. The present document
describes the latest grammar without the sensing reservation, the PUSCH slots and the sampling
threshold.

Table: Revisions of the grammar

| Item | First RAN revision | Later revision, used by the RAN, the dApp and the xApp side | Present document |
|---|---|---|---|
| Apply outcome (`Spectrum-PRBBlockApplyOutcome`, `Spectrum-ApplyOutcomePayload`, `Spectrum-ApplyOutcomeData`) | absent | present | present |
| constant `maxSlotIndex` | absent | 159 | 159 |
| content of `protocolData` | `Spectrum-SensingIndication` | `Spectrum-Indication`, with the cell configuration indication | `Spectrum-Indication` |
| alternatives of the dApp control | the PRB block in the root, the sensing policy as an extension | as the first revision, plus the sensing reservation and the PUSCH slots as extensions | as the first revision |
| alternatives of the xApp control | `prbBlockedControl` and `configControl`, both in the root | the same | the same |
| optional fields of the PRB block control | `samplingThreshold` and `validityPeriod` | the same | `validityPeriod` only |
| unit and range of `validityPeriod`, in the PRB block and in the sensing policy | seconds, 1..3600 | the same | milliseconds, 1..3600000 |

A RAN of the first revision cannot send an apply outcome. An xApp that waits for it gets the answer
after the timeout, with no timestamps.

The effect on the wire was checked by encoding the same values with the grammar of the later
revision and with the grammar of the present document.

* A dApp report, a sensing indication, a cell configuration indication, an apply outcome and an xApp
  control of either alternative have the same octets in both grammars.
* A PRB block control with neither optional field has the same octets in both grammars. A control
  with one of them does not. The present grammar has one presence bit where the later revision has
  two, and its `validityPeriod` has a length field. The table after this list gives examples.
* In Protocol Buffers and JSON, `validity_period` and `validityPeriod` keep their field number and
  key, and the unit changes. A value of 60 from a sender of the later revision means one minute. A
  receiver of the present document reads it as 60 ms.
* In Protocol Buffers, a receiver of the present document skips field 2, `sampling_threshold`, as an
  unknown field. In JSON, it ignores the key `samplingThreshold` (clause 8).
* A sensing policy control without `validityPeriod` has the same octets in both grammars. It is the
  first extension alternative in both. With a `validityPeriod`, the field is wider in the present
  grammar. The open type keeps the error inside the alternative: the later revision reads 60000 ms as
  16619, and the present grammar reads 60 s as 60 ms.
* A sensing reservation or PUSCH slots control of the later revision is an extension alternative that
  the present grammar does not know. A receiver rejects it (clause 9).
* The indication of the first revision is a bare `Spectrum-SensingIndication`. A receiver of the
  later revisions decodes `protocolData` as `Spectrum-Indication`, so the two do not interwork.

Table: PRB block controls between the later revision and the present grammar, in APER

| Sent with the grammar of | Optional fields sent | Read by the other grammar as |
|---|---|---|
| the later revision | `samplingThreshold` 5, `validityPeriod` 60 | `validityPeriod` 1 |
| the later revision | `validityPeriod` 60 | no `validityPeriod` |
| the later revision | `samplingThreshold` 5 | a decoding error |
| the present document | `validityPeriod` 60000 | `samplingThreshold` 32, no `validityPeriod` |

Only the third case reports an error. In the others, the receiver accepts wrong values.

## The indications have three spellings

The same fields have different names in the three encodings.

Table: Names of the reference to the ring

| Meaning | ASN.1 | Protocol Buffers | JSON |
|---|---|---|---|
| ring name | `shmName` | `shm_name` | `sensing_shm.shm_name` |
| ring slot index | `shmWriteIdx` | `shm_write_idx` | `sensing_shm.write_idx` |
| number of ranges | `nRanges` | `n_ranges` | `sensing_shm.n_ranges` |

The JSON form also nests the three values in an object, while `timestamp`, `sfn`, `slot` and `beam` stay
at the level of the sensing indication. A JSON consumer written from the grammar does not find its
keys. A consumer that reads JSON needs the nested names. The two other encodings are flat.

The JSON cell configuration indication writes every field in lower case with underscores, for example
`ul_carrier_prbs` for `ulCarrierPRBs` (clause 8). It has no Protocol Buffers form.

A JSON indication also holds the member `indicationType`, which is not in the grammar. A Protocol
Buffers indication is a sensing indication with no wrapper, so nothing in it tells the two
indications apart.

## Fields that the RAN decodes and does not use

Table: Fields without an effect

| Field | What the RAN does |
|---|---|
| `Spectrum-PRBBlacklistControl.validityPeriod` | decodes it and ignores it. A block does not expire. |
| `Spectrum-SensingPolicyControl.validityPeriod` | decodes it and writes it to a log line. A policy does not expire. |
| telemetry identifiers 2 to 5 | advertises them. The indication does not depend on them. |

A dApp that relies on a block to expire after `validityPeriod` milliseconds is wrong. It shall clear the
block itself.

## The three decoders have different strictness

The range of a field is part of the grammar. Only the APER decoder enforces all of it.

Table: Range checks by encoding

| Field | APER | JSON | Protocol Buffers |
|---|---|---|---|
| PRB index 0..272 | checked | checked | checked |
| PRB list size at most 273 | checked | checked | checked |
| symbol mask 0..16383 | checked | checked | checked |
| mask list size 1..160 | checked | checked | checked |
| `validityPeriod` 1..3600000 | checked | not checked | not checked |

A value of 5000000 for `validityPeriod` is refused in APER and accepted in JSON and Protocol Buffers.
Because the RAN ignores the field, the effect is none today. The difference matters when the RAN starts
to use it.

## The sensing policy needs a mask to be cleared

The grammar sets the size of `maskPerSlot` to 1..160. The `deactivate` flag clears the policy and
ignores the masks. A control that only wants to clear the policy shall still send at least one mask. An
empty list is rejected in all three encodings. This is legal, and it is an unusual rule. A dApp
usually sends the list that it used before.

## Fields that are wider than their values

* `sfn` is `0..65535` in the sensing indication. A frame number has 10 bits. The apply outcome uses
  `0..1023`.
* `slot` is `0..65535` in the sensing indication. A slot number is at most 159. The apply outcome uses
  `0..159`.
* `shmWriteIdx` is `0..65535`. The ring has 256 slots.
* `beam` is `0..3` and optional. The RAN always sends it. It sends the beam index of the publish. The
  grammar does not say what a RAN does with an index above 3. An APER encoder is expected to refuse
  it. No test of this case was found.
* `shmName` has no size limit in this service model. The shared memory reference of the L1-KPM
  service model limits its name to 1..63 octets.

Wider fields cost bits in APER. They also hide errors. They are not wrong.

## Fields and alternatives without a codec

* `Spectrum-ConfigControl` (`noiseFloorThreshold`, `averagingFrames`, `enable`) is in the grammar and in
  the Protocol Buffers file. This specification found no encoder, no decoder and no handler for it on
  the RAN, the dApp or the xApp side. The unit of `noiseFloorThreshold` is not defined anywhere.
* The cell configuration indication has an APER and a JSON encoder on the RAN side and a JSON decoder
  on the dApp side. It has no Protocol Buffers form.
* `Spectrum-PRBBlockedControl` has an xApp encoder and a dApp decoder. No other xApp control alternative
  has one.
* The apply outcome has an encoder on the RAN side and a decoder on the xApp side, in APER only. It has
  no JSON and no Protocol Buffers form.
* The dApp report has an xApp decoder in APER only. No JSON decoder or encoder for the report was found.

## The xApp leg has no JSON codec

The xApp side encodes the xApp control and decodes the dApp report with APER only. The RAN decodes the
dApp control with the encoding of the run. The result for a run with JSON or Protocol Buffers:

* The dApp control follows the run. This is verified.
* The xApp control and the dApp report stay APER. A dApp that runs with JSON then receives binary
  octets in a field that carries nested JSON in the E3AP JSON encoding.

The E3AP specification says that the payload fields hold JSON values in the JSON encoding, so the xApp
control cannot be carried in a JSON E3 message as binary octets. This specification requires a JSON form
of the dApp report and of the xApp control (see [encoding](04-encoding.md#json)). The codecs are to be
added. Until they exist, a deployment with an xApp shall use APER for the whole run.

## Dead code

* The RAN clamps the number of listed PRBs to the size of the largest bandwidth part, 275. The grammar
  limits the list to 273 entries. The clamp cannot trigger.
* The RAN ignores a PRB index of 275 or more when it builds the block mask. The grammar limits the index
  to 272. The check cannot trigger.

## Revisions that do not match

* The apply outcome needs the sequence identifier of E3AP in the control. A RAN of the first revision
  does not pass the identifier to the service model, so it cannot report an apply outcome for the
  procedure.
* The pending procedure table and the timeout are properties of the later revision only.

## Open items

Table: Open items

| Item | State |
|---|---|
| What a version bump means. The field `version` is "1" and the ring header has `version` "1". No rule says how a dApp reacts to another value. | Not defined. |
| What a dApp does with a ring when the RAN stops and starts the service model. The RAN removes the region and creates a new one. A mapping that the dApp holds still points to the old region. | Not defined. A dApp should unmap and map again on every new subscription. This is not verified. |
| The wire values of the telemetry identifiers. A comment says that they follow a convention of the L1-KPM service model, in which the identifier is a bit position plus one. | The convention is not a rule of this specification. No code uses a bit mask. The numbers are tracked on the [interoperability page](../../interop.md). |
| Unit and range of `noiseFloorThreshold`. | Not defined. |
| `Spectrum-Indication` has no extension marker. A third kind of indication would change the width of its alternative index. | Open. An extension marker added now would change the encoding of every indication once. |
| The JSON codecs of the dApp report and of the xApp control. | Required by this specification and missing. See [encoding](04-encoding.md#json). |
| The behavior of an unknown extension alternative in an APER choice. The RAN rejects a control that holds an alternative other than the one for its control identifier. What the decoder reports for an unknown extension alternative was not tested. | Not tested. |
| An end to end test of the sensing indication, the ring and the apply outcome against a second implementation. | None found. No golden vectors exist for this service model. |
