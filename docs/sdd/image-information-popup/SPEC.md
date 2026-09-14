# Image Information Popup Redesign — SPEC

## Overview

The Image Information popup is a modal overlay that presents structured metadata about the currently viewed image in the HoloNight Viewer. It replaces a shared Basic.Dialog (with detailDialog===1) that previously showed plain-text EXIF information. The redesign introduces a dedicated QML component with:

- A fixed header (file name, summary line, transformed-dimensions line, modified date)
- A 96px image preview (shown when document is Ready)
- Organized sections (CAMERA, LOCATION, FILE) with structured data
- Responsive layout that adapts to narrow windows
- Keyboard and mouse closure mechanisms (× button, Escape, I-key toggle, click-outside)
- Live updates when document content changes
- Lighter modal dimming than the existing Shortcut Help, so the image remains visible

The Shortcut Help dialog keeps its current content and appearance (it may keep using the existing dialog plumbing, now dedicated to Help).

---

## Non-Goals

- Redesign of the Shortcut Help dialog
- GPS map links or external navigation
- Per-field copy actions (beyond the existing ability to select and copy file path text)
- Metadata HUD changes
- Thumbnail strip or multi-image carousel
- Navigation ([ / ] keys) within the popup; navigation is blocked while open

---

## Glossary

| Term | Definition |
|------|-----------|
| ImageInformationPopup.qml | New dedicated QML component for displaying image metadata |
| ImageDocument | C++ class that holds image state; exposes structured EXIF/file metadata |
| ExifDetails | C++ struct or class holding camera and location metadata |
| Transformed-dimensions | A line shown only when the applied rotation swaps width and height (90°/270°), indicating the view orientation |
| Modal dimming | Semi-transparent overlay that darkens the background when the popup is open |
| EARS | Easy Approach to Requirements Syntax; five template forms for writing requirements |

---

## Functional Requirements

### F.1 Component Architecture

**REQ-F-001 (Ubiquitous — Component Separation)**

The Image Information popup shall be implemented as a dedicated QML component named `ImageInformationPopup.qml`, physically separate from the existing Shortcut Help dialog in `apps/viewer/Main.qml`.

**Acceptance Criteria:**
- A new file `apps/viewer/ImageInformationPopup.qml` exists and is registered in `CMakeLists.txt`
- The file is listed in `scripts/check-qml-format.sh` and the `Taskfile.yml` format task
- `Main.qml` no longer shows image information through the shared details dialog; it instantiates `ImageInformationPopup`
- Shortcut Help opens via `?` and the menu, and shows the same shortcut text as before

**REQ-F-002 (Ubiquitous — Data Model Replacement)**

The C++ class `ImageDocument` shall expose structured image metadata to QML via properties instead of a single `informationText()` method.

**Acceptance Criteria:**
- `ImageDocument` exposes QML-readable properties for the header (`fileName`, summary line, transformed-dimensions line, modified date) and an ordered list of sections, each with a label and a list of non-empty value lines, plus the display path
- The method `ImageDocument::informationText()` is removed entirely
- C++ unit tests confirm the properties for: EXIF-rich JPEG, image without EXIF, failed load, and a non-local document

### F.2 Header Content

**REQ-F-003 (Ubiquitous — File Name Display)**

The popup header shall display the image file name as a subheading with middle-text elision.

**Acceptance Criteria:**
- The file name appears as a heading line in the popup
- When the name exceeds available width, it elides in the middle (e.g., "very_long_file_name_here.jpeg" becomes "very_long_file_…_here.jpeg")

**REQ-F-004 (Ubiquitous — Summary Line)**

The popup header shall display a summary string formatted as "FORMAT · W × H · N.N MP · SIZE" (e.g., "JPEG · 3072 × 4080 · 12.5 MP · 2.5 MB").

**Acceptance Criteria:**
- A summary line appears below the file name
- The line contains: image format, dimensions (width × height), megapixels, and file size
- Elements are joined with middle-dot separators (" · ")
- The line is one logical row of metadata; it may wrap when width constraints make single-line rendering impossible.
- If any element is unavailable, it is omitted (e.g., if dimensions are unknown, both dimensions and megapixels are omitted: "JPEG · 2.5 MB")
- C++ unit tests verify the summary string formatting for various image dimensions and formats

**REQ-F-005 (State-driven — Transformed-Dimensions Line)**

