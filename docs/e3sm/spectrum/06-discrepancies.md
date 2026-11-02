# E3SM-SPEC: discrepancies, dead fields and open items

Where the grammar, the Protocol Buffers file and the codecs disagree. Fields that no code uses. Facts
that this specification could not verify. The text names no product. The code evidence is in
[the integrator guide](integrator-guide.md).

This is Annex A of the specification. It is informative.

## Two copies of the grammar

The grammar exists in more than one copy. The copies are meant to be identical. They are not.

Table: Differences between the grammar copies

| Item | Copy used by the RAN, first revision | Copy used by the xApp side and the later RAN revision |
|---|---|---|
| `Spectrum-PRBBlockApplyOutcome`, `Spectrum-ApplyOutcomePayload`, `Spectrum-ApplyOutcomeData` | absent | present |
| constant `maxSlotIndex` | absent | 159 |
| comment about the copies | none | a comment says that all copies are byte identical |

A RAN of the first revision cannot send an apply outcome. An xApp that waits for it gets the answer
after the timeout, with no timestamps. This specification describes the later copy.

## The sensing indication has three spellings

The same fields have different names in the three encodings.

Table: Names of the reference to the ring

| Meaning | ASN.1 | Protocol Buffers | JSON |
|---|---|---|---|
| ring name | `shmName` | `shm_name` | `sensing_shm.shm_name` |
| ring slot index | `shmWriteIdx` | `shm_write_idx` | `sensing_shm.write_idx` |
| number of ranges | `nRanges` | `n_ranges` | `sensing_shm.n_ranges` |

The JSON form also nests the three values in an object, while `timestamp`, `sfn`, `slot` and `beam` stay
at the top level. A JSON consumer written from the grammar does not find its keys. A consumer that
reads JSON needs the nested names. The two other encodings are flat.

## Fields that the RAN decodes and does not use

Table: Fields without an effect

| Field | What the RAN does |
|---|---|
| `Spectrum-PRBBlacklistControl.samplingThreshold` | decodes it and ignores it |
| `Spectrum-PRBBlacklistControl.validityPeriod` | decodes it and ignores it. A block does not expire. |
| `Spectrum-SensingPolicyControl.validityPeriod` | decodes it and writes it to a log line. A policy does not expire. |
| telemetry identifiers 2 to 5 | advertises them. The indication does not depend on them. |

A dApp that relies on a block to expire after `validityPeriod` seconds is wrong. It shall clear the
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
| `samplingThreshold` 0..100 | checked | not checked | not checked |
| `validityPeriod` 1..3600 | checked | not checked | not checked |

A value of 500 for `samplingThreshold` is refused in APER and accepted in JSON and Protocol Buffers.
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
| The JSON codecs of the dApp report and of the xApp control. | Required by this specification and missing. See [encoding](04-encoding.md#json). |
| The behavior of an unknown extension alternative in an APER choice. The RAN rejects a control that holds an alternative other than the one for its control identifier. What the decoder reports for an unknown extension alternative was not tested. | Not tested. |
| An end to end test of the sensing indication, the ring and the apply outcome against a second implementation. | None found. No golden vectors exist for this service model. |
