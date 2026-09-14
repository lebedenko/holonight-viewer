# Viewer UI Polish — Verification

## Scope and environment

The 2026-09-14 review and user-authorized corrections cover
[SPEC.md](SPEC.md), [DESIGN.md](DESIGN.md) and [TASKS.md](TASKS.md), implementing
the scoped portions of `/home/andrii/Documents/HoloNight/viewer/viewer-review-1.md`.
Header sizing, header/footer auto-hide, thumbnails and the prior Information
popup redesign remain outside this cycle.

Checks use the installed HoloNight packages in `build/deps/prefix`, Qt 6.11.2,
the offscreen platform and software rendering. No provider sources are changed
or rebuilt. Generated logs, fixtures and captures stay under `build/`.

## Review and regression evidence

| Command/check | Outcome | Evidence |
| --- | --- | --- |
| Initial `task test` | All 8 CTest targets passed; the old tests missed keycap/description overlap | `build/viewer-ui-polish-review-tests.log` |
| Initial `task format-check` / `task qml-lint` | Passed | `build/viewer-ui-polish-review-format.log`, `build/viewer-ui-polish-review-qmllint.log` |
| Standalone Help layout probe with `tests/fixtures/dark.toml` | Zoom keycap width 155.90625 px; description x=146, confirming overlap | `build/viewer-ui-polish-review-layout.log` |
| `ShortcutHelpPopup.KeycapsStayWithinColumns` before the QML correction | Failed as expected at 12/18 pt, including the 1000/420/400 px hosts | `build/viewer-ui-polish-fixes-regression-before.log` |

The fix constrains each `HnKeyHint` to its column and wraps its themed code text.
The wrapped implicit height expands the row; descriptions retain their aligned
column. Accessibility still exposes one combined row and ignores keycap text.
The new regression walks visual children and verifies keycap, rendered text and
row bounds plus separation from descriptions. Merely checking the parent row's
width did not detect the original defect.

## Correction verification

| Command/check | Outcome | Evidence |
| --- | --- | --- |
| Post-fix `ShortcutHelpPopup.*` | All 6 tests passed, including bounds, accessibility, modal behavior and resize settling | `build/viewer-ui-polish-fixes-regression-after.log` |
| Full `task check` | Debug/Release builds, all 8 CTest targets (48.74 s), formatting, clang-tidy and QML lint passed; stopped at REUSE's sandbox socket error | `build/viewer-ui-polish-fixes-check.log` |
| `task license-check` outside the sandbox | Passed; 166/166 files licensed | `build/viewer-ui-polish-fixes-license.log` |
| `task install-check` | Passed staged payload, four-format CLI checks and installed GIO launch | `build/viewer-ui-polish-fixes-install.log` |
| `task uninstall-check` | Passed in disposable staged filesystems | `build/viewer-ui-polish-fixes-uninstall.log` |
| Dark/light scale captures | All 6 runs passed (5 tests per run) at 1×, 1.25× and 1.5× | `build/viewer-ui-polish-visual/{dark,light}-{1,1.25,1.5}.log` |

REUSE's multiprocessing forkserver failed to bind a local socket with
`PermissionError: [Errno 1] Operation not permitted`. The authorized rerun
outside the sandbox passed, and the remaining aggregate-check steps were run
separately. This is not a claim that `task check` exited zero. Installation and
removal checks use staged trees; no host installation or removal is performed.

Focused command after rebuilding with `cmake --build --preset test`:

```sh
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
build/test/tests/viewer-smoke --gtest_filter='ShortcutHelpPopup.*'
```

Visual checks use `HOLONIGHT_APPEARANCE_FILE=tests/fixtures/{dark,light}.toml`
(resolved to an absolute path), `QT_SCALE_FACTOR=1/1.25/1.5`, and
`VIEWER_CAPTURE_PREFIX` under `build/viewer-ui-polish-visual/`. The filter is
`ShortcutHelpPopup.KeycapsStayWithinColumns:ShortcutHelpPopup.WidthsAndScrollingWithFixedHeader:ShortcutHelpPopup.RowsAreExposedOnceToAccessibility:Viewer.StaticWorkflowControls:Viewer.WindowAndKeyboard`.
This captures Help at 12/18 pt, the grouped menu, and empty-state layouts, while
also checking short-popup scrolling and row accessibility at each scale.

Rendered inspection of the dark 1× Help at both text sizes and the light 1.5×
Help at 18 pt confirms wrapped keys with clear separation from descriptions.
The inspected dark 1× and light 1.5× 420px menu captures keep the shortcut column
clear of the scroll bar; the light 1.5× minimum-window empty state retains both
hint lines while hiding the glyph. These are offscreen captures, not a native
desktop walkthrough. Representative files:

- `build/viewer-ui-polish-visual/dark-1-help-12pt.png`
- `build/viewer-ui-polish-visual/dark-1-help-18pt.png`
- `build/viewer-ui-polish-visual/light-1.5-help-18pt.png`
- `build/viewer-ui-polish-visual/dark-1-workflow-menu-420.png`
- `build/viewer-ui-polish-visual/light-1.5-workflow-menu-420.png`
- `build/viewer-ui-polish-visual/light-1.5-empty-420x280.png`

## Acceptance limits

Offscreen checks do not establish native compositor behavior or human screen
reader acceptance. Native UI walkthrough and Docker installed-runtime
qualification have not been performed for this cycle; these remain explicit
follow-up qualification, not passing results. Existing release deferrals in
the backlog remain unchanged.