While the current view transform swaps the image width and height (90° or 270° rotation applied), the popup shall display a transformed-dimensions line (e.g., "Rotated view 4080 × 3072").

**Acceptance Criteria:**
- A transformed-dimensions line appears below the summary line only when a 90° or 270° rotation is applied to the view
- The line reads "Rotated view W × H" where W and H are the post-rotation dimensions
- Flip transforms (mirror or vertical flip without rotation) do not trigger this line
- C++ unit tests verify the line is empty with no transform, empty after a flip only, and "Rotated view H × W" after one clockwise rotation

**REQ-F-006 (Ubiquitous — Modified Date)**

The popup header shall display the image file's modified date formatted using the system locale's short date and time format.

**Acceptance Criteria:**
- The modified date appears in the header (below the summary/transformed-dimensions lines)
- The date is formatted according to the system locale, not a hard-coded pattern (e.g., on a US system: "09/14/26 3:45 PM"; on a German system: "14.09.26 15:45")
- A C++ unit test with `QLocale::setDefault` to two different locales yields strings equal to `QLocale().toString(modified.toLocalTime(), QLocale::ShortFormat)` for each
- If the modified time is unknown, the date line is omitted

**REQ-F-007 (State-driven — Image Preview)**

While the image document is Ready, the popup shall display a 96px × 96px preview image at the left side of the popup.

**Acceptance Criteria:**
- A thumbnail-size preview (96px) is displayed alongside the popup content when the document is Ready
- The preview is an ImageCanvas or equivalent component showing the current image at 96px size
- While the document is loading or in a failed state, no preview is shown
- The preview updates when the document changes (e.g., when navigating to a new image)

### F.3 Structured Data Sections

**REQ-F-008 (Ubiquitous — Section Labels)**

Each data section in the popup shall begin with an uppercase label (CAMERA, LOCATION, FILE) on its own line.

**Acceptance Criteria:**
- Section labels appear in uppercase text (e.g., "CAMERA", "LOCATION", "FILE")
- Each label is placed on its own line above the section's values
- Labels are styled in secondary or accent text color (per HoloNight design system)
- A QML test confirms each section label item's color equals the secondary/accent palette token and differs from the value text color

**REQ-F-009 (Ubiquitous — Camera Section)**

The popup shall display camera metadata in three lines below the CAMERA label.

**Acceptance Criteria:**
- Line 1: camera make and model (e.g., "Google Pixel 7 Pro")
- Line 2: lens, focal length, and aperture joined with " · " (e.g., "Pixel 7 Pro back camera · 6.9 mm · f/2.2")
- Line 3: shutter speed, ISO joined with " · " (e.g., "1/20 s · ISO 1423")
- All values are extracted from `ExifDetails` (not a single text blob)
- Missing values are omitted from their respective lines (e.g., if lens data is absent, Line 2 reads "6.9 mm · f/2.2")
- A line with all parts empty is omitted entirely
- C++ unit tests verify the three lines for a full EXIF fixture and for a fixture missing lens and ISO

**REQ-F-010 (Ubiquitous — ExifDetails Field Split)**

The C++ `ExifDetails` struct shall expose camera metadata as separate fields: `aperture`, `shutter`, `iso`, `focalLength`.

**Acceptance Criteria:**
- `ExifDetails` contains individual fields for aperture, shutter speed, ISO, and focal length (rather than a pre-formatted string)
- C++ unit tests confirm that these fields are populated correctly from the image file's EXIF data
- The fields support both present and absent states (e.g., some images may not have lens metadata)

**REQ-F-011 (Ubiquitous — Location Section)**

The popup shall display location metadata below the LOCATION label in the format "COORDINATES · ALTITUDE" (e.g., "49.79849° N, 24.02951° E · 386 m").

**Acceptance Criteria:**
- Coordinates are displayed as decimal degrees with directional indicators (N/S, E/W)
- Altitude is displayed in meters with the "m" unit suffix
- Elements are joined with " · " (middle-dot separators)
- Missing elements are omitted (e.g., if altitude is unavailable, the line reads "49.79849° N, 24.02951° E")
- If all location data is absent, the entire LOCATION section is omitted
- C++ unit tests verify coordinate and altitude formatting

**REQ-F-012 (Ubiquitous — File Section)**

