#!/bin/bash
# Build dependencies of the libe3py wheel, installed inside the manylinux_2_28
# (AlmaLinux 8) container that cibuildwheel runs (see pyproject.toml).
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

yum install -y epel-release
yum install -y pkgconfig git curl cmake autoconf automake libtool bison flex m4

# libzmq: static, without CURVE, GSSAPI, PGM and NORM, so the wheel does not drag in
# libsodium, libkrb5 and friends (the EPEL package would).
curl -fsSL https://github.com/zeromq/libzmq/releases/download/v4.3.5/zeromq-4.3.5.tar.gz \
    | tar -xz -C /tmp
cmake -S /tmp/zeromq-4.3.5 -B /tmp/zeromq-build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/zeromq -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_TESTS=OFF -DWITH_DOCS=OFF \
    -DENABLE_CURVE=OFF -DWITH_LIBSODIUM=OFF -DWITH_LIBBSD=OFF -DWITH_OPENPGM=OFF -DWITH_NORM=OFF \
    -DWITH_DOC=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build /tmp/zeromq-build -j"$(nproc)"
cmake --install /tmp/zeromq-build
rm -rf /tmp/zeromq-4.3.5 /tmp/zeromq-build

# protobuf: the el8 one (3.5) cannot compile the proto3 `optional` fields of the E3AP
# schema, so build 3.21.12 (the version Ubuntu 24.04 ships), static, into /opt/protobuf.
curl -fsSL https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-3.21.12.tar.gz \
    | tar -xz -C /tmp
cmake -S /tmp/protobuf-3.21.12 -B /tmp/protobuf-build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/protobuf -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF
cmake --build /tmp/protobuf-build -j"$(nproc)"
cmake --install /tmp/protobuf-build
rm -rf /tmp/protobuf-3.21.12 /tmp/protobuf-build

# asn1c, at the commit build_libe3 pins (one place for the pin).
pinned="$(sed -n 's/^ *local pinned_commit="\(.*\)"/\1/p' "${HERE}/build_libe3" | head -1)"
[ -n "${pinned}" ] || { echo "no asn1c pin found in build_libe3" >&2; exit 1; }
git clone https://github.com/mouse07410/asn1c /tmp/asn1c
cd /tmp/asn1c
git checkout "${pinned}"
autoreconf -iv
./configure --prefix /opt/asn1c
make -j"$(nproc)"
make install
rm -rf /tmp/asn1c
