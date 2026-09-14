#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$root/build"
formatted=$(mktemp "$root/build/qml-format.XXXXXX")
trap 'rm -f "$formatted"' EXIT
for file in Main.qml FooterKeyHints.qml; do
  bash "$root/scripts/qml-format.sh" "$root/apps/viewer/$file" > "$formatted"
  diff -u "$root/apps/viewer/$file" "$formatted"
done
