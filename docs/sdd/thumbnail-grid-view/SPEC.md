# SPEC: Thumbnail Grid View

Approved scope: Stage 0 grilling decisions, 2026-09-29.
Feature: thumbnail grid view for static image browsing with cell-by-cell navigation, asynchronous thumbnail decoding, and seamless mode toggling with the single-image view.

## Overview

The HoloNight Viewer gains a keyboard-navigable thumbnail grid view showing all images in the current folder, with Ctrl+G toggling between single-image and grid presentations. The grid displays fitted 256x256px thumbnails with on-demand asynchronous decoding, keyboard navigation (hjkl/arrow keys, pagination, bracketed navigation), and selection that persists through rescans. Grid mode disables image editing and playback; single-view behavior does not regress.

## Scope

Grid view implementation: toggle, navigation, cell selection, presentation in dedicated QML, asynchronous thumbnail decoding and caching, rescan behavior, keyboard focus and accessibility.

**Non-goals:** thumbnail zoom or resizing, multi-select, file operations (delete/rename), sort options, recursive folders, Freedesktop/disk thumbnail cache, persisted grid mode, filesystem watching, configurable cell size.

## Glossary

- **Grid mode:** Presentation showing all folder images as a scrollable grid of cells.
- **Single view (or single-image view):** The existing full-window image presentation with pan, zoom, transform, and playback.
- **Cell:** A single grid item displaying one image thumbnail, filename, and selection chrome.
- **Selection:** The currently active cell, with rounded tinted background and keyboard focus ring.
- **Visible rows:** The number of complete grid rows currently visible in the grid viewport.
- **Thumbnail:** A fitted, never-upscaled decode of an image into a 256x256 logical-pixel box.
- **DirectoryModel:** The existing model listing all supported image files in the opened file's folder in natural-sorted order.

## Requirements

### REQ-F-001: Grid View Toggle (Event-driven)
When Ctrl+G is pressed while a document is open and its folder listing is non-empty, the viewer shall toggle between single-image view and grid view.

**Acceptance:** Press Ctrl+G in single-image view → grid appears. Press Ctrl+G again → single-image view returns with the previously open image.

### REQ-F-002: Unshifted G Only (Unwanted)
If Ctrl+Shift+G is pressed, then the viewer shall not toggle grid mode; every place that displays the toggle hint shall render it as "Ctrl+G".

**Acceptance:** Send Ctrl+Shift+G in single view and in grid mode → the mode is unchanged in both. The footer, menu and help hints for the toggle read "Ctrl+G" (no Shift keycap).

### REQ-F-003: Grid Entry Blocked When No Document (Unwanted)
If the folder has no files or no document is open, then Ctrl+G shall not toggle into grid view.

**Acceptance:** Open Viewer with an empty folder. Press Ctrl+G → grid mode does not activate. No error message is shown; single-image view remains or shows empty state.

### REQ-F-004: Leaving Grid Keeps Current Image (Event-driven)
When Escape or Ctrl+G is pressed in grid mode, the viewer shall return to single view showing the image that was open before entering grid mode, regardless of the grid selection.

**Acceptance:** Open image A, Ctrl+G, move the selection to image B, press Escape → single view shows A. Repeat with Ctrl+G instead of Escape → single view shows A.

### REQ-F-005: Escape in Grid Does Not Exit Fullscreen (Unwanted)
If the grid view is active in fullscreen, then Escape shall not exit fullscreen; it shall only close the grid.

**Acceptance:** Enter fullscreen (F), toggle to grid (Ctrl+G), press Escape → grid closes, fullscreen remains active.

### REQ-F-006: Escape Outside Grid Still Exits Fullscreen (State-driven)
While single view is active and the window is fullscreen, Escape shall exit fullscreen.

**Acceptance:** Enter fullscreen, press Escape → fullscreen exits. Single-image view is displayed in windowed mode.

### REQ-F-007: Grid Displays All DirectoryModel Files (Ubiquitous)
The grid shall display every image file in the current DirectoryModel in the same natural-sorted order.

**Acceptance:** Open a folder with 50 mixed image files (PNG, JPEG, WEBP, GIF). Ctrl+G → grid shows exactly 50 cells in the same order as DirectoryModel. Reopen the folder → order is identical.

