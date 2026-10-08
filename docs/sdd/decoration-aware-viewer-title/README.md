# Decoration-aware Viewer toolbar titles

Archived: replaced by [Decoration-independent Viewer](../decoration-independent-viewer/README.md). The requirements below describe historical work, not the current API.

Status: Done (local automated verification)

Work package: I-002

Upstream baseline: `e8f081e3f904d583f6e940f267ed1f00074bb460`

## Requirements and design

Instantiate the provider helper on Main's window. ViewerHeader adds titleVisible=true and binds only headerTitle visibility. Main binds fullscreen || !decoration.externalDecorationPresent for image and grid titles. Preserve height, centered title, actions, tab navigation, menu anchor, native window.title and fullscreen auto-hide. No CSD controls or move/resize regions.

## Files and tasks

- apps/viewer/qml/Main.qml: helper and policy.
- apps/viewer/qml/header/ViewerHeader.qml: heading-only visibility.
- tooling/module.json: enable BUILD_WAYLAND for the provider dependency; otherwise native detection would always return Unknown.
- tests/header_title_test.cpp and tests/CMakeLists.txt: empty/image/grid title visibility, geometry, actions, navigation and menu anchor.

## Verification

Automated verification on 2026-10-05 with Qt 6.11.2 and GCC 16.2.1:

- Provider dependency installed from the modified 43cae7b9 baseline with BUILD_WAYLAND=ON into build/deps/prefix; package metadata and Core QML type are available. The verified provider content is committed locally as aa26e2e.
- Focused `ctest --test-dir build/test -R '^viewer-header-title$' --output-on-failure`: passes. The final strengthened test also passes after a focused rebuild. It checks actual helper/window binding and Unknown policy, empty/image/grid label contents, label-only visibility, title width, action geometry, focus target identities, forward/backward tab links, menu anchor identity and unchanged native title.
- `task check`: passes (debug and release builds, all 27 CTests, format-check, clang-tidy, QML lint, REUSE licensing, staged installation, QML import policy and generated metadata).
- Direct clang-tidy of the new header test: passes. Final git diff whitespace checks: pass.
- The acceptance build recovered an incomplete existing Ninja log; it completed successfully without source diagnostics.

Logs: `/tmp/hn-viewer-check.log`, `/tmp/hn-header-final-build.log`, `/tmp/hn-viewer-new-tidy.log`. The sandbox denied isolated D-Bus socket creation, so Viewer tests and acceptance ran outside the sandbox with offscreen rendering and their private bus.

 Native Wayland SSD, Qt CSD, undecorated, fullscreen entry/exit and surface recreation require user interaction; offscreen tests do not establish native detection correctness. Publication and pins remain pending authorization.

## Publication dependency

Hosted CI currently fetches holonight-qt `f10e8c8ba57282e953f8ddc4a6b0c1109e05a3bf` in scripts/ci/lane.sh. That revision has no HnWindowDecoration. Update that CI dependency only after the completed provider commit is published and available from its canonical remote. No unpublished commit is pinned during this session. Local checks use the changed provider from the requested 43cae7b9 baseline, built with Wayland enabled. The user requested that native manual checks remain pending.
