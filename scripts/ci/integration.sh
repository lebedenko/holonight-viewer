#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
task build:release
DESTDIR="$PWD/build/stage" cmake --install build/release
bash scripts/check-install.sh
bash scripts/prepare-runtime-check.sh
cmake --preset dev -B build/sanitizer -DVIEWER_ENABLE_SANITIZERS=ON
cmake --build build/sanitizer
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/sanitizer --output-on-failure --no-tests=error
