# HoloNight Viewer UI Polish — Requirements Specification

**Status:** Requirements authorized by the grilling phase. This specification defines enhancements to the empty-state hint, menu layout, shortcut help redesign, transient UI elements (arrows, HUD), and keyboard shortcut mapping for HoloNight Viewer.

Implementation and review corrections passed local automated and offscreen
visual checks, tracked in [TASKS.md](TASKS.md). Command outcomes and pending
native/Docker qualification are recorded in [VERIFICATION.md](VERIFICATION.md).
The 2026-09-14 “fix findings” instruction authorizes the Help layout correction
and evidence alignment within the existing requirements.

---

## Overview

HoloNight Viewer is a Qt6/QML image viewer (apps/viewer: Main.qml, FooterKeyHints.qml, ImageInformationPopup.qml, C++ ImageDocument/ImageCanvas/WindowKeyRouter) built on the holonight-qt style library. This specification covers six interconnected UI improvements addressing user discoverability, accessibility, and control responsiveness:

1. **Empty State Hint** — A centered two-line hint text below the "No image open" glyph, guiding users to open an image via Ctrl+O or drag-and-drop.
2. **Menu Reorganization** — The hamburger menu regrouped with separators, shortcut labels right-aligned, and improved keyboard navigation.
3. **Shortcut Help Redesign** — A new styled modal popup replacing the basic dialog, with grouped sections and a keyboard toggle (? key).
4. **Transient Navigation Arrows** — Previous/Next buttons that appear on pointer movement and fade after 2 seconds of inactivity, staying visible on hover.
5. **Transient Metadata HUD** — The metadata display (detailsStrip) triggered by navigation, zoom, and transforms, fading after 3 seconds; observable via a `shown` property.
6. **Flip Key Remapping** — Keyboard shortcuts X and Shift+X for horizontal and vertical flip (replacing H and V), consistently reflected in menu and help.

---

## Scope & Non-Goals

### In Scope

- Empty-state hint text display, layout, and visibility rules
- Menu order, separators, shortcut labels, and keyboard navigation
- New ShortcutHelpPopup QML component with styled content and sections
- Transient visibility and fade behavior for navigation arrows (previousButton, nextButton)
- Transient visibility and observable state for metadata HUD (detailsStrip)
- Keyboard shortcut reassignment (X / Shift+X for flip; H / V removal)
- Integration of new QML files into CMake, format scripts, and Taskfile
- Accessibility exposure and compliance for all components
- Comprehensive test coverage for each feature area

### Out of Scope

- Header height reduction (handled in holonight-qt)
- Auto-hiding header/footer chrome or fullscreen-specific chrome hiding
- Thumbnail strip or multi-image carousel
- GPS map links or external navigation services
- Image Information popup redesign (already completed in a prior cycle)
- Changes to existing keyboard navigation (Tab/Shift+Tab header cycling, J/K menu skip behavior) beyond menu separator handling
- Sound or haptic feedback for UI interactions

---

## Glossary

| Term | Definition |
|------|-----------|
| Empty State | The viewer state when no image document is open; displays "No image open" glyph and supporting text. |
| ImageDocument | C++ class holding image state; exposes document state enum (Empty, Loading, Ready, Error). |
| HnStyle | holonight-qt library components: HnStyle.Menu, HnStyle.MenuItem, HnKeyHint, HnLabel, HnSeparator, HoloniightPalette, HnMetrics. |
| Transient UI | Components that appear conditionally and fade after a timer expires (arrows, metadata HUD). |
| detailsStrip | The metadata HUD strip (objectName) showing filename, dimensions, timings, and file size. |
| ShortcutHelpPopup | New dedicated QML component replacing the Basic.Dialog for shortcut reference; styled like ImageInformationPopup. |
| Modal Dimmer | Semi-transparent overlay darkening the background when a modal popup is open. |
| EARS | Easy Approach to Requirements Syntax; five template forms for writing requirements. |
| ViewerButton, ViewerMenuItem | Custom component templates in Main.qml for consistent styling. |

