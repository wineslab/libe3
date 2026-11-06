# E2SM-DAPP: encoding

This chapter describes how an implementation turns every E2SM-DAPP information
element into bytes. It gives worked examples that an implementer can check against
the reference test vectors, the size limits and the type map.

The index is [README.md](../e2sm-dapp/README.md). The field lists are in
[02-ie-mapping.md](02-ie-mapping.md). The validation rules are in
[05-semantics-and-constraints.md](05-semantics-and-constraints.md). Code references,
product names and build switches are in [integrator-guide.md](integrator-guide.md).

## What is on the wire

An implementation shall encode each E2SM-DAPP information element as one ASN.1 value
with **APER (ASN.1 Packed Encoding Rules, aligned variant, X.691)**. It shall carry
the value as the content of an `OCTET STRING` in the E2AP message. E2AP does the
framing. E2SM-DAPP adds no header, no length prefix, no checksum and no version byte
of its own.

Table: ASN.1 type that each E2AP field carries

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

The grammar is in the last clause of this chapter. Its module identifier is
`E2SM-DAPP-IEs {... oran(53148) e2(1) version1(1) e2sm(255) e2sm-DAPP-IEs (3)}`, so
the OID is `1.3.6.1.4.1.53148.1.1.255.3`.

Plain and FlatBuffers encodings **do not exist**. A conforming stack encodes APER.
Some implementations ship unused stubs for the other encodings.

### The three rules that decide almost every byte

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

### Encoding rules, by ASN.1 construct

Table: APER bits for each ASN.1 construct

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
  largest value of a 32-bit unsigned integer) needs five octets on the wire, because the
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

### Byte order, packing, length prefixes

Table: Byte order, padding and length prefixes

| Topic | Rule |
|---|---|
| Byte order | APER is a bit stream, written most significant bit first. Multi-octet integers and lengths are big-endian. There is nothing host-endian on the wire. |
| Padding | Encoders write zero pad bits. The last octet is padded with zero bits to a whole octet. |
| Struct packing | None on the wire. Native structures in memory are the implementation's own business. |
| Length prefixes | Only where the table above says so. E2SM-DAPP never prefixes a whole element with its length. |
| Extension markers | Written as `0`. The next subsection says what a receiver does with `1`. |
| Trailing data | APER has no end marker. A decoder checks that the decode succeeded. It does not need to check that it consumed every octet, and several implementations ignore trailing octets. A receiver should not rely on a sender to append nothing. |

### A receiver that sees extension additions

The grammar has no extension additions yet, so every extension bit that a conforming
sender writes is `0`. A future version could add fields after the `...` marker. A
decoder that meets extension bit `1` has three options, and implementations pick
differently.

Table: Handling of extension bit 1 on a SEQUENCE

| Implementation | Extension bit `1` on a `SEQUENCE` |
|---|---|
| A (generated codec) | The decoder reads the extension bitmap and skips additions that it does not know. The value decodes with the added fields ignored. |
| B (hand-written codec) | The decoder reads the additions as opaque open types (X.691 19.7 to 19.9) and drops them. The encoder refuses a structure with the extension flag set. |

A `CHOICE` alternative beyond the defined ones (a future format 3) is not skipped by
any implementation. The decode fails.

An extension bit of `1` on a constrained `INTEGER (0..4294967295, ...)` means that the
value is outside the root range and uses a different encoding. Implementations
reject it. See [05-semantics-and-constraints.md](05-semantics-and-constraints.md).

## Maximum sizes

Table: Size constants of the grammar

| Grammar constant | Value | What it bounds |
|---|---|---|
| `maxE2SMDAPPIndMsgSize` | 32768 | `data` in indication message format 1, and `data-size` |
| `maxE2SMDAPPCtrlMsgSize` | 32768 | `data` in control message format 1, and `data-size` |
| `maxnoofDApps` | 256 | items in a `DAppE3Subscription-List` |
| `maxnoofSubscribedE3RANFunctions` | 64 | functions in one `DAppE3Subscription-Item` |
| `maxnoofDAPPReportStyles` | 2 | items in `ric-ReportStyle-List` |
| `maxnoofDAPPControlStyles` | 1 | items in `ric-ControlStyle-List` |

