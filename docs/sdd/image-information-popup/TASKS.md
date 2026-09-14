# SDD Tasks — image-information-popup

Review corrections are authorized by the user's “fix findings” instruction.
Commands, outcomes and acceptance limitations are recorded in [VERIFICATION.md](VERIFICATION.md).

- [x] T-001: Split ExifDetails exposure into four fields
  - REQs: F-010
  - Check: ExifDetails exposes aperture, shutter, iso, focalLength and no longer has exposure; exif_metadata_test.cpp passes with field-extraction tests.

- [x] T-002: Add pure formatter helper functions
  - REQs: F-004, F-005, F-006, F-012, F-014
  - Check: image_document.{h,cpp} declare and define abbreviateHomePath, formatSummaryLine, formatTransformedLine, formatModifiedText, joinNonEmpty with unit tests in exif_metadata_test.cpp.

- [x] T-003: Add ImageDocument properties and informationSections
  - REQs: F-002, F-003, F-004, F-005, F-006, F-008, F-009, F-011, F-012, F-013, F-014
  - Check: ImageDocument exposes Q_PROPERTYs (summaryLine, transformedLine, modifiedText, displayPath, informationSections); informationSections returns correct QVariantList structure with section labels and lines.

- [x] T-004: Create ImageInformationPopup.qml skeleton and register in build
  - REQs: F-001, C-002, C-003, C-004
  - Check: ImageInformationPopup.qml exists; registered in CMakeLists.txt, scripts/check-qml-format.sh, and Taskfile.yml; qml-format produces no changes.

- [x] T-005: Implement ImageInformationPopup layout and preview
  - REQs: F-003, F-007, F-008, F-015, NF-001, NF-002, NF-003, NF-004
  - Check: Popup renders with fixed header (file name eliding middle), responsive 96px preview (hidden below 360px width), scrollable sections layout, and close button; widths 400/500/1920px render as 376/476/480px respectively.

- [x] T-006: Implement popup close interactions and modal styling
  - REQs: F-015, F-016, F-017, NF-005, F-021, C-005
  - Check: Popup closes via × button, Escape key, and outside click; Overlay.modal alpha 0.22 is lower than Help's 0.5; Accessible.name set to "Image Information"; all user-facing text uses qsTr().

- [x] T-007: Wire popup into Main.qml, make details dialog Help-only, remove informationText
  - REQs: F-001, F-002, F-018, F-019
  - Check: Main.qml opens ImageInformationPopup via informationOpen/Loader with I opening via the window action (enabled by hasPath) and closing via a popup-owned shortcut, the details dialog has no information branch, `grep -rn informationText apps tests` finds nothing, and existing tests referencing the old dialog are migrated so the full suite passes.

- [x] T-008: Add keyboard workflow tests
  - REQs: F-016, F-018, F-019
  - Check: static_workflow_test.cpp passes; I twice opens then closes popup; Escape in fullscreen closes popup without leaving fullscreen; ] pressed while open does not advance image index.

- [x] T-009: Add accessibility tests
  - REQs: F-021, F-022, F-023, F-024
  - Check: accessibility_test.cpp passes; popup Accessible.name correct, section Accessible.names match labels, close button reachable by Tab and activated by Space, path text selectable and copyable via Ctrl+A then Ctrl+C.

- [x] T-010: Add image-information-popup dedicated tests
  - REQs: NF-001, NF-002, NF-003, NF-004, NF-005, C-007, F-007, F-017, F-020
  - Check: image_information_popup_test.cpp passes; responsive layout widths verified, scrolling contentHeight > height on tall content, preview hidden at 340px and visible at 420px, modal dimming alpha assertion, click outside closes / inside does not, standalone 400px load without errors, live transform updates.

- [x] T-011: Visual check of the popup in the real app
  - REQs: F-008, F-009, F-011, NF-005
  - Check: A screenshot of the running viewer with an EXIF-rich JPEG shows uppercase secondary-colored section labels, dot-joined CAMERA/LOCATION lines, `~` path, × button, and the image visibly less dimmed than under Help.

- [x] T-012: QML format check and full test run
  - REQs: C-001, C-003, C-006
  - Check: task format-check passes; ctest runs all tests without failure; no new compiler warnings or qWarning logs during image transitions and popup operations.

- [x] T-013: Verify keyboard scrolling to the file path in a short popup
  - REQs: F-024, NF-002
  - Check: At 400×300 with EXIF and a wrapped path, Tab reveals the focused path cursor; Ctrl+End and Ctrl+Home keep the cursor visible while the header stays fixed. Fix scrolling if the regression test fails.

- [x] T-014: Align review documentation and record full verification
  - REQs: F-004, F-012, F-020, C-006
  - Check: Requirements allow wrapping, README distinguishes document updates from blocked modal shortcuts, backlog links the cycle, and VERIFICATION.md records task check results and visual evidence with limitations.
