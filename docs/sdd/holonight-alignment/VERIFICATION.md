# Verification — 2026-09-19

## Baselines and scope

Viewer started at c76a3c683ac500a75aab56c68998698cf428aad9. Canonical main refs were verified before work: Qt eadfe482000e64ee7ead83d63f878e3f365686b1 and Config fe69a59e6b73167fd5349223a4d265d75386c139. Provider sources remain unchanged. Native user checks passed for default HoloNight and explicit Fusion: image opening, menu, Information/Help popups, Tab/Escape and image-navigation shortcuts.

## Local results

- External Debug configure/build: `/tmp/holonight-viewer-alignment`, `BUILD_TESTING=ON`, explicit `/usr` provider and QML paths. Build passes; owned sources compile with Wall/Wextra/Wpedantic. Generated Wayland bindings are isolated.
- CMake `format-check qml-import-check qmltypes-check qml-lint`: passed. Local Qt formatter selected with `QMLFORMAT=/usr/lib/qt6/bin/qmlformat`; the unrelated `/usr/bin/qmlformat` exits silently with status 1 on this host. Formatting inventories are identical in Task and CMake.
- CMake `tidy`: passed across 33 owned translation units with an explicit source config file, including an external build directory. New missing-initializer warnings were resolved.
- CTest: initial complete matrix passed 19/19, including full smoke (five existing opt-in/native-only skips), CLI, runtime probe, formatter discovery, ten dark/light physical-hairline scale cases and explicit Fusion popup/menu/keyboard checks. Final matrix including the retired-uninstall refusal check passed 20/20 in 50.72 seconds.
- REUSE lint: passed, 188/188 files before this verification record (aggregate license metadata also covers this record).
- Staged Debug install: `HOLONIGHT_DEPENDENCY_PREFIX=/usr HOLONIGHT_QML_IMPORT_PATH=/usr/lib/qt6/qml bash scripts/check-install.sh /tmp/holonight-viewer-alignment` passed; `/usr` plus DESTDIR, desktop validation, four-format CLI and GIO launch, only installed provider imports.
- Fresh Config provider: configure/build/install under `/tmp/viewer-alignment-providers`; 2/2 CTest entries passed. Fresh Qt provider built and installed at the accepted pin; provider verification is recorded in the umbrella handoff.
- Clean Release source copy `/tmp/viewer-alignment-clean-source`, external build `/tmp/viewer-alignment-release`: passed. A deliberately failing HolonightQtConfig.cmake under the copy's `build/deps/prefix` was ignored. Cache selects both providers from `/tmp/viewer-alignment-providers/prefix`, preserving the explicit tooling QML path.
- Release staging against those fresh providers: `HOLONIGHT_DEPENDENCY_PREFIX=/tmp/viewer-alignment-providers/prefix HOLONIGHT_QML_IMPORT_PATH=/tmp/viewer-alignment-providers/prefix/lib/qt6/qml bash scripts/check-install.sh /tmp/viewer-alignment-release` passed, including PNG/JPEG/BMP/WebP and GIO launch without source imports.

Local prerequisites installed with user approval: qt6-imageformats (required WebP plugin), patchelf (Qt provider package test). D-Bus/Xvfb tests ran outside the filesystem sandbox to allow private sockets; they used offscreen rendering and did not manipulate desktop pointer/focus.

## Handoff

Hosted CI and isolated container runtime are checked on the published implementation revision and recorded by the umbrella coordinator. Historical release deferrals remain unchanged. No actual Viewer system installation or removal was performed.
