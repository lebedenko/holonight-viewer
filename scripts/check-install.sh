#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
stage=$(mktemp -d "$root/build/install-check.XXXXXX")
DESTDIR="$stage" cmake --install "$root/build/release"
for file in bin/hn-viewer share/applications/org.holonight.Viewer.desktop \
  share/icons/hicolor/scalable/apps/org.holonight.Viewer.svg \
  share/licenses/holonight-viewer/LICENSE share/licenses/holonight-viewer/GPL-3.0-or-later.txt; do
  test -s "$stage/usr/$file"
done
test ! -e "$stage/usr/bin/holonight-viewer"
desktop-file-validate "$stage/usr/share/applications/org.holonight.Viewer.desktop"
# Only installed provider imports are available; no source or build QML paths.
export QML_IMPORT_PATH="${HOLONIGHT_QML_IMPORT_PATH:-${HOLONIGHT_DEPENDENCY_PREFIX:-$root/build/deps/prefix}/lib/qt6/qml}"
export LD_LIBRARY_PATH="${HOLONIGHT_DEPENDENCY_PREFIX:-$root/build/deps/prefix}/lib"
unset QML2_IMPORT_PATH QT_QUICK_CONTROLS_STYLE QT_QUICK_CONTROLS_CONF QT_QUICK_CONTROLS_FALLBACK_STYLE
cd "$stage"
QT_QPA_PLATFORM=offscreen "$stage/usr/bin/hn-viewer" --version
set +e
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software timeout 3 "$stage/usr/bin/hn-viewer" > runtime.log 2>&1
status=$?
set -e
cat runtime.log
test "$status" -eq 124
if rg -i 'failed|error|not installed|not found|unavailable' runtime.log; then exit 1; fi
python3 "$root/scripts/check-opening.py" "$stage/usr/bin/hn-viewer"
PATH="$stage/usr/bin:$PATH" QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  python3 "$root/scripts/check-installed-desktop.py" "$stage/usr/share/applications/org.holonight.Viewer.desktop"
printf 'Staged installation passed: %s\n' "$stage"
