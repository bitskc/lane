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
mkdir -p "$APP/Contents/Resources"

is_macho() { case "$(file -b "$1")" in Mach-O*) return 0 ;; *) return 1 ;; esac }

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

# --- fix dylib IDs: Homebrew Qt ships absolute install names; macdeployqt ----
# rewrites dependents to @rpath but leaves each framework's own LC_ID_DYLIB
# pointing at /opt/homebrew. Rewrite IDs to @rpath before signing.
while IFS= read -r f; do
    if is_macho "$f"; then
        id="$(otool -D "$f" | awk 'NR>1 {print $1; exit}')"
        case "$id" in
            /opt/homebrew/*|/usr/local/*)
                base="$(basename "$f")"
                case "$f" in
                    *.framework/*) new_id="@rpath/${base}.framework/Versions/A/${base}" ;;
                    *)             new_id="@rpath/${base}" ;;
                esac
                install_name_tool -id "$new_id" "$f" || die "install_name_tool failed: $f"
                ;;
        esac
    fi
done < <(find "$APP" -type f)


# --- sign every nested Mach-O, then the bundle -------------------------------
# Factored into a function: the install step must re-run the whole pass at
# the destination because copying a signed bundle (cp, ditto, anything)
# invalidates embedded signatures on the copied Mach-Os.
sign_bundle() {
    local app="$1" f
    while IFS= read -r f; do
        if is_macho "$f"; then
            codesign --force --sign "$IDENTITY" "$f" >/dev/null || die "codesign failed: $f"
        fi
    done < <(find "$app" -type f)
    codesign --force --sign "$IDENTITY" "$app" >/dev/null || die "codesign failed: $app"
}

# --- verify each nested item and the bundle individually ---------------------
verify_bundle() {
    local app="$1" f
    while IFS= read -r f; do
        if is_macho "$f"; then
            codesign --verify --strict "$f" || die "signature invalid: $f"
        fi
    done < <(find "$app" -type f)
    codesign --verify --strict --verbose=2 "$app" || die "bundle signature invalid"
}

sign_bundle "$APP"
verify_bundle "$APP"

# --- no Homebrew or /usr/local paths may survive inside the bundle -----------
bad=""
while IFS= read -r f; do
    if is_macho "$f"; then
        refs="$(otool -L "$f" | awk 'NR>1 {print $1}' | grep -E '^/opt/homebrew|^/usr/local' || true)"
        [ -n "$refs" ] && bad="$bad\n$f:\n$refs"
    fi
done < <(find "$APP" -type f)
[ -z "$bad" ] || die "otool check failed, bundle references local prefixes:$bad"

# --- quarantine strip: no-op when absent, needed if the bundle moved hosts ---
xattr -dr com.apple.quarantine "$APP" 2>/dev/null || true

# --- optional install --------------------------------------------------------
if [ -n "$INSTALL_DIR" ]; then
    mkdir -p "$INSTALL_DIR"
    rm -rf "$INSTALL_DIR/Lane.app"
    ditto "$APP" "$INSTALL_DIR/Lane.app"
    # Copying invalidates embedded signatures even when bytes survive,
    # so sign and verify the installed copy, not just the staged one.
    sign_bundle "$INSTALL_DIR/Lane.app"
    verify_bundle "$INSTALL_DIR/Lane.app"
    xattr -dr com.apple.quarantine "$INSTALL_DIR/Lane.app" 2>/dev/null || true
    echo "installed: $INSTALL_DIR/Lane.app"
fi
echo "ok: $APP"
