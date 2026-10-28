# E2SM-DAPP: encoding

How every E2SM-DAPP information element is turned into bytes, with worked
examples you can check against the golden vectors, the size limits, and the build
switches that select the codec in each implementation.

This is section 4 of the specification. The index is [README.md](README.md). The
field lists are in [02-ie-mapping.md](02-ie-mapping.md), the validation rules in
[05-semantics-and-constraints.md](05-semantics-and-constraints.md).

Code references use these roots, all at the commits listed in the
[README](README.md#where-the-code-references-point): `flexric/` is
flexric with the `dapp_sm` service model, `libe3/` is this repository, `ocudu/` is
OCUDU with the E2SM-DAPP port, `oai/` is OpenAirInterface, `xdevsm/` is `xDevSM-dApp`.

## 1. What is on the wire

Each E2SM-DAPP information element is one ASN.1 value, encoded with **APER
(ASN.1 Packed Encoding Rules, aligned variant, X.691)**, and carried as the
content of an `OCTET STRING` in the E2AP message. E2AP does the framing. E2SM-DAPP
adds no header, no length prefix, no checksum and no version byte of its own.

| E2AP field that carries it | ASN.1 type that is encoded into it |
|---|---|
| RAN Function Definition (E2 Setup Request, RIC Service Update) | `E2SM-DAPP-RANFunctionDefinition` |
| RIC Event Trigger Definition (RIC Subscription Request) | `E2SM-DAPP-EventTrigger` |
| RIC Action Definition (RIC Subscription Request) | `E2SM-DAPP-ActionDefinition` |
| RIC Indication Header (RIC Indication) | `E2SM-DAPP-IndicationHeader` |
| RIC Indication Message (RIC Indication) | `E2SM-DAPP-IndicationMessage` |
| RIC Control Header (RIC Control Request) | `E2SM-DAPP-ControlHeader` |
| RIC Control Message (RIC Control Request) | `E2SM-DAPP-ControlMessage` |
| RIC Control Outcome (RIC Control Acknowledge) | `E2SM-DAPP-ControlOutcome` |

The grammar is `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`. It is a
byte-identical copy of flexric's
`src/sm/dapp_sm/ie/asn/e2sm_dapp_v0_standard.asn` (checked with `diff`). Module
identifier `E2SM-DAPP-IEs {... oran(53148) e2(1) version1(1) e2sm(255)
e2sm-DAPP-IEs (3)}`, so the OID is `1.3.6.1.4.1.53148.1.1.255.3`.

Plain and FlatBuffers encodings **do not exist**. flexric has stub files for
them (`enc/dapp_enc_plain.c`, `dec/dapp_dec_plain.c`, `enc/dapp_enc_fb.c`,
`dec/dapp_dec_fb.c`), and all 31 functions in them are
`assert(0 != 0 && "Not implemented")` (see [06-discrepancies.md](06-discrepancies.md)).
A conforming stack encodes APER.

### 1.1 The three rules that decide almost every byte

1. **Alignment.** In aligned PER a value that needs whole octets (an integer longer
   than a byte, an octet string, a character string, a length determinant of
   more than 8 bits) first skips to the next octet boundary. The skipped bits are
   zero. Bit fields (extension bits, presence bits, small counts, choice indices)
   are not aligned and are packed MSB first.
2. **Every `SEQUENCE` and `CHOICE` here has an extension marker (`...`)**, and so
   do the constrained integers except `data-size`. An encoder always writes the
   extension bit as `0`. Each marker costs one bit.
3. **`OPTIONAL` fields cost one presence bit each**, written before the first
   component, in the order the fields are declared. A cleared bit means the field is
   absent and takes no further space.

### 1.2 Encoding rules, by ASN.1 construct

| Construct in the grammar | APER bits | Where it appears |
|---|---|---|
| `SEQUENCE { ... }` with `...` | 1 extension bit (`0`), then one presence bit per `OPTIONAL` field, then the fields | every `SEQUENCE` |
| `CHOICE { a, ... }` with one alternative | 1 extension bit, **no** index bits | event trigger, action definition, control header, control message, control outcome |
| `CHOICE { a, b, ... }` with two alternatives | 1 extension bit, then 1 index bit (`0` = first, `1` = second) | indication header, indication message |
| `INTEGER (0..4294967295, ...)` | 1 extension bit, then a 2-bit field holding (number of octets - 1), then pad to the octet boundary, then the value in 1 to 4 octets, big-endian, no leading zero octets | `ran-function-id`, `dapp-id`, `node-nb-id`, `node-cu-du-id`, `SubscribedE3RANFunction-ID` |
| `INTEGER (0..255, ...)` | 1 extension bit, pad, 1 octet | `node-type` |
| `INTEGER (0..32768)` (no `...`) | pad, 2 octets big-endian | `data-size` |
| `INTEGER` with no bounds | pad, 1 length octet, then the value as the fewest two's-complement octets, big-endian | `Timestamp`, `SequenceId`, `RIC-Style-Type`, `RIC-Format-Type`, `ranFunction-Instance` |
| `OCTET STRING (SIZE(3))` | pad, 3 octets, **no length** | `node-plmn-id` |
| `OCTET STRING (SIZE(1..32768))` | pad, 2-octet length holding (length - 1), then the octets | `data` in indication message format 1 and control message format 1 |
| `OCTET STRING` with no bounds | pad, length determinant, then the octets. Length 0..127: 1 octet. Length 128..16383: 2 octets with the top two bits `10`. Longer: fragmentation in 16 KiB blocks | `e3-control-outcome` |
| `PrintableString (SIZE(1..150, ...))` | 1 extension bit, an 8-bit field holding (length - 1), pad, one octet per character | short name, description, style names |
| `PrintableString (SIZE(1..1000, ...))` | 1 extension bit, pad, a 2-octet field holding (length - 1), one octet per character | `ranFunction-E2SM-OID` |
| `SEQUENCE (SIZE(1..64)) OF` | a 6-bit count holding (count - 1), then the elements | `subscribed-e3-ran-functions` |
| `SEQUENCE (SIZE(0..256)) OF` | pad, a 2-octet count, then the elements | `DAppE3Subscription-List` |
| `SEQUENCE (SIZE(1..2)) OF` | a 1-bit count holding (count - 1) | report style list |
| `SEQUENCE (SIZE(1..1)) OF` | no bits for the count | control style list |

Notes on the table.

* **Unbounded integers are not 32-bit.** A `RIC-Style-Type` of `4294967295` (the
  value flexric stores in a `uint32_t`) needs five octets on the wire, because the
  top bit of `0xFF` would otherwise read as negative: `00 ff ff ff ff`. See
  vector `ad_style_max` below.
* **Timestamps are 8 octets** for any value in this century. A nanosecond
  timestamp since 1970 is about `1.7e18`, which needs 61 bits, so it is
  `08` followed by 8 octets. The largest value the grammar can hold in all the
  implementations is `2^63 - 1`, `08 7f ff ff ff ff ff ff ff`.
* **Octet-string content is never bit-shifted.** Because octet strings are
  aligned, the payload inside `data` and `e3-control-outcome` is byte-identical to
  what the sender passed in. A receiver can hand a pointer into the E2SM-DAPP
  buffer to the inner decoder.
* The outer E2AP `OCTET STRING` length is E2AP's, not E2SM-DAPP's. The encoded
  length of an E2SM-DAPP element is whatever the APER value occupies, rounded up
  to a whole octet (see the single-byte examples below).

### 1.3 Byte order, packing, length prefixes

| Topic | Rule |
|---|---|
| Byte order | APER is a bit stream, written most significant bit first. Multi-octet integers and lengths are big-endian. There is nothing host-endian on the wire. |
| Padding | Encoders write zero pad bits. The last octet is padded with zero bits to a whole octet. |
| Struct packing | None on the wire. The in-memory structs are ordinary C structs with natural alignment and no `packed` attribute (flexric `src/sm/dapp_sm/ie/ir/*.h`). |
| Length prefixes | Only where the table above says so. E2SM-DAPP never prefixes a whole element with its length. |
| Extension markers | Written as `0`. See 1.4 for what a receiver does with `1`. |
| Trailing data | APER has no end marker. flexric and libe3 check only that the decode succeeded, not that every octet was consumed (`flexric/src/sm/dapp_sm/dec/dapp_dec_asn.c` asserts `RC_OK` only; libe3's `unpack_pdu` in `libe3/src/e2sm_dapp/e2sm_dapp.cpp` tests `rv.code`). OCUDU's `unpack_e2sm_dapp_ie` (`ocudu/lib/e2/e2sm/e2sm_dapp/e2sm_dapp_asn1_packer.h:35-39`) checks only the return code, so trailing octets are ignored there too. |

### 1.4 A receiver that sees extension additions

The grammar has no extension additions yet, so every extension bit a conforming
sender writes is `0`. A future version could add fields after the `...` marker. A
decoder meeting extension bit `1` has three options, and the implementations pick
differently.

| Implementation | Extension bit `1` on a `SEQUENCE` |
|---|---|
| flexric (asn1c) | asn1c reads the extension bitmap and skips additions it does not know (`SEQUENCE_decode_aper`, "Skip over overflow extensions", in the generated `constr_SEQUENCE_aper.c`), so the value decodes with the added fields ignored. |
| libe3 (asn1c) | Same, because libe3 uses the same runtime family. |
| OCUDU (hand-written codec) | `skip_ext_additions` (`ocudu/lib/asn1/e2sm/e2sm_dapp.cpp`, called at the end of every `unpack`) reads the additions as opaque open types (X.691 19.7 to 19.9) and drops them. `pack` refuses a struct with `ext == true` (`pack_unsupported_ext_flag`). |

A `CHOICE` alternative beyond the defined ones (a future format 3) is not skipped by any
of them: asn1c fails the decode (`CHOICE_decode_aper`), and OCUDU's choice `unpack`
returns `OCUDUASN_ERROR_DECODE_FAIL`.

An extension bit of `1` on a constrained `INTEGER (0..4294967295, ...)` means the
value is outside the root range and uses a different encoding. libe3 and OCUDU
reject it. See [05-semantics-and-constraints.md](05-semantics-and-constraints.md).

## 2. Maximum sizes

| Grammar constant | Value | What it bounds |
|---|---|---|
| `maxE2SMDAPPIndMsgSize` | 32768 | `data` in indication message format 1, and `data-size` |
| `maxE2SMDAPPCtrlMsgSize` | 32768 | `data` in control message format 1, and `data-size` |
| `maxnoofDApps` | 256 | items in a `DAppE3Subscription-List` |
| `maxnoofSubscribedE3RANFunctions` | 64 | functions in one `DAppE3Subscription-Item` |
| `maxnoofDAPPReportStyles` | 2 | items in `ric-ReportStyle-List` |
| `maxnoofDAPPControlStyles` | 1 | items in `ric-ControlStyle-List` |

Smallest and largest encodings. The smallest values are golden vectors. The
largest values assume 64-bit signed timestamps and sequence ids (what every
implementation holds), ids of `4294967295`, and are computed from the rules in
1.2. The ones marked "vector" are in the golden file.

| Element | Smallest | Largest | Notes |
|---|---|---|---|
| Event trigger | 1 octet | 1 octet | vector `et_f1` |
| Action definition | 4 octets | 8 octets | vectors `ad_style1`, `ad_style_max` |
| Indication header format 1 | 12 octets | 44 octets | `ih1_min` is a vector. The 44 differs from the 41 of `ih1_bounds` only because that vector uses `ran-function-id` 0 (2 octets) where the maximum uses `4294967295` (5 octets). |
| Indication header format 2 | 7 octets | 33 octets | `ih2_min` |
| Indication message format 1 | 6 octets | 32773 octets | overhead is 5 octets; `im1_one`, `im1_max` |
| Indication message format 2 | 3 octets (empty list) | 83459 octets | 256 items of 64 functions, every id `4294967295`. A list of 256 items with one such function each is 2819 octets. Vector `im2_256` (first item 64 functions, the rest one, small ids) is 1473. |
| Control header format 1 | 4 octets | 28 octets | `ch1_min`; `ch1_bounds` (25 octets) has `dapp-id` 0 |
| Control message format 1 | 6 octets | 32773 octets | overhead is 5 octets |
| Control outcome format 1 | 1 octet | 19 octets without a payload; the payload adds its own length and its 1 or 2 length octets | `co1_min`, `co1_tsseq` |
| RAN function definition | 78 octets | large: the dApp map is listed once per style that carries it | `fd_min`; the OID and name strings dominate the minimum |

Format 2 is the one element that can be larger than 32768 octets, because the
grammar bounds only the number of items, not the encoded size of the list.

**Implementation limits are tighter than the grammar.** flexric encodes into fixed
buffers and aborts when an element does not fit:

| Element | flexric buffer | Reference |
|---|---|---|
| Event trigger | 512 KiB | `flexric/src/sm/dapp_sm/enc/dapp_enc_asn.c:84` |
| Action definition | 1 KiB | `dapp_enc_asn.c:119` |
| Indication header | 1 KiB | `dapp_enc_asn.c:228` |
| Indication message | 128 KiB | `dapp_enc_asn.c:344` |
| Control header | 16 KiB | `dapp_enc_asn.c:403` |
| Control message | 16 KiB | `dapp_enc_asn.c:443` |
| Control outcome | 16 KiB | `dapp_enc_asn.c:504` |
| RAN function definition | 16 KiB | `dapp_enc_asn.c:660` |

So a control message above about 16 KiB aborts flexric's encoder even though the
grammar allows 32768 (the golden vector `cm1_large` stops at 16000 octets for this
reason, see `libe3/tests/e2sm_dapp_fixtures.inc:296`). The indication message and
header encoders return an empty buffer instead of aborting
(`dapp_enc_asn.c:232-236`, `350-354`). The others assert at lines 87, 122, 406,
446, 507 and 663.

## 3. Worked examples

All the bytes below are from `libe3/tests/e2sm_dapp_golden.hpp`. They were
produced by flexric's own encoder at flexric commit `71631466`. Every example here
was re-derived by hand from the grammar and the rules in 1.2, and all 31 vectors
(27 inline and 4 by length and hash) were also reproduced by an independent
bit-level encoder written for this document, which agreed byte for byte.

Reading the tables: a row is a run of bits in stream order. `pad` is alignment
padding. Bit groups are written MSB first.

### 3.1 `et_f1` = `00` (event trigger)

One octet. The event trigger has one format and no fields.

| Bits | Meaning |
|---|---|
| `0` | `E2SM-DAPP-EventTrigger` extension bit |
| `0` | `ric-eventTrigger-formats` CHOICE extension bit (one alternative, so no index bits) |
| `0` | `E2SM-DAPP-EventTrigger-Format1` extension bit (the SEQUENCE has no fields and no `OPTIONAL`) |
| `00000` | pad to the octet boundary |

### 3.2 `ad_style1` = `00 01 01 00` (action definition, report style 1)

| Octet | Bits | Meaning |
|---|---|---|
| 0 | `0` + `0000000` | `E2SM-DAPP-ActionDefinition` extension bit, then pad (the next item is an unbounded integer, whose length is octet-aligned) |
| 1 | `00000001` | length determinant: 1 octet follows |
| 2 | `00000001` | `ric-Style-Type` = 1 |
| 3 | `0` `0` `000000` | `actionDefinition-formats` CHOICE extension bit, `Format1` SEQUENCE extension bit, pad |

`ad_style2` is the same with `02` in octet 2. `ad_style_max` = `00 05 00 ff ff ff ff 00`
has length 5 and the value `00 ff ff ff ff` (4294967295 needs a leading zero octet
in two's complement), then the same closing octet.

### 3.3 `ih1_min` = `00 00 ff 00 01 00 00 13 00 14 00 00` (indication header format 1, only mandatory fields)

Input (from `libe3/tests/e2sm_dapp_fixtures.inc`, function `ih1_min`):
`ran-function-id` 255, `dapp-id` 1, `node-type` 0, `node-plmn-id` `13 00 14`,
`node-nb-id` 0, no `node-cu-du-id`, no `timestamp`, no `sequence-id`.

| Octet(s) | Bits | Meaning |
|---|---|---|
| 0 | `0` | `E2SM-DAPP-IndicationHeader` extension bit |
| | `0` | `ric-indicationHeader-formats` extension bit |
| | `0` | alternative index, 2 alternatives so 1 bit: `0` = `indicationHeader-Format1` |
| | `0` | `Format1` SEQUENCE extension bit |
| | `0` `0` `0` | presence bits: `node-cu-du-id`, `timestamp`, `sequence-id` all absent |
| | `0` | `ran-function-id` INTEGER extension bit (value is in the root range) |
| 1 | `00` + `000000` | `ran-function-id`: number of octets minus 1 = 0, then pad |
| 2 | `ff` | `ran-function-id` = 255 |
| 3 | `0` `00` + `00000` | `dapp-id`: extension bit, number of octets minus 1 = 0, pad |
| 4 | `01` | `dapp-id` = 1 |
| 5 | `0` + `0000000` | `node-type`: extension bit, pad (range is exactly 256, so one aligned octet) |
| 6 | `00` | `node-type` = 0 |
| 7-9 | `13 00 14` | `node-plmn-id`, three raw octets, no length |
| 10 | `0` `00` + `00000` | `node-nb-id`: extension bit, number of octets minus 1 = 0, pad |
| 11 | `00` | `node-nb-id` = 0 |

Total 12 octets. The first octet is entirely header bits: the whole presence map
and the first extension bit of the first integer fit in it.

### 3.4 `ih1_full` = `0e 00 ff 00 07 00 02 00 f1 10 20 04 d2 00 05 08 17 97 9c fe 3d 85 cd 15 01 2a`

Same shape, with all three optional fields present. Differences from `ih1_min`:

| Octet(s) | Bits or bytes | Meaning |
|---|---|---|
| 0 | `0000 111` + `0` | extensions `0`, `0`, index `0`, SEQUENCE extension `0`, then presence bits `1 1 1`, then the `ran-function-id` extension bit `0`. The octet is `0x0e`. |
| 1-2 | `00 ff` | `ran-function-id` = 255 |
| 3-4 | `00 07` | `dapp-id` = 7 |
| 5-6 | `00 02` | `node-type` = 2 |
| 7-9 | `00 f1 10` | `node-plmn-id` |
| 10-12 | `20 04 d2` | `node-nb-id` = 1234: extension bit `0`, number of octets minus 1 = `01` (two octets), so bits `0 01` + pad = `0x20`, then `04 d2` |
| 13-14 | `00 05` | `node-cu-du-id` = 5 (present) |
| 15-23 | `08 17 97 9c fe 3d 85 cd 15` | `timestamp`: length 8, value `0x17979cfe3d85cd15` = 1700000000123456789 ns, which is 2023-11-14 22:13:20.123456789 UTC |
| 24-25 | `01 2a` | `sequence-id` = 42 |

### 3.5 `ch1_min` = `00 01 00 02` (control header, no timestamp, no sequence-id)

Input: `ran-function-id` 1, `dapp-id` 2.

| Octet | Bits | Meaning |
|---|---|---|
| 0 | `0` | `E2SM-DAPP-ControlHeader` extension bit |
| | `0` | `ric-controlHeader-formats` extension bit (one alternative, no index) |
| | `0` | `Format1` SEQUENCE extension bit |
| | `0` `0` | presence bits: `timestamp`, `sequence-id` absent |
| | `0` | `ran-function-id` extension bit |
| | `00` | number of octets minus 1 = 0 (the length field closes the octet, so there is no pad here) |
| 1 | `01` | `ran-function-id` = 1 |
| 2 | `0` `00` + `00000` | `dapp-id`: extension bit, length field, pad |
| 3 | `02` | `dapp-id` = 2 |

Compare with `ih1_min`: the control header has one choice alternative and two
`OPTIONAL` fields, so the preamble is 5 bits instead of 7 and the first octet is
completely full.

### 3.6 `co1_full` = `1c 08 17979cfe3d85cd15 01 2a 0a 10 20 30 40 50 60 70 80 90 a0` (control outcome, everything present)

This is the outcome that carries the Service-Model payload.

| Octet(s) | Bits or bytes | Meaning |
|---|---|---|
| 0 | `0` `0` `0` | `E2SM-DAPP-ControlOutcome` extension bit; `ric-controlOutcome-formats` extension bit (one alternative); `Format1` extension bit |
| | `1` `1` `1` | presence bits: `timestamp`, `sequence-id`, `e3-control-outcome` all present |
| | `00` | pad before the first octet-aligned item. The octet is `000 111 00` = `0x1c`. |
| 1 | `08` | `timestamp` length: 8 octets |
| 2-9 | `17 97 9c fe 3d 85 cd 15` | `timestamp` = 1700000000123456789 |
| 10 | `01` | `sequence-id` length: 1 octet |
| 11 | `2a` | `sequence-id` = 42 |
| 12 | `0a` | `e3-control-outcome` length: 10 octets |
| 13-22 | `10 20 30 40 50 60 70 80 90 a0` | the opaque Service-Model payload, copied unchanged |

23 octets. The three smaller outcomes show how the presence bits move:

| Vector | Hex | Presence bits | Why |
|---|---|---|---|
| `co1_min` | `00` | `000` | acknowledge only: three zero extension bits, three zero presence bits, two pad bits |
| `co1_tsseq` | `18 08 17979cfe3d85cd15 01 2a` | `110` | octet 0 = `000 110 00` |
| `co1_payload` | `04 0a 10 20 30 40 50 60 70 80 90 a0` | `001` | octet 0 = `000 001 00` |

### 3.7 Two APER layers nest in the control outcome

`e3-control-outcome` is an `OCTET STRING`, and the Spectrum Service Model fills it
with its own APER encoding of `Spectrum-ApplyOutcomeData`
([02-ie-mapping.md](02-ie-mapping.md#8-the-e3-control-outcome-payload)). So a received
outcome has two independent APER values, one inside the other:

```text
E2SM-DAPP-ControlOutcome            (APER, this specification)
  timestamp, sequence-id
  e3-control-outcome OCTET STRING   (octet-aligned, copied as is)
    Spectrum-ApplyOutcomeData       (APER, owned by the Spectrum Service Model)
```

The inner value starts on an octet boundary and its bit alignment is its own: it
is decoded from its first octet as if it were a standalone message. The same
tunnel is used by `ControlMessage-Format1.data` (xApp to RAN) and
`IndicationMessage-Format1.data` (RAN to xApp).

### 3.8 `im1_small` = `00 00 04 00 03 de ad be ef` (indication message format 1, 4 octets of payload)

| Octet(s) | Bits or bytes | Meaning |
|---|---|---|
| 0 | `0` `0` `0` `0` + `0000` | `E2SM-DAPP-IndicationMessage` extension bit; `ric-indicationMessage-formats` extension bit; index bit `0` = `Format1`; `Format1` extension bit; pad |
| 1-2 | `00 04` | `data-size` = 4. The integer `(0..32768)` has a range above 256, so it takes two aligned octets. |
| 3-4 | `00 03` | `data` length minus 1 = 3, two aligned octets (the size range 1..32768 is above 256 too) |
| 5-8 | `de ad be ef` | `data` |

`data-size` and the octet-string length both carry the payload size. See
[06-discrepancies.md](06-discrepancies.md) for what each implementation does when
the two disagree.

### 3.9 `im2_one` = `20 00 01 00 01 00 00 01` (indication message format 2, one dApp, one function)

| Octet(s) | Bits or bytes | Meaning |
|---|---|---|
| 0 | `0` `0` `1` `0` + `0000` | extension bit, extension bit, index bit `1` = `Format2`, `Format2` extension bit, pad before the 2-octet list count. The octet is `0x20`. |
| 1-2 | `00 01` | `DAppE3Subscription-List` count = 1 |
| 3 | `0` `0` `00` + `0000` | `DAppE3Subscription-Item` extension bit; `dapp-id` extension bit; `dapp-id` length field (octets minus 1 = 0); pad |
| 4 | `01` | `dapp-id` = 1 |
| 5-6 | `000000` `0` `0` / `0` + `0000000` | 6-bit function count minus 1 = 0; the function id's extension bit; the first bit of its 2-bit length field (octet 5 ends here); then the second bit of the length field and pad (octet 6). Both octets are `00`. |
| 7 | `01` | function id = 1 |

Octets 5 and 6 show a bit field spanning the octet boundary: the first length
bit is the last bit of octet 5 and the second is the first bit of octet 6.

### 3.10 fd_min, the smallest RAN function definition

`00 10 45 32 53 4d 2d 44 41 50 50 00 00 1a 31 2e 33 2e 36 2e 31 2e 34 2e 31 2e 35 33 31 34 38 2e 31 2e 31 2e 32 35 35 2e 33 11 00 44 41 50 50 20 53 65 72 76 69 63 65 20 4d 6f 64 65 6c 20 66 6f 72 20 45 32 2f 45 33 20 62 72 69 64 67 65`

| Octet(s) | Meaning |
|---|---|
| 0 | bit 0: `E2SM-DAPP-RANFunctionDefinition` extension bit. Bits 1-3: presence bits for event trigger, report, control, all `0`. Bit 4: `RANfunction-Name` extension bit. Bit 5: its `ranFunction-Instance` presence bit. Bit 6: short-name extension bit. Bit 7: first bit of the 8-bit short-name length field. All `0`, so the octet is `00`. |
| 1 | the other 7 bits of the length field, then 1 pad bit. Length minus 1 = 8 = `00001000`, so the first bit is `0` (in octet 0) and the rest are `0001000`, then pad `0`: `0001 0000` = `10`. |
| 2-10 | `E2SM-DAPP` (`45 32 53 4d 2d 44 41 50 50`), nine characters |
| 11 | `00`: the OID string's extension bit, then pad |
| 12-13 | `00 1a`: OID length minus 1 = 26 (the OID `1.3.6.1.4.1.53148.1.1.255.3` has 27 characters) |
| 14-40 | the OID as characters |
| 41-42 | `11 00`: description extension bit `0`, then the 8-bit length minus 1 = 34 (`00100010`), then pad. As bits: `0 00100010 0000000`, which is `11 00` |
| 43-77 | `DAPP Service Model for E2/E3 bridge`, 35 characters |

The other three definition vectors add the optional sections:

| Vector | Octets | Content |
|---|---|---|
| `fd_min` | 78 | name only |
| `fd_report` | 143 | name, report section with two styles (style 2 carries a subscription map of dApps 7 and 9) |
| `fd_ctrl` | 101 | name, event trigger section (empty), control section with one style, no map |
| `fd_full` | 175 | name, empty event trigger section, two report styles (style 2 with a map), one control style with a map |

## 4. Golden vectors (full list)

`libe3/tests/e2sm_dapp_golden.hpp` holds the bytes. `libe3/tests/e2sm_dapp_fixtures.inc`
builds each input. Vectors too large to inline carry a length and an FNV-1a 64-bit
hash instead (the offset basis is `14695981039346656037`, the prime is
`1099511628211`; see `libe3/tools/e2sm_dapp_golden/gen_golden.c`, function `emit`).

| Vector | Element | Input | Octets |
|---|---|---|---|
| `et_f1` | event trigger | format 1 | 1 |
| `ad_style1` | action definition | style 1 | 4 |
| `ad_style2` | action definition | style 2 | 4 |
| `ad_style_max` | action definition | style 4294967295 | 8 |
| `ih1_full` | indication header 1 | all optional fields | 26 |
| `ih1_min` | indication header 1 | no optional fields | 12 |
| `ih1_bounds` | indication header 1 | id fields at 0 and 4294967295, `node-type` 255, timestamp and sequence-id `INT64_MAX` | 41 |
| `ih2_full` | indication header 2 | all optional fields | 21 |
| `ih2_min` | indication header 2 | no optional fields | 7 |
| `im1_small` | indication message 1 | 4 octets of payload | 9 |
| `im1_one` | indication message 1 | 1 octet of payload | 6 |
| `im1_max` | indication message 1 | 32768 octets (pattern `i*7+3`) | 32773, hash `0xf1a4af710fb93cc5` |
| `im2_empty` | indication message 2 | empty list | 3 |
| `im2_one` | indication message 2 | one dApp, one function | 8 |
| `im2_multi` | indication message 2 | two dApps, three and two functions, ids at 0 and 4294967295 | 25 |
| `im2_255` | indication message 2 | 255 dApps, first one with 63 functions | 1464, hash `0x19011514fbcfc2de` |
| `im2_256` | indication message 2 | 256 dApps, first one with 64 functions | 1473, hash `0x74d95212a317dbb8` |
| `ch1_full` | control header | all optional fields | 15 |
| `ch1_min` | control header | no optional fields | 4 |
| `ch1_bounds` | control header | `ran-function-id` 4294967295, `dapp-id` 0, timestamp and sequence-id `INT64_MAX` | 25 |
| `cm1_small` | control message | 3 octets of payload | 8 |
| `cm1_one` | control message | 1 octet of payload | 6 |
| `cm1_large` | control message | 16000 octets (pattern `i*13+5`) | 16005, hash `0x5250310ee619ccfe` |
| `co1_full` | control outcome | timestamp, sequence-id, 10-octet payload | 23 |
| `co1_min` | control outcome | nothing | 1 |
| `co1_tsseq` | control outcome | timestamp and sequence-id | 12 |
| `co1_payload` | control outcome | payload only | 12 |
| `fd_min` | RAN function definition | name only | 78 |
| `fd_report` | RAN function definition | report section | 143 |
| `fd_full` | RAN function definition | all sections | 175 |
| `fd_ctrl` | RAN function definition | control section | 101 |

Properties these vectors pin down:

* The all-ones values (`4294967295`, `INT64_MAX`) and the minimum values (`0`)
  of every bounded and unbounded integer.
* An absent versus a present optional field, in each combination for the outcome.
* The smallest and largest legal payloads of both message formats (the control
  message stops at 16000 because of flexric's buffer).
* A subscription list of 0, 1, 2, 255 and 256 items.

The vectors do **not** cover: extension bit `1`, a `ranFunction-Instance`, an
`e3-control-outcome` longer than 127 octets (two-octet length form), a PLMN that is
not three bytes, a timestamp with the top bit set, and any invalid input.

## 5. The type map across implementations

The same ASN.1 types appear in four code bases under four spellings. Field names in
libe3's C++ and OCUDU are identical, on purpose (see
[integrator-guide.md](integrator-guide.md)).

### 5.1 Type names

| ASN.1 type | libe3 C++ (`libe3::e2sm_dapp`, `include/libe3/e2sm_dapp.hpp`) | libe3 C (`include/libe3/e2sm_dapp_c.h`) | flexric IR (`src/sm/dapp_sm/ie/`) | OCUDU (`asn1::e2sm_dapp`, `include/ocudu/asn1/e2sm/e2sm_dapp.h`) |
|---|---|---|---|---|
| `E2SM-DAPP-EventTrigger` | `e2sm_dapp_event_trigger_s` | `libe3_e2sm_dapp_event_trigger_t` | `e2sm_dapp_event_trigger_t` | `e2sm_dapp_event_trigger_s` |
| `E2SM-DAPP-EventTrigger-Format1` | `e2sm_dapp_event_trigger_format1_s` | (no fields, folded into the enum) | `e2sm_dapp_ev_trg_frmt_1_t` | `e2sm_dapp_event_trigger_format1_s` |
| `E2SM-DAPP-ActionDefinition` | `e2sm_dapp_action_definition_s` | `libe3_e2sm_dapp_action_definition_t` | `e2sm_dapp_action_def_t` | `e2sm_dapp_action_definition_s` |
| `E2SM-DAPP-ActionDefinition-Format1` | `e2sm_dapp_action_definition_format1_s` | (no fields) | `e2sm_dapp_action_def_frmt_1_t` | `e2sm_dapp_action_definition_format1_s` |
| `E2SM-DAPP-IndicationHeader` | `e2sm_dapp_ind_hdr_s` | `libe3_e2sm_dapp_ind_hdr_t` | `e2sm_dapp_ind_hdr_t` | `e2sm_dapp_ind_hdr_s` |
| `E2SM-DAPP-IndicationHeader-Format1` | `e2sm_dapp_ind_hdr_format1_s` | `libe3_e2sm_dapp_ind_hdr_format1_t` | `e2sm_dapp_ind_hdr_frmt_1_t` | `e2sm_dapp_ind_hdr_format1_s` |
| `E2SM-DAPP-IndicationHeader-Format2` | `e2sm_dapp_ind_hdr_format2_s` | `libe3_e2sm_dapp_ind_hdr_format2_t` | `e2sm_dapp_ind_hdr_frmt_2_t` | `e2sm_dapp_ind_hdr_format2_s` |
| `E2SM-DAPP-IndicationMessage` | `e2sm_dapp_ind_msg_s` | `libe3_e2sm_dapp_ind_msg_t` | `e2sm_dapp_ind_msg_t` | `e2sm_dapp_ind_msg_s` |
| `E2SM-DAPP-IndicationMessage-Format1` | `e2sm_dapp_ind_msg_format1_s` | `libe3_e2sm_dapp_ind_msg_format1_t` | `e2sm_dapp_ind_msg_frmt_1_t` | `e2sm_dapp_ind_msg_format1_s` |
| `E2SM-DAPP-IndicationMessage-Format2` | `e2sm_dapp_ind_msg_format2_s` | `libe3_e2sm_dapp_ind_msg_format2_t` | `e2sm_dapp_ind_msg_frmt_2_t` | `e2sm_dapp_ind_msg_format2_s` |
| `E2SM-DAPP-ControlHeader` | `e2sm_dapp_ctrl_hdr_s` | `libe3_e2sm_dapp_ctrl_hdr_t` (format and fields in one struct) | `e2sm_dapp_ctrl_hdr_t` | `e2sm_dapp_ctrl_hdr_s` |
| `E2SM-DAPP-ControlHeader-Format1` | `e2sm_dapp_ctrl_hdr_format1_s` | (same struct) | `e2sm_dapp_ctrl_hdr_frmt_1_t` | `e2sm_dapp_ctrl_hdr_format1_s` |
| `E2SM-DAPP-ControlMessage` | `e2sm_dapp_ctrl_msg_s` | `libe3_e2sm_dapp_ctrl_msg_t` (format and data in one struct) | `e2sm_dapp_ctrl_msg_t` | `e2sm_dapp_ctrl_msg_s` |
| `E2SM-DAPP-ControlMessage-Format1` | `e2sm_dapp_ctrl_msg_format1_s` | (same struct) | `e2sm_dapp_ctrl_msg_frmt_1_t` | `e2sm_dapp_ctrl_msg_format1_s` |
| `E2SM-DAPP-ControlOutcome` | `e2sm_dapp_ctrl_outcome_s` | `libe3_e2sm_dapp_ctrl_outcome_t` (format and fields in one struct) | `e2sm_dapp_ctrl_out_t` | `e2sm_dapp_ctrl_outcome_s` |
| `E2SM-DAPP-ControlOutcome-Format1` | `e2sm_dapp_ctrl_outcome_format1_s` | (same struct) | `e2sm_dapp_ctrl_out_frmt_1_t` | `e2sm_dapp_ctrl_outcome_format1_s` |
| `DAppE3Subscription-Item` | `dapp_e3_subscription_item_s` | `libe3_e2sm_dapp_sub_item_t` | `dapp_e3_subscription_item_t` | `dapp_e3_subscription_item_s` |
| `DAppE3Subscription-List` | `dapp_e3_subscription_list_l` | `libe3_e2sm_dapp_sub_list_t` | `dapp_e3_subscription_list_t` | `dapp_e3_subscription_list_l` |
| `RANfunction-Name` | `ran_function_name_s` | `libe3_e2sm_dapp_ran_function_name_t` | `ran_function_name_t` (`flexric/src/lib/sm/ie/ran_function_name.h`, shared with other service models) | `ran_function_name_s` |
| `RANFunctionDefinition-Report-Item` | `ran_function_definition_report_item_s` | `libe3_e2sm_dapp_report_style_t` | `seq_report_sty_dapp_sm_t` | `ran_function_definition_report_item_s` |
| `RANFunctionDefinition-Control-Item` | `ran_function_definition_ctrl_item_s` | `libe3_e2sm_dapp_ctrl_style_t` | `seq_ctrl_style_dapp_sm_t` | `ran_function_definition_ctrl_item_s` |
| `RANFunctionDefinition-Report` | `ran_function_definition_report_s` | (array inside the definition struct) | `ran_func_def_report_dapp_sm_t` | `ran_function_definition_report_s` |
| `RANFunctionDefinition-Control` | `ran_function_definition_ctrl_s` | (array inside the definition struct) | `ran_func_def_ctrl_dapp_sm_t` | `ran_function_definition_ctrl_s` |
| `RANFunctionDefinition-EventTrigger` | `ran_function_definition_event_trigger_s` | `has_event_trigger` flag | `ran_func_def_ev_trig_dapp_sm_t` | `ran_function_definition_event_trigger_s` |
| `E2SM-DAPP-RANFunctionDefinition` | `e2sm_dapp_ran_function_definition_s` | `libe3_e2sm_dapp_ran_function_definition_t` | `e2sm_dapp_func_def_t` | `e2sm_dapp_ran_function_definition_s` |

### 5.2 How the same field is held

| ASN.1 field | flexric IR | libe3 C++ | libe3 C | OCUDU |
|---|---|---|---|---|
| `ran-function-id`, `dapp-id` (0..4294967295) | `uint32_t` | `uint64_t` | `uint32_t` | `uint64_t` |
| `node-type` (0..255) | `uint8_t` | `uint8_t` | `uint8_t` | `uint8_t` |
| `node-plmn-id` (SIZE(3)) | `uint8_t[3]` | `fixed_octstring<3, true>` (a `std::array<uint8_t, 3>`) | `uint8_t[3]` | `fixed_octstring<3, true>` |
| `node-nb-id` (0..4294967295) | `uint32_t` | `uint64_t` | `uint32_t` | `uint64_t` |
| `node-cu-du-id` OPTIONAL | `bool node_cu_du_id_present` + `uint64_t` | `node_cu_du_id_present` + `uint64_t` | `has_node_cu_du_id` + `uint32_t` | `node_cu_du_id_present` + `uint64_t` |
| `timestamp` OPTIONAL | `int64_t timestamp_ns`, **0 means absent** | `timestamp_present` + `int64_t` | `has_timestamp` + `int64_t` | `timestamp_present` + `int64_t` |
| `sequence-id` OPTIONAL | `int64_t sequence_id`, **0 means absent** | `sequence_id_present` + `int64_t` | `has_sequence_id` + `int64_t` | `sequence_id_present` + `int64_t` |
| `data-size` | `size_t data_size` (indication), `uint32_t data_size` (control) | `uint16_t data_size` | none, it is `data.len` | `uint16_t data_size` |
| `data` | `uint8_t* data` | `std::vector`-like `bounded_octstring<1, 32768, true>` | `libe3_e2sm_dapp_bytes_t data` | `bounded_octstring<1, 32768, true>` |
| `e3-control-outcome` OPTIONAL | `uint8_t*` + `uint32_t e3_control_outcome_size`, **NULL and 0 mean absent** | `e3_ctrl_outcome_present` + `unbounded_octstring<true>` | `has_e3_control_outcome` + bytes | `e3_ctrl_outcome_present` + `unbounded_octstring<true>` |
| `ric-style-type`, format and style types | `uint32_t` | `int64_t` | `int64_t` | `int64_t` |
| `ranFunction-Instance` OPTIONAL | `ran_function_name_t::instance` pointer, **encoder asserts it is NULL** | `ran_function_instance_present` + `int64_t` | `has_instance` + `int64_t` | `ran_function_instance_present` + `int64_t` |
| the format choice | `enum` named `FORMAT_1_...`, values start at **0** | `type()` of an `enumerated`, `to_number()` is the wire format number, starting at **1** | `enum`, values 1, 2, ... | same as libe3 C++ |

Two traps in this table:

* **flexric's format enums start at 0 and the wire's format numbers start at 1.**
  `FORMAT_1_E2SM_DAPP_IND_HDR` is `0` in memory
  (`flexric/src/sm/dapp_sm/ie/dapp_data_ie.h:168-173`). Python code that mirrors
  these enums through `ctypes` has to use the same numbering
  (`xdevsm/src/xdevsm/sm_framework/py_oran/dapp/enums.py`). The enum value is
  **not** the same as the `RIC-Format-Type` carried in the definition, which is
  `1` and `2`.
* **A zeroed flexric struct is a valid format 1.** Because `FORMAT_1_..._IND_MSG`
  is `0`, an all-zero `e2sm_dapp_ind_msg_t` reads as "format 1, no data". The
  encoder guards against this (`dapp_enc_asn.c:295-321`).

### 5.3 The struct layout of flexric is an ABI

`xdevsm` does not link against a header. It mirrors flexric's C structs field by
field with `ctypes`, and calls `dapp_dec_*_asn` and `dapp_enc_*_asn` in
`libdapp_sm.so`. The decoders return the structs **by value**. A mirror whose
field order or type differs from `libdapp_sm.so` reads the wrong offsets and does
not complain. Examples: `e2sm_dapp_ctrl_out_frmt_1_t` in
`xdevsm/src/xdevsm/sm_framework/py_oran/dapp/control/DAppControlOut.py:12-22`, and
`spectrum_sm_report_t` in `.../dapp/e3/DAppE3IndPayload.py`. Treat the IR structs in
`flexric/src/sm/dapp_sm/ie/ir/*.h` as frozen between releases of
`libdapp_sm.so`.

## 6. Build switches

### 6.1 flexric: `SM_ENCODING_DAPP`

| Item | Value | Reference |
|---|---|---|
| CMake variable | `SM_ENCODING_DAPP`, a cache string | `flexric/CMakeLists.txt:317-319` |
| Allowed values | `ASN` only (the property `STRINGS` lists nothing else) | `CMakeLists.txt:318` |
| What `PLAIN` does if forced | `src/sm/dapp_sm/CMakeLists.txt` has a `PLAIN` branch that builds `enc/dapp_enc_plain.c`, and every function in it asserts | `dapp_enc_plain.c` |
| What `FLATBUFFERS` does if forced | `message(FATAL_ERROR "DAPP SM FB not implemented")` | `src/sm/dapp_sm/CMakeLists.txt` |
| asn1c invocation | `/opt/asn1c/bin/asn1c -no-gen-BER -no-gen-UPER -no-gen-OER -no-gen-JER -fcompound-names -no-gen-example -findirect-choice -fno-include-deps` | `src/sm/dapp_sm/ie/asn/CMakeLists.txt` |
| Compile definitions on the SM | `-DASN_DISABLE_OER_SUPPORT -DASN_DISABLE_JER_SUPPORT` and the encoding name (`ASN`) as a public definition | `src/sm/dapp_sm/CMakeLists.txt` |
| Generated sources | written **into the source tree** at configure time (`execute_process` with `-D ${CMAKE_CURRENT_SOURCE_DIR}`) | `ie/asn/CMakeLists.txt` |
| Agent-side switch | the agent file is built with `ASN`, `FLATBUFFERS` or `PLAIN` defined and falls through to `static_assert(false, ...)` | `dapp_sm_agent.c:16-24` |
| E3 relay | the `E3_AGENT` macro adds the real I/O to the control and setup paths | `dapp_sm_agent.c:72-79`, `179-201`, `251-256` |

### 6.2 libe3: `LIBE3_ENABLE_E2SM_DAPP`

| Item | Value | Reference |
|---|---|---|
| CMake option | `LIBE3_ENABLE_E2SM_DAPP`, default `ON` | `libe3/cmake/libe3Options.cmake:26` |
| Dependency | needs `LIBE3_ENABLE_ASN1`; if that is `OFF` the option is forced `OFF` with a status message | `libe3Options.cmake:27-30` |
| Targets | `libe3::e2sm_dapp` (static) and `libe3::e2sm_dapp_shared` | `libe3/cmake/libe3Targets.cmake` |
| Compile definition | `LIBE3_ENABLE_E2SM_DAPP`, public | `libe3Targets.cmake` |
| asn1c invocation | `-pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example` | `libe3/messages/CMakeLists.txt:91` |
| asn1c version | the fork `mouse07410/asn1c`, commit `940dd5fa9f3917913fd487b13dfddfacd0ded06e` | `libe3/build_libe3:195-227` |
| Generated sources | in the build tree: `${PROJECT_BINARY_DIR}/generated/e2sm_dapp` | `messages/CMakeLists.txt:86` |
| asn1c runtime | a private copy, symbols hidden, so a consumer that links its own asn1c (flexric does) does not clash | `messages/CMakeLists.txt:76-82` |

Targets: `libe3_e2sm_dapp` and `libe3_e2sm_dapp_shared` with the aliases above
(`cmake/libe3Targets.cmake:190-209`). The install ships the headers `e2sm_dapp.hpp` and
`e2sm_dapp_c.h`, the grammar under `share/libe3/e2sm_dapp/` and a `libe3_e2sm_dapp.pc`
(`cmake/libe3Install.cmake:98-161`). `find_package(libe3 CONFIG REQUIRED COMPONENTS
e2sm_dapp)` finds the component only if the install has the codec
(`cmake/libe3Config.cmake.in:30-50`).

### 6.3 OCUDU: `e2sm_dapp_enabled`

OCUDU does not use asn1c for E2SM-DAPP. The codec is hand-written in the style of
OCUDU's other E2SM headers: `include/ocudu/asn1/e2sm/e2sm_dapp.h` declares the
structs and `lib/asn1/e2sm/e2sm_dapp.cpp` implements `pack` and `unpack` on
OCUDU's `bit_ref` and `cbit_ref`. It is built into `e2ap_asn1`
(`ocudu/lib/asn1/CMakeLists.txt`, the `add_library(e2ap_asn1 ...)` line).

The run-time switch is `e2sm_dapp_enabled`, default `false`:

| Item | Reference |
|---|---|
| `e2ap_configuration::e2sm_dapp_enabled` | `ocudu/include/ocudu/e2/e2ap_configuration.h:27` |
| application config `e2_config::e2sm_dapp_enabled` | `ocudu/apps/helpers/e2/e2_appconfig.h:33` |
| command line `--e2sm_dapp_enabled`, and the YAML key of the same name in the E2 section | `apps/helpers/e2/e2_cli11_schema.cpp:35`, `e2_config_yaml_writer.cpp:20` |
| copied into the E2AP configuration for the **DU-high only**; the CU-CP and CU-UP translators set it `false` | `apps/units/flexible_o_du/o_du_high/e2/o_du_high_e2_config_translators.cpp:24`, `apps/units/o_cu_cp/e2/o_cu_cp_e2_config_translators.cpp:20`, `apps/units/o_cu_up/e2/o_cu_up_e2_config_translators.cpp:24` |
| effect: `create_e2_du_agent` registers the service model (RAN function 255, the OID) and the E2 Setup Request lists it | `lib/e2/common/e2_du_factory.cpp:90-102`, `lib/e2/common/e2ap_asn1_helpers.h:138-148` |

With the flag on and no E3 link attached, the agent advertises the RAN function,
admits subscriptions, never sends an indication, and refuses every control (see
[03-procedure-flow.md](03-procedure-flow.md#8-the-same-flow-in-each-implementation)).

## 7. Reproducing and checking an encoding

* To regenerate the golden vectors from flexric:
  [integrator-guide.md](integrator-guide.md#reproducing-the-golden-vectors).
* To check a stack: encode every fixture and compare with the hex in
  `tests/e2sm_dapp_golden.hpp`, then decode every hex and compare with the fixture
  ([checklist](integrator-guide.md#checklist-for-a-new-stack)).
