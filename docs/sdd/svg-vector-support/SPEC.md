# SVG Vector Support Specification

**Feature**: Add SVG support to the image viewer with true vector rendering and crisp zoom at any magnification.

**Scope**: Static, non-animated `.svg` files with vector rendering via `QSvgRenderer` and dual-mode paint path in `ImageCanvas`.

**Status**: Specification-Driven Development

---

## Non-Goals

The following capabilities are explicitly out of scope and must not be implemented in this cycle:

- `.svgz` (gzip-compressed SVG) files
- Animated SVG (SMIL/CSS animation) — initial/static frame only
- Sandboxed or hardened rendering beyond the 10 MiB file-size cap
- Vector rendering in the thumbnail/info-popup preview (remains raster-only)

---

## Requirements

### Format Support & File Discovery

#### REQ-F-001: SVG Format Registration (Ubiquitous)

**Requirement**: The system shall register `.svg` as a supported image format in the file-open dialog and sibling-file browsing independent of `QImageReader::supportedImageFormats()`.

**EARS Template**: Ubiquitous.

**Rationale**: SVG decoding uses `QSvgRenderer`, not `QImageReader`, so format availability must not depend on unrelated Qt image-plugin installations.

**Acceptance Criterion**: Verify that (1) the file-open dialog name filters include "SVG images (*.svg)" or equivalent, (2) directory browsing via `directory_model.cpp` includes `.svg` in its suffix set, and (3) adding or removing an unrelated image-codec plugin does not change SVG availability.

---

#### REQ-F-002: SVG Format in Name Filters (Ubiquitous)

**Requirement**: The system shall include `.svg` in `ImageDocument::nameFilters()` with an explicit, unconditional list entry that does not derive from `QImageReader::supportedImageFormats()`.

**EARS Template**: Ubiquitous.

**Rationale**: Decouples SVG registration from raster-codec plugin availability.

**Acceptance Criterion**: Inspect `image_document.cpp` and confirm `ImageDocument::nameFilters()` contains a hardcoded SVG entry (e.g., `"*.svg"`); trace code to verify it is not derived from `QImageReader` at that point.

---

#### REQ-F-003: SVG Format in Directory Scanning (Ubiquitous)

**Requirement**: The system shall include `.svg` in the sibling-file suffix set scanned by `directory_model.cpp::scanDirectory()`, independent of image-codec plugins.

**EARS Template**: Ubiquitous.

**Rationale**: Ensures sibling navigation (next/prev file in directory) works correctly for SVG files.

**Acceptance Criterion**: Confirm `directory_model.cpp` contains an explicit `.svg` entry in the suffix set used during directory scan; navigate to a folder with mixed `.jpg` and `.svg` files and verify both appear as navigable siblings.

---

### Rendering & Display

#### REQ-F-004: Dual-Mode Canvas Paint Path (Ubiquitous)

**Requirement**: The system shall implement a dual-mode paint path in `ImageCanvas::paint()`: for raster formats (existing behavior), call `painter->drawImage(...)` with a decoded `QImage`; for SVG, call `QSvgRenderer::render(painter, targetRect)` directly without rasterization to an intermediate `QImage`.

**EARS Template**: Ubiquitous.

**Rationale**: Preserves crisp rendering at any zoom level during interactive zoom operations.

**Acceptance Criterion**: (1) Open an SVG file in the viewer, (2) perform an interactive zoom drag (pinch, scroll wheel, or zoom gesture) and observe that edges remain crisp/sharp during the drag (not jagged or aliased), (3) verify via debugger/trace that `QSvgRenderer::render()` is called, not `QImage` rasterization, at paint time.

---

#### REQ-F-005: Intrinsic Size from ViewBox (Ubiquitous)

**Requirement**: The system shall compute SVG intrinsic size from `QSvgRenderer::viewBoxF().size()` when a viewBox attribute is present.

**EARS Template**: Ubiquitous.

**Rationale**: viewBox is the authoritative intrinsic size in the SVG spec when declared.

**Acceptance Criterion**: (1) Open an SVG with an explicit viewBox attribute (e.g., `viewBox="0 0 100 200"`), (2) verify the image info popup displays the correct width and height (100 and 200, or with appropriate scaling if ViewBox units differ), (3) verify fit-to-window magnification `ViewGeometry::fitMagnification()` uses this size to compute the correct zoom ratio.