### REQ-F-008: Cell Layout Calculation (Ubiquitous)
The number of grid columns shall equal max(1, floor(available_width / cell_width)), where cell_width is the 256 px thumbnail box plus the cell padding and spacing; the grid content shall be horizontally centered within the available width.

**Acceptance:** With cell_width = W, set the grid width to 3W + W/2 → 3 columns, left and right margins equal within 1 px. Set width to 2W → 2 columns. Set width to W/2 → 1 column.

### REQ-F-009: Thumbnail Fitting (Ubiquitous)
Each cell shall show its thumbnail fitted inside a 256x256 logical-pixel box with the aspect ratio preserved and without upscaling, above a single-line filename.

**Acceptance:** A 1024x768 image paints at 256x192 logical px; a 768x1024 image at 192x256; a 100x200 image at 100x200 (not upscaled). The filename label sits below the box on exactly one line.

### REQ-F-010: Filename Elision (Ubiquitous)
The single-line filename below each thumbnail shall be elided in the middle if it exceeds cell width; the full filename shall appear in the cell's tooltip and accessible name.

**Acceptance:** Cell with filename "very_long_image_name_with_many_characters.png" → the label's elide mode is middle, its painted text contains "…" and ends with ".png"; the tooltip text and Accessible.name equal the full filename.

### REQ-F-011: Thumbnail Decode Resolution (Ubiquitous)
The viewer shall decode each thumbnail to fit a box of ceil(256 × devicePixelRatio) device pixels per side, without upscaling, and display it at its logical fitted size.

**Acceptance:** Request a thumbnail for a 4000x3000 image at DPR 1.25 → decoded image is 320x240 px; at DPR 2 → 512x384 px; at DPR 1 → 256x192 px. A 100x100 source at DPR 2 decodes to 100x100 px.

### REQ-F-012: Selection Background and Focus Ring (Ubiquitous)
The selected cell shall have a tinted rounded background behind the thumbnail and the HoloNight keyboard focus ring around the cell; no chrome shall be drawn over the thumbnail image itself.

**Acceptance:** Select a cell in the grid → a tinted rounded rectangle appears behind the thumbnail, and a focus ring (the standard HoloNight ring) appears around the cell bounds. Thumbnail content is never obscured.

### REQ-F-013: Selection on Grid Entry (Event-driven)
When grid view is entered, the currently open image shall be selected and scrolled into view.

**Acceptance:** Open image file "photo_005.jpg", then Ctrl+G → the cell for "photo_005.jpg" is selected, and the grid's content position places that cell fully inside the viewport, including when it is item 500 of 2000.

### REQ-F-014: Selection Stays in View After Navigation (State-driven)
While navigating in grid mode, the selected cell shall always remain fully visible in the viewport.

**Acceptance:** In a 2000-item grid, after each of j×20, Ctrl+D×5, k×40, Ctrl+U×5 and l×7, the selected cell's rectangle lies fully inside the viewport rectangle.

### REQ-F-015: Horizontal Navigation (Event-driven)
When h, l, Left, or Right is pressed in grid mode, the selection shall move -1 or +1 in linear file order, flowing across row ends, without wrapping past the first or last item.

**Acceptance:** In a 3-column grid with 10 items, starting at item 1: h → no change. l → item 2. l at item 3 → item 4. No wrap from item 10 back to 1.

### REQ-F-016: Vertical Navigation (Event-driven)
When j, k, Down, or Up is pressed in grid mode, the selection shall move down or up by one row. If the target row has no cell in the current column, the selection shall clamp to the last item; at the first or last row, no navigation shall occur.

**Acceptance:** 3-column grid with 7 items (cells 1-3, 4-6, 7). Select cell 7 (alone in last row), press j → no change. Select cell 3, press j → cell 6. Select cell 6, press j → cell 7 (clamped). Select cell 2, press k → no change. Select cell 5, press Up → cell 2.

### REQ-F-017: Page-Down Navigation (Event-driven)
When Ctrl+D is pressed in grid mode, the selection shall move down by max(1, floor(visible_rows/2)) rows, clamped to the last item.

