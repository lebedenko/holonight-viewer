# SDD Tasks — viewer-ui-polish

Each task includes the test updates its change forces, so the build and the full ctest preset stay green after every task.

- [x] T-001: Remap flips to X / Shift+X and update the existing H/V tests
  - REQs: REQ-F-027, REQ-F-029, REQ-C-008
  - Check: In static_workflow_test, X and Shift+X change the orientation (including with a header button focused), while H, V, Shift+H, Shift+V, X with no image leave it unchanged, X with the menu open flips and closes the menu (like R), and the full ctest preset passes.

- [x] T-002: Extend ViewerMenuItem with shortcutText and a two-label layout; widen the menu to 380 px
  - REQs: REQ-F-006
  - Check: Opening the menu with no other changes shows every existing item's label unchanged, and a menu item given shortcutText "Ctrl+O" renders a right-aligned caption whose left edge is right of the label's right edge, with the full ctest preset passing.

- [x] T-003: Reorder the menu, add six separators, set labels/shortcut texts, rename the accessible name to "Menu"; recompute existing currentIndex literals
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-028
  - Check: static_workflow_test and accessibility_test pass with currentIndex expectations derived from the new 25-slot model (Down from "Refresh" makes "Previous" current), and actionsButton's accessible name is "Menu".

- [x] T-004: Add tests/menu_layout_test.cpp
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-009, REQ-NF-001, REQ-NF-004
  - Check: menu_layout_test is registered in tests/CMakeLists.txt and passes, and it fails if any two items are swapped, a separator is removed, a shortcut text differs from the REQ-F-005 table, or Fit's shortcut label is not textDisabled with no image open.

- [x] T-005: Create ShortcutHelpPopup.qml (sections model, card, header with ×, scrolling rows) and register it in CMake, the format script and Taskfile
  - REQs: REQ-F-010, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-017, REQ-C-002, REQ-C-003
  - Check: The viewer builds with ShortcutHelpPopup.qml in the QML module, the QML format check passes, and a standalone load of the popup exposes `sections` with 6 sections and 19 rows matching DESIGN §3.4.

- [x] T-006: Replace the Basic.Dialog Help with ShortcutHelpPopup (helpOpen, ? toggle inside the popup); update tests referencing detailsDialog/detailsText/closeDetailsButton and DimsLessThanShortcutHelp
  - REQs: REQ-F-010, REQ-F-011, REQ-F-016, REQ-C-004
  - Check: No file under apps/ or tests/ references detailsDialog, detailsText, closeDetailsButton or detailDialog; pressing ? twice opens then closes shortcutHelpPopup; and the full ctest preset passes.

- [x] T-007: Add tests/shortcut_help_popup_test.cpp
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-NF-001, REQ-NF-004
  - Check: shortcut_help_popup_test is registered and passes, covering width 480/396/376, the 0.22 dimmer, close button, Esc/outside-click/? close, exact row contents, no Tab/J/K rows, scrolling at 420×280 with a fixed header, `[` blocked while open, and one StaticText per row.

- [x] T-008: Add the empty-state Column with two hint lines and glyph shrink/hide sizing
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-NF-002
  - Check: With no image open at 1000×700, emptyStateHintPrimary reads "No image open" and emptyStateHintSecondary reads "Ctrl+O to open · or drop an image here", both below the emptyState glyph, and the full ctest preset passes.

- [x] T-009: Add tests/empty_state_test.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-NF-001
  - Check: empty_state_test is registered and passes, and it fails if the hint text shrinks at 420×280, if the glyph stays visible when the available height is below the hide threshold, or if the hint is visible in the Ready or Error state.

- [x] T-010: Implement the transient controller for arrows (shown, 2 s timer, hover pause, keyboard hide, 150 ms fade) and migrate IndependentOverlayTimers off arrowTimer.running
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-022, REQ-F-025
  - Check: previousButton.shown and nextButton.shown are true after a pointer move and false by 2.2 s, stay true while nextButton is hovered, and no test reads arrowTimer.running, with the full ctest preset passing.

- [x] T-011: Implement the HUD controller (detailsShown, 3 s timer, fade, revealDetails from first render, pointer move, fit/actual size/zoom/wheel/transform; hide on leaving Ready) and migrate tests off detailsTimer.running
  - REQs: REQ-F-023, REQ-F-024, REQ-F-025, REQ-F-026, REQ-NF-003
  - Check: With the HUD hidden, pressing Ctrl++ sets detailsStrip.shown to true without pointer movement, resizing the window does not, and no test reads detailsTimer.running, with the full ctest preset passing.

- [x] T-012: Add tests/transient_overlay_test.cpp
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025, REQ-F-026, REQ-NF-003
  - Check: transient_overlay_test is registered and passes, covering the 1.8 s/2.2 s and 2.8 s/3.2 s bounds, restart on a second trigger, hover pause, keyboard hide, mid-fade opacity, no click/accessibility node after the fade, every REQ-F-023 key trigger, no reveal on resize or in Empty/Error, and a drag during the fade still panning.

- [x] T-013: Final audit and full verification
  - REQs: REQ-NF-001, REQ-NF-002, REQ-NF-004, REQ-NF-005, REQ-C-001, REQ-C-005, REQ-C-006, REQ-C-007, REQ-F-030
  - Check: The full ctest preset, QML format check and QML lint pass; git diff shows no production .cpp/.h or holonight-qt changes, no color literals or font.family/pixelSize, and no user-visible string outside qsTr() in changed QML; and footer_key_hints_test passes with FooterKeyHints.qml unmodified.
  - Evidence: [VERIFICATION.md](VERIFICATION.md) records passing local checks, the corrected Help overlap and the successful license rerun after a sandbox interruption. Native desktop and Docker qualification remain pending separately below.

- [x] T-014: Constrain and wrap Help keycaps; verify their rendered bounds
  - REQs: REQ-F-015, REQ-F-017, REQ-NF-001
  - Check: `ShortcutHelpPopup.KeycapsStayWithinColumns` checks keycaps, text bounds, description separation and row height at 1000/420/400 px hosts with 12/18 pt key text. Existing accessibility and layout-settling tests pass. Capture dark/light rendering at normal and fractional scales.
  - Evidence: Before/after regression, six dark/light scale runs and inspected captures in [VERIFICATION.md](VERIFICATION.md).

- [x] T-015: Record verification and align cycle documentation
  - Check: Record commands, outcomes and limitations in VERIFICATION.md; update SPEC, DESIGN, README and backlog; keep pending checks explicit.

## Pending qualification

- [ ] Native desktop and screen-reader walkthrough for this cycle.
- [ ] Separate Docker installed-runtime qualification for this cycle.

These checks are not established by the passing offscreen tests and staged
installation checks. Existing release deferrals remain unchanged.