---

#### REQ-F-006: Intrinsic Size Fallback to Default Size (Conditional)

**Requirement**: If an SVG does not declare a viewBox, the system shall compute intrinsic size from `QSvgRenderer::defaultSize()` (Qt's resolution of width/height attributes or the SVG spec's 300×150 default).

**EARS Template**: Conditional.

**Rationale**: Handles SVG files that omit viewBox.

**Acceptance Criterion**: (1) Create or open an SVG with no viewBox (only width/height attributes or neither), (2) verify `QSvgRenderer::defaultSize()` is called and used for fit-to-window magnification, (3) confirm the info popup displays the correct size (or 300×150 if neither viewBox nor width/height are present).

---

#### REQ-F-007: Fit-to-Window Magnification (Ubiquitous)

**Requirement**: The system shall compute fit-to-window magnification by dividing canvas size by SVG intrinsic size, using the same formula as raster images.

**EARS Template**: Ubiquitous.

**Rationale**: Ensures consistent UI behavior between raster and vector formats.

**Acceptance Criterion**: (1) Open an SVG and a raster image of the same dimensions, (2) apply fit-to-window to both, (3) verify magnification values are identical or within rounding error of each other.

---

#### REQ-NF-001: Crisp Zoom During Interactive Drag (Non-Functional)

**Requirement**: During an active interactive zoom drag operation (pinch, scroll wheel, or zoom gesture), SVG rendering shall remain crisp without visible aliasing or jaggedness at intermediate zoom levels.

**EARS Template**: Non-Functional (Event-driven variant: "When the user initiates an interactive zoom drag, the system shall render SVG content crisply at each intermediate zoom level").

**Rationale**: Distinguishes this behavior from raster image zoom, which may pre-compute a single cached resolution.

**Acceptance Criterion**: Perform an interactive zoom gesture on an SVG; capture video/screenshot of edge quality during the drag; verify no visible jaggedness or pixelation is introduced during the drag (compare to a reference screenshot of the final zoomed state).

---

#### REQ-NF-002: Thumbnail Preview Remains Raster (Non-Functional)

**Requirement**: The small preview `ImageCanvas` instance in `ImageInformationPopup.qml` shall remain raster-only and shall not require the dual-mode vector paint path.

**EARS Template**: State-driven.

**Rationale**: Vector fidelity is not necessary at thumbnail scale; rasterization is acceptable and simplifies implementation.

**Acceptance Criterion**: Open the image information popup for an SVG file; observe that the small preview renders correctly (exact rendering implementation is deferred to Stage 2 design); verify that the popup preview code does not call the new vector-mode paint path or `QSvgRenderer::render()`.

---

### File Handling & Validation

#### REQ-F-008: SVG File Size Limit (Conditional)

**Requirement**: If an SVG file exceeds 10 MiB in size, the system shall reject the file before passing it to `QSvgRenderer`.

**EARS Template**: Conditional.

**Rationale**: Bounds worst-case parse/render cost for pathological markup (e.g., deeply nested `<use>` references).

**Acceptance Criterion**: (1) Create or obtain an SVG file > 10 MiB (or mock a file-size check), (2) attempt to open it, (3) verify it is rejected with a clean error message (same style as other unreadable-file errors), not a crash or hang.

---

#### REQ-C-001: 10 MiB SVG Size Constraint (Constraint)

**Requirement**: SVG files shall not exceed 10 MiB; this is a new, SVG-specific limit separate from `kFileLimitBytes` (256 MiB) and `kImageLimitBytes` (128 MiB) in `image_limits.h`.

**EARS Template**: Constraint.

**Rationale**: Raster-specific limits do not apply to text/XML formats; a new constant balances coverage of realistic SVG files with worst-case parse cost.

**Acceptance Criterion**: Confirm `image_limits.h` (or equivalent configuration) defines a new constant (e.g., `kSvgFileLimitBytes = 10 * 1024 * 1024`) and that this constant is referenced in the SVG file-size check (REQ-F-008).

---

#### REQ-F-009: SVG Document Retention (Ubiquitous)

