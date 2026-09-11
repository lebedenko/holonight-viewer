#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$root/build"
formatted=$(mktemp "$root/build/qml-format.XXXXXX")
trap 'rm -f "$formatted"' EXIT
bash "$root/scripts/qml-format.sh" "$root/apps/viewer/Main.qml" > "$formatted"
diff -u "$root/apps/viewer/Main.qml" "$formatted"