The popup shall display the image file path below the FILE label, with the user's home directory displayed as "~".

**Acceptance Criteria:**
- The file path appears as one logical line under the FILE label
- Paths beginning with the user's home directory are abbreviated (e.g., "/home/alice/Pictures/image.jpg" displays as "~/Pictures/image.jpg")
- The path text is selectable via mouse and keyboard selection
- The path line is allowed to wrap in narrow layouts so long paths remain readable within popup width.
- The existing Copy Path action (Ctrl+Shift+C, available when the popup is closed) still copies the absolute path; selecting and copying the displayed text copies the displayed (abbreviated) text
- Non-local documents (with no local path) do not display a FILE section
- C++ unit tests verify "~" abbreviation for a path under home, no abbreviation for a path outside home, and no abbreviation for a sibling like "/home/alice2/x" when home is "/home/alice"

**REQ-F-013 (Ubiquitous — Section Omission)**

If a section has no displayable lines (all parts empty or unavailable), the popup shall omit that section entirely.

**Acceptance Criteria:**
- An image with no EXIF camera data displays no CAMERA section
- An image with no location data displays no LOCATION section
- A non-local document displays no FILE section
- Visual inspection or unit tests confirm that empty sections do not appear

**REQ-F-014 (Unwanted behaviour — No Summary Data)**

If format, dimensions and file size are all unavailable, then the popup shall display "Details unavailable" in place of the summary line.

**Acceptance Criteria:**
- A C++ test with a document whose format, dimensions and size are unknown yields the summary "Details unavailable"
- A document with only file size known yields the size alone, not "Details unavailable"
- Sections are still governed by REQ-F-013 (e.g., FILE still shows for a local failed file)

### F.4 User Interaction

**REQ-F-015 (Event-driven — Close Button)**

When the user clicks a close button at the top-right of the popup, the popup shall close.

**Acceptance Criteria:**
- A small × icon button is positioned at the top-right of the popup
- Clicking the button closes the popup
- The button is keyboard-focusable (reachable by Tab key)
- The button has an accessible name such as "Close Image Information"

**REQ-F-016 (Event-driven — Escape Key Close)**

When the user presses the Escape key while the popup is open, the popup shall close.

**Acceptance Criteria:**
- Pressing Escape dismisses the popup
- Escape does not leave fullscreen while the popup is open (only the popup closes)
- After closing, keyboard shortcuts (e.g. `]`) work again without clicking

**REQ-F-017 (Event-driven — Click-Outside Close)**

When the user clicks outside the popup (on the modal dimming), the popup shall close.

**Acceptance Criteria:**
- Clicking the semi-transparent overlay behind the popup closes the popup
- Clicking inside the popup content does not close it
- An automated test clicks the overlay outside the popup and asserts the popup is closed

**REQ-F-018 (Event-driven — I Key Toggle)**

When the user presses I while the popup is open, the popup shall close; when the user presses I while it is closed and no other modal is open, the popup shall open.

**Acceptance Criteria:**
- An automated test presses I twice and asserts the popup opened then closed
- Pressing I while Shortcut Help is open does not open the Information popup

**REQ-F-019 (State-driven — Navigation Block)**

While the Image Information popup is open, the `[` and `]` keys shall not advance to the previous or next image.

**Acceptance Criteria:**
- An automated test opens a folder of ≥2 images, opens the popup, presses `]`, and asserts the current index is unchanged
- After closing, pressing `]` advances the index

**REQ-F-020 (State-driven — Live Updates)**

While the Image Information popup is open, if the underlying image document changes (e.g., via refresh or a transform), the displayed information shall update automatically via QML bindings.

**Acceptance Criteria:**
- All fields (header, sections, preview) are bound to properties on the image document
- Changing the image or refreshing metadata updates the popup content without requiring a manual close/reopen cycle
- No console warnings or runtime errors occur during live updates
- An automated test changes the document (e.g., rotates via the document API) while the popup is open and asserts the transformed-dimensions line appears

---

## Non-Functional Requirements

**REQ-NF-001 (Responsive Layout — Width)**

The Image Information popup's width shall be `min(480, window_width − 24)` to ensure padding on narrow screens.

**Acceptance Criteria:**
- On a 1920px wide window, the popup is 480px wide
- On a 400px wide window, the popup is 376px wide (400 − 24)
- On a 500px wide window, the popup is 476px wide (500 − 24)
- An automated layout test asserts these widths