**Acceptance:** 3-column grid of 30 items, viewport shows 4 complete rows. At item 1, Ctrl+D → item 7. At item 25, Ctrl+D → item 30. With 1 visible row, Ctrl+D moves 1 row. Ctrl+Shift+D does nothing.

### REQ-F-018: Page-Up Navigation (Event-driven)
When Ctrl+U is pressed in grid mode, the selection shall move up by max(1, floor(visible_rows/2)) rows, clamped to the first item.

**Acceptance:** 3-column grid of 30 items, 4 visible rows. At item 14, Ctrl+U → item 8. At item 5, Ctrl+U → item 1.

### REQ-F-019: Bracket Navigation (Event-driven)
When "[" or "]" is pressed in grid mode, the selection shall move -1 or +1 in linear order.

**Acceptance:** "[" at item 5 → item 4. "]" at item 5 → item 6. At the first/last item, they do not wrap.

### REQ-F-020: Single Click Selects Cell (Event-driven)
When a cell is clicked once in grid mode, that cell shall become selected.

**Acceptance:** Grid shows 20 cells. Click cell 12 → cell 12 becomes selected (background and focus ring appear), previously selected cell is deselected.

### REQ-F-021: Double Click Opens File (Event-driven)
When a cell is double-clicked in grid mode, the corresponding file shall open in single-image view.

**Acceptance:** Double-click cell 7 → grid closes, single-image view shows the image for cell 7.

### REQ-F-022: Enter Opens Selected File (Event-driven)
When Enter, Return or keypad Enter is pressed in grid mode and no header button has keyboard focus, the viewer shall open the selected file in single view.

**Acceptance:** Select item 7 with l presses, press Enter → single view shows item 7 and document position equals 7. Repeat with Return → same.

### REQ-F-023: Focused Header Button Keeps Enter (State-driven)
While a header button (fullscreen or actions) has keyboard focus in grid mode, Enter and Space shall activate that button instead of opening the selected file.

**Acceptance:** In grid mode Tab to the fullscreen button, press Enter → fullscreen toggles and the mode stays grid. Tab to the actions button, press Space → actions menu opens and the mode stays grid.

### REQ-F-024: Header Displays Folder Name and Count (State-driven)
While grid mode is active, the header title shall display the folder name and the total image count.

**Acceptance:** Grid shows 42 images from a folder named "vacation_photos". Header title reads "vacation_photos — 42 images" (translatable, plural-aware). While the folder is still scanning, the title shows only the folder name.

### REQ-F-025: Footer Displays Grid Navigation Hints (State-driven)
While grid mode is active, the footer shall display hints for grid-specific navigation: hjkl, Ctrl+U/Ctrl+D, Enter, and Ctrl+G.

**Acceptance:** Enter grid mode → the footer hint model contains entries for h/j/k/l, Ctrl+U/Ctrl+D, Enter and Ctrl+G and none for zoom, fit, 1:1, rotate or playback. Leave grid → the original single-view hints return.

### REQ-F-026: Disabled Features in Grid (State-driven)
While grid mode is active, the following features shall be disabled: zoom in/out, fit (Ctrl+0), 1:1 (1), rotate/flip/reset transform, playback, pan, copy image, and image information popup.

**Acceptance:** In grid mode with an animated image current, press Ctrl++, Ctrl+-, Ctrl+0, 1, R, Shift+R, X, Shift+X, Space, I and Ctrl+C, and use the arrow keys (which move the selection per REQ-F-015/016) → canvas pan offset, zoom, orientation, playback state and clipboard are unchanged and no information popup opens. The corresponding menu items report enabled = false.

### REQ-F-027: Enabled Features in Grid (State-driven)
While grid mode is active, the following features shall remain functional: Open (Ctrl+O), fullscreen toggle (F), quit (Q), help (?), rescan (Ctrl+R), and the Ctrl+G toggle.

**Acceptance:** In grid mode, Ctrl+O opens a file dialog, F toggles fullscreen, Q quits, ? opens help popup, Ctrl+R rescans folder, Ctrl+G closes grid.

### REQ-F-028: Actions Menu Grid Toggle (Ubiquitous)
The actions menu shall include a "Grid View" toggle item with the shortcut hint "Ctrl+G".

