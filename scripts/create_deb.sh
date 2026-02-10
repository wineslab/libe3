#!/bin/bash
set -euo pipefail

# Helper to build libe3 and produce a .deb package under build/package.deb
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

VERSION=$(cat VERSION | tr -d '[:space:]')
BUILD_DIR="$ROOT_DIR/build"
PKG_ROOT="$ROOT_DIR/build/package_root"
DEB_OUT_DIR="$ROOT_DIR/build"

rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT"

echo "Building libe3 (Release)..."
chmod +x ./build_libe3 || true
./build_libe3 -c -r

echo "Installing into staging area..."
# Use cmake --install to populate the staging root
cmake --install "$BUILD_DIR" --prefix /usr --destdir "$PKG_ROOT"

echo "Creating DEBIAN control metadata..."
DEBIAN_DIR="$PKG_ROOT/DEBIAN"
mkdir -p "$DEBIAN_DIR"

cat > "$DEBIAN_DIR/control" <<EOF
Package: libe3
Version: ${VERSION}
Section: libs
Priority: optional
Architecture: amd64
Maintainer: libe3 maintainers <noreply@example.com>
Depends: libstdc++6
Description: libe3 - Vendor-neutral E3AP C++ library
EOF

echo "Fixing permissions..."
find "$PKG_ROOT" -type f -exec chmod 644 {} \;
find "$PKG_ROOT" -type d -exec chmod 755 {} \;
chmod 755 "$DEBIAN_DIR"

echo "Building .deb package..."
mkdir -p "$DEB_OUT_DIR"
dpkg-deb --build "$PKG_ROOT" "$DEB_OUT_DIR/package.deb"

echo "Package created: $DEB_OUT_DIR/package.deb"

exit 0
