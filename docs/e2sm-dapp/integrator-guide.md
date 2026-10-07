# E2SM-DAPP: integrator guide

For an engineer adding E2SM-DAPP to a RAN stack. OCUDU did it. OAI has it through
its flexric submodule. A future stack may follow. This guide is about the codec and
about proving that your stack produces and accepts the same bytes as the others. It
is not part of the specification: the specification is the six numbered files in
[README.md](README.md).

What you do **not** need: the real E3 link and the Spectrum service-model codec. They
are out of scope of the E2SM-DAPP codec. The codec packs and unpacks E2SM-DAPP
elements and nothing else; the payloads inside (`data`, `e3-control-outcome`) are
opaque octets to it.

## Choose a route

| Route | You get | You need | Pick it when |
|---|---|---|---|
| **A. Link libe3's codec** | C++ and C APIs, already tested against the golden vectors | libe3 built with `LIBE3_ENABLE_E2SM_DAPP` (needs `LIBE3_ENABLE_ASN1`), and the asn1c fork it builds with | your stack can link a C++17 library, or call C |
| **B. Generate your own asn1c code** from the grammar | the asn1c C structs, nothing else | the grammar file and the pinned asn1c | you want only the ASN.1 layer and will write the glue yourself |
| **C. Write a codec by hand** (what OCUDU did) | full control, no asn1c | a bit-level APER library | your stack already has one and forbids a second ASN.1 runtime |