---

## Functional Requirements

### F.1 Empty State Hint

**REQ-F-001 (State-driven — Empty-State Text Display)**

While the ImageDocument is in the Empty state, the viewer shall display a centered two-line muted hint text below the existing subdued glyph (objectName "emptyState"). Line 1 shall read "No image open" (muted body text, qsTr-wrapped); Line 2 shall read "Ctrl+O to open · or drop an image here" (muted caption, plain text, no keycaps).

**Acceptance Criteria:**
- An automated layout test loads the viewer without an image and asserts the emptyState glyph is present.
- Two text elements appear directly below the glyph with the exact wording and line breaks.
- Both lines use HoloniightPalette.textMuted; line 1 uses body typography and line 2 uses HnTypographyRole.Caption.
- The second line uses plain text (no HnKeyHint keycap styling for "Ctrl+O").
- A grep of the QML code finds both strings wrapped in qsTr().

**REQ-F-002 (State-driven — Responsive Sizing)**

While the ImageDocument is in the Empty state, the glyph and hint text shall fit within the canvas; on small canvases (window minimum height 280), text keeps its size and the glyph shrinks first; if the canvas is too short for both, the glyph hides and only text remains. Line 2 wraps at narrow widths. The layout shall never overflow horizontally.

**Acceptance Criteria:**
- At the default 1000×700 window, the glyph and both hint lines are visible, the glyph's bottom edge lies above line 1's top edge, and both lie within the canvas bounds.
- At the 420×280 minimum window, both hint lines are fully inside the canvas and the hint text's font size equals its size at 1000×700.
- When the canvas height is set below (hint text height + minimum glyph size + padding) — e.g. via a standalone load of the canvas area — the glyph is not visible and both hint lines remain visible inside the canvas.
- At a 420 px wide window, line 2's width does not exceed the canvas width (it wraps to more than one line if its implicit width is larger).

**REQ-F-003 (State-driven — Hint Visibility)**

While the ImageDocument state is Loading, Ready, or Error, the hint text shall be hidden, together with the glyph (as today).

**Acceptance Criteria:**
- With no image open, the hint is visible; after opening a valid image (state Ready), neither the hint nor the glyph is visible.
- After opening an unreadable file (state Error), the hint is not visible and the existing error feedback ("documentFeedback") is visible.

**REQ-F-004 (Ubiquitous — Accessibility)**

The hint text shall be exposed to the accessibility tree as static text; the glyph shall remain Accessible.ignored.

**Acceptance Criteria:**
- An accessibility test finds StaticText nodes whose names are the two hint lines.
- The accessibility tree contains no node for the glyph.

---

### F.2 Menu Reorganization

**REQ-F-005 (Ubiquitous — Menu Item Order and Separators)**

The hamburger menu (objectName "actionsMenu", button objectName "actionsButton") shall display menu items in this order with separators (—) as listed:

Open… (Ctrl+O), Refresh (Ctrl+R) — Previous ([), Next (]) — Fit (Ctrl+0), Actual Size (1), Zoom In (Ctrl++), Zoom Out (Ctrl+−) — Rotate Clockwise (R), Rotate Counterclockwise (Shift+R), Flip Horizontally (X), Flip Vertically (Shift+X), Reset Transform (no shortcut) — Copy Image (Ctrl+C), Copy Path (Ctrl+Shift+C) — Image Information (I), Shortcut Help (?) — Fullscreen (F), Quit (Q).

**Acceptance Criteria:**
- An automated test enumerates the menu's contentModel items and asserts exactly 19 command items and 6 separators, in the listed order, with separators only at the listed positions.
- The menu never contains two adjacent separators, nor a separator as first or last entry.

**REQ-F-006 (Ubiquitous — Menu Item Labels and Shortcuts)**