**REQ-NF-002 (Responsive Layout — Scrolling)**

The popup's body (section content) shall scroll vertically when content exceeds available height, while the header row (title + ×) remains fixed.

**Acceptance Criteria:**
- When sections extend beyond the available viewport height, a vertical scrollbar appears in the body area
- The header row (file name, × button) stays visible and does not scroll
- At a 400×300 window with an EXIF-rich image, a test asserts the body Flickable's contentHeight > height and the header row's y is unchanged after scrolling.
- With a wrapped FILE path whose cursor is initially clipped, Tab reveals the cursor. Ctrl+End and Ctrl+Home move to the corresponding text ends and keep the complete cursor rectangle inside the viewport while the header remains fixed.

**REQ-NF-003 (Responsive Layout — Preview Hidden on Narrow)**

Below approximately 360px window width, the 96px image preview shall be hidden to preserve space.

**Acceptance Criteria:**
- On windows narrower than ~360px, the preview image is not displayed
- The popup content adapts to a single-column layout without the preview
- An automated test at 340px window width asserts the preview item is not visible, and at 420px asserts it is visible

**REQ-NF-004 (Responsive Layout — Content Width)**

The popup shall work correctly at window widths below 420px without layout breakage.

**Acceptance Criteria:**
- At 400px window width, all text is readable and no elements overflow
- Section labels and values are properly formatted
- Long file paths wrap rather than overflow the popup width
- A test at 400px width asserts every text item's right edge is within the popup's content width

**REQ-NF-005 (Modal Dimming — Lighter Than Help)**

The modal dimming (semi-transparent overlay) behind the Image Information popup shall be lighter than the dimming used by the Shortcut Help dialog.

**Acceptance Criteria:**
- A test reads the Information popup's modal overlay color alpha and asserts it is lower than the Shortcut Help overlay alpha

---

## Accessibility Requirements

**REQ-F-021 (Ubiquitous — Popup Accessible Name)**

The Image Information popup shall have an accessible name (e.g., "Image Information").

**Acceptance Criteria:**
- The popup's `Accessible.name` is set to a user-facing string like "Image Information"
- An accessibility test (`tests/accessibility_test.cpp`) confirms the name is present

**REQ-F-022 (Ubiquitous — Section Accessible Names)**

Each section (CAMERA, LOCATION, FILE) shall have an accessible name derived from its label.

**Acceptance Criteria:**
- An accessibility test asserts section accessible names "Camera", "Location", "File" for an EXIF-rich local image

**REQ-F-023 (Ubiquitous — Close Button Keyboard Accessible)**

The close (×) button shall be keyboard-focusable and have a descriptive accessible name.

**Acceptance Criteria:**
- Pressing Tab navigates focus to the close button
- The button's `Accessible.name` is set to "Close Image Information" or similar
- An automated test focuses the button and presses Space, asserting the popup closes
- Accessibility tests confirm keyboard navigation

**REQ-F-024 (Ubiquitous — Path Text Selectable)**

The FILE section's path text shall be selectable and copyable via keyboard and mouse.

**Acceptance Criteria:**
- The file path can be selected by clicking and dragging with the mouse
- The path item is a read-only text edit with `selectByMouse` enabled and is keyboard-focusable via Tab
- In a short popup, Tab reveals the path cursor; Ctrl+End and Ctrl+Home keep the cursor inside the body viewport without moving the header (REQ-NF-002).
- An automated test focuses it, sends Ctrl+A then Ctrl+C, and asserts the clipboard equals the displayed path

---

## Constraint Requirements

**REQ-C-001 (Technology Stack)**

The Image Information popup shall be implemented using Qt6 C++ and QML, following HoloNight design system components.

**Acceptance Criteria:**
- The QML component uses only Qt6 standard libraries and HoloNight custom components (HnLabel, HnMetrics, HolonightPalette, etc.)
- No external dependencies are introduced
- Code compiles without new warnings
- Existing C++ unit tests (`tests/exif_metadata_test.cpp`) pass with the updated `ImageDocument`

**REQ-C-002 (QML File Registration)**

All new QML files for the Image Information popup shall be registered in `CMakeLists.txt`.