**Acceptance:** Open the actions menu → an item labeled "Grid View" is present with shortcut hint "Ctrl+G" and checked state equal to grid mode. Triggering it in single view enters grid mode; triggering it in grid mode leaves it. It is disabled when no document is open.

### REQ-F-029: Help Popup Lists Grid Shortcuts (Ubiquitous)
The shortcut help popup (?) shall list Ctrl+G, hjkl, Ctrl+U/Ctrl+D, and Enter.

**Acceptance:** Press ? in single view and in grid mode → the help popup contains rows for Ctrl+G, h/j/k/l, Ctrl+U, Ctrl+D and Enter in a grid section.

### REQ-F-030: Asynchronous Thumbnail Decode (Ubiquitous)
Thumbnails shall be decoded asynchronously on a worker pool thread using a scaled QImageReader, never blocking the GUI thread.

**Acceptance:** A test decoder hook records the thread of every thumbnail decode → none equals the GUI thread. With a decoder that sleeps 500 ms, entering grid mode and pressing l updates the selection before any thumbnail finishes.

### REQ-F-031: Worker Pool Size (Constraint)
The thumbnail worker pool shall contain approximately half of QThread::idealThreadCount, with a minimum of 1 thread.

**Acceptance:** The pool's maxThreadCount equals max(1, idealThreadCount / 2): 8 when idealThreadCount is 16, 1 when it is 1.

### REQ-F-032: Thumbnail Cache (Ubiquitous)
The viewer shall keep thumbnails in an in-memory LRU cache bounded by at most 1000 entries and at most 256 MiB of pixel data, and shall write no thumbnail data to disk.

**Acceptance:** Request 1200 distinct thumbnails → the cache holds at most 1000 entries and the least recently used ones are evicted first; inserting 512x512 ARGB thumbnails keeps the byte total ≤ 256 MiB. Requesting a cached thumbnail again does not invoke the decoder. With XDG_CACHE_HOME pointed at an empty temp dir, it is still empty after the run.

### REQ-F-033: Visible-Plus-Lookahead Requests (State-driven)
While grid mode is active, thumbnail decode requests shall be issued only for visible cells plus approximately one page (rows) ahead; requests for cells scrolled out of range shall be cancelled or dropped before decoding.

**Acceptance:** With a counting decoder over 2000 files, entering grid at item 1 decodes at most (visible cells + one page of cells). Jumping to the end with repeated Ctrl+D while the decoder is blocked, then unblocking it → decodes for cells more than one page outside the final viewport are not executed.

### REQ-F-034: Image Limits Apply (Unwanted)
If a file exceeds the limits defined in image_limits.h, then the viewer shall not decode the thumbnail and shall display the broken-image glyph.

**Acceptance:** A PNG whose header declares dimensions beyond the image_limits.h maximum → the cell shows the broken glyph, the pixel decode is never invoked (header-only read), and Enter opens it in single view showing the usual limit error.

### REQ-F-035: Placeholder During Loading (State-driven)
While a thumbnail is being decoded, the cell shall display a neutral placeholder.

**Acceptance:** With a blocked test decoder, a visible cell's thumbnail status is Loading and the placeholder item is visible. After the decoder is released, status is Ready and the placeholder is hidden.

### REQ-F-036: Failed Thumbnail Display (Unwanted)
If a thumbnail decode fails, then the cell shall display a broken-image glyph and remain fully selectable.

**Acceptance:** A corrupt JPEG file is in the folder. Its cell shows the broken glyph. The cell can be selected and navigated. Enter opens it in single view, showing the decode error message.

### REQ-F-037: Animated Image First Frame (Conditional)
Where an image is animated (GIF, APNG, animated WebP), its grid cell shall display only the first frame of the animation.

**Acceptance:** A 3-frame GIF whose frames are solid red, green and blue → the thumbnail's centre pixel is red, and it is still red 2 s later.

### REQ-F-038: SVG Rendering (Conditional)
Where a file is an SVG, the viewer shall render the SVG into the 256x256 box with aspect ratio preserved.

**Acceptance:** An SVG with viewBox 0 0 200 100 at DPR 1 → the thumbnail is 256x128 px (vector sources scale up to the box) with non-transparent content.