Each menu item shall have a title case label (e.g., "Zoom In", "Zoom Out") and a right-aligned muted caption displaying the shortcut text (e.g., "Ctrl++", "Shift+R"). Items without a shortcut shall show no caption. Disabled items shall display label and shortcut in the disabled palette color.

**Acceptance Criteria:**
- An automated test asserts each command item's label text equals the exact label listed in REQ-F-005 ("Zoom In", "Zoom Out", etc.).
- For each command item, the shortcut text equals the string listed in REQ-F-005; Reset Transform's shortcut text is empty and no shortcut label is visible.
- Each visible shortcut label's right edge is within one padding of the item's right content edge, and its left edge is right of the item label's right edge (no overlap) at the menu's maximum width.
- With no image open, Fit is disabled and both its label and shortcut label use HoloniightPalette.textDisabled.
- Menu, Help and footer spell shared shortcuts identically (e.g. "Ctrl+0", "[", "]", "R", "F", "?").

**REQ-F-007 (Event-driven — Keyboard Navigation Skips Separators)**

When the user presses Up or Down (or J/K) while the menu is open, the menu shall skip over separators and only highlight focusable menu items. Separators shall not be focusable or triggerable by Enter/Space.

**Acceptance Criteria:**
- With the menu open and "Refresh" (index before the first separator) current, pressing Down makes "Previous" current; pressing J from "Refresh" does the same.
- With "Previous" current, pressing Up (or K) makes "Refresh" current.
- Pressing Down repeatedly from "Open…" visits all 19 command items and never makes a separator current.
- The accessibility tree exposes no separator with a MenuItem or Button role.

**REQ-F-008 (Ubiquitous — Hamburger Button Accessible Name)**

The hamburger button's accessible name shall be "Menu" (not "Actions").

**Acceptance Criteria:**
- An automated test asserts `actionsButton.Accessible.name === "Menu"` (or equivalent).
- The existing objectName "actionsButton" is preserved.

**REQ-F-009 (Ubiquitous — Existing Menu Behavior Preserved)**

The menu's enablement rules and triggered commands shall remain unchanged apart from order, labels and flip shortcuts. Existing header Tab/Shift+Tab cycling is unaffected.

**Acceptance Criteria:**
- Existing tests that trigger menu items by objectName (openButton, fitButton, actualSizeButton, zoomInButton, zoomOutButton, resetTransformMenuItem, informationMenuItem) pass, adjusted only for label text.
- Existing header focus-cycling tests pass unchanged.

---

### F.3 Shortcut Help Redesign

**REQ-F-010 (Ubiquitous — New ShortcutHelpPopup Component)**

The Shortcut Help shall be implemented as a new dedicated QML component `ShortcutHelpPopup.qml` (separate from ImageInformationPopup.qml and Main.qml), styled as a rounded surfaceRaised card with a light modal dimmer. The popup shall measure max width `min(480, window_width − 24)` and height `min(implicit_height, window_height − 24)`, with an icon-only × close button at the top-right and no footer Close button.

**Acceptance Criteria:**
- A new file `apps/viewer/ShortcutHelpPopup.qml` exists and is registered in `apps/viewer/CMakeLists.txt` qml module.
- Main.qml no longer contains the Basic.Dialog with objectName "detailsDialog"; opening Help instantiates a popup with objectName "shortcutHelpPopup".
- At a 1000 px wide window the popup is 480 px wide; at a 420 px window it is 396 px (420 − 24); in a standalone 400 px load it is 376 px.
- The modal dimmer color equals Qt.alpha(HoloniightPalette.shadow, 0.22), the same as ImageInformationPopup's dimmer.
- An icon-only close button (objectName "shortcutHelpCloseButton", accessible name "Close Shortcut Help") lies in the top-right corner of the header row; clicking it closes the popup.
- The popup has no footer and no button with text "Close".

**REQ-F-011 (Event-driven — Toggle and Close Mechanisms)**

When the user presses ? (question mark), the Shortcut Help popup shall toggle (open if closed, close if open). When the user presses Escape while the popup is open, it shall close. When the user clicks outside the popup on the modal dimmer, it shall close.