**Acceptance Criteria:**
- `ImageInformationPopup.qml` is added to the `qt_add_qml_module()` block in `apps/viewer/CMakeLists.txt`
- The QML module builds successfully
- The component is accessible from `Main.qml` by import and instantiation

**REQ-C-003 (Code Formatting)**

New QML files shall be listed in the QML format-script configuration.

**Acceptance Criteria:**
- `ImageInformationPopup.qml` is added to `scripts/check-qml-format.sh`
- Running the format script on the file produces no changes (i.e., the file is already correctly formatted)

**REQ-C-004 (Build Tooling)**

New QML files shall be listed in the project's `Taskfile` QML tooling entries.

**Acceptance Criteria:**
- `ImageInformationPopup.qml` is added to the `Taskfile.yml` qml-format invocation
- `task` format/check targets pass

**REQ-C-005 (Translatable Strings)**

All user-facing strings in the Image Information popup shall use `tr()` (C++) or `qsTr()` (QML) for translation.

**Acceptance Criteria:**
- All labels, section headers, accessible names, and messages use `tr()` or `qsTr()`
- `grep` of the new QML and C++ code finds no user-visible string literal outside `qsTr`/`tr` (object names and icon paths excepted)

**REQ-C-006 (Existing Tests Pass)**

All existing unit and integration tests shall pass after implementation.

**Acceptance Criteria:**
- `tests/exif_metadata_test.cpp` passes with updated `ImageDocument` API
- `tests/static_workflow_test.cpp` passes with new objectNames and popup behavior
- `tests/accessibility_test.cpp` passes with new popup and button accessible names
- All tests are run and confirmed to pass in the CI/build pipeline

**REQ-C-007 (Standalone QML Load Test)**

The `ImageInformationPopup.qml` component shall be loadable and functional in a standalone test at window widths below 420px.

**Acceptance Criteria:**
- A test loads `ImageInformationPopup.qml` in a 400px-wide window without runtime errors
- The popup layout is correct and no elements are cut off
- The test is registered in the existing viewer test executable

---

## Verification Summary

Each requirement above has acceptance criteria that can be verified through:

1. **Unit Tests** (C++ and QML): Verify data formatting, field splitting, section omission rules, "~" abbreviation, transformed-dimension logic, and EXIF field extraction.
2. **Integration Tests**: Verify popup opening/closing, navigation blocking, live updates, and keyboard/mouse interaction.
3. **Accessibility Tests**: Confirm accessible names, keyboard navigation, and screen reader compatibility.
4. **Visual/Manual Tests**: Confirm layout, responsive behavior, modal dimming lightness, and aesthetic alignment with HoloNight design.
5. **Regression Tests**: Ensure Shortcut Help continues to work unchanged and all existing tests pass.

---

## Change Summary

| Component | Change |
|-----------|--------|
| `apps/viewer/ImageInformationPopup.qml` | New file (dedicated popup component) |
| `apps/viewer/Main.qml` | Stop routing information through the details dialog; instantiate `ImageInformationPopup` |
| `apps/viewer/CMakeLists.txt` | Register new QML file |
| `scripts/check-qml-format.sh` | Add new QML file to format list |
| `Taskfile` | Add new QML file to QML tooling entries |
| `ImageDocument` (C++ class) | Add structured information properties; remove `informationText()` method; expose structured metadata |
| `ExifDetails` (C++ struct) | Add separate fields: `aperture`, `shutter`, `iso`, `focalLength` |
| `tests/exif_metadata_test.cpp` | Update tests for new structured data model |
| `tests/static_workflow_test.cpp` | Update objectNames and workflow expectations |
| `tests/accessibility_test.cpp` | Update accessibility tests for new component |

---

## Appendix: Implementation Notes

- **Modal Background**: The dim-overlay color should use a reduced opacity or lighter color compared to the Shortcut Help dialog to keep the image visible.
- **Preview Timing**: The preview image (96px) should only appear after the document state transitions to Ready; showing it during load states may cause visual flicker.
- **Locale-Aware Date Formatting**: Use `QLocale().toString(dateTime, QLocale::ShortFormat)` or equivalent to respect the user's system date preferences.
- **Shortcut Help Isolation**: Ensure that changes to the Image Information popup do not affect the existing Shortcut Help dialog. Help's content and appearance must not change.
- **Keyboard Focus Management**: After closing the popup with Escape or the × button, window-level shortcuts must work again without an extra click.
