#!/usr/bin/env bash
# Prepare only installed payloads for a container context beneath build/.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
context=$(mktemp -d "$root/build/runtime-check.XXXXXX")
# The fresh runtime container receives only installed payloads and check scripts.
mkdir -p "$context/payload/usr"
cp -a "$root/build/deps/prefix/." "$context/payload/usr/"
DESTDIR="$context/payload" cmake --install "$root/build/release" --prefix /usr
DESTDIR="$context/payload" cmake --install "$root/build/dev" --prefix /usr --component qualification
mkdir -p "$context/check/scripts" "$context/check/build"
cp "$root/scripts/check-opening.py" "$root/scripts/format-fixtures.py" \
  "$root/scripts/check-metadata.py" "$root/scripts/check-installed-desktop.py" "$root/scripts/isolated-runtime.sh" "$context/check/scripts/"
printf '%s\n' "${context#"$root/"}" > "$root/build/runtime-check-context"
printf 'Runtime context: %s\n' "$context"