**Acceptance Criteria:**
- An automated test presses ?, asserts the popup is open, presses ? again, asserts it is closed.
- A test presses ? to open the popup, then Escape, asserts the popup is closed.
- A test opens the popup and simulates a click on the modal overlay; asserts the popup closes.
- The ? key handler is declared inside the modal (ShortcutHelpPopup.qml) to prevent window-level routing while the popup is open.

**REQ-F-012 (Ubiquitous — Content Sections)**

The Shortcut Help popup shall display a "Shortcuts"-style header followed by grouped sections with uppercase caption labels. The sections shall be: NAVIGATION, VIEW, TRANSFORM, IMAGE, APPLICATION, MOUSE. Each section contains rows pairing keyboard keys (rendered as HnKeyHint keycaps) with descriptions (primary text).

**Acceptance Criteria:**
- The popup shows exactly six section labels, in order: NAVIGATION, VIEW, TRANSFORM, IMAGE, APPLICATION, MOUSE, each using HnTypographyRole.Caption and HoloniightPalette.textSecondary.
- Every section contains at least one row; the header row shows the title "Shortcuts".

**REQ-F-013 (Ubiquitous — Shortcut Content Details)**

The Shortcut Help sections shall list the following shortcuts (verbatim wording in descriptions):

- **NAVIGATION:** Ctrl+O open image | [ / ] previous/next image | Ctrl+R refresh folder and image
- **VIEW:** Ctrl+0 fit | 1 actual size | Ctrl++ / Ctrl+− zoom | F fullscreen | Esc close dialog or leave fullscreen
- **TRANSFORM:** R / Shift+R rotate clockwise/counterclockwise | X / Shift+X flip horizontally/vertically
- **IMAGE:** I image information | Ctrl+C copy image | Ctrl+Shift+C copy path
- **APPLICATION:** ? toggle this help | Q quit
- **MOUSE:** wheel/touchpad scroll zoom at pointer | left-drag pan | arrow keys pan | drop an image to open

Descriptions are displayed in sentence case (e.g. "Open image", "Previous / next image"). Non-keyboard rows (MOUSE gestures) may use plain labels without HnKeyHint keycaps.

