#!/usr/bin/env bash
# Build Lane from this checkout, run the tests, and pack the install tree
# into a per-architecture tarball.
#
# Usage: build-tarball.sh [out-dir]
#   [out-dir]  where lane-<version>-linux-<arch>.tar.gz and its .sha256 go
#              (default: dist/ under the repo root)
#
# Environment:
#   LANE_VERSION  version string for the file name (default: the version in
#                 CMakeLists.txt; the release workflow passes the tag)
#   LANE_ARCH     architecture label (default: uname -m, so x86_64 or aarch64)
#   BUILD_DIR     CMake build tree (default: <repo>/build-package)
#
# The tree is configured with prefix /usr and installed through DESTDIR, so
# the .desktop, D-Bus service, and systemd unit files inside it carry
# /usr/bin/lane. Extract it at / (as root) to install. The binary links the
# distribution's Qt 6 and KDE Frameworks, so the target needs the same
# runtime libraries; packaging/linux/fedora-build-deps.txt lists the
# -devel packages this was built against.
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="${1:-$SRC_ROOT/dist}"
BUILD_DIR="${BUILD_DIR:-$SRC_ROOT/build-package}"
ARCH="${LANE_ARCH:-$(uname -m)}"
VERSION="${LANE_VERSION:-$(sed -n 's/^project(lane VERSION \([0-9][0-9.]*\).*/\1/p' "$SRC_ROOT/CMakeLists.txt")}"

die() { echo "build-tarball: $*" >&2; exit 1; }
[ -n "$VERSION" ] || die "could not determine the version from CMakeLists.txt"
case "$ARCH" in
    x86_64|aarch64) ;;
    *) die "unsupported architecture: $ARCH" ;;
esac

NAME="lane-$VERSION-linux-$ARCH"
STAGE="$BUILD_DIR/stage"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR"

# appstreamtest reads install_manifest.txt, so it needs a real --prefix
# install (see ci.yml); the throwaway prefix lives inside the build tree.
cmake --install "$BUILD_DIR" --prefix "$BUILD_DIR/throwaway-install"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
    ctest --test-dir "$BUILD_DIR" --output-on-failure

# The configured prefix is /usr, so a plain DESTDIR install is what ships.
rm -rf "$STAGE"
DESTDIR="$STAGE" cmake --install "$BUILD_DIR"
[ -x "$STAGE/usr/bin/lane" ] || die "$STAGE/usr/bin/lane missing after install"

tar -C "$STAGE" --owner=0 --group=0 --numeric-owner -czf "$OUT_DIR/$NAME.tar.gz" usr
(cd "$OUT_DIR" && sha256sum "$NAME.tar.gz" > "$NAME.tar.gz.sha256")
echo "built $OUT_DIR/$NAME.tar.gz"