### REQ-NF-001: GUI Thread Responsiveness (Non-functional)
The GUI thread shall never block waiting for a thumbnail decode. Measurably, holding j through 2000 synthetic files shall keep the GUI responsive with no observable decode on the GUI thread.

**Acceptance:** In a 2000-file folder, 200 synthetic auto-repeat j presses each complete their key handling in under 16 ms (measured with QElapsedTimer in the test), and the decode-thread hook records zero GUI-thread decodes.

### REQ-F-039: Rescan Preserves Selection by URL (Event-driven)
When Ctrl+R is pressed in grid mode, the selection shall remain on the same URL; if that file no longer exists, the selection shall move to the item at the old index (clamped to the list bounds).

**Acceptance:** Select "photo_010.jpg" (index 10). Add a file sorting before it, Ctrl+R → "photo_010.jpg" (now index 11) stays selected. Delete it, Ctrl+R → index 10 is selected. Delete all but 3 files, Ctrl+R → the last item is selected.

### REQ-F-040: Empty Folder After Rescan (Unwanted)
If a rescan in grid mode yields no existing files, then the grid area shall display the empty state (the same as single-view empty state). A current file that no longer exists on disk shall be hidden from the grid only; single-view browsing keeps its existing behaviour.

**Acceptance:** Grid is displaying 20 images. All images are deleted from the folder externally. Ctrl+R → grid shows the empty state placeholder within the grid area.

### REQ-F-041: Busy State During Rescan (State-driven)
While grid mode is rescanning the folder, the grid area shall display a busy or placeholder state until the scan is complete.

**Acceptance:** Ctrl+R on a folder with many files → grid shows a loading/busy indicator. Once the scan completes, the grid reappears.

### REQ-F-042: No Filesystem Watching (Ubiquitous)
The grid shall not monitor the filesystem for changes; rescans shall only occur when explicitly requested (Ctrl+R).

**Acceptance:** Add, delete, or modify image files in the folder while grid mode is active. Grid content does not change until Ctrl+R is pressed.

### REQ-F-043: Accessibility Grid Structure (Ubiquitous)
The grid shall be exposed as an Accessible.List; each cell shall be a ListItem named after the file with a selected state flag.

**Acceptance:** In a test, the grid item's Accessible.role is List; each instantiated cell's Accessible.role is ListItem, Accessible.name equals its filename, and Accessible.selected is true only for the selected cell. Manual check with accerciser confirms the tree.

### REQ-F-044: Keyboard Focus Ring Visible on Selection (Ubiquitous)
The keyboard focus ring shall be visually visible on the selected cell in the grid.

**Acceptance:** After entering grid mode and after each navigation key, the selected cell's focus-ring item is visible and all other instantiated cells' rings are hidden. The ring uses the shared holonight-qt tokens `HnMetrics.focusBorderWidth` and `HoloniightPalette.borderFocus`, the same way other HoloNight controls draw theirs, because holonight-qt has no standalone ring type.

### REQ-F-045: Opening Another File Leaves Grid (Event-driven)
When a file is successfully opened through Ctrl+O or drag-and-drop while grid mode is active, the viewer shall leave grid mode and show that file in single view.

**Acceptance:** In grid mode, drop file X from another folder → single view shows X and grid mode is false. Cancel the Ctrl+O dialog → grid mode stays true.

### REQ-F-046: Menu Previous/Next Move the Selection (State-driven)
While grid mode is active, the actions menu Previous and Next items shall move the grid selection by -1/+1 instead of changing the open image.

**Acceptance:** In grid mode at item 5, trigger the Next menu item → selection is item 6 and the document position is unchanged.

### REQ-F-047: Toggle During Scan (State-driven)
While the folder is still scanning, Ctrl+G shall enter grid mode and the grid area shall show the busy state until the scan completes.

**Acceptance:** With a scanner blocked mid-scan, press Ctrl+G → grid mode is true and the busy indicator is visible. After the scanner is released, the cells appear and the current image is selected.

### REQ-C-001: QML Implementation in Dedicated File (Constraint)
Grid presentation logic shall be implemented in a dedicated QML file under apps/viewer/qml, separate from Main.qml.

**Acceptance:** A file such as `apps/viewer/qml/grid/ThumbnailGrid.qml` exists and contains the grid presentation code. Main.qml imports and uses this component.

