#!/usr/bin/env bash
set -euo pipefail
formatted=$(mktemp)
trap 'rm -f "$formatted"' EXIT
/usr/lib/qt6/bin/qmlformat apps/viewer/Main.qml > "$formatted"
diff -u apps/viewer/Main.qml "$formatted"
