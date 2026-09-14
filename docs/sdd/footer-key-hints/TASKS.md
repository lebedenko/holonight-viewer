# SDD Tasks — footer-key-hints

- [x] T-001: Create FooterKeyHints.qml component with hints model and delegates
  - REQs: F-001, F-002, F-003, F-004, F-006, NF-001, NF-002, NF-003, C-003
  - Check: FooterKeyHints.qml created with Flow root (objectName "footer"), JS array of 7 hints in spec order, Repeater with Row delegates containing HnKeyHint + HnLabel, Accessible.role/name overrides on Row and Accessible.ignored on children, 4px intra-hint spacing, no color/font literals.

- [x] T-002: Register FooterKeyHints in CMakeLists.txt and replace inline footer in Main.qml
  - REQs: C-002
  - Check: apps/viewer/CMakeLists.txt adds FooterKeyHints.qml to QML_FILES list; Main.qml lines 807-822 replaced with single FooterKeyHints element with Layout.fillWidth, Layout.leftMargin, Layout.rightMargin, Layout.bottomMargin properties; git diff shows only footer block modified.

- [x] T-003: Create tests/footer_key_hints_test.cpp with content order and count test
  - REQs: F-001, F-003, F-004
  - Check: ContentOrderAndCount test loads Main via loadFromModule, finds footer, asserts exactly 7 footerHint* children in order (Navigate, Zoom, Fit, ActualSize, Rotate, Fullscreen, Help), compares keycap text to spec, confirms no "Quit" or "Information" text present.

- [x] T-004: Add keycap/label pairing and spacing verification tests
  - REQs: F-002
  - Check: KeycapLabelPairingAndSpacing test verifies per hint that label x > keycap x, |keycap centerY − label centerY| ≤ 1 px, and QQmlProperty reads confirm Flow.spacing (12) > Row.spacing (4).

- [x] T-005: Add 400px width wrapping test for narrower layouts
  - REQs: F-005
  - Check: NarrowWidthWraps test instantiates FooterKeyHints standalone, sets width to 400, verifies every hint fits within bounds (x+width ≤ 400), at least two distinct hint y positions exist, and each keycap/label pair shares same row within 1 px tolerance.

- [x] T-006: Add persistent visibility and accessibility exposure tests
  - REQs: NF-001, NF-002, NF-003
  - Check: PersistentVisibility test asserts footer visible in empty state, after sample.png loads, and after shortened timer events (mimicking mouse movement scenario); AccessibleNaming test verifies 7 StaticText nodes with names "<key> <label>"; NotFocusable test confirms no hint has activeFocusOnTab true and focusPolicy is NoFocus; existing Tab-cycle test still passes.

- [x] T-007: Add visual capture test and update scripts/check-visual.sh
  - REQs: F-002, F-005, F-006
  - Check: FooterKeyHints.VisualCapture test added to footer_key_hints_test.cpp with grabWindow calls at 420×280, 1000×700, and 400 px standalone widths guarded by VIEWER_CAPTURE_PREFIX environment variable; check-visual.sh filter list includes new test.

- [x] T-008: Full build, lint, and regression test cycle; README review
  - REQs: C-001
  - Check: cmake --build succeeds with no errors; qmllint reports no unresolved-type or missing-property warnings for HnKeyHint; full existing test suite passes unchanged; README.md reviewed for footer-related documentation (no update needed per DESIGN decision 7, unless reviewers request Quit/Information removal callout).
