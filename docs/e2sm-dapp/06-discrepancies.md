# E2SM-DAPP: implementation notes and known discrepancies

This note is informative. It lists the places where an implementation and the grammar
disagree, where an implementation is incomplete, and where code is dead. Each entry says
what it means for someone who implements or consumes E2SM-DAPP. Nothing here is a design
decision. The design decisions are in the other clauses.

The index is [README.md](../e2sm-dapp/README.md). The
implementations are named by letter:

* **Implementation A** is the original C implementation of the service model, in an
  E2 agent and a RIC library.
* **Implementation B** is the C++ and C codec that ships with this repository.
* **Implementation C** is a C++ RAN stack with a hand-ported codec.
* **The OpenAirInterface implementation** is the RAN bridge described in reference
  [i.2] of the README.
* **An xApp framework** is a Python framework that wraps implementation A.

The evidence for each entry (file names, line numbers and code roots) is in the
[integrator guide](integrator-guide.md).

Severity: **abort** means an assertion or equivalent takes the process down in a build
with assertions on. **wrong** means a wrong result with no error. **latent** means only
a corner case triggers it. **doc** means documentation only.

## 1. Dead code

Table: Dead code

| # | What | Notes |
|---|---|---|
| 1.1 | The plain and FlatBuffers encoders and decoders of implementation A | All 31 functions fail with "not implemented". The build allows only the ASN.1 encoding. A build branch for the plain encoding would still build the stubs. A branch for FlatBuffers stops the build. The generic dispatch macros still point to them. **doc** |
| 1.2 | Unused includes of the headers of other service models in the codec of implementation A | Nothing from them is used. Building the codec alone requires deleting them. |
| 1.3 | `Bool-Type ::= BOOLEAN` | Defined and referenced nowhere. The ASN.1 compiler still generates code for it. |
| 1.4 | A structure with a pointer to a control outcome in implementation A | Used nowhere. |
| 1.5 | A RIC Service Update structure in the read union of implementation A | Nothing fills or reads it for E2SM-DAPP. The RIC Service Update goes through the generic path. |
| 1.6 | The sweep of pending controls in implementation A | **Nothing calls it**, in the library or in the OpenAirInterface implementation. The 200 ms safety net for an unanswered deferred control never runs. The only timeout in effect is the 150 ms timeout of the OpenAirInterface implementation. **latent** |
| 1.7 | The function that sends a RIC Control Failure in implementation A | Nothing calls it. A control for E2SM-DAPP always ends in a RIC Control Acknowledge. An xApp framework registers failure handlers that a RAN of implementation A never triggers. |
| 1.8 | The handlers of the RIC Service Update in the agent and in the RIC of implementation A | They are stubs. The agent comment says that the RIC Service Update is not used for this service model, yet the RAN sends one. The stub is the hook for *receiving* one. Sending does not go through it. **doc** |
| 1.9 | Commented-out code in an xApp framework | Dead branches left in place. |

## 2. Where the grammar and the implementations disagree

Table: Disagreements between the grammar and the implementations

