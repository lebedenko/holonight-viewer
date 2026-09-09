# Mockup UI verification

Scope: approved Viewer portion of `docs/mockups/moc1.png`; installed HoloNight
packages, with no provider source modifications. Evidence is local and generated
under `build/mockup-ui/` and `build/visual/`.

| Requirements | Evidence |
| --- | --- |
| R1 | WindowAndKeyboard preserves native-decoration flags and fullscreen restoration; StaticWorkflowControls exercises the expanded scrollable menu with keyboard activation. Theme/scale captures show the centered header and wrapping footer. |
| R2 | IndependentOverlayTimers checks default five-second intervals, movement resets in header/footer, immediate Tab dismissal, and non-consuming input. FolderBrowsingControls exercises mouse-revealed arrows and boundaries. |
| R3 | FirstRenderGeneration rejects queued stale paints, emits once per image, does not repeat for zoom/pan/rotation, and recognizes clear/reselect of a cached image. IndependentOverlayTimers checks timeout independence, no repaint reveal, refresh, keyboard navigation and cached return. Existing document tests cover stale asynchronous loads. |
| R4 | IndependentOverlayTimers checks the metadata snapshot formatter and unchanged image geometry across overlay timeout. Geometry/inspection tests cover Fit, actual size, fractional physical pixels, pan and rotation. Captures cover narrow/wide wrapping and scale text. |
| R5 | InspectionControlsAndLifecycle retains wheel/drag/keyboard input. OpeningCanvasAndAdapters retains drop/Open paths. Accessibility tests cover names, roles, enabled states, focus, menu access, announcements and dialog Close activation. StaticWorkflowControls covers information/help and fullscreen modality. |

Commands and outcomes (final local verification passed):

- `task deps`: passed; providers built/installed beneath `build/deps/`, consumed through installed package paths.
- `task build`, `task test`: passed. Final rebuild/CTest passed all 7 targets (42 smoke cases passed, 3 opt-in cases skipped), recorded in `test-final-build.log` and `test-final.log`.
- `task build PRESET=release`: passed (`release.log`, `release-final.log`).
- `task format`, `task format-check`: passed (`format-check.log`).
- `task tidy`: passed (`tidy.log`); first-pass missing-braces diagnostics were corrected. The final modified workflow test also passed a focused clang-tidy rerun (`tidy-final-test.log`).
- `task qml-lint`: passed (`qml-lint.log`).
- `task license-check`: passed (`license-check.log`). The first sandboxed invocation could not create REUSE's multiprocessing socket; the approved unsandboxed rerun passed.
- `task install-check`: passed (`install-check.log`).
- `task visual-check`: passed all six combinations of dark/light and 1/1.25/1.5 scale (`visual.log`); the final capture script rerun also passed all 36 executions (`visual-final.log`). Captures under `build/visual/` include narrow/wide overlays, focus transitions, fullscreen, menus, information/help, Fit and actual-size/pan views.

Visual inspection found and corrected excess strip width, split shortcut hints,
and an absent dialog Close control. Final dark and light fractional-scale dialog
captures confirm the explicit Close button is visible, and its activation test
passes. Wide/narrow overlays, focus transitions and fullscreen captures were
inspected. The scroll assertion now checks visibility of the final text line;
it no longer demands an offset when only trailing padding exceeds the viewport.

Local acceptance is complete. There are no blocked checks within the automated
and offscreen scope above.

Limitations: captures and the standard test suite use Qt's offscreen/software
platform. They verify client-area presentation and fullscreen state, not a real
compositor's decorations or physical mixed-DPI monitor transitions. The standard
suite skips opt-in large-folder/performance and native separate-process clipboard
checks; existing release/native acceptance gates in other cycles remain open.
No new provider, clipboard protocol or decoder behavior is claimed by this cycle.

## Point-only typography correction (R6)

Audited font-size assignments throughout Viewer QML/C++ and installed HoloNight
QML. Removed the sole Viewer `font.pixelSize` assignment from ViewerButton; it
now inherits the shared button’s `font.pointSize: HolonightTheme.bodySize`. No
pixel-based font assignments remain in Viewer sources or installed HoloNight
QML (the provider has one pixel-size read used for height estimation).

`task deps`, debug/release builds, all seven test targets, format-check, tidy,
QML lint, license-check and install-check pass. An offscreen startup probe and
the complete test runtime log contain no “Both point size and pixel size set”
warning. Evidence: `build/typography/`. No provider sources were changed.

## Header SVGs and menu inset (R7)

Replaced font glyphs with three original outline SVG resources (information,
fullscreen, menu). Shared button rendering tints the icons for the active theme
and disabled state. Icon geometry uses the 24-unit Hero token inside 40-unit
Large controls; no pixel-based font settings were introduced. The popup is
right-aligned beneath its button with a 12-unit client-edge inset and the shared
normal spacing between button and popup.

All required local checks pass: dependencies, debug/release build, all seven
CTest targets, formatting, tidy, QML lint, licensing and staged installation.
The final visual matrix passes all 36 executions across dark/light themes and
100%, 125%, 150% scale. Inspected wide header and narrow-menu captures in dark
and fractional-scale light themes: icons are correctly tinted and consistently
sized, and the menu keeps its inset and right alignment. No missing SVG-resource
or conflicting font-size warnings were reported. Logs are under
`build/header-icons/`; final captures remain under `build/visual/`. These visual
checks use the existing offscreen/software scope described above.

### Header icon binding-loop correction

Replaced `icon.height: icon.width` with a direct binding to the same shared
size token used by width. This removes the grouped-property dependency without
changing dimensions. Debug/release builds, all seven CTest targets, QML lint,
format-check and installation pass. Explicit dark/light startup probes at 125%
scaling with an image loaded report no binding-loop or conflicting font-size
warnings; the full CTest runtime log also contains neither warning. Evidence:
`build/icon-binding/`.

## Metadata sections (R8)

Replaced the pipe-delimited metadata string with an array-backed Repeater. Each
section uses a plain-text HnLabel and vertical HnSeparator. Field order, reactive
formatting, filename elision, wrapping and overlay timing remain intact.

Passed `task deps`, `task build`, `task test` (all seven CTest targets),
`task build PRESET=release`, `task format`, `task format-check`, `task tidy`,
`task qml-lint`, `task license-check`, `task install-check` and `task visual-check`.
The license check initially hit a sandbox socket restriction; its authorized
outside-sandbox rerun passed. All 36 visual test executions passed across dark/light
themes and 100%, 125%, 150% scale. Inspected normal/narrow dark captures and the
narrow light capture at 125%: separators render and filename elision/wrapping remain
readable. No QML binding-loop or reference/type errors appeared in runtime logs.

Evidence: `build/metadata-*.log`, `build/test/Testing/Temporary/LastTest.log` and
`build/visual/` captures. Visual verification uses offscreen/software rendering;
earlier native-desktop acceptance limitations remain open independently.
