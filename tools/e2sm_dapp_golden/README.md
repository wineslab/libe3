# E2SM-DAPP golden vectors

`tests/e2sm_dapp_golden.hpp` holds the APER bytes that flexric's own encoder
(`dapp_enc_*_asn()`) produces for every input listed in `gen_golden.c`. They are
the byte-compatibility contract between flexric, libe3 and OCUDU: libe3's codec
must reproduce each one exactly (`tests/test_e2sm_dapp_encode.cpp`) and decode it
back to an equal struct (`tests/test_e2sm_dapp_decode.cpp`). The header names the
flexric commit it came from.

## Regenerate

```sh
tools/e2sm_dapp_golden/regen.sh <flexric-checkout> <asn1c-binary> <gcc> [output-file]
```

* `<flexric-checkout>` must be unmodified under `src/` (the script refuses
  otherwise; `ALLOW_DIRTY=1` overrides). It is only read.
* `<asn1c-binary>` is the asn1c that libe3 itself builds with.
* `<gcc>` must be a real GCC, for example `gcc-15` from Homebrew: flexric's
  `defer` macro needs GCC nested functions, which Apple clang cannot compile.

With no output file the header is written to stdout. To check that the committed
header is reproducible:

```sh
tools/e2sm_dapp_golden/regen.sh ~/flexric /opt/asn1c/bin/asn1c gcc-15 /tmp/golden.hpp
cmp /tmp/golden.hpp tests/e2sm_dapp_golden.hpp
```

The script copies the needed parts of flexric's `dapp_sm` into a scratch tree,
drops four unused includes that pull in other service models, regenerates the
asn1c code from flexric's grammar, compiles `gen_golden.c` against flexric's real
encoder and decoder (which also checks that flexric's decoder returns every
input), and pipes the output through `gen_header.py`.

`gen_header.py <golden.txt> <flexric-commit> <namespace> [ocudu]` turns the
`GOLDEN` / `GOLDEN_HASH` lines into the header; the `ocudu` argument emits the
SPDX header and namespace OCUDU's copy needs.

## Changing the inputs

`tests/e2sm_dapp_fixtures.inc` rebuilds every input of `gen_golden.c` in C++, and
a byte-identical copy lives in OCUDU. Change the inputs in all three places
together, then regenerate.
