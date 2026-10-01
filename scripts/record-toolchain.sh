#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
evidence="$root/build/ci-provenance"
mkdir -p "$evidence"
pacman -Q > "$evidence/packages.txt"
c++ --version > "$evidence/compiler.txt"
cmake --version > "$evidence/cmake.txt"
qmake6 --version > "$evidence/qt.txt"
cp "$root/packaging/Dockerfile.ci" "$evidence/Dockerfile.ci"
