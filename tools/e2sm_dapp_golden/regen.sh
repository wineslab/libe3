#!/bin/sh
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
#
# Regenerate tests/e2sm_dapp_golden.hpp from an unmodified flexric checkout.
#
# usage: regen.sh <flexric-checkout> <asn1c-binary> <gcc> [output-file]
#
# The header goes to stdout unless an output file is given. Progress goes to stderr.
# <gcc> must be a real GCC (e.g. gcc-15 from Homebrew): flexric's `defer` macro
# needs GCC nested functions, which Apple clang cannot compile. <asn1c-binary>
# is the asn1c that generates the grammar's C code (the mouse07410 fork libe3
# builds with). Set ALLOW_DIRTY=1 to run on a checkout with local changes.
#
# What it does: copy the parts of flexric's dapp_sm that the encoder and decoder
# need into a scratch tree (leaving flexric untouched), generate the asn1c code
# for flexric's own grammar, compile gen_golden.c against flexric's real
# dapp_enc_*_asn() / dapp_dec_*_asn(), run it, and turn its output into the
# header with gen_header.py. The header names the flexric commit it came from.
set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 <flexric-checkout> <asn1c-binary> <gcc> [output-file]" >&2
    exit 2
fi

here=$(cd "$(dirname "$0")" && pwd)
flexric=$(cd "$1" && pwd)
asn1c=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
gcc=$3
out=${4:-}

commit=$(git -C "$flexric" rev-parse HEAD)
if [ -z "${ALLOW_DIRTY:-}" ] && [ -n "$(git -C "$flexric" status --porcelain --untracked-files=no -- src)" ]; then
    echo "error: $flexric has local changes under src/; golden vectors must come from an unmodified checkout (ALLOW_DIRTY=1 to override)" >&2
    exit 1
fi
[ -x "$asn1c" ] || { echo "error: asn1c not executable: $asn1c" >&2; exit 1; }
command -v "$gcc" >/dev/null 2>&1 || { echo "error: compiler not found: $gcc" >&2; exit 1; }

work=$(mktemp -d "${TMPDIR:-/tmp}/e2sm_dapp_golden.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM

echo "flexric commit $commit" >&2

# 1. Copy what the codec needs, keeping the relative layout under src/.
src="$work/src"
mkdir -p "$src/sm" "$src/util/alg_ds/alg" "$src/lib/sm/ie"
cp -R "$flexric/src/sm/dapp_sm" "$src/sm/dapp_sm"
cp "$flexric/src/util/byte_array.c" "$flexric/src/util/byte_array.h" "$src/util/"
cp "$flexric/src/util/alg_ds/alg/defer.c" "$flexric/src/util/alg_ds/alg/defer.h" "$src/util/alg_ds/alg/"
cp "$flexric/src/lib/sm/ie/ran_function_name.c" "$flexric/src/lib/sm/ie/ran_function_name.h" "$src/lib/sm/ie/"

# 2. Drop the committed asn1c output: only the grammar stays, the code is regenerated below.
asn="$src/sm/dapp_sm/ie/asn"
find "$asn" -type f ! -name '*.asn' -delete

# 3. The codec includes four headers it does not use, which pull in other service models.
drop_include() { # <file> <header-name>...
    f=$1
    shift
    for h in "$@"; do
        sed "/#include .*\\/$h\"/d" "$f" > "$f.tmp" && mv "$f.tmp" "$f"
    done
}
drop_include "$src/sm/dapp_sm/enc/dapp_enc_asn.c" enc_cell_global_id.h enc_ue_id.h
drop_include "$src/sm/dapp_sm/dec/dapp_dec_asn.c" dec_ue_id.h dec_cell_global_id.h

# 4. Generate the ASN.1 code from flexric's grammar.
echo "generating ASN.1 code" >&2
(cd "$asn" && "$asn1c" -no-gen-BER -no-gen-UPER -no-gen-OER -no-gen-JER -fcompound-names \
    -no-gen-example -findirect-choice -fno-include-deps -D . e2sm_dapp_v0_standard.asn >"$work/asn1c.log" 2>&1) \
    || { cat "$work/asn1c.log" >&2; exit 1; }

# 5. Build gen_golden against flexric's own encoder and decoder.
echo "compiling gen_golden" >&2
# (Homebrew gcc on macOS prints one assembler deployment-target warning per file; it goes to the log.)
(cd "$work" && "$gcc" -std=gnu11 -w -DASN_DISABLE_OER_SUPPORT -DASN_DISABLE_JER_SUPPORT \
    -I src -I src/sm/dapp_sm/ie/asn \
    -o gen_golden "$here/gen_golden.c" \
    src/util/byte_array.c src/util/alg_ds/alg/defer.c src/lib/sm/ie/ran_function_name.c \
    src/sm/dapp_sm/ie/dapp_data_ie.c \
    src/sm/dapp_sm/ie/ir/*.c \
    src/sm/dapp_sm/e3/service_models/spectrum_sm/ir/*.c \
    src/sm/dapp_sm/enc/dapp_enc_asn.c src/sm/dapp_sm/dec/dapp_dec_asn.c \
    src/sm/dapp_sm/ie/asn/*.c) >"$work/cc.log" 2>&1 \
    || { cat "$work/cc.log" >&2; exit 1; }

# 6. Run it (it also checks that flexric's decoder returns every input) and write the header.
echo "running gen_golden" >&2
"$work/gen_golden" > "$work/golden.txt"
if [ -n "$out" ]; then
    python3 "$here/gen_header.py" "$work/golden.txt" "$commit" libe3_e2sm_dapp_test > "$out"
else
    python3 "$here/gen_header.py" "$work/golden.txt" "$commit" libe3_e2sm_dapp_test
fi
