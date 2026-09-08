#!/usr/bin/env bash
set -euo pipefail
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
unset QML_IMPORT_PATH QML2_IMPORT_PATH LD_LIBRARY_PATH HOLONIGHT_DEPENDENCY_PREFIX HOLONIGHT_QML_IMPORT_PATH
unset QT_QUICK_CONTROLS_STYLE QT_QUICK_CONTROLS_CONF QT_QUICK_CONTROLS_FALLBACK_STYLE
export QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software
# No workspace volume may be mounted into this container.
test ! -e /work/viewer/CMakeLists.txt
test ! -e /work/viewer/build
for file in /usr/bin/holonight-viewer /usr/share/applications/org.holonight.Viewer.desktop \
  /usr/share/icons/hicolor/scalable/apps/org.holonight.Viewer.svg; do test -s "$file"; done
# Installed runtime search paths must not refer to development locations.
while IFS= read -r -d '' file; do
  if readelf -d "$file" 2>/dev/null | rg '\((RPATH|RUNPATH)\)' | rg '/work|/home|/build|/tmp'; then
    printf 'Development runtime path in %s\n' "$file" >&2
    exit 1
  fi
done < <(find /usr/bin/holonight-viewer /usr/lib -type f \( -iname '*holonight*' -o -path '*/Holonight/*' \) -print0)
python3 scripts/format-fixtures.py build/fixtures
/usr/libexec/holonight-viewer/installed-runtime-probe /opt/check/build/fixtures
python3 scripts/check-opening.py /usr/bin/holonight-viewer
python3 scripts/check-installed-desktop.py /usr/share/applications/org.holonight.Viewer.desktop