**Acceptance Criteria:**
- An automated test enumerates all sections and rows (via the popup's row model, since Repeater delegates are not reachable by findChild) and checks the key and description of every row against the list above, in order.
- Every row whose key is a keyboard key renders it with HnKeyHint; wheel, drag and drop rows render the gesture as plain text.

**REQ-F-014 (Unwanted Behaviour — No Widget Navigation Listed)**

The Shortcut Help shall NOT list or reference Tab/Shift+Tab header cycling or J/K menu navigation.

**Acceptance Criteria:**
- No row key contains "Tab", "J" or "K", and no row description contains "header" or "menu".

**REQ-F-015 (State-driven — Content Scrolling and Layout)**

While the window is short, the popup's body content shall scroll vertically; the header row (title) shall remain fixed. Rows may wrap at narrow widths rather than overflow horizontally.

**Acceptance Criteria:**
- At a 420×280 window, the body's Flickable contentHeight exceeds its height; after scrolling the body to its end, the header row's y position is unchanged.
- At a 420 px window (and a standalone 400 px load), every row's width is ≤ the body's available width and the body's contentWidth equals its availableWidth.
- Keycaps and their rendered text stay within their columns without overlapping
  descriptions, including at 12 pt and 18 pt key text; wrapped content expands
  the row height. This checks the rendered children as well as row bounds.

**REQ-F-016 (State-driven — Modal Behavior)**

While the Shortcut Help popup is open, the viewer window is modal: other window shortcuts (e.g., [, ], Ctrl+O from the main canvas) shall be blocked unless they are declared inside the popup. After the popup closes, window shortcuts resume.

**Acceptance Criteria:**
- With a folder of images open at position 2, opening Help and pressing [ leaves the position at 2; after closing Help, pressing [ moves to position 1.
- While Help is open, `modalActive` is true and arrow keys do not pan the canvas; after closing it, `modalActive` is false.

**REQ-F-017 (Ubiquitous — Accessibility)**

The Shortcut Help popup shall expose its content as a Popup with Accessible.role Dialog and Accessible.name "Shortcut Help". Each section shall be a Grouping. Each row shall be readable as "key description" (keycap internals ignored, same pattern as FooterKeyHints).

**Acceptance Criteria:**
- An automated accessibility test finds the popup with Accessible.name "Shortcut Help" and role Dialog.
- Each section is found as a Grouping under the popup.
- Each row is exposed exactly once as StaticText named "key description" (e.g., "Ctrl+O Open image"); no separate node exists for the keycap text or the description label.

---

### F.4 Transient Navigation Arrows

**REQ-F-018 (Event-driven — Pointer Movement Visibility)**

When the user moves the pointer over the canvas, the navigation arrow buttons (objectNames "previousButton", "nextButton") shall become visible and start a 2-second countdown timer. When 2 seconds elapse without pointer movement, the arrows shall fade out (over ~150 ms) and become invisible/non-clickable.

**Acceptance Criteria:**
- After a pointer move over the canvas, both arrows report shown = true within one frame.
- With no further pointer movement, the arrows still report shown = true at 1.8 s and shown = false by 2.2 s after the move.
- A second pointer move at 1.5 s keeps them shown until at least 3.3 s after the first move.

**REQ-F-019 (State-driven — Hover Pause)**

While the pointer hovers over either navigation arrow button, the timer shall be paused (the countdown does not elapse). On hover exit, the 2-second countdown shall restart.

**Acceptance Criteria:**
- After moving the pointer onto "nextButton" and holding it there for 3 s, both arrows still report shown = true.
- After moving the pointer off the button (without further canvas movement), the arrows report shown = true at 1.8 s and shown = false by 2.2 s after leaving.

**REQ-F-020 (Event-driven — Keyboard Hides Arrows)**

When the canvas receives keyboard input, the navigation arrows shall start hiding immediately; if the arrows are hidden, then keyboard navigation with [ or ] shall not show them.

**Acceptance Criteria:**
- With the arrows shown after a pointer move (pointer not over an arrow), pressing an arrow key sets shown = false within one frame, and they are not visible after the fade duration.
- With the arrows hidden, pressing ] (navigating to the next image) leaves shown = false for at least 2.5 s.

**REQ-F-021 (Ubiquitous — Fade Animation)**

The navigation arrows shall fade in and out over approximately 150 milliseconds using opacity animation. When fully faded (opacity ≈ 0), they shall not be visible or clickable (pointer-events: none).

**Acceptance Criteria:**
- About 75 ms into a hide (after shown becomes false), the arrow opacity is strictly between 0 and 1; by 300 ms it is 0 and `visible` is false.
- Clicking the former position of "nextButton" 300 ms after the hide began does not change the image position.
- While `visible` is false, every accessibility node named "Next image" or "Previous image" reports the invisible state (Qt keeps hidden items in the tree flagged invisible, which assistive technology skips).

**REQ-F-022 (Ubiquitous — Arrow Availability Unchanged)**

The navigation arrows' enablement (canPrevious / canNext, not modal) and their eligibility to appear in any document state shall remain as today; only timing, hover and fade behaviour change.

**Acceptance Criteria:**
- With a folder open and the current file in the Error state, a pointer move shows the arrows and clicking "nextButton" moves to the next image.

---

### F.5 Transient Metadata HUD

**REQ-F-023 (Event-driven — HUD Visibility Triggers)**

The metadata HUD (detailsStrip, objectName) shall appear only while an image is Ready. It shall be shown on first render, on pointer movement over the canvas, and on navigation, zoom (Ctrl++, Ctrl+−), fit (Ctrl+0), actual size (1), rotate (R, Shift+R), and flip (X, Shift+X) actions, whether triggered by mouse, menu, or keyboard. The HUD shall hide 3 seconds after the last trigger.

**Acceptance Criteria:**
- After an image's first render, the HUD reports shown = true, still true at 2.8 s, and false by 3.2 s.
- With the HUD hidden, each of these (no pointer movement) sets shown = true within one frame of the command taking effect: Ctrl++, Ctrl+−, Ctrl+0, 1, R, Shift+R, X, Shift+X, and the menu's Zoom In item.
- With the HUD hidden, pressing ] shows the HUD once the next image reaches Ready (first render).
- A trigger at 2 s after a previous trigger keeps the HUD shown until at least 4.8 s after the first trigger.

**REQ-F-024 (State-driven — HUD Fade and Visibility)**

When 3 seconds elapse without a trigger, the metadata HUD shall fade out over approximately 150 milliseconds. While fully faded (opacity ≈ 0), the HUD shall not be visible. The HUD shall not respond to pointer events or screen reader inspection while faded.

**Acceptance Criteria:**
- About 75 ms after shown becomes false, the HUD opacity is strictly between 0 and 1; by 300 ms it is 0 and `visible` is false.
- A left-button drag starting on the HUD's former area 300 ms after hiding pans the canvas.

**REQ-F-025 (Ubiquitous — Observable Visibility State)**

The arrows ("previousButton", "nextButton") and the HUD ("detailsStrip") shall each expose a boolean `shown` property that is true exactly while the element is meant to be displayed (target of the fade), independent of the fade's progress.

**Acceptance Criteria:**
- `shown` becomes true in the same frame as a trigger and false in the same frame the hide begins, while `opacity` animates afterwards.
- No viewer test reads `arrowTimer.running` or `detailsTimer.running` to decide visibility.

**REQ-F-026 (State-driven — No HUD Before Ready)**

The metadata HUD shall not appear if the ImageDocument is not in the Ready state, even if a trigger event (e.g., pointer movement or menu action) occurs.

**Acceptance Criteria:**
- With no image open, a pointer move over the canvas leaves the HUD's shown = false.
- With the current file in the Error state, pressing Ctrl++ or a pointer move leaves shown = false.
- When navigating away from a Ready image while the HUD is shown, shown becomes false before the next image reaches Ready.

---

### F.6 Flip Key Remapping

**REQ-F-027 (Ubiquitous — Flip Hotkeys)**

The horizontal flip action shall be triggered by pressing X; the vertical flip action shall be triggered by pressing Shift+X. The H and V keys shall no longer have any effect (no flip actions, no aliases to X or Shift+X).

**Acceptance Criteria:**
- An automated test loads an image, presses X; asserts the image is flipped horizontally.
- A test presses Shift+X; asserts the image is flipped vertically.
- A test presses H; asserts no flip occurs (image transform unchanged).
- A test presses V; asserts no flip occurs.
- Pressing Shift+H or Shift+V also leaves the orientation unchanged.

**REQ-F-028 (Ubiquitous — Menu and Help Reflect New Keys)**

The hamburger menu shall list "Flip Horizontally (X)" and "Flip Vertically (Shift+X)" with the correct shortcut labels. The Shortcut Help popup's TRANSFORM section shall list "X / Shift+X flip horizontally/vertically".

**Acceptance Criteria:**
- An automated test reads the menu items for flip actions; asserts labels are "Flip Horizontally" and "Flip Vertically" with shortcuts "X" and "Shift+X" respectively.
- A test of Shortcut Help content asserts the TRANSFORM section lists "X / Shift+X flip horizontally/vertically".
- No menu shortcut text or Help row key equals "H" or "V".

**REQ-F-029 (State-driven — Modal and Ready Checks)**

Flip actions (X, Shift+X) shall only execute while an image is Ready and no modal is open (consistent with existing transform action rules).

**Acceptance Criteria:**
- An automated test with no image open presses X; asserts no flip occurs.
- A test opens a modal (Shortcut Help), presses Shift+X; asserts no flip occurs.
- A test with an image Ready and no modal open presses X; asserts the flip executes.

**REQ-F-030 (Ubiquitous — No Footer Listing)**

The footer (FooterKeyHints.qml) shall NOT list flip shortcuts; it continues to show the seven existing hints (Navigate, Zoom, Fit, 100%, Rotate, Fullscreen, Help) unchanged.

**Acceptance Criteria:**
- An automated test asserts the footer contains exactly seven hints.
- The footer content does not mention X, Shift+X, H, or V.
- FooterKeyHints.qml is not modified (existing test suite still passes).

---

## Non-Functional Requirements

**REQ-NF-001 (Minimum Window Size Compatibility)**

All new and changed layouts shall work at the minimum window size (420×280) without horizontal overflow. Where a ~400 px width must be tested, a standalone component load is used, since the window cannot be resized below its minimum.

**Acceptance Criteria:**
- At 420×280, the empty-state hint, open menu and open Help popup each lie fully within the window's horizontal bounds.
- In a standalone 400 px load of ShortcutHelpPopup, no row is wider than the popup's content width.

**REQ-NF-002 (Theme Awareness)**

All colors and fonts shall be sourced from HoloniightPalette and HnTypographyRole. No hard-coded color literals (#…, Qt.rgba()) or font specifications (font.family, font.pixelSize) shall be used outside of HnStyle component defaults.

**Acceptance Criteria:**
- A search of the new and modified QML finds no color literals (`"#…"`, `Qt.rgba(`, named colors) and no `font.family` / `font.pixelSize` assignments.
- Switching the HoloNight theme between light and dark while the empty state, menu and Help are shown changes their text and background colors to the new palette values.

**REQ-NF-003 (Fade Does Not Disturb Input)**

If a fade animation is running, then pointer gestures and keyboard commands on the canvas shall behave exactly as when no fade is running.

**Acceptance Criteria:**
- Starting a left-button drag on the canvas while the HUD is fading out pans the canvas by the drag delta.
- Existing input-epoch/gesture tests pass unchanged.

**REQ-NF-004 (Accessibility)**

Every new interactive element shall have an accessible name, and new informational text shall be exposed exactly once.

**Acceptance Criteria:**
- The accessibility test finds a non-empty name on "shortcutHelpCloseButton" and on every enabled menu item.
- The accessibility tree contains no duplicate StaticText nodes for the empty-state hint lines or Help rows.

**REQ-NF-005 (Translation Support)**

All new user-visible strings (menu labels and shortcut texts, Help content, empty-state hint, accessible names) shall be wrapped in `qsTr()`.

**Acceptance Criteria:**
- A search of new and modified QML finds no user-visible string literal assigned to `text`, `rawText`, `label`, `key` or `Accessible.name` outside `qsTr()`.

---

## Constraint Requirements

**REQ-C-001 (Qt6 and holonight-qt Stack)**

All implementations shall use Qt6 QML and C++ with holonight-qt (HnStyle, HoloniightPalette, HnMetrics, HnLabel, HnKeyHint, HnSeparator) components. No external dependencies outside holonight-qt shall be introduced.

**Acceptance Criteria:**
- The build system (CMake) compiles successfully with no new external library dependencies.
- qmllint produces no unresolved-type or missing-property warnings for new QML components.
- Existing dependencies on holonight-qt (HnStyle.Menu, HnStyle.MenuItem, HnKeyHint) work without version bumps.

**REQ-C-002 (QML File Registration)**

All new QML files (ShortcutHelpPopup.qml and any supporting components) shall be registered in `apps/viewer/CMakeLists.txt` within the qt_add_qml_module() block.

**Acceptance Criteria:**
- `ShortcutHelpPopup.qml` appears in the CMakeLists.txt qml module.
- The QML module builds successfully (cmake, ninja/make steps pass).
- The component is importable and instantiable in Main.qml.

**REQ-C-003 (Format Script and Taskfile)**

All new QML files shall be registered in `scripts/check-qml-format.sh` and in every Taskfile.yml task that lists viewer QML files.

**Acceptance Criteria:**
- `ShortcutHelpPopup.qml` appears in scripts/check-qml-format.sh and in each Taskfile.yml list that contains `ImageInformationPopup.qml`.
- The QML format check and QML lint tasks pass.

**REQ-C-004 (Existing Tests Updated)**

Existing tests that reference removed or modified features (detailsDialog, detailsText, closeDetailsButton, arrowTimer/detailsTimer visibility checks, H/V flip keys, "Actions" accessible name) shall be updated.

**Acceptance Criteria:**
- No test references `detailsDialog`, `detailsText` or `closeDetailsButton`.
- The full ctest preset passes.

**REQ-C-005 (New Tests for Feature Areas)**

New automated tests shall be added for each feature area: empty-state text visibility, menu order/separators/shortcuts, Help sections/toggle/accessibility, arrow hover/timer behavior, HUD triggers/observable state, and flip key remapping.

**Acceptance Criteria:**
- Each feature area (F.1–F.6) has at least one new test case registered with ctest that fails when that area's primary behaviour is reverted.
- All new tests pass under the ctest preset.

**REQ-C-006 (Existing Behavior Preserved)**

The implementation shall not modify existing keyboard shortcuts (except flip key remapping), menu enablement rules, focus cycling, or canvas behavior.

**Acceptance Criteria:**
- The diff touches no C++ files other than tests, and no files in holonight-qt.
- Tests for shortcuts other than flip (Ctrl+O, [, ], Ctrl+R, Ctrl+0, 1, Ctrl++, Ctrl+−, R, Shift+R, Ctrl+C, Ctrl+Shift+C, I, ?, F, Escape, Q) pass.

**REQ-C-007 (Object Names Preserved)**

Existing object names (actionsButton, actionsMenu, previousButton, nextButton, detailsStrip, emptyState, footer, informationPopup) shall remain unchanged to preserve test compatibility.

**Acceptance Criteria:**
- A grep of the QML code asserts these objectNames are still present.
- Test code referencing these names by objectName (via findChild) still passes.

**REQ-C-008 (Key Routing)**

If WindowKeyRouter or any other key handler consumes X or Shift+X before the window shortcuts, then the implementation shall change routing so that X and Shift+X reach the flip actions. (Pre-check: WindowKeyRouter currently contains no H/V/X handling; flips are bound only through Main.qml `Action.shortcut`.)

**Acceptance Criteria:**
- With focus on a header button, pressing X flips the image horizontally.
- With the menu open, pressing X flips the image and closes the menu, matching the existing R behaviour.

---

## Open Questions & Risks

1. **HnStyle.Menu Separator Handling:** Does HnStyle.Menu automatically skip MenuSeparator items on Up/Down navigation, or must the viewer implement custom key handling? If not, the implementation must override KeyNavigation or onPressed in the menu.

2. **ShortcutHelpPopup Fit at 480px Max Width:** Lines like "Ctrl++ / Ctrl+−" with keycaps may exceed 480px at some font scales. Acceptance criteria allow row wrapping; visual regression testing at standard and fractional scales (96, 120, 144 DPI) shall confirm fit.

3. **Shortcut Help replaces `detailDialog`:** Help moves from the `detailDialog` int to a boolean like `informationOpen`; `modalActive` and the input-epoch increment on modal changes must keep working.

4. **Hover detection on arrows:** the arrows sit above the canvas, so pointer movement over them does not reach the canvas `mouseMoved` signal; hover must be read from the buttons themselves.

5. **Keyboard HUD triggers:** zoom/fit/rotate/flip go through several paths (Shortcut, Action, menu item, C++ canvas methods); the trigger should hook a single point (e.g. canvas viewport/orientation change) to avoid missing a path — while not firing on window resize.

---

**End of Specification**