The next table gives the smallest and the largest encodings. The smallest values are
reference test vectors. The largest values assume 64-bit signed timestamps and
sequence ids (what every implementation holds) and ids of `4294967295`. They follow
from the encoding rules. The names in the Notes column are reference test vectors.

Table: Smallest and largest encodings

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

**Implementation limits are tighter than the grammar.** An implementation can encode
into fixed buffers and fail when an element does not fit. The buffers range from 1 KiB
for an action definition to 512 KiB for an event trigger, and one implementation
limits a control message to about 16 KiB, although the grammar allows 32768 octets.
For this reason the reference test vector `cm1_large` stops at 16000 octets. A sender
that wants to work with every implementation should keep a control message below 16
KiB. The details are in [integrator-guide.md](integrator-guide.md).

## Worked examples

All the bytes below are reference test vectors. They were produced by the encoder of
a reference implementation. Every example here was re-derived by hand from the
grammar and the encoding rules. All 31 vectors (27 inline and 4 by length and hash)
were also reproduced by an independent bit-level encoder, which agreed byte for
byte.

Reading the tables: a row is a run of bits in stream order. `pad` is alignment
padding. Bit groups are written MSB first.

### `et_f1` = `00` (event trigger)

One octet. The event trigger has one format and no fields.

Table: Bits of the event trigger `et_f1`

| Bits | Meaning |
|---|---|
| `0` | `E2SM-DAPP-EventTrigger` extension bit |
| `0` | `ric-eventTrigger-formats` CHOICE extension bit (one alternative, so no index bits) |
| `0` | `E2SM-DAPP-EventTrigger-Format1` extension bit (the SEQUENCE has no fields and no `OPTIONAL`) |
| `00000` | pad to the octet boundary |

### `ad_style1` = `00 01 01 00` (action definition, report style 1)

Table: Octets of the action definition `ad_style1`

| Octet | Bits | Meaning |
|---|---|---|
| 0 | `0` + `0000000` | `E2SM-DAPP-ActionDefinition` extension bit, then pad (the next item is an unbounded integer, whose length is octet-aligned) |
| 1 | `00000001` | length determinant: 1 octet follows |
| 2 | `00000001` | `ric-Style-Type` = 1 |
| 3 | `0` `0` `000000` | `actionDefinition-formats` CHOICE extension bit, `Format1` SEQUENCE extension bit, pad |

