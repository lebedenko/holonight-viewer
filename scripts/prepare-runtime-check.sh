#!/usr/bin/env bash
# Prepare only installed payloads for a container context beneath build/.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
context=$(mktemp -d "$root/build/runtime-check.XXXXXX")
for provider in holonight-config holonight-qt; do
  DESTDIR="$context/payload" cmake --install "$root/build/deps/$provider" --prefix /usr
done
DESTDIR="$context/payload" cmake --install "$root/build/release" --prefix /usr
DESTDIR="$context/payload" cmake --install "$root/build/test" --prefix /usr --component qualification
mkdir -p "$context/check/scripts" "$context/check/build"
cp "$root/scripts/check-opening.py" "$root/scripts/format-fixtures.py" \
  "$root/scripts/check-installed-desktop.py" "$root/scripts/isolated-runtime.sh" "$context/check/scripts/"
cp "$root/packaging/Dockerfile.runtime-check" "$context/Dockerfile"
printf '%s\n' "$context" > "$root/build/runtime-check-context"
printf 'Runtime context: %s\n' "$context"