**Requirement**: The system shall retain the loaded SVG renderer and raw SVG bytes in `ImageDocument` directly, bypassing the `decoded_image_cache` LRU budget cache.

**EARS Template**: Ubiquitous.

**Rationale**: Vector painting never produces a cacheable `QImage`; caching rasterized output defeats the crisp-zoom goal.

**Acceptance Criterion**: (1) Trace `ImageDocument::load()` for SVG files and verify it stores the renderer/bytes, not a `QImage`, (2) verify `decoded_image_cache` is not consulted for SVG lookups, (3) confirm memory usage scales with file size + renderer state, not with canvas size.

---

### Error Handling

#### REQ-F-010: Malformed SVG Handling (Conditional - Unwanted Behaviour)

**Requirement**: If an SVG file is malformed or unparseable (i.e., `QSvgRenderer::load()` returns false), the system shall fail cleanly through the same error path used for other format failures (e.g., tiled TIFF rejection).

**EARS Template**: Unwanted Behaviour.

**Rationale**: No crash, hang, or silent blank canvas.

**Acceptance Criterion**: (1) Create or obtain a malformed SVG (invalid XML, missing required elements), (2) attempt to open it, (3) verify the app displays a user-facing error message (same UI pattern as other "unreadable" errors, not a crash or freeze).

---

#### REQ-F-011: SVG Load Failure Reporting (Conditional - Unwanted Behaviour)

**Requirement**: If `QSvgRenderer::load()` fails, the system shall report the error to the user via the existing "damaged, unreadable" error dialog.

**EARS Template**: Unwanted Behaviour.

**Rationale**: Maintains consistent user-facing error handling.

**Acceptance Criterion**: Attempt to open a malformed SVG; verify an error dialog appears with a message indicating the file cannot be read (e.g., "Unable to open file: invalid SVG" or similar).

---

### Security & Trust

#### REQ-F-012: No Network Access During SVG Opening (Ubiquitous)

**Requirement**: The system shall not trigger network access when opening or rendering an SVG file.

**EARS Template**: Ubiquitous.

**Rationale**: SVG opening is a local, synchronous operation; no external services or network calls are required.

**Acceptance Criterion**: (1) Open an SVG file in an offline environment (network disabled or disconnected), (2) verify the file opens and renders without attempting to connect to the network (confirm via network monitor tool or firewall logs), (3) verify no timeouts or failures occur due to missing network.

---

#### REQ-F-013: Local File References Trust Level (State-driven)

**Requirement**: While an SVG document is open, local external file references (e.g., `<image xlink:href="...">` pointing to a local path) shall be resolved by `QSvgRenderer` at the same trust level as the SVG file itself.

**EARS Template**: State-driven.

**Rationale**: The user's act of opening the SVG establishes trust; no additional sandboxing is required in this cycle.

**Acceptance Criterion**: (1) Create an SVG with an `<image>` tag referencing a local file (e.g., a JPEG in the same directory), (2) open the SVG, (3) verify the referenced image is loaded and displayed (if applicable to the SVG structure), (4) verify no sandbox/permission prompt is triggered for the local reference beyond the user's initial choice to open the SVG.

---

### Non-Regression & Metadata

#### REQ-F-014: EXIF Metadata Exclusion for SVG (Ubiquitous)

**Requirement**: The system shall not attempt to extract or display EXIF metadata from SVG files; the image information popup shall show no Camera or Location sections for SVG files.

**EARS Template**: Ubiquitous.

**Rationale**: `ExifMetadata::payload()` already signature-sniffs file headers and returns empty for XML/text formats; no new code changes required.

**Acceptance Criterion**: (1) Open an SVG file, (2) open the image information popup, (3) verify that Camera and Location sections are not displayed (they should be absent or hidden, not showing "no data").

---

### Architecture & Dependencies

#### REQ-C-002: Qt6::Svg Dependency (Constraint)

**Requirement**: The system shall add `Qt6::Svg` to the viewer's CMakeLists.txt as a new dependency to provide `QSvgRenderer`.

**EARS Template**: Constraint.

**Rationale**: Qt6::Svg is a Qt-provided native module, not a third-party or KDE dependency; it is an approved exception to the earlier "no new codecs" constraint, which referred to bundled raster codec plugins.