`ad_style2` is the same with `02` in octet 2. `ad_style_max` = `00 05 00 ff ff ff ff 00`
has length 5 and the value `00 ff ff ff ff` (4294967295 needs a leading zero octet
in two's complement), then the same closing octet.

### `ih1_min` = `00 00 ff 00 01 00 00 13 00 14 00 00` (indication header format 1, only mandatory fields)

Input:
`ran-function-id` 255, `dapp-id` 1, `node-type` 0, `node-plmn-id` `13 00 14`,
`node-nb-id` 0, no `node-cu-du-id`, no `timestamp`, no `sequence-id`.

Table: Octets of the indication header `ih1_min`

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

### `ih1_full` = `0e 00 ff 00 07 00 02 00 f1 10 20 04 d2 00 05 08 17 97 9c fe 3d 85 cd 15 01 2a`

Same shape, with all three optional fields present. Differences from `ih1_min`:

Table: Octets of the indication header `ih1_full`

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

### `ch1_min` = `00 01 00 02` (control header, no timestamp, no sequence-id)

Input: `ran-function-id` 1, `dapp-id` 2.

Table: Octets of the control header `ch1_min`

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

### `co1_full` = `1c 08 17979cfe3d85cd15 01 2a 0a 10 20 30 40 50 60 70 80 90 a0` (control outcome, everything present)

This is the outcome that carries the Service-Model payload.

Table: Octets of the control outcome `co1_full`

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

Table: Smaller control outcome vectors

| Vector | Hex | Presence bits | Why |
|---|---|---|---|
| `co1_min` | `00` | `000` | acknowledge only: three zero extension bits, three zero presence bits, two pad bits |
| `co1_tsseq` | `18 08 17979cfe3d85cd15 01 2a` | `110` | octet 0 = `000 110 00` |
| `co1_payload` | `04 0a 10 20 30 40 50 60 70 80 90 a0` | `001` | octet 0 = `000 001 00` |

### Two APER layers nest in the control outcome

`e3-control-outcome` is an `OCTET STRING`, and the Spectrum Service Model fills it
with its own APER encoding of `Spectrum-ApplyOutcomeData`
([02-ie-mapping.md](02-ie-mapping.md)). So a received
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

### `im1_small` = `00 00 04 00 03 de ad be ef` (indication message format 1, 4 octets of payload)

Table: Octets of the indication message `im1_small`

| Octet(s) | Bits or bytes | Meaning |
|---|---|---|
| 0 | `0` `0` `0` `0` + `0000` | `E2SM-DAPP-IndicationMessage` extension bit; `ric-indicationMessage-formats` extension bit; index bit `0` = `Format1`; `Format1` extension bit; pad |
| 1-2 | `00 04` | `data-size` = 4. The integer `(0..32768)` has a range above 256, so it takes two aligned octets. |
| 3-4 | `00 03` | `data` length minus 1 = 3, two aligned octets (the size range 1..32768 is above 256 too) |
| 5-8 | `de ad be ef` | `data` |

`data-size` and the octet-string length both carry the payload size.

### `im2_one` = `20 00 01 00 01 00 00 01` (indication message format 2, one dApp, one function)

Table: Octets of the indication message `im2_one`

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

### `fd_min`, the smallest RAN function definition

`00 10 45 32 53 4d 2d 44 41 50 50 00 00 1a 31 2e 33 2e 36 2e 31 2e 34 2e 31 2e 35 33 31 34 38 2e 31 2e 31 2e 32 35 35 2e 33 11 00 44 41 50 50 20 53 65 72 76 69 63 65 20 4d 6f 64 65 6c 20 66 6f 72 20 45 32 2f 45 33 20 62 72 69 64 67 65`

Table: Octets of the RAN function definition `fd_min`

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

Table: Other RAN function definition vectors

| Vector | Octets | Content |
|---|---|---|
| `fd_min` | 78 | name only |
| `fd_report` | 143 | name, report section with two styles (style 2 carries a subscription map of dApps 7 and 9) |
| `fd_ctrl` | 101 | name, event trigger section (empty), control section with one style, no map |
| `fd_full` | 175 | name, empty event trigger section, two report styles (style 2 with a map), one control style with a map |

## Reference test vectors (full list)

The reference test vectors are encoded values with the inputs that produce them. An
implementer uses them to check a codec. Vectors too large to inline carry a length
and an FNV-1a 64-bit hash instead. The offset basis is `14695981039346656037`, and
the prime is `1099511628211`.

Table: Reference test vectors

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

Properties that these vectors pin down:

* The all-ones values (`4294967295`, `INT64_MAX`) and the minimum values (`0`)
  of every bounded and unbounded integer.
* An absent versus a present optional field, in each combination for the outcome.
* The smallest and largest legal payloads of both message formats (the control
  message stops at 16000 because of the buffer limit of one implementation).
* A subscription list of 0, 1, 2, 255 and 256 items.

The vectors do **not** cover: extension bit `1`, a `ranFunction-Instance`, an
`e3-control-outcome` longer than 127 octets (two-octet length form), a PLMN that is
not three bytes, a timestamp with the top bit set, and any invalid input.


## Native types for the grammar types

Implementations hold the grammar types in different native types. This clause lists
the typical choices and the traps. Field names in the C++ and the C structures of a
codec can follow the grammar names exactly, so that the same names work across
implementations.

Table: Typical native types for the fields

| ASN.1 field | Typical native type | Note |
|---|---|---|
| `ran-function-id`, `dapp-id`, `node-nb-id` (0..4294967295) | unsigned 32-bit or 64-bit integer | Use 64 bits when `4294967295` shall stay positive in a signed language. |
| `node-type` (0..255) | unsigned 8-bit integer | |
| `node-plmn-id` (SIZE(3)) | array of 3 octets | |
| `node-cu-du-id` OPTIONAL | presence flag and an unsigned integer | |
| `timestamp` OPTIONAL | presence flag and a signed 64-bit integer | One implementation treats `0` as absent. |
| `sequence-id` OPTIONAL | presence flag and a signed 64-bit integer | One implementation treats `0` as absent. |
| `data-size` (0..32768) | unsigned 16-bit integer | Some implementations drop the field and use the length of `data`. |
| `data` | octet string, size 1..32768 | |
| `e3-control-outcome` OPTIONAL | presence flag and an octet string with no bounds | One implementation treats a null pointer and a size of `0` as absent. |
| `ric-style-type`, format and style types | signed 64-bit integer | The grammar has no bounds for these types. |
| `ranFunction-Instance` OPTIONAL | presence flag and a signed 64-bit integer | One encoder rejects a present instance. |
| the format choice | enumeration | The wire format numbers start at **1**. |

Two traps:

* **A native format enumeration can start at 0 while the wire format numbers start at
  1.** The enumeration value is not the `RIC-Format-Type` that the definition
  carries, which is `1` and `2`. A code that mirrors the enumeration in another
  language shall use the same numbering as the library.
* **A zero-initialized structure can read as a valid format 1.** An all-zero
  indication message reads as "format 1, no data" when the first enumeration value
  is `0`. An encoder should guard against this.

## Checking an encoding

To check a stack, an implementer encodes every input of the reference test vectors
and compares the output with the expected bytes. Then the implementer decodes every
expected value and compares the result with the input. The checklist and the way to
regenerate the vectors are in [integrator-guide.md](integrator-guide.md).

## Abstract syntax

The ASN.1 definition of E2SM-DAPP conforms to ITU-T Rec. X.680 and ITU-T Rec. X.681.
If the ASN.1 definition contradicts a table in this specification, the ASN.1
definition takes precedence.

```asn1
-- ASN1START
E2SM-DAPP-IEs {iso(1) identified-organization(3) dod(6) internet(1) private(4) enterprise(1) oran(53148) e2(1) version1(1) e2sm(255) e2sm-DAPP-IEs (3)}

DEFINITIONS AUTOMATIC TAGS ::=
BEGIN

-- **************************************************************
-- E2SM Common IEs
-- **************************************************************
Bool-Type ::=											    BOOLEAN

RIC-Format-Type ::= 										INTEGER
RIC-Style-Type ::= 											INTEGER
RIC-Style-Name ::= 											PrintableString(SIZE(1..150,...))

-- Nanoseconds since the Unix epoch, from the realtime clock (time_now_ns).
-- OPTIONAL: absent means "no producer reference", not a malformed message. A peer
-- that does not stamp is not an error and the codec must not reject it. Same
-- convention as the E3AP envelope timestamp in libe3.
Timestamp ::= 													INTEGER

-- The correlation id of one detection-to-mitigation procedure. Same identifier
-- as libe3's E3-SequenceID: a dApp assigns it on the report, the RAN relays it
-- here, the xApp echoes it on its control, and the RAN reports it back on the
-- control outcome. Named to match, because they are the same value.
-- OPTIONAL, same convention as Timestamp above: absent (no value encoded) means
-- no producer reference, not a malformed message. 0 is never assigned by any
-- producer in this codebase, so it is safe to use as the IR struct's own
-- "absent" sentinel alongside the OPTIONAL ASN.1 encoding.
SequenceId ::= 													INTEGER

RANfunction-Name ::= 										SEQUENCE{
    															ranFunction-ShortName						PrintableString(SIZE(1..150,...)),
    															ranFunction-E2SM-OID						PrintableString(SIZE(1..1000,...)),
    															ranFunction-Description						PrintableString(SIZE(1..150,...)),
    															ranFunction-Instance						INTEGER															OPTIONAL,
    															...
}




-- **************************************************************
-- CONSTANTS
-- **************************************************************

maxE2SMDAPPIndMsgSize                                       INTEGER ::= 32768
maxE2SMDAPPCtrlMsgSize                                      INTEGER ::= 32768
maxnoofDApps                                                INTEGER ::= 256
maxnoofSubscribedE3RANFunctions                             INTEGER ::= 64
maxnoofDAPPReportStyles                                     INTEGER ::= 2
maxnoofDAPPControlStyles                                    INTEGER ::= 1

-- **************************************************************
-- Event Trigger OCTET STRING contents
-- **************************************************************

E2SM-DAPP-EventTrigger ::= 									SEQUENCE {
																ric-eventTrigger-formats CHOICE {
																	eventTrigger-Format1					E2SM-DAPP-EventTrigger-Format1,
																	...
																},
																...
}

E2SM-DAPP-EventTrigger-Format1 ::= 							SEQUENCE {

}


-- **************************************************************
-- Action Definition OCTET STRING contents
-- **************************************************************

E2SM-DAPP-ActionDefinition ::=                              SEQUENCE {
                                                                ric-Style-Type                              RIC-Style-Type,
                                                                actionDefinition-formats CHOICE {
                                                                    actionDefinition-Format1                E2SM-DAPP-ActionDefinition-Format1,
                                                                    ...
                                                                },
                                                                ...
}

E2SM-DAPP-ActionDefinition-Format1 ::=                      SEQUENCE {

}



-- **************************************************************
-- IEs
-- **************************************************************

RANFunctionDefinition-Report-Item ::= 						SEQUENCE {
																ric-ReportStyle-Type                 		RIC-Style-Type,
																ric-ReportStyle-Name                 		RIC-Style-Name,
																ric-IndicationHeaderFormat-Type      		RIC-Format-Type,
																ric-IndicationMessageFormat-Type     		RIC-Format-Type,
																dappE3Subscriptions                         DAppE3Subscription-List                           				OPTIONAL,
																...
}

RANFunctionDefinition-Control-Item ::= 						SEQUENCE {
																ric-ControlStyle-Type         				RIC-Style-Type,
																ric-ControlStyle-Name         				RIC-Style-Name,
																ric-ControlHeaderFormat-Type  				RIC-Format-Type,
																ric-ControlMessageFormat-Type 				RIC-Format-Type,
																ric-ControlOutcomeFormat-Type 				RIC-Format-Type,
																dappE3Subscriptions                         DAppE3Subscription-List                           				OPTIONAL,
																...
}




-- **************************************************************
-- Indication Header OCTET STRING contents
-- **************************************************************

E2SM-DAPP-IndicationHeader ::= 								SEQUENCE {
																ric-indicationHeader-formats CHOICE {
																		indicationHeader-Format1			E2SM-DAPP-IndicationHeader-Format1,
																		indicationHeader-Format2			E2SM-DAPP-IndicationHeader-Format2,
																		...
																	},
																	...
}

E2SM-DAPP-IndicationHeader-Format1 ::=                      SEQUENCE {
                                                                ran-function-id      						INTEGER (0..4294967295, ...),
                                                                dapp-id              						INTEGER (0..4294967295, ...),
																node-type            						INTEGER (0..255, ...),
																node-plmn-id         						OCTET STRING (SIZE(3)),
																node-nb-id           						INTEGER (0..4294967295, ...),
																node-cu-du-id        						INTEGER (0..4294967295, ...) 									OPTIONAL,
																timestamp            						Timestamp OPTIONAL,
															sequence-id          						SequenceId OPTIONAL,
                                                                ...
}

E2SM-DAPP-IndicationHeader-Format2 ::=                      SEQUENCE {
																node-type            						INTEGER (0..255, ...),
																node-plmn-id         						OCTET STRING (SIZE(3)),
																node-nb-id           						INTEGER (0..4294967295, ...),
																node-cu-du-id        						INTEGER (0..4294967295, ...) 									OPTIONAL,
																timestamp            						Timestamp OPTIONAL,
															sequence-id          						SequenceId OPTIONAL,
                                                                ...
}




-- **************************************************************
-- Indication Message OCTET STRING contents
-- **************************************************************

E2SM-DAPP-IndicationMessage ::= 							SEQUENCE {
																ric-indicationMessage-formats CHOICE {
																	indicationMessage-Format1				E2SM-DAPP-IndicationMessage-Format1,
																	indicationMessage-Format2				E2SM-DAPP-IndicationMessage-Format2,
																	...
																},
																...
}

E2SM-DAPP-IndicationMessage-Format1 ::=                     SEQUENCE {
                                                                data-size   								INTEGER (0..maxE2SMDAPPIndMsgSize),
                                                                data        								OCTET STRING (SIZE(1..maxE2SMDAPPIndMsgSize)),
                                                                ...
}

E2SM-DAPP-IndicationMessage-Format2 ::=                     SEQUENCE {
                                                                dappE3Subscriptions     					DAppE3Subscription-List,
                                                                ...
}




-- **************************************************************
-- Control Header OCTET STRING contents
-- **************************************************************

E2SM-DAPP-ControlHeader ::= 								SEQUENCE {
																ric-controlHeader-formats CHOICE {
																	controlHeader-Format1					E2SM-DAPP-ControlHeader-Format1,
																	...
																},
																...
}

E2SM-DAPP-ControlHeader-Format1 ::= 						SEQUENCE {
	                                                            ran-function-id      						INTEGER (0..4294967295, ...),
                                                                dapp-id              						INTEGER (0..4294967295, ...),
																timestamp            						Timestamp OPTIONAL,
															sequence-id          						SequenceId OPTIONAL,
                                                                ...
}




-- **************************************************************
-- Control Message OCTET STRING contents
-- **************************************************************

E2SM-DAPP-ControlMessage ::= 								SEQUENCE {
																ric-controlMessage-formats CHOICE {
																	controlMessage-Format1					E2SM-DAPP-ControlMessage-Format1,
																	...
																},
																...
}

E2SM-DAPP-ControlMessage-Format1 ::= 						SEQUENCE {
                                                                data-size   								INTEGER (0..maxE2SMDAPPCtrlMsgSize),
                                                                data        								OCTET STRING (SIZE(1..maxE2SMDAPPCtrlMsgSize)),
                                                                ...
}




-- **************************************************************
-- Control Outcome OCTET STRING contents
-- **************************************************************

E2SM-DAPP-ControlOutcome ::= 								SEQUENCE {
																ric-controlOutcome-formats CHOICE {
																	controlOutcome-Format1					E2SM-DAPP-ControlOutcome-Format1,
																	...
																},
																...
}

-- e3-control-outcome carries the Service Model's own account of what applying
-- the control did, encoded by the SM named by the control's ran-function-id and
-- opaque here. It is the same tunnelling that
-- E2SM-DAPP-ControlMessage-Format1 already does for the control itself.
-- "Applied" means something different for every SM, so this layer defines the
-- envelope and not the meaning.
-- OPTIONAL: an outcome that only acknowledges, with nothing to report about the
-- application, omits it.
E2SM-DAPP-ControlOutcome-Format1 ::= 						SEQUENCE {
																timestamp            						Timestamp OPTIONAL,
															sequence-id          						SequenceId OPTIONAL,
																e3-control-outcome					OCTET STRING OPTIONAL,
																...
}


-- **************************************************************
-- dApp E3 Subscription Info
-- **************************************************************
SubscribedE3RANFunction-ID ::=                              INTEGER 										(0..4294967295, ...)

DAppE3Subscription-Item ::=                                 SEQUENCE {
                                                                dapp-id                 					INTEGER (0..4294967295, ...),
                                                                subscribed-e3-ran-functions 				SEQUENCE (SIZE(1..maxnoofSubscribedE3RANFunctions)) OF SubscribedE3RANFunction-ID,
                                                                ...
}

DAppE3Subscription-List ::=                                   SEQUENCE 										(SIZE(0..maxnoofDApps)) OF DAppE3Subscription-Item

-- **************************************************************
-- RAN Function Definition IEs
-- **************************************************************

E2SM-DAPP-RANFunctionDefinition ::= 						SEQUENCE{
																ranFunction-Name							RANfunction-Name,
																ranFunctionDefinition-EventTrigger			RANFunctionDefinition-EventTrigger								OPTIONAL,
																ranFunctionDefinition-Report				RANFunctionDefinition-Report									OPTIONAL,
																ranFunctionDefinition-Control				RANFunctionDefinition-Control									OPTIONAL,
																...
}




-- **************************************************************
-- Event Trigger
-- **************************************************************

RANFunctionDefinition-EventTrigger ::= 						SEQUENCE {
																...
}




-- **************************************************************
-- Report
-- **************************************************************

RANFunctionDefinition-Report ::= 							SEQUENCE {
    															ric-ReportStyle-List 						SEQUENCE (SIZE(1..maxnoofDAPPReportStyles)) OF RANFunctionDefinition-Report-Item,
    															...
}




-- **************************************************************
-- Control
-- **************************************************************

RANFunctionDefinition-Control ::= 							SEQUENCE {
    															ric-ControlStyle-List 						SEQUENCE (SIZE(1..maxnoofDAPPControlStyles)) OF RANFunctionDefinition-Control-Item,
    															...
}




END
-- ASN1STOP
```