### REQ-C-002: Thumbnail Size Constant (Ubiquitous)
The 256 logical-pixel thumbnail size shall be defined exactly once as a named constant consumed by both C++ and QML.

**Acceptance:** Grep the sources for the literal 256 in grid/thumbnail code → exactly one definition; changing it to 128 in a test build changes both the decode box and the cell box.

### REQ-C-003: QtQuick.Controls Import (Constraint)
Grid QML shall import QtQuick.Controls as Controls and never import a concrete style (Basic, Fusion, etc.) or specify a style directly.

**Acceptance:** Grep the grid QML file → contains `import QtQuick.Controls as Controls`, contains no imports of `Basic`, `Fusion`, or style-specific modules.

### REQ-C-004: Shared Primitives from Holonight (Constraint)
Grid presentation shall use shared primitives from holonight-qt (Holonight.Core and Holonight.Controls), importing them explicitly where used.

**Acceptance:** Grid QML contains explicit imports: `import Holonight.Core`, `import Holonight.Controls`. Focus ring, buttons, menus use HoloNight components, not Qt Quick Basic.

### REQ-C-005: Format Check Pass (Constraint)
The grid QML code shall pass the project's format-check validation.

**Acceptance:** Run `task format-check` → no format errors are reported in grid-related QML files.

### REQ-C-006: QML Import Check Pass (Constraint)
The grid QML code shall pass qml-import-check validation.

**Acceptance:** Run `task qml-import-check` → no missing or invalid imports are reported in grid QML.

### REQ-C-007: QMLTypes Check Pass (Constraint)
The grid QML code shall pass qmltypes-check validation.

**Acceptance:** Run `task qmltypes-check` → no type inconsistencies are reported.

### REQ-C-008: QML Lint Pass (Constraint)
The grid QML code shall pass qml-lint with no errors or warnings.

**Acceptance:** Run `task qml-lint` → zero warnings or errors for grid QML files.

### REQ-C-009: Clang Tidy Pass (Constraint)
Any C++ code supporting grid functionality shall pass clang-tidy checks.

**Acceptance:** Run `task tidy` over C++ files implementing thumbnail decoding, worker pools, or cache → no errors or unaddressed warnings are reported.

### REQ-C-010: CTest Pass (Constraint)
Grid functionality shall pass all CTest tests.

**Acceptance:** Run CTest via the project preset (BUILD_TESTING=ON) → all tests pass (0 failures).

### REQ-C-011: Single-View Pan Not Regressed (State-driven)
While single view is active, arrow keys shall continue to pan the image as before; they shall not enter grid mode or navigate grid cells.

**Acceptance:** Open a large image in single-image view. Press arrow keys → image pans. Ctrl+G to open grid, Escape to close grid, single-image view restores pan behavior unchanged.

### REQ-C-012: Single-View Playback Not Regressed (State-driven)
While single view is active, Space shall continue to toggle playback on animated images; it shall not interact with grid mode.

**Acceptance:** Open an animated GIF in single-image view. Press Space → playback starts/stops. Space never activates grid cells or buttons.

## Requirements Traceability

