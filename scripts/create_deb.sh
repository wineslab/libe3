#!/bin/bash
set -euo pipefail

# Helper to produce .deb packages for specified architectures.
# By default it rebuilds libe3 (Release). Use --use-existing-build to package
# the already configured/built CMake tree.
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

VERSION=$(cat VERSION | tr -d '[:space:]')
BUILD_DIR="$ROOT_DIR/build"
DEB_OUT_DIR="$BUILD_DIR"
USE_EXISTING_BUILD=0

usage() {
    echo "Usage: $0 [--use-existing-build] [--build-dir DIR] [amd64|arm64|all]"
    echo "If no architecture argument is given, host architecture (dpkg --print-architecture) is used."
}

ARG_ARCH=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --use-existing-build)
            USE_EXISTING_BUILD=1
            shift
            ;;
        --build-dir)
            if [ -z "${2:-}" ]; then
                echo "Missing value for --build-dir"
                usage
                exit 1
            fi
            BUILD_DIR="$2"
            shift 2
            ;;
        amd64|arm64|all|x86_64|x64|aarch64)
            if [ -n "$ARG_ARCH" ]; then
                echo "Only one architecture argument is allowed."
                usage
                exit 1
            fi
            ARG_ARCH="$1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
done

HOST_ARCH=$(dpkg --print-architecture)

# Resolve relative path from repository root.
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$ROOT_DIR/$BUILD_DIR"
fi
DEB_OUT_DIR="$BUILD_DIR"

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
    PKG_ROOT="$BUILD_DIR/package_root_${ARCH}"
    local build_dir_arg=""

    rm -rf "$PKG_ROOT"
    mkdir -p "$PKG_ROOT"

    if [ "$USE_EXISTING_BUILD" -eq 0 ]; then
        echo "Building libe3 (Release) for $ARCH..."
        chmod +x ./build_libe3 || true

        if [ "$ARCH" != "$HOST_ARCH" ]; then
            echo "Warning: building for $ARCH on host $HOST_ARCH. Ensure cross-compilers or a proper build environment (e.g. Docker/qemu) are configured."
        fi

        # build_libe3 expects a path relative to repository root for --build-dir.
        if [[ "$BUILD_DIR" == "$ROOT_DIR/"* ]]; then
            build_dir_arg="${BUILD_DIR#$ROOT_DIR/}"
            ./build_libe3 -c -r --build-dir "$build_dir_arg"
        else
            echo "build_libe3 cannot rebuild with --build-dir outside repository root: $BUILD_DIR"
            exit 1
        fi
    else
        echo "Using existing build directory: $BUILD_DIR"
    fi

    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "Build directory is not configured: $BUILD_DIR"
        echo "Run build_libe3 first, or omit --use-existing-build."
        exit 1
    fi

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
