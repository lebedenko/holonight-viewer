# Decoration-independent Viewer

Work package: I-001. Exact upstream baseline: `c79bb8eff4199b6d7504adb4526672a3274d5652` (origin/main).

## Requirements and design

Fullscreen-only heading and accurate native titles. Follow the [umbrella contract](../../../../docs/initiatives/decoration-independent-viewer/README.md). No decoration settings or desktop heuristics. Preserve unrelated behavior.

## Implementation and verification

Main.qml binds title visibility to fullscreen directly and derives native grid titles from headerTitle, including translated plurals and the scanning folder-only state. Toolbar visibility, actions, anchors and geometry remain unchanged. Removed the obsolete title-policy plan and the detection-only SystemServices tooling dependency. Historical SDDs are explicitly archived.

Tests now exercise actual window bindings through empty/image/grid/scanning transitions, navigation, scan completion, count changes and maximized mode. Existing geometry, focus traversal, menu anchoring and auto-hide coverage remains. Two drag fixtures use the nondeprecated QPointF Qt constructor.

## Verification — 2026-10-08

- Focused actual-binding tests: ViewerHeader.* and GridMode.NativeTitlesAndHeading* passed.
- `HOLONIGHT_DEPENDENCY_PREFIX=/tmp/holonight-decoration-independent/viewer HOLONIGHT_QML_IMPORT_PATH=/tmp/holonight-decoration-independent/viewer/lib/qt6/qml task check` passed: Debug/Release builds, all 27 CTests, formatting, C++/QML lint, REUSE, staged install/desktop launch and QML import/type checks.
- The stage contains the changed Qt provider built with Qt 6.12.0 and BUILD_WAYLAND=ON. Unchanged Config d6a392b41991f70a004d58f7694c7b6115cb7280, Images d834984dc413dc9e56f7f3157fa605d6a8667088 and Thumbnails 2284b1822b0f8f856677e14d21d91e8fa10bd98c artifacts were reused from the existing matching provider records.
- Initial sandbox tests could not create D-Bus sockets; final acceptance ran with IPC permission. The initial lint failure was corrected by the test-only Qt constructor updates.
- Full logs: /tmp/decoration-viewer-final-check.log. Final diff review and removed-API source scan passed.

Native checks passed, confirmed by the user on 2026-10-08 after the requested Hyprland/Sway decoration-enabled/disabled title, toolbar and fullscreen checklist. Publication, deployment and pin updates remain separate and pending.

## Publication preparation

CI provider fetches now match the exact provider revisions used for local acceptance above. Shell syntax and CI launcher regression checks passed before publication.