**Acceptance Criterion**: Confirm `CMakeLists.txt` in the viewer includes `Qt6::Svg` in its `find_package()` and `target_link_libraries()` calls; verify the build succeeds with this dependency.

---

#### REQ-NF-003: Dual-Mode Canvas Architecture (Non-Functional)

**Requirement**: `ImageCanvas` shall be a dual-mode painter supporting both raster `QImage` drawing (existing formats) and direct vector painting (SVG) via `QSvgRenderer`, rather than a single uniform code path.

**EARS Template**: Non-Functional (Ubiquitous variant).

**Rationale**: This is a deliberate, accepted architectural tradeoff; code review and documentation shall reflect this intentional design.

**Acceptance Criterion**: (1) Review `image_canvas.cpp` and confirm the `paint()` method contains conditional branches for raster vs. vector rendering paths, (2) verify this branching is clearly documented with comments explaining the dual-mode rationale, (3) confirm test coverage includes both paths (e.g., test opening a raster image and an SVG in the same session).

---

#### REQ-C-003: SVG-Specific Module (Constraint)

**Requirement**: SVG rendering shall use `QSvgRenderer` from `Qt6::Svg` and shall not introduce any additional third-party, bundled, or codec-plugin dependencies.

**EARS Template**: Constraint.

**Rationale**: Maintains the project's constraint against new codec dependencies while clarifying that Qt's native SVG module is an approved exception.

**Acceptance Criterion**: Scan CMakeLists.txt and source code for any new dependencies (third-party libraries, KDE Frameworks, codec plugins); confirm only `Qt6::Svg` is added and no other new external dependencies are introduced.

---

## Summary of Requirements by Category

| Category | ID | Type | Title |
|----------|----|----|-------|
| Format Support | REQ-F-001 | F | SVG Format Registration |
| | REQ-F-002 | F | SVG in Name Filters |
| | REQ-F-003 | F | SVG in Directory Scanning |
| Rendering | REQ-F-004 | F | Dual-Mode Canvas Paint Path |
| | REQ-F-005 | F | Intrinsic Size from ViewBox |
| | REQ-F-006 | F | Intrinsic Size Fallback |
| | REQ-F-007 | F | Fit-to-Window Magnification |
| | REQ-NF-001 | NF | Crisp Zoom During Drag |
| | REQ-NF-002 | NF | Thumbnail Preview Remains Raster |
| File Handling | REQ-F-008 | F | SVG File Size Limit |
| | REQ-C-001 | C | 10 MiB SVG Size Constraint |
| | REQ-F-009 | F | SVG Document Retention |
| Error Handling | REQ-F-010 | F | Malformed SVG Handling |
| | REQ-F-011 | F | SVG Load Failure Reporting |
| Security | REQ-F-012 | F | No Network Access |
| | REQ-F-013 | F | Local File References Trust |
| Non-Regression | REQ-F-014 | F | EXIF Metadata Exclusion |
| Architecture | REQ-C-002 | C | Qt6::Svg Dependency |
| | REQ-NF-003 | NF | Dual-Mode Canvas Architecture |
| | REQ-C-003 | C | SVG-Specific Module Constraint |

---

## EARS Template Usage Summary

- **Ubiquitous**: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-007, REQ-F-009, REQ-F-012, REQ-F-014, REQ-C-002, REQ-NF-003
- **Event-driven**: REQ-NF-001 (zoom drag trigger)
- **State-driven**: REQ-F-013, REQ-NF-002
- **Conditional**: REQ-F-006, REQ-F-008
- **Unwanted Behaviour**: REQ-F-010, REQ-F-011

---

## Acceptance Criteria Verification Strategy

All requirements above define specific, falsifiable acceptance criteria that can be verified through:

1. **Code Inspection**: CMakeLists.txt, image_canvas.cpp, image_document.cpp, directory_model.cpp
2. **Functional Testing**: Opening SVG files, zoom interactions, error cases
3. **Integration Testing**: Sibling navigation, thumbnail preview, info popup
4. **Architecture Review**: Dual-mode paint path, storage strategy, dependency analysis
5. **Non-Regression Testing**: EXIF behavior, existing raster formats still work

Each requirement's acceptance criterion is independently testable and does not restate the requirement itself.
