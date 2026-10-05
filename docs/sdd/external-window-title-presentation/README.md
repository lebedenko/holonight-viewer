# External window title presentation

Work package: I-004. Baseline: `bcfdf7fa7324952dd749f8170fee00413b48d59a`.

See the umbrella initiative for settled contracts and scope. Implementation remains local; publication and integration are pending.

## Requirements

Bind titleVisible to fullscreen or state other than Present. Preserve centered heading layout, header geometry, actions, focus navigation, menu anchor, native window.title and fullscreen auto-hide.

## Implementation

Main.qml binds heading visibility to fullscreen or presentation state other than Present. Unknown retains the heading. Native title, header height, centered layout, actions, focus traversal, menu anchoring and fullscreen auto-hide retain their existing behavior. Dependency bootstrap supplies Compositor before Qt.

## Verification — 2026-10-05

- Clean acceptance CTest: all 27 tests passed.
- Full `task check` passed, including debug/release builds, tests, formatting, lint, QML checks, licensing and staged installation.
- Header fixtures cover empty, image and grid headings with visible/hidden geometry, action and keyboard/menu behavior.
- Native Hyprland/Sway/Qt-CSD checks remain manual and pending. Existing untracked paste-image verification report was preserved.