| # | What | Effect |
|---|---|---|
| 2.1 | **`data-size` duplicates the octet string length** in `IndicationMessage-Format1` and `ControlMessage-Format1`. Implementation A writes its own `data_size` into both the ASN.1 field and the octet string length. On decode it takes the size from the octet string and ignores the ASN.1 field. Implementations B and C keep the ASN.1 field as its own value on encode and decode. They check neither against the octet count. | A sender that writes different values is read as the octet length by implementation A and as the field by B and C. **wrong**. Always send equal values. |
| 2.2 | The grammar allows **0..256** dApps and **1..64** functions per dApp. Implementation A accepts fewer. Its decoder, its encoder for the report and control item lists, and its list helpers assert on 256 dApps and on 64 functions. The generator of the reference test vectors documents this. | A list of 256 dApps, a list of 0 items in a definition, or a dApp with 64 functions **aborts** implementation A. The indication message format 2 path tolerates an empty list. It does not tolerate 256 items on decode. **abort** |
| 2.3 | The definition item lists. Implementation A checks `0 < n < 64`. The grammar allows 1 or 2 report styles and 1 control style. | The implementation accepts what the grammar forbids and leaves the rejection to the ASN.1 runtime. |
| 2.4 | `ranFunction-Instance` is in the grammar and unusable in implementation A. Its encoder asserts that the instance is absent. Its decoder never reads it. | A definition with an instance is lost on decode and cannot be encoded. B and C support it. |
| 2.5 | `RIC-Style-Type` and `RIC-Format-Type` are unbounded `INTEGER`. Implementation A stores 32 bits. | A style above `4294967295` or a negative style cannot be represented. B and C hold 64 bits. **latent** |
| 2.6 | `Timestamp` and `SequenceId` are unbounded `INTEGER`. Every implementation holds 64 bits. | A value that needs more than 8 octets cannot be decoded. C rejects it. B checks that it fits. |
| 2.7 | `node-plmn-id` is `OCTET STRING (SIZE(3))`, not the 3GPP `PLMN-Identity`. The OpenAirInterface implementation writes the digits as three shifted values. This drops a digit of a three-digit MNC and is not BCD. | A consumer cannot read the PLMN without knowing the packing of the RAN. The grammar does not define a packing. |
| 2.8 | The dApp E3 subscription list may be empty (`SIZE(0..256)`). A **subscription item** needs at least one function. The report and control item path of implementation A needs at least one item. | The OpenAirInterface implementation omits the field when no dApp has a function. An empty list in indication message format 2 is legal and works. |
| 2.9 | E2SM-DAPP ids are `0..4294967295`. E3AP ids are `1..65535`. | Values that E3 cannot carry are legal in E2SM-DAPP. The reference test vectors include them. |
| 2.10 | `SequenceId` is unbounded. `E3-SequenceID` is `1..4294967295`. The control path has no range check. Implementation A converts a signed value to an unsigned key. The OpenAirInterface implementation truncates it to 32 bits. | An id above `2^32 - 1` or a negative id makes the parked key and the completion key differ. The relay never completes the control. The 150 ms timeout of the OpenAirInterface implementation retires it. **latent** |
| 2.11 | The OID text. The grammar and the string use `e2sm(255)`. One comment uses `e2sm(2)`. | Stale comment. **doc** |
| 2.12 | The grammar file in implementation A has a `v0` in its name and its module is version 1. | The `v0` does not mean a version 0 grammar. The grammar files of implementations A and B are byte-identical. **doc** |
| 2.13 | The control header carries no style number. | This works while there is one control style. A second control style needs a new header format. |
| 2.14 | The control outcome carries no `ran-function-id`. | The xApp shall remember the E3 RAN function per `sequence-id` to decode the payload. See [02-ie-mapping.md](02-ie-mapping.md#73-e2sm-dapp-controloutcome). |
| 2.15 | An absent `timestamp` or `sequence-id` is not an error in the grammar. In implementation A, `0` means absent. | Implementation A cannot send a present `0`. See [05-semantics-and-constraints.md](05-semantics-and-constraints.md#4-optional-fields-absent-is-not-an-error). |

## 3. Aborts and error handling in implementation A

Table: Aborts and error handling in implementation A

| # | What |
|---|---|
| 3.1 | Every decoder asserts a successful decode. A malformed or truncated PDU aborts the process. |
| 3.2 | The encoders of the event trigger, action definition, control header, control message, control outcome and definition assert instead of returning an error. Only the encoders of the indication header and message return an empty buffer. |
| 3.3 | A control request without "ack requested" aborts the E2 agent. |
| 3.4 | A subscription with more or fewer than one action, or with an insert or policy action, aborts the agent. |
| 3.5 | An unknown report style. The OpenAirInterface implementation returns an empty answer, then an assertion of the agent aborts. |
| 3.6 | A deferred control with key `0` aborts the agent. The service model defers on whatever `sequence-id` the header holds, and the table asserts that it is not `0`. The OpenAirInterface implementation avoids this by answering at once when the id is `0`. |
| 3.7 | The inner decode on the RIC side asserts. A format 1 indication whose `ran-function-id` has no decoder aborts the xApp. Only id `1` (Spectrum) is known. An unknown indication format aborts too. |
| 3.8 | The control path on the RIC side aborts when it cannot encode the inner control payload. It returns an **empty** request without error when the E3 type is "none" or the message already carries data. |
| 3.9 | The RIC Service Update handler on the RIC side does not decode the new definition. |
| 3.10 | The first RIC Service Update repeats the revision of the E2 Setup. The trigger also finds the service model by the literal `255`. **wrong** |
| 3.11 | A comment says that unanswered controls are acknowledged "negatively". The sweep sends a RIC Control Acknowledge with no outcome. **doc** |
| 3.12 | Two parked controls with the same key. A completion releases one. The other waits for a sweep that nothing runs (see 1.6). |
| 3.13 | A fixed encode buffer means that large elements abort. The sizes are 512 KiB for the event trigger, 1 KiB for the action definition, 1 KiB for the indication header, 128 KiB for the indication message, 16 KiB for the control header, control message and control outcome, and 16 KiB for the definition. A control message above about 16 KiB aborts although the grammar allows 32768. The reference test vector for a large control message stops at 16000 octets. A definition with the map for 256 dApps of 64 functions each (about 83 KiB per copy, listed twice) does not fit either. |
| 3.14 | The clock helper returns `-1` on error and the OpenAirInterface implementation uses it unchecked. |
| 3.15 | The OpenAirInterface implementation does not clamp the dApp list to the grammar. More than 256 dApps, or a dApp with more than 64 functions, would reach the encoder of implementation A. |
| 3.16 | The equality helper of the event trigger compares the format of the first argument only. **latent** (there is one format) |

## 4. Consumers

Table: Findings about xApp consumers

| # | What | Effect |
|---|---|---|
| 4.1 | The wrapper of the control outcome in an xApp framework has an open "to do" and **no free binding** for the outcome. The likely reason is that the library hides symbols by default and the free function for the outcome is not exported, unlike the free functions of the indication header, indication message and definition. | The `e3-control-outcome` buffer leaks at every acknowledge. **latent** |
| 4.2 | An xApp framework mirrors the C structures of implementation A field by field. | The layouts of the library are an **ABI**. A changed field order or type reads the wrong numbers without an error. The decoders return structures by value. |
| 4.3 | The mirror of the event trigger definition has two fields. The structure of implementation A is empty. | Reading a field of a decoded definition that has the event trigger section reads outside the allocation. The OpenAirInterface implementation omits the section, so it does not trigger today. **latent** |
| 4.4 | An xApp framework finds the control style by the name `DAPP-CONTROL-STYLE-1`. | A RAN that names it differently is refused. The name is free text in the grammar. |
| 4.5 | An xApp framework sends a call process id of `1` in every control request. | Implementation A ignores it. A conforming sender omits it. |
| 4.6 | An xApp framework hard-codes RAN function id `255`. | |
| 4.7 | A decision engine caches the decoded definition per gNB for its lifetime. | The cache is stale after a RIC Service Update. The engine takes the `dapp-id` from reports, so this does not matter today. |

## 5. Differences between the implementations

The following behavior differs between implementations A, B and C for the same input.
The reference test vectors do not detect these differences, because every vector is a
valid message and none sets a present `0` or disagreeing sizes.

Table: Behavior that differs between implementations for the same input

| # | Input | Implementation A | Implementation B | Implementation C |
|---|---|---|---|---|
| 5.1 | present `timestamp` or `sequence-id` equal to `0` | encoded as absent | encoded as present `0` | encoded as present `0` |
| 5.2 | `data-size` different from the octet count | encoded as the stored value, decoded as the octet length | encoded and decoded as the field | encoded and decoded as the field |
| 5.3 | extension additions on a `SEQUENCE` | skipped by the ASN.1 runtime | skipped by the ASN.1 runtime | skipped as opaque open types |
| 5.4 | extension bit on a structure to be packed | not applicable | the member is never read when packing. The bit is always `0`. | a set member makes `pack` fail |
| 5.5 | a `PrintableString` with a character outside the alphabet | encoded, no check | rejected on encode and decode | rejected on encode, accepted on decode |
| 5.6 | an `INTEGER` of exactly `2^(8k-1)`, for example a `SequenceId` of 128 | correct (two octets `00 80`) | correct | handled by a local routine. The generic helpers of its runtime size it wrongly. |
| 5.7 | an empty subscription list in a definition | **abort** | allowed | allowed |
| 5.8 | a dApp item with 0 functions | the ASN.1 encoder rejects it | rejected on encode and decode | rejected on encode and decode |
| 5.9 | the unbounded octet string above 16383 octets | not reachable (16 KiB buffer) | handed to the ASN.1 runtime, untested | rejected on encode |
| 5.10 | malformed input | **abort** | decoding failure code | decoding failure code |
| 5.11 | the format number | enumeration from `0` | `to_number()` from `1` | `to_number()` from `1` |

### 5.1 Implementation C's E2 module against implementation A and the OpenAirInterface RAN

Table: Behavior of the E2 module of implementation C

| # | Item | Implementation C | Implementation A and the OpenAirInterface implementation |
|---|---|---|---|
| 5.12 | RAN function revision in the E2 Setup Request | `0`, although its service model class holds `1` | `1` |
| 5.13 | dApp E3 subscription map in the definition | never present ("dynamic, so not part of the definition") | present in report style 1 and control style 1 when any dApp has a function |
| 5.14 | RIC Service Update for E2SM-DAPP | never sent | sent when the dApp set changes |
| 5.15 | Style names | `E3 Data Report`, `E3 Subscription Map`, `dApp Control` | `DAPP-E3-DATA-REPORT`, `DAPP-E3-SUBSCRIPTION-MAP`, `DAPP-CONTROL-STYLE-1` (the name an xApp framework requires) |
| 5.16 | Event trigger section in the definition | present | absent in the OpenAirInterface implementation |
| 5.17 | `timestamp` of the control outcome | echoes the **control header's** timestamp | the time the outcome was built, on the gNB clock |
| 5.18 | Outcome on failure | RIC Control Failure with the cause of the sink, outcome included | RIC Control Acknowledge only. No failure path exists. |
| 5.19 | Control without "ack requested" | applied, not answered | the agent of implementation A aborts |
| 5.20 | `data-size` not equal to the octet count | an indication is dropped. A control is refused with `ctrl_msg_invalid`. | not checked. The ASN.1 field is ignored on decode. |
| 5.21 | Reporting rate | one indication per subscription per 10 ms poll | event driven, one indication per report |
| 5.22 | Size caps | indication header plus message at most 16319 octets. `e3-control-outcome` at most 16287 octets. The E2AP codec cannot carry 16384 octets or more. | the fixed buffers of implementation A (section 3) |
| 5.23 | Where it is enabled | the DU-high only, by a configuration flag | not applicable |
| 5.24 | An unknown report style | not admitted in the RIC Subscription Response | aborts (3.5) |

### 5.2 Runtime helpers of implementation C that the port works around

These are limits of the shared ASN.1 helpers of implementation C. The authors found them
while they hand-ported the grammar. They affect any other generated E2 service model
there. They are listed for whoever maintains that runtime. The E2SM-DAPP codec does not
depend on them.

Table: Limits of the ASN.1 helpers of implementation C

| # | Helper | Problem | What the E2SM-DAPP codec does |
|---|---|---|---|
| 5.25 | the packer for an unconstrained integer | It sizes the values `2^(8k-1)` wrongly. `128` goes out as the single octet `0x80`, which a conforming peer reads as `-128`. Decode zero-extends, so negative values come back wrong. | A local two's complement encoder and decoder. `128` encodes as `02 00 80`. |
| 5.26 | the packer for an 8-bit integer with upper bound 255 | It treats the field as unbounded and writes nothing. | `node-type` is held in a 16-bit integer while packing. |
| 5.27 | the integer unpacker, extension path | It can reach an assertion on a long length read from the wire. | Out-of-root values are rejected before that path. |
| 5.28 | the string unpacker | It ignores the result of its length decode, so an out-of-range length is accepted. There is no alphabet check. | A wrapper checks the size on decode. It checks the size and the PrintableString alphabet on encode. |
| 5.29 | the unbounded octet string, length packer and unpacker | They cannot produce or parse OCTET STRINGs of 16384 octets or more. | The codec refuses to emit such payloads (see 5.22). |

## 6. What the reference test vectors prove and what they do not

* They prove that implementations B and C **encode** every listed input to the exact
  bytes of implementation A and **decode** those bytes back (see the
  [integrator guide](integrator-guide.md#the-contract-the-golden-vectors)).
* Implementation A produces them, so they inherit its limits. There is no present
  `0`, no `ranFunction-Instance`, no control message above 16000 octets, no
  `e3-control-outcome` longer than 10 octets and no extension bits.
* They say nothing about invalid input. Section 5 above lists the places where the
  three codecs differ on it.
* The expected behavior on extension bits, trailing octets and outcome payloads above
  127 octets (two-octet length form) comes from ITU-T X.691 and the ASN.1 runtime. It
  does not come from a vector.

## 7. Open items

Table: Open items

| # | Item | Owner | Status |
|---|---|---|---|
| 7.1 | The real E3 link and the Spectrum codec are **out of scope** of the codec work of implementations B and C. The E2SM-DAPP codec does not touch E3. | | not scheduled |
| 7.2 | Decide the `data-size` rule: equal and enforced, or ignored | the specification owners | open. Recommendation: send equal values. |
| 7.3 | Define the `node-plmn-id` packing and the `node-type` values | the specification owners | open |
| 7.4 | Fix the limits of implementation A (2.2, 3.13) or document them as the limits of that implementation | maintainers of implementation A | open |
| 7.5 | Wire the sweep of pending controls into a RAN, or remove it | maintainers of implementation A and of RAN stacks | open |
| 7.6 | Add a free binding for the control outcome to the xApp framework | maintainers of the xApp framework | open |
| 7.7 | Extend the reference test vectors with cases that implementation A cannot produce (present zero, instance, long payload outcome), from a second source | | not scheduled |
| 7.8 | The E3 side of implementation C: the indication source and the control sink that carry the dApp traffic. Only the no-op versions exist. | maintainers of implementation C | not in the module |