| ID | Category | EARS template | Title |
|---|---|---|---|
| REQ-F-001 | Functional | Event-driven | Grid View Toggle |
| REQ-F-002 | Functional | Unwanted | Unshifted G Only |
| REQ-F-003 | Functional | Unwanted | Grid Entry Blocked When No Document |
| REQ-F-004 | Functional | Event-driven | Leaving Grid Keeps Current Image |
| REQ-F-005 | Functional | Unwanted | Escape in Grid Does Not Exit Fullscreen |
| REQ-F-006 | Functional | State-driven | Escape Outside Grid Still Exits Fullscreen |
| REQ-F-007 | Functional | Ubiquitous | Grid Displays All DirectoryModel Files |
| REQ-F-008 | Functional | Ubiquitous | Cell Layout Calculation |
| REQ-F-009 | Functional | Ubiquitous | Thumbnail Fitting |
| REQ-F-010 | Functional | Ubiquitous | Filename Elision |
| REQ-F-011 | Functional | Ubiquitous | Thumbnail Decode Resolution |
| REQ-F-012 | Functional | Ubiquitous | Selection Background and Focus Ring |
| REQ-F-013 | Functional | Event-driven | Selection on Grid Entry |
| REQ-F-014 | Functional | State-driven | Selection Stays in View After Navigation |
| REQ-F-015 | Functional | Event-driven | Horizontal Navigation |
| REQ-F-016 | Functional | Event-driven | Vertical Navigation |
| REQ-F-017 | Functional | Event-driven | Page-Down Navigation |
| REQ-F-018 | Functional | Event-driven | Page-Up Navigation |
| REQ-F-019 | Functional | Event-driven | Bracket Navigation |
| REQ-F-020 | Functional | Event-driven | Single Click Selects Cell |
| REQ-F-021 | Functional | Event-driven | Double Click Opens File |
| REQ-F-022 | Functional | Event-driven | Enter Opens Selected File |
| REQ-F-023 | Functional | State-driven | Focused Header Button Keeps Enter |
| REQ-F-024 | Functional | State-driven | Header Displays Folder Name and Count |
| REQ-F-025 | Functional | State-driven | Footer Displays Grid Navigation Hints |
| REQ-F-026 | Functional | State-driven | Disabled Features in Grid |
| REQ-F-027 | Functional | State-driven | Enabled Features in Grid |
| REQ-F-028 | Functional | Ubiquitous | Actions Menu Grid Toggle |
| REQ-F-029 | Functional | Ubiquitous | Help Popup Lists Grid Shortcuts |
| REQ-F-030 | Functional | Ubiquitous | Asynchronous Thumbnail Decode |
| REQ-F-031 | Functional | Constraint | Worker Pool Size |
| REQ-F-032 | Functional | Ubiquitous | Thumbnail Cache |
| REQ-F-033 | Functional | State-driven | Visible-Plus-Lookahead Requests |
| REQ-F-034 | Functional | Unwanted | Image Limits Apply |
| REQ-F-035 | Functional | State-driven | Placeholder During Loading |
| REQ-F-036 | Functional | Unwanted | Failed Thumbnail Display |
| REQ-F-037 | Functional | Conditional | Animated Image First Frame |
| REQ-F-038 | Functional | Conditional | SVG Rendering |
| REQ-NF-001 | Non-functional | Non-functional | GUI Thread Responsiveness |
| REQ-F-039 | Functional | Event-driven | Rescan Preserves Selection by URL |
| REQ-F-040 | Functional | Unwanted | Empty Folder After Rescan |
| REQ-F-041 | Functional | State-driven | Busy State During Rescan |
| REQ-F-042 | Functional | Ubiquitous | No Filesystem Watching |
| REQ-F-043 | Functional | Ubiquitous | Accessibility Grid Structure |
| REQ-F-044 | Functional | Ubiquitous | Keyboard Focus Ring Visible on Selection |
| REQ-F-045 | Functional | Event-driven | Opening Another File Leaves Grid |
| REQ-F-046 | Functional | State-driven | Menu Previous/Next Move the Selection |
| REQ-F-047 | Functional | State-driven | Toggle During Scan |
| REQ-C-001 | Constraint | Constraint | QML Implementation in Dedicated File |
| REQ-C-002 | Constraint | Ubiquitous | Thumbnail Size Constant |
| REQ-C-003 | Constraint | Constraint | QtQuick.Controls Import |
| REQ-C-004 | Constraint | Constraint | Shared Primitives from Holonight |
| REQ-C-005 | Constraint | Constraint | Format Check Pass |
| REQ-C-006 | Constraint | Constraint | QML Import Check Pass |
| REQ-C-007 | Constraint | Constraint | QMLTypes Check Pass |
| REQ-C-008 | Constraint | Constraint | QML Lint Pass |
| REQ-C-009 | Constraint | Constraint | Clang Tidy Pass |
| REQ-C-010 | Constraint | Constraint | CTest Pass |
| REQ-C-011 | Constraint | State-driven | Single-View Pan Not Regressed |
| REQ-C-012 | Constraint | State-driven | Single-View Playback Not Regressed |

## Summary

- Functional (REQ-F): 47
- Non-functional (REQ-NF): 1
- Constraints (REQ-C): 12

**Total:** 60 requirements, each with an independent acceptance criterion.
