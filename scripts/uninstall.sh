#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
prefix="${DESTDIR:-}/usr"
rm -f -- "$prefix/bin/hn-viewer" "$prefix/bin/holonight-viewer" \
  "$prefix/share/applications/org.holonight.Viewer.desktop" \
  "$prefix/share/icons/hicolor/scalable/apps/org.holonight.Viewer.svg" \
  "$prefix/share/licenses/holonight-viewer/LICENSE" \
  "$prefix/share/licenses/holonight-viewer/GPL-3.0-or-later.txt"
licenses="$prefix/share/licenses/holonight-viewer"
if [[ -d "$licenses" ]]; then
  rmdir --ignore-fail-on-non-empty -- "$licenses"
fi
if [[ -d "$prefix/share/applications" ]]; then
  update-desktop-database "$prefix/share/applications"
fi
