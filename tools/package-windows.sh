#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:?usage: package-windows.sh <build-dir> <qt-win-prefix> <out-zip>}"
QT_WIN="${2:?qt windows prefix}"
OUT_ZIP="${3:?output zip path}"

EXE="$BUILD_DIR/anomaly-spotter.exe"
[ -f "$EXE" ] || { echo "exe not found: $EXE" >&2; exit 1; }

STAGE="$(mktemp -d)"
DIST="$STAGE/anomaly-spotter"
mkdir -p "$DIST/platforms" "$DIST/styles" "$DIST/multimedia" "$DIST/tls" \
         "$DIST/networkinformation"

cp "$EXE" "$DIST/"

for dll in Qt6Core Qt6Gui Qt6Widgets Qt6Multimedia Qt6Network \
           libc++ libunwind \
           avcodec-61 avformat-61 avutil-59 swresample-5 swscale-8; do
    cp "$QT_WIN/bin/$dll.dll" "$DIST/"
done

cp "$QT_WIN/plugins/platforms/qwindows.dll"          "$DIST/platforms/"
cp "$QT_WIN/plugins/styles/qmodernwindowsstyle.dll"  "$DIST/styles/"
cp "$QT_WIN/plugins/multimedia/ffmpegmediaplugin.dll" "$DIST/multimedia/"
cp "$QT_WIN/plugins/tls/qschannelbackend.dll"        "$DIST/tls/"
cp "$QT_WIN/plugins/networkinformation/qnetworklistmanager.dll" "$DIST/networkinformation/"

printf '[Paths]\nPlugins = .\n' > "$DIST/qt.conf"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cp "$REPO_ROOT/LICENSE" "$DIST/LICENSE.txt"
cp "$REPO_ROOT/legal/THIRD-PARTY-NOTICES.txt" "$DIST/"
mkdir -p "$DIST/licenses"
cp "$REPO_ROOT"/legal/licenses/*.txt "$DIST/licenses/"

rm -f "$OUT_ZIP"
( cd "$STAGE" && zip -rq "$OUT_ZIP" "anomaly-spotter" )
echo "=== package contents ==="
( cd "$STAGE" && find anomaly-spotter -type f | sort )
echo "=== zip: $OUT_ZIP ($(du -h "$OUT_ZIP" | cut -f1)) ==="
echo "STAGE=$STAGE"
