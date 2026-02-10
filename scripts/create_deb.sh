#!/bin/bash
set -euo pipefail

# Helper to build libe3 and produce .deb packages for specified architectures
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

VERSION=$(cat VERSION | tr -d '[:space:]')
BUILD_DIR="$ROOT_DIR/build"
DEB_OUT_DIR="$ROOT_DIR/build"

usage() {
    echo "Usage: $0 [amd64|arm64|all]"
    echo "If no argument is given the host architecture (dpkg --print-architecture) is used."
}

if [ "$#" -gt 1 ]; then
    usage
    exit 1
fi

ARG_ARCH="${1:-}"
HOST_ARCH=$(dpkg --print-architecture)

case "$ARG_ARCH" in
	"") ARCHS="$HOST_ARCH" ;;
	all) ARCHS="amd64 arm64" ;;
	amd64 | x86_64 | x64) ARCHS="amd64" ;;
	arm64 | aarch64) ARCHS="arm64" ;;
	*)
		echo "Unknown arch: $ARG_ARCH"
		usage
		exit 1
		;;
esac

build_for() {
    ARCH="$1"
    PKG_ROOT="$ROOT_DIR/build/package_root_${ARCH}"

    rm -rf "$PKG_ROOT"
    mkdir -p "$PKG_ROOT"

    echo "Building libe3 (Release) for $ARCH..."
    chmod +x ./build_libe3 || true

    if [ "$ARCH" != "$HOST_ARCH" ]; then
        echo "Warning: building for $ARCH on host $HOST_ARCH. Ensure cross-compilers or a proper build environment (e.g. Docker/qemu) are configured."
    fi

    # build_libe3 must be able to produce artifacts for the requested architecture
    ./build_libe3 -c -r

    echo "Installing into staging area for $ARCH..."
    DESTDIR="$PKG_ROOT" cmake --install "$BUILD_DIR" --prefix /usr

    echo "Creating DEBIAN control metadata for $ARCH..."
    DEBIAN_DIR="$PKG_ROOT/DEBIAN"
    mkdir -p "$DEBIAN_DIR"

    cat >"$DEBIAN_DIR/control" <<EOF
Package: libe3
Version: ${VERSION}
Section: libs
Priority: optional
Architecture: ${ARCH}
Maintainer: Northeastern University <thecave003@gmail.com>
Depends: libstdc++6
Description: libe3 - Vendor-neutral E3AP C++ library
EOF

    echo "Fixing permissions for $ARCH..."
    find "$PKG_ROOT" -type f -exec chmod 644 {} \;
    find "$PKG_ROOT" -type d -exec chmod 755 {} \;
    chmod 755 "$DEBIAN_DIR"

    echo "Building .deb package for $ARCH..."
    mkdir -p "$DEB_OUT_DIR"
    dpkg-deb --build "$PKG_ROOT" "$DEB_OUT_DIR/libe3_${VERSION}_${ARCH}.deb"

    echo "Package created: $DEB_OUT_DIR/libe3_${VERSION}_${ARCH}.deb"
}

for ARCH in $ARCHS; do
	build_for "$ARCH"
done

exit 0
