#!/bin/bash
# Packages the built SkateTune.vst3 + SkateTune.component into a single
# double-clickable macOS installer (.pkg) that installs both into the
# CURRENT USER's plugin folders (no admin password needed):
#   ~/Library/Audio/Plug-Ins/VST3/SkateTune.vst3
#   ~/Library/Audio/Plug-Ins/Components/SkateTune.component
#
# Usage: ./installer/build_pkg.sh <cmake-build-dir> <output-dir>
# Must be run on macOS (uses pkgbuild/productbuild). This is exactly what
# the GitHub Actions workflow runs on the macos-14 runner; it's also usable
# directly on a real Mac that has Xcode command line tools installed.
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-installer_output}"
# Real, bumped-every-release version (CI passes this in from CMakeLists.txt's
# project() version, so there is one source of truth). Falls back to 1.0.0
# for a manual/local run that doesn't set it.
VERSION="${SKATETUNE_VERSION:-1.0.0}"
# Fixed filename of the very first release, kept forever so the download
# link already shared with testers never breaks even as VERSION moves on -
# see the "cp" near the end of this script, which publishes the SAME
# installer under both names.
LEGACY_FIXED_FILENAME="SkateTune-Installer-1.0.0.pkg"
IDENTIFIER_BASE="com.skatetune.skatetune"

VST3_SRC="$BUILD_DIR/SkateTune_artefacts/Release/VST3/SkateTune.vst3"
AU_SRC="$BUILD_DIR/SkateTune_artefacts/Release/AU/SkateTune.component"

for f in "$VST3_SRC" "$AU_SRC"; do
    if [ ! -d "$f" ]; then
        echo "ERROR: expected build output not found: $f" >&2
        echo "Did the build finish, and did it run on macOS (AU only builds there)?" >&2
        exit 1
    fi
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT

mkdir -p "$STAGING/vst3root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGING/auroot/Library/Audio/Plug-Ins/Components"
mkdir -p "$OUT_DIR"

ditto "$VST3_SRC" "$STAGING/vst3root/Library/Audio/Plug-Ins/VST3/SkateTune.vst3"
ditto "$AU_SRC"   "$STAGING/auroot/Library/Audio/Plug-Ins/Components/SkateTune.component"

pkgbuild --root "$STAGING/vst3root" \
         --install-location "/" \
         --identifier "$IDENTIFIER_BASE.vst3" \
         --version "$VERSION" \
         "$OUT_DIR/SkateTuneVST3.pkg"

pkgbuild --root "$STAGING/auroot" \
         --install-location "/" \
         --identifier "$IDENTIFIER_BASE.au" \
         --version "$VERSION" \
         "$OUT_DIR/SkateTuneAU.pkg"

cat > "$STAGING/welcome.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica,sans-serif;padding:18px;line-height:1.4;">
<h2>SkateTune</h2>
<p>This installs the SkateTune pitch-correction plugin into your personal plugin
folders (no admin password needed):</p>
<ul>
<li><code>~/Library/Audio/Plug-Ins/VST3/SkateTune.vst3</code></li>
<li><code>~/Library/Audio/Plug-Ins/Components/SkateTune.component</code> (Audio Unit, for Logic Pro)</li>
</ul>
<p>If your DAW / Logic Pro was already open, restart it afterwards so it
picks up the new plugin.</p>
<p style="color:#666;font-size:12px;">This installer isn't signed with an Apple
Developer certificate, so macOS may warn that it's from an "unidentified
developer" the first time you open it. That's normal for independently
shared plugins - go to <b>System Settings &gt; Privacy &amp; Security</b> and
click <b>Open Anyway</b>, then confirm once more.</p>
</body></html>
HTML

cat > "$STAGING/Distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>SkateTune</title>
    <organization>$IDENTIFIER_BASE</organization>
    <domains enable_currentUserHome="true" enable_localSystem="false" enable_anywhere="false"/>
    <options customize="never" require-scripts="false" rootVolumeOnly="false"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <choices-outline>
        <line choice="choiceVST3"/>
        <line choice="choiceAU"/>
    </choices-outline>
    <choice id="choiceVST3" title="SkateTune (VST3)" visible="false">
        <pkg-ref id="$IDENTIFIER_BASE.vst3"/>
    </choice>
    <choice id="choiceAU" title="SkateTune (Audio Unit)" visible="false">
        <pkg-ref id="$IDENTIFIER_BASE.au"/>
    </choice>
    <pkg-ref id="$IDENTIFIER_BASE.vst3" version="$VERSION" onConclusion="none">SkateTuneVST3.pkg</pkg-ref>
    <pkg-ref id="$IDENTIFIER_BASE.au" version="$VERSION" onConclusion="none">SkateTuneAU.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$STAGING/Distribution.xml" \
             --package-path "$OUT_DIR" \
             --resources "$STAGING" \
             "$OUT_DIR/SkateTune-Installer-$VERSION.pkg"

rm -f "$OUT_DIR/SkateTuneVST3.pkg" "$OUT_DIR/SkateTuneAU.pkg"

echo "Installer written to: $OUT_DIR/SkateTune-Installer-$VERSION.pkg"

# Also publish under the original fixed filename (a no-op if VERSION is
# still 1.0.0, since source and dest would already be the same file) so
# whoever has the very first download link keeps getting the latest build.
if [ "$OUT_DIR/SkateTune-Installer-$VERSION.pkg" != "$OUT_DIR/$LEGACY_FIXED_FILENAME" ]; then
    cp "$OUT_DIR/SkateTune-Installer-$VERSION.pkg" "$OUT_DIR/$LEGACY_FIXED_FILENAME"
    echo "Also published as: $OUT_DIR/$LEGACY_FIXED_FILENAME (stable link, never renamed)"
fi
