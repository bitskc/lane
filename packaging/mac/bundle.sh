#!/usr/bin/env bash
# Stage, deploy Qt into, sign, verify and optionally install Lane.app.
# Signing (design Decision 11): ad-hoc by default; set LANE_SIGN_IDENTITY for the
# Apple Development identity, which needs an unlocked login keychain (GUI session).
#
# Usage: bundle.sh <build-dir> [install-dir]
#   <build-dir>   CMake build tree containing src/lane.app (required)
#   [install-dir] if given, Lane.app is copied there (e.g. ~/Applications)
set -euo pipefail

BUILD_DIR="${1:?usage: bundle.sh <build-dir> [install-dir]}"
INSTALL_DIR="${2:-}"
APP="$BUILD_DIR/src/lane.app"
IDENTITY="${LANE_SIGN_IDENTITY:--}"
SRC_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SVG="$SRC_ROOT/data/icons/app.lane.Lane.svg"

die() { echo "bundle: $*" >&2; exit 1; }
[ -d "$APP" ] || die "$APP not found - run cmake --build $BUILD_DIR first"
command -v macdeployqt >/dev/null || die 'macdeployqt not on PATH (export PATH=/opt/homebrew/bin:$PATH)'

# --- icon: render the SVG to an iconset, then Lane.icns ----------------------
iconset="$(mktemp -d)/Lane.iconset"
mkdir -p "$iconset"
render() { # render <points> <filename>
    qlmanage -t -s "$1" -o "$iconset" "$SVG" >/dev/null 2>&1 || die "qlmanage failed at size $1"
    mv "$iconset/$(basename "$SVG").png" "$iconset/$2"
}
render 16   icon_16x16.png
render 32   icon_16x16@2x.png
render 32   icon_32x32.png
render 64   icon_32x32@2x.png
render 128  icon_128x128.png
render 256  icon_128x128@2x.png
render 256  icon_256x256.png
render 512  icon_256x256@2x.png
render 512  icon_512x512.png
render 1024 icon_512x512@2x.png
iconutil -c icns "$iconset" -o "$APP/Contents/Resources/Lane.icns" || die "iconutil failed"
rm -rf "$(dirname "$iconset")"

# --- Qt deploy ---------------------------------------------------------------
macdeployqt "$APP" -qmldir="$SRC_ROOT/src/qml" || die "macdeployqt failed"

# --- sign every nested Mach-O, then the bundle -------------------------------
sign_one() { codesign --force --sign "$IDENTITY" "$1" >/dev/null || die "codesign failed: $1"; }
while IFS= read -r f; do
    case "$(file -b "$f")" in Mach-O*) sign_one "$f" ;; esac
done < <(find "$APP" -type f)
codesign --force --sign "$IDENTITY" "$APP" >/dev/null || die "codesign failed: $APP"

# --- verify each nested item and the bundle individually ---------------------
while IFS= read -r f; do
    case "$(file -b "$f")" in
        Mach-O*) codesign --verify --strict "$f" || die "signature invalid: $f" ;;
    esac
done < <(find "$APP" -type f)
codesign --verify --strict --verbose=2 "$APP" || die "bundle signature invalid"

# --- no Homebrew or /usr/local paths may survive inside the bundle -----------
bad=""
while IFS= read -r f; do
    case "$(file -b "$f")" in
        Mach-O*)
            refs="$(otool -L "$f" | awk 'NR>1 {print $1}' | grep -E '^/opt/homebrew|^/usr/local' || true)"
            [ -n "$refs" ] && bad="$bad\n$f:\n$refs"
            ;;
    esac
done < <(find "$APP" -type f)
[ -z "$bad" ] || die "otool check failed, bundle references local prefixes:$bad"

# --- quarantine strip: no-op when absent, needed if the bundle moved hosts ---
xattr -dr com.apple.quarantine "$APP" 2>/dev/null || true

# --- optional install --------------------------------------------------------
if [ -n "$INSTALL_DIR" ]; then
    mkdir -p "$INSTALL_DIR"
    rm -rf "$INSTALL_DIR/Lane.app"
    cp -R "$APP" "$INSTALL_DIR/Lane.app"
    xattr -dr com.apple.quarantine "$INSTALL_DIR/Lane.app" 2>/dev/null || true
    echo "installed: $INSTALL_DIR/Lane.app"
fi
echo "ok: $APP"