Whichever you choose, **the golden vectors are the acceptance test**
([below](#the-contract-the-golden-vectors)).

## Route A: link libe3

### Build and install libe3

```sh
cmake -S . -B build -DLIBE3_ENABLE_ASN1=ON -DLIBE3_ENABLE_E2SM_DAPP=ON
cmake --build build
cmake --install build --prefix /opt/libe3
```

`LIBE3_ENABLE_E2SM_DAPP` is `ON` by default and is forced `OFF` when
`LIBE3_ENABLE_ASN1` is `OFF` (`cmake/libe3Options.cmake:26-30`). The codec does not need
the E3 agent, ZeroMQ or the E3AP grammar: it has its own private copy of the asn1c
runtime, with the runtime symbols hidden, so it can sit next to another asn1c in the
same process (`messages/CMakeLists.txt:74-105`).

### Use it from CMake

```cmake
find_package(libe3 CONFIG REQUIRED COMPONENTS e2sm_dapp)

add_executable(my_ran main.cpp)
target_link_libraries(my_ran PRIVATE libe3::e2sm_dapp)          # static
# or:
target_link_libraries(my_ran PRIVATE libe3::e2sm_dapp_shared)   # shared
```

`libe3::e2sm_dapp` (static) and `libe3::e2sm_dapp_shared` are the two targets.
The component is "found" exactly when the install was built with the codec; without
it, `find_package(... REQUIRED COMPONENTS e2sm_dapp)` fails with a message that says
how to rebuild (`cmake/libe3Config.cmake.in:30-50`). The compile definition
`LIBE3_ENABLE_E2SM_DAPP` comes with the target.

pkg-config: `pkg-config --cflags --libs libe3_e2sm_dapp`, and
`pkg-config --variable=asndir libe3_e2sm_dapp` for the grammar directory
(`cmake/libe3_e2sm_dapp.pc.in`). The out-of-tree consumer checks in `tests/consume/`
(`e2sm_dapp.cpp`, `e2sm_dapp_c.c`) show both routes against an install prefix.

### C++: pack an indication header and indication message

```cpp
#include <libe3/e2sm_dapp.hpp>

using namespace libe3::e2sm_dapp;

// Indication header, format 1: one dApp report.
e2sm_dapp_ind_hdr_s hdr;
auto& h = hdr.ric_ind_hdr_formats.set_ind_hdr_format1();
h.ran_function_id     = 1;            // the E3 RAN function of the report
h.dapp_id             = 7;
h.node_type           = 2;
h.node_plmn_id[0] = 0x00; h.node_plmn_id[1] = 0xf1; h.node_plmn_id[2] = 0x10;
h.node_nb_id          = 1234;
h.timestamp_present   = true;
h.timestamp           = now_ns;       // CLOCK_REALTIME, nanoseconds
h.sequence_id_present = true;
h.sequence_id         = seq;          // the dApp's E3-SequenceID

std::vector<uint8_t> hdr_bytes;
if (hdr.pack(hdr_bytes) != ASN_SUCCESS) { /* a value outside the grammar */ }

// Indication message, format 1: the opaque report.
e2sm_dapp_ind_msg_s msg;
auto& m = msg.ric_ind_msg_formats.set_ind_msg_format1();
m.data.assign(report.begin(), report.end());   // 1..32768 octets
m.data_size = static_cast<uint16_t>(report.size());   // keep equal to data.size()

std::vector<uint8_t> msg_bytes;
if (msg.pack(msg_bytes) != ASN_SUCCESS) { /* empty or oversized payload */ }
// hdr_bytes and msg_bytes are the contents of the E2AP RIC Indication Header and Message.
```

### C++: unpack a control and read it

```cpp
e2sm_dapp_ctrl_hdr_s ch;
if (ch.unpack(hdr_bytes.data(), hdr_bytes.size()) != ASN_SUCCESS) { return; }
const auto& f = ch.ric_ctrl_hdr_formats.ctrl_hdr_format1();
uint64_t e3_function = f.ran_function_id;
uint64_t dapp        = f.dapp_id;
bool     have_seq    = f.sequence_id_present;     // absent is normal
int64_t  seq         = f.sequence_id;

e2sm_dapp_ctrl_msg_s cm;
if (cm.unpack(msg_bytes.data(), msg_bytes.size()) != ASN_SUCCESS) { return; }
const auto& payload = cm.ric_ctrl_msg_formats.ctrl_msg_format1().data;   // opaque, for the service model
```

The choice getters throw `std::bad_variant_access` when the other alternative is
held. For the two-format elements (indication header and message) test `type()`
first:

```cpp
using types = e2sm_dapp_ind_hdr_s::ric_ind_hdr_formats_c_::types;
if (hdr.ric_ind_hdr_formats.type() == types::ind_hdr_format1) { /* ... */ }
```

### C++: build the control outcome (the RAN side of an acknowledge)

```cpp
e2sm_dapp_ctrl_outcome_s out;
auto& o = out.ric_ctrl_outcome_formats.ctrl_outcome_format1();
o.timestamp_present   = true;  o.timestamp   = built_ns;
o.sequence_id_present = true;  o.sequence_id = seq;          // from the control header
o.e3_ctrl_outcome_present = true;
o.e3_ctrl_outcome.from_bytes(sm_payload.data(), sm_payload.size());  // the service model's PDU
std::vector<uint8_t> bytes;
out.pack(bytes);   // the RIC Control Outcome of the deferred RIC Control Acknowledge
```

Leave `e3_ctrl_outcome_present` false for an acknowledge with nothing to say.

### C: the same, from C

The C API is in `<libe3/e2sm_dapp_c.h>`. Every optional field has an explicit
`has_*` flag, and `0` is a normal value.

```c
#include <libe3/e2sm_dapp_c.h>

libe3_e2sm_dapp_ctrl_hdr_t hdr = {0};
hdr.format          = LIBE3_E2SM_DAPP_CTRL_HDR_FORMAT_1;
hdr.ran_function_id = 1;
hdr.dapp_id         = 2;
hdr.has_sequence_id = true;
hdr.sequence_id     = seq;

libe3_e2sm_dapp_bytes_t bytes = {0, 0};
if (libe3_e2sm_dapp_encode_ctrl_hdr(&hdr, &bytes) != E3_SUCCESS) { /* error */ }
/* send bytes.data, bytes.len; then: */
libe3_e2sm_dapp_bytes_free(&bytes);

/* decoding */
libe3_e2sm_dapp_ctrl_hdr_t back = {0};
if (libe3_e2sm_dapp_decode_ctrl_hdr(buf, len, &back) == E3_SUCCESS) {
    /* use back */
    libe3_e2sm_dapp_free_ctrl_hdr(&back);
}
```

Ownership rules (`include/libe3/e2sm_dapp_c.h:10-20`): inputs to `encode_*` stay yours.
`encode_*` allocates `out->data`; release it with `libe3_e2sm_dapp_bytes_free` (plain
`free` also works). `decode_*` allocates everything inside `*out`; release with the
matching `libe3_e2sm_dapp_free_*`. On failure `*out` is zeroed and needs no freeing,
and every `free` function zeroes its argument, so freeing twice is harmless. Errors are
`E3_INVALID_PARAM`, `E3_ENCODE_FAILED`, `E3_DECODE_FAILED` and `E3_SM_ERROR_MEMORY`.
Unlike flexric's C IR, **nothing aborts**.

## Route B: run asn1c yourself

The grammar is `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn` in the libe3 source,
and `share/libe3/e2sm_dapp/e2sm_dapp-1.0.0.asn` in an install
(`cmake/libe3Install.cmake:160-161`). It is a byte-identical copy of flexric's
`src/sm/dapp_sm/ie/asn/e2sm_dapp_v0_standard.asn`. **Do not edit it.**

### The asn1c you must use

libe3 builds with the fork `mouse07410/asn1c`, pinned to commit
`940dd5fa9f3917913fd487b13dfddfacd0ded06e` (`build_libe3:195-227`, function
`install_asn1c_from_source`):

```sh
git clone https://github.com/mouse07410/asn1c /tmp/asn1c
cd /tmp/asn1c && git checkout 940dd5fa9f3917913fd487b13dfddfacd0ded06e
./configure --prefix /opt/asn1c/ && make && sudo make install
```

(`build_libe3` does this for you when the pinned commit is not installed.) A different
asn1c can produce a different APER encoding for the same grammar, in particular for
extension handling and unconstrained integers. Check against the golden vectors if you
use another.

### The exact flags

From `messages/CMakeLists.txt:91`:

```sh
asn1c -pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER \
      -fno-include-deps -fcompound-names -findirect-choice -no-gen-example \
      -D <output-dir> e2sm_dapp-1.0.0.asn
```

flexric uses a slightly different line, with `-no-gen-UPER` and without `-pdu=all`
(`src/sm/dapp_sm/ie/asn/CMakeLists.txt`). The APER rules are the same; confirm with the
golden vectors.

### Compile definitions and layout

Compile the generated `*.c` files and the asn1c runtime it emits (the list is in
`messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.cmake`: runtime support first, then the
protocol types) with `-DASN_DISABLE_OER_SUPPORT`. libe3 also compiles them `-w`
and with hidden symbol visibility, so the runtime does not clash with another asn1c in
the process. Encode with `ATS_ALIGNED_BASIC_PER` and decode with `aper_decode`.

Pitfalls when you call asn1c directly (all seen in flexric's code, see
[06-discrepancies.md](06-discrepancies.md)):

* asn1c does not reject an oversized octet string cleanly; it can fault inside the
  encoder. Check `1..32768` yourself first.
* Check constraints (`asn_check_constraints`) before encoding. Do not rely on the
  encoder to refuse out-of-range values.
* Do not `assert` on `RC_OK`. Return an error: the bytes come from the network.
* Free the decoded structure on every path, including a failed decode: asn1c leaves a
  half-built structure behind.
* The unbounded integers (`Timestamp`, `SequenceId`, `RIC-Style-Type`) are `long`
  in the generated structs. They are 64-bit on LP64 and 32-bit on 32-bit targets.

## Route C: a hand-written codec (OCUDU)

OCUDU writes the structs in the style of its other E2SM headers
(`ocudu/include/ocudu/asn1/e2sm/e2sm_dapp.h`) and implements `pack` and `unpack` on its
`bit_ref` and `cbit_ref` (`ocudu/lib/asn1/e2sm/e2sm_dapp.cpp`). Things it had to handle
by hand, which any hand-written codec must:

* **Unconstrained integers** use the fewest two's-complement octets, with an
  octet-aligned length. A `SequenceId` of `128` is two octets, `00 80`. The generic
  OCUDU helpers mis-sized the exact values `2^(8k-1)`, so it has its own `pack_i64`
  and `unpack_i64`.
* **`INTEGER (0..4294967295, ...)`** is an extension bit, a 2-bit octet count minus one,
  alignment, then 1 to 4 octets.
* **The extension bit** on a `SEQUENCE` set by a peer: read and skip the additions
  as open types (`skip_ext_additions`).
* **`PrintableString`** alphabet and length are checked by the codec, because the
  generic string utilities check neither.

## The OCUDU to libe3 C++ interface

libe3's `include/libe3/e2sm_dapp.hpp` deliberately copies OCUDU's
`asn1::e2sm_dapp`: same type names, same field names, so a message-building function
reads the same in both. The differences are small and mechanical.

| Item | OCUDU (`asn1::e2sm_dapp`) | libe3 (`libe3::e2sm_dapp`) |
|---|---|---|
| Namespace | `asn1::e2sm_dapp` | `libe3::e2sm_dapp` |
| Struct and field names | the same | the same |
| Constants | `max_e2sm_dapp_ind_msg_size`, `maxnoof_dapps`, ... (`int64_t`) | the same names (`int64_t`) |
| `pack` | on **every** struct: `OCUDUASN_CODE pack(bit_ref&) const` | only on the **eight top-level types**: `asn_code pack(std::vector<uint8_t>&) const` |
| `unpack` | `OCUDUASN_CODE unpack(cbit_ref&)` | `asn_code unpack(const uint8_t* data, size_t len)` |
| Result type | `OCUDUASN_CODE`: `OCUDUASN_SUCCESS`, `OCUDUASN_ERROR_DECODE_FAIL`, `OCUDUASN_ERROR_ENCODE_FAIL` | `asn_code` with the same values: `ASN_SUCCESS` (0), `ASN_ERROR_DECODE_FAIL` (-1), `ASN_ERROR_ENCODE_FAIL` (-2) |
| Top-level types | no distinction: every struct has `pack` and `unpack`. The eight below are the ones whose bytes go into an E2AP field | `e2sm_dapp_event_trigger_s`, `e2sm_dapp_action_definition_s`, `e2sm_dapp_ind_hdr_s`, `e2sm_dapp_ind_msg_s`, `e2sm_dapp_ctrl_hdr_s`, `e2sm_dapp_ctrl_msg_s`, `e2sm_dapp_ctrl_outcome_s`, `e2sm_dapp_ran_function_definition_s` |
| Octet-string helper | OCUDU's `bounded_octstring`, `fixed_octstring`, `unbounded_octstring` | thin wrappers: `bounded_octstring` is a `std::vector<uint8_t>`, `fixed_octstring<N>` a `std::array<uint8_t, N>`, `unbounded_octstring` a vector with `from_bytes(ptr, len)` (OCUDU's `from_bytes` takes a `span`) |
| String helper | `printable_string<LB, UB, Ext, Aligned>` | the same template over `std::string`, with `from_string` and `to_string` |
| Lists | `dyn_array`, `bounded_array` | the same names, deriving from `std::vector` |
| `to_json` | on every struct | **not provided** |
| `operator==` | **not provided** | provided on every type (the `ext` member is never compared; an absent optional field compares equal whatever its value) |
| Choice getters | `assert_choice_type`: a wrong alternative is an assertion | `std::get`: a wrong alternative throws `std::bad_variant_access` |
| Choice setters | `set_ind_hdr_format1()` and the like | the same |
| `ext` member | present; `pack` fails when it is `true` | present; ignored by `pack`, always written `0` |

A function written for one compiles for the other if it avoids `to_json`, the getters
on an unchecked choice, and `pack` of a non-top-level struct. The shared test fixtures
rely on exactly that.

## The contract: the golden vectors

`tests/e2sm_dapp_golden.hpp` holds the APER bytes of 31 inputs
([04-encoding.md](04-encoding.md#4-golden-vectors-full-list)), generated by flexric's
own encoder at the flexric commit named in the file. It is **generated; do not edit it
by hand**. libe3 and OCUDU each carry the same table (`ocudu/tests/unittests/e2/e2sm_dapp_golden.h`).
If your stack reproduces every vector and decodes every vector to an equal value, it is
byte-compatible with flexric, libe3 and OCUDU.

### The shared test-fixture technique

The inputs are not described by hand in each repository. Each stack builds them through
**the same code**:

* `tests/e2sm_dapp_fixtures.inc` in libe3 and
  `tests/unittests/e2/e2sm_dapp_fixtures.inc` in OCUDU are **byte-identical**. They
  hold one builder function per vector, named after the vector (`ih1_min()`,
  `co1_full()`, ...), written against the shared type and field names.
* The including file defines three things first, so the same body compiles in either
  repository:

  | Macro | Meaning |
  |---|---|
  | `E2SM_DAPP_NS` | the namespace that holds the types |
  | `E2SM_DAPP_SET_STRING(dst, "text")` | assign a `PrintableString` |
  | `E2SM_DAPP_SET_OCTETS(dst, ptr, n)` | assign an unbounded `OCTET STRING` |

* A test then does, for each vector: build the input with the fixture, `pack`, compare
  with the golden hex; and `unpack` the golden hex, compare with the fixture.
* The flexric side of the same table is `tools/e2sm_dapp_golden/gen_golden.c`, which
  builds the same inputs in C. The three places are changed together
  (`tools/e2sm_dapp_golden/README.md`, section "Changing the inputs").

To add E2SM-DAPP to a new stack, copy `e2sm_dapp_fixtures.inc` and
`e2sm_dapp_golden.hpp`, define the three macros, and write the two loops. If your
stack's types differ from the shared names, add thin adapters. Do not change the
fixtures.

### Reproducing the golden vectors

```sh
tools/e2sm_dapp_golden/regen.sh <flexric-checkout> <asn1c-binary> <gcc> [output-file]
```

* `<flexric-checkout>`: an unmodified flexric checkout that carries the `dapp_sm` service model. The script reads it
  only, and refuses a checkout with local changes under `src/` (`ALLOW_DIRTY=1`
  overrides).
* `<asn1c-binary>`: the pinned asn1c above.
* `<gcc>`: a real GCC, for example `gcc-15` from Homebrew. flexric's `defer` macro
  needs GCC nested functions, which Apple clang cannot compile.

What it does (header comment of `regen.sh` and `tools/e2sm_dapp_golden/README.md`): copies
the parts of flexric's `dapp_sm` the codec needs into a scratch directory, deletes four
unused includes (`enc_cell_global_id.h`, `enc_ue_id.h`, `dec_ue_id.h`,
`dec_cell_global_id.h`) that pull in other service models, regenerates the asn1c code
from flexric's grammar, compiles `gen_golden.c` against flexric's real
`dapp_enc_*_asn()` and `dapp_dec_*_asn()`, runs it (it also checks flexric's decoder
returns every input), and turns the `GOLDEN` and `GOLDEN_HASH` lines into the header
with `gen_header.py`.

To check the committed header is reproducible:

```sh
tools/e2sm_dapp_golden/regen.sh ~/flexric /opt/asn1c/bin/asn1c gcc-15 /tmp/golden.hpp
cmp /tmp/golden.hpp tests/e2sm_dapp_golden.hpp
```

## Checklist for a new stack

1. **Identity.** Register RAN function `255`, revision `1`, OID
   `1.3.6.1.4.1.53148.1.1.255.3`
   ([01-identity-and-registration.md](01-identity-and-registration.md#4-what-must-be-true-for-a-ran-to-announce-e2sm-dapp)).
2. **Grammar.** Use the grammar unchanged: `diff` your copy against
   `messages/asn1/e2sm_dapp/V1/e2sm_dapp-1.0.0.asn`.
3. **Encode every vector.** Build each input with the shared fixtures, pack, compare
   with the hex of `tests/e2sm_dapp_golden.hpp`. All 27 inline vectors byte for byte;
   the four large ones by length and FNV-1a 64-bit hash.
4. **Decode every vector.** Unpack each hex, compare with the fixture.
5. **Round trip.** `unpack(pack(x)) == x` for every fixture.
6. **Refuse bad input without aborting.** At minimum: an empty payload, a payload of
   32769 octets, an id above `4294967295`, a subscription item with no functions, a
   list of 257 items, a truncated buffer, and a choice index beyond the defined formats
   must return errors.
7. **Absent is not an error.** Decode a header with no `timestamp` and no `sequence-id`
   (`ih1_min`, `ch1_min`) and an outcome with nothing (`co1_min`). Your code must treat
   them as absent, and must keep a present `0` distinct from absent.
8. **Payloads stay opaque.** `data` and `e3-control-outcome` come out byte-identical to
   what went in (`im1_max`, `cm1_large`, `co1_full`).
9. **Deferred acknowledge.** Send the RIC Control Acknowledge when the mitigation lands,
   not when the control request arrives, and carry the control's `sequence-id` and
   the service model's outcome
   ([03-procedure-flow.md](03-procedure-flow.md#63-the-e2-acknowledge-is-deferred-to-b19-not-sent-at-b7)).
   Answer at once, without applying, when the control has no `sequence-id`.
10. **Clocks.** Stamp with `CLOCK_REALTIME` nanoseconds on the same clock the dApp uses
    for its reports
    ([05-semantics-and-constraints.md](05-semantics-and-constraints.md#3-clocks-and-units)).
11. **Interoperate with `xdevsm`.** If its xApps are to work against your RAN, name the
    control style `DAPP-CONTROL-STYLE-1` (OCUDU currently names it `dApp Control`, so
    `xdevsm` refuses it), accept a RIC Control Request that asks for an
    acknowledge, and accept a call process id you ignore.
12. **Do not abort the RAN** on a malformed message from the RIC.

## What is deliberately not here

* The real E3 link (libe3's E3 agent and its transports). The E2SM-DAPP codec has no
  dependency on it.
* The Spectrum service-model codec. `data` and `e3-control-outcome` are opaque to
  E2SM-DAPP; a service model has its own grammar and its own codec
  ([02-ie-mapping.md](02-ie-mapping.md#8-the-e3-control-outcome-payload)).
* A RIC-side or xApp-side E2SM-DAPP framework. `xdevsm` is the existing one, built on
  flexric's codec.
