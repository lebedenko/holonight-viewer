# Shortcut remapping design

Approved through the user-supplied implementation plan on 2026-09-11.

## Key Bindings Architecture

HoloNight Viewer handles keyboard shortcuts primarily through QML `Shortcut` and `Action` elements defined in `apps/viewer/Main.qml`. These interact with `ImageCanvas` and `ImageDocument`.

1. **Previous / Next Navigation (`[` / `]`)**:
   - The navigation shortcuts `sequence: "PgUp"` and `sequence: "PgDown"` are updated to `sequence: "["` and `sequence: "]"` respectively.
   - Qt evaluates `[` as `Qt::Key_BracketLeft` and `]` as `Qt::Key_BracketRight`.
   - The existing guard `!window.modalActive && window.document.canPrevious` (and `canNext`) is retained.

2. **Folder Refresh (`Ctrl+R`)**:
   - Remapped from `sequence: "F5"` to `sequence: "Ctrl+R"`.
   - Rotations use `R` (clockwise) and `Shift+R` (counterclockwise); `Ctrl+R` provides a collision-free, standard desktop refresh shortcut.
   - Retains guard `!window.modalActive && window.document.localPath.length > 0`.

3. **Zoom In / Out (`Ctrl++`, `Ctrl+=`, `Ctrl+-`)**:
   - Replaces un-modified `+`, `=`, and `-` with modifier-prefixed standard desktop shortcuts.
   - For Zoom In, both `Ctrl++` (keypad / explicit plus) and `Ctrl+=` (unshifted equals key on standard keyboards) are bound via `sequences: ["Ctrl++", "Ctrl+="]`.
   - For Zoom Out, `sequence: "Ctrl+-"` is bound.
   - Retains guard `window.canInspect` and active focus redirection to the canvas.

4. **Fit Window (`Ctrl+0`)**:
   - Remapped from single-key `sequence: "0"` to standard `sequence: "Ctrl+0"`.
   - Actual Size remains bound to `1`.
   - Retains guard `window.canInspect` and calls `window.fitImage()`.

5. **Shortcut Help (`?`)**:
   - Remapped on `Action` `shortcutHelp` from `shortcut: "F1"` to `shortcut: "?"`.
   - In Qt, `QKeySequence("?")` corresponds to `Qt::Key_Question`.
   - Retains guard `!window.modalActive`.

6. **UI and Documentation Consistency**:
   - The shortcut help dialog text inside `Main.qml` reflects the new sequences.
   - The bottom caption strip (`Flow` layout) displays `[/]  navigate`, `Ctrl++/−  zoom`, `Ctrl+0  fit`, `1  100%`, `R  rotate`, `F  fullscreen`, `I  information`, `?  help`, `Q  quit`.
   - `README.md` shortcut table and narrative references are updated.

## Test Strategy

All modified shortcuts are covered by existing automated regressions in `tests/`:
- `tests/accessibility_test.cpp`: verify `Qt::Key_Question` triggers Shortcut Help dialog.
- `tests/image_inspection_test.cpp`: verify `Qt::Key_Equal` and `Qt::Key_Minus` with `Qt::ControlModifier`, `Qt::Key_0` with `Qt::ControlModifier`, `Qt::Key_R` with `Qt::ControlModifier`, and `Qt::Key_BracketLeft` / `Qt::Key_BracketRight`.
- `tests/folder_browsing_test.cpp`: verify `Qt::Key_BracketRight` / `Qt::Key_BracketLeft`, `Qt::Key_Plus` with `Qt::ControlModifier`, and `Qt::Key_R` with `Qt::ControlModifier`.
- `tests/static_workflow_test.cpp`: verify `Qt::Key_Question` and `Qt::Key_R` with `Qt::ControlModifier`.
