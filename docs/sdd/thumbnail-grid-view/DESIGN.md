# DESIGN: Thumbnail Grid View

Input: `docs/sdd/thumbnail-grid-view/SPEC.md` (60 requirement IDs, after the S-1..S-10 amendments). Every path and function named below was checked against the tree at commit `9b71c53`, or is explicitly marked as new.

## 0. Findings that shape the design

These are facts about the existing code, and several of them differ from the SPEC's assumptions.

1. **No decode duplication is needed.** `HolonightImages::decode(QIODevice&, DecodeOptions{limits, bound, orientation}, cancelled)` in `../holonight-images/include/holonight_images/image.h` already does everything a thumbnail needs:
   - It runs `header()` first (a size and pixel-limit check via `acceptableSize`) and only then `reader.read()`. This is the header-only limit rejection REQ-F-034 asks for.
   - It sets `QImageReader::setScaledSize` from `bound`, never upscales, and falls back to `QImage::scaled` when the format handler ignores scaling.
   - It applies EXIF orientation by default, exactly as single view does. `readImage()` in `image_document.cpp` uses the same default and passes `.bound = {}`.
   - It honours `cancelled`.

   Viewer's limits are `kRasterLimits` in `apps/viewer/image_limits.h`. SVG has the matching `HolonightImages::rasterizeSvg(bytes, {.bound, .outputBytes, .animation}, cancelled)`. It enlarges to the bound, which matches REQ-F-038, and it revalidates resources. `loadSvg(file, kSvgFileLimitBytes, cancelled)` supplies the bytes.
2. **`scanDirectory()` never returns an empty list.** `apps/viewer/directory_model.cpp` does `if (!result.urls.contains(explicitUrl)) result.urls.append(explicitUrl)`. A deleted current file therefore stays in the listing. REQ-F-040 (empty state after rescan) and the "delete it, Ctrl+R" case of REQ-F-039 cannot be satisfied without a change. See §1.9.
3. **There is no focus-ring primitive in holonight-qt** (`../holonight-qt/qml`, `qml/controls`). Every control draws its own ring inline: a `Rectangle` with `border.width: HnMetrics.focusBorderWidth` and `border.color: HoloniightPalette.borderFocus`, driven by `visualFocus`. `HoloniightPalette.focusRing` also exists as a token. `ViewerButton` in `Main.qml` does the same. REQ-F-044's wording "the Holonight.Controls focus ring primitive" cannot be met literally. The design uses the shared tokens and metrics (§1.4) and does not invent a type.
4. **Available holonight-qt pieces**, verified in `qml/`:
   - Palette tokens: `HoloniightPalette.surfaceSelected`, `surfaceSelectedHover`, `surfaceElevated`, `borderFocus`, `borderPassive`, `textPrimary`, `textMuted`, `selectionIndicator`.
   - Metrics: `HnMetrics.focusBorderWidth`, `borderWidth`, `internalSpacing()`, `iconSize()`.
   - Shape: `HnAppearance.roundedRadius(HnSurfaceRole.Control, w, h, HnAppearance.revision)`.
   - Widgets: `HnLabel`, `HnIcon`, `HnKeyHint`, `HnKeySequenceLabel`, `HnLoadingState` (an indeterminate `ProgressBar` plus title), `HnEmptyState`.

   The palette type really is spelled `HoloniightPalette` (double i); use it as written.
   There is no broken-image icon in `../holonight-icons`, so the viewer adds `apps/viewer/icons/broken-image.svg`.
5. **Tests are GoogleTest.** They form a single binary, `viewer-smoke` (`tests/CMakeLists.txt`, `add_viewer_application(viewer-smoke ...)`), with QtTest helpers such as `QTest::keyClick`.
   - There is no QtQuickTest. "QML tests" means `QQmlApplicationEngine.loadFromModule("HolonightViewer","Main")` or `QQmlComponent::loadFromModule("HolonightViewer","<Name>")` plus C++ inspection. `tests/footer_key_hints_test.cpp` is the reference, and `tests/smoke.cpp` shows the engine setup.
   - `ImageDocument` takes an injectable `Decoder` and `DirectoryModel::Scanner`. The existing test seam style is `std::function` injection.
6. **Tooling lists are stale in memory and AGENTS-adjacent notes.**
   - `scripts/format-sources.py` and `scripts/check-qml-import-policy.py` use `rglob`. The `tidy` target globs `apps/*.cpp` and `tests/*.cpp`, and `qml-lint` follows the `QML_FILES` of the module.
   - A new QML file needs only a `QML_FILES` entry in `apps/viewer/CMakeLists.txt` (`add_viewer_application`). The QML-file lists are not in the Taskfile any more.
   - A new icon needs an entry in the `foreach(icon close empty-viewer ...)` list. A new QML-exposed C++ type needs an entry in the `SOURCES` of `qt_add_qml_module` if it lives outside `viewer-private`, and its name in the `for type_name in ...` loop of `scripts/check-qmltypes.sh`.
7. **`WindowKeyRouter` (`apps/viewer/window_key_router.h`) is an event filter on the window.**
   - It returns `false` for any modifier other than Shift or Keypad. Ctrl+U and Ctrl+D therefore need explicit handling.
   - It returns `false` when `modalActive`. It maps j/k to Down/Up while `menuOpen`, and it owns Tab cycling over `informationButton`, `fullscreenButton` and `actionsButton`.
   - It uses `headerOrPlaybackButtonFocused()` to leave Space to focused buttons.
   - Qt `Shortcut`s win over event filters. `Shortcut`s declared in `Main.qml` for `[`, `]`, `Escape`, `Ctrl+R` and others never reach the router.
8. **`Main.qml` structure that constrains us:**
   - `canInspect = Ready && !modalActive` gates zoom, fit, 1:1, rotate/flip/reset, copy image, the `DragHandler`/`WheelHandler` and the router's `imageReady`/`playbackAvailable`.
   - `hasPath` gates image information, Refresh and the information button.
   - `clearImageFocus()` parks focus on the 1x1 `neutralFocus` item after every action. Tab moves focus only among the three header buttons.
   - `emptyStateGroup` is tied to `document.state === ImageDocument.Empty`.
   - `folderError` shows "Scanning…" while `document.scanning`.
   - `viewerHeader`/`viewerFooter` are hidden in fullscreen unless `arrowsShown`. The canvas area goes full-bleed in fullscreen.
9. **Behavioural traps in `ImageDocument`/`DirectoryModel`:**
   - `open()` always rescans (`directory_.scan`). `select()` alone does not. Grid "open this file" must therefore not call `open()`. See `openFromFolder` in §3.2.
   - `scan()` first publishes a one-row model `{selected}` (`replace({*pending_})`, `scanning_ = true`), then the real list. A grid bound naively to the model would flash one cell.
   - `changed()` is the only notify signal for `scanning`, `count`, `position` and so on. A QML `onScanningChanged` handler on `document` fires on every `changed()`. Derive a QML-declared `property bool` from it; that property notifies only on value change.
   - `scan()` calls `clear()` (which sets `scanning_ = false`) and then sets `scanning_ = true` within one call stack, so a fast Ctrl+R while scanning produces a transient false.

## 1. Components

### 1.1 New C++ (in `viewer-private` unless noted)

| File | Responsibility |
|---|---|
| `apps/viewer/thumbnail_size.h` | The single definition `inline constexpr int kThumbnailBoxLogical = 256;` plus `constexpr int thumbnailBoxPixels(qreal dpr)`. It computes `ceil(kThumbnailBoxLogical * dpr - 1e-6)`; the epsilon prevents `256 * 2.0000000001 -> 513`. It also declares the pool-size and cache-limit constants (`kThumbnailCacheEntries = 1000`, `kThumbnailCacheBytes = 256 MiB`) and `thumbnailThreadCount(int ideal) = max(1, ideal / 2)`. No other file contains the literal 256 for thumbnails (REQ-C-002, checked by a CTest script, §6). |
| `apps/viewer/thumbnail_decoder.{h,cpp}` | Pure worker-side function `ThumbnailResult decodeThumbnail(const ThumbnailRequest&, const std::atomic_bool& cancelled)`. It reuses `kRasterLimits`, `kSvgFileLimitBytes` and `HolonightImages::decode`/`rasterizeSvg`. It runs on a worker thread and touches no GUI state. |
| `apps/viewer/thumbnail_cache.{h,cpp}` | GUI-thread-only LRU `ThumbnailCache` (list plus hash). It is bounded by both an entry count and a byte budget, and it is unit-testable without Qt Quick. |
| `apps/viewer/thumbnail_provider.{h,cpp}` | `ThumbnailProvider : QQuickAsyncImageProvider`. It owns the `QThreadPool`, the `ThumbnailCache` and the injectable decoder, and defines `ThumbnailResponse : QQuickImageResponse` with cancellation. Registered as `image://thumbnail/`. |
| `apps/viewer/grid_navigation.h` (module `SOURCES`, header-only like `window_state.h`) | The pure navigation function `GridNavigation::target(...)` plus a `QML_SINGLETON` wrapper `GridNavigation` exposing `enum Move` and the same function to QML. |
| `apps/viewer/thumbnail_metrics.h` (module `SOURCES`, header-only) | `QML_SINGLETON ThumbnailMetrics`. It has a `boxSize` `CONSTANT` property (from `kThumbnailBoxLogical`), `static Q_INVOKABLE int devicePixels(qreal dpr)` and `static Q_INVOKABLE QString sourceFor(const QUrl&, qreal dpr, int generation)`. It is the only place that builds thumbnail URLs. |
| `apps/viewer/folder_grid_model.{h,cpp}` (module `SOURCES`) | `FolderGridModel : QSortFilterProxyModel` over `DirectoryModel`. It hides a listing entry that is known to be missing on disk (§1.9) and offers `urlAt`/`indexOfUrl` in proxy row space. |
| `apps/viewer/icons/broken-image.svg` | The failed-thumbnail glyph. Add it to the icon `foreach` in `apps/viewer/CMakeLists.txt`. |

### 1.2 Changed C++

| File | Change |
|---|---|
| `apps/viewer/main.cpp` | After creating `QQmlApplicationEngine engine;`, call `engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);` before `loadFromModule`. The engine takes ownership. `tests/installed_runtime.cpp` and any test that enters grid mode do the same, with an injected decoder in tests. |
| `apps/viewer/image_document.{h,cpp}` | Add `Q_PROPERTY(QUrl url READ url NOTIFY changed)` (returns `selected_url_`), `Q_PROPERTY(QString folderName READ folderName NOTIFY changed)`, `Q_PROPERTY(FolderGridModel* folder READ folder CONSTANT)` and `Q_INVOKABLE void openFromFolder(const QUrl&)`. Add a `FolderGridModel grid_model_` member constructed after `directory_`. See §3.2. |
| `apps/viewer/directory_model.{h,cpp}` | `DirectoryResult` gets `QUrl missing;`, set in `scanDirectory()` when the explicit URL was injected because it is not an existing regular file. `DirectoryModel` stores it (`missingUrl()`), clears it in `replace({*pending_})` and `clear()`, and sets it when a scan result is applied. `indexOf`/`urlAt` are untouched, so single-view browsing keeps its current behaviour. |
| `apps/viewer/window_key_router.h` | Grid routing. See §1.6 and §3.3. |
| `apps/viewer/qml/Main.qml`, `qml/footer/FooterKeyHints.qml`, `qml/shortcuts/ShortcutHelpPopup.qml` | See §1.5, §1.7 and §1.8. |
| `apps/viewer/CMakeLists.txt` | Add `qml/grid/ThumbnailGrid.qml` and `qml/grid/ThumbnailCell.qml` to `qml_files`, the new headers to `SOURCES`, the new `.cpp` files to `viewer-private`, and `broken-image` to the icon list. |
| `scripts/check-qmltypes.sh` | Add `ThumbnailMetrics GridNavigation FolderGridModel` to the type loop. |

### 1.3 The provider versus a QObject service

- **Chosen: `QQuickAsyncImageProvider`.**
  - `Image.status` (`Loading`/`Ready`/`Error`) maps directly onto REQ-F-035 and REQ-F-036, with no per-delegate plumbing.
  - Source change or delegate destruction already calls `QQuickImageResponse::cancel()` on the pending request. That is exactly the REQ-F-033 cancel path with no extra QML glue.
  - ~~The response is a QObject on the GUI thread, so cache hits and inserts need no locking.~~ **Corrected in T-018:** Qt calls `requestImageResponse` and `cancel` on its image-loading (pixmap reader) thread, so each response lives there. The provider guards its cache with a mutex, and the decode task is a `QObject` + `QRunnable` that reports by a queued `decoded` signal connected to the response, which makes destroying a response at any moment safe (no raw or `QPointer` dereference from the worker or the GUI thread).
  - The decoder seam is one `std::function`, in the same style as `ImageDocument::Decoder`.
- **Rejected: a `QObject` service** (`ThumbnailLoader` with `Q_INVOKABLE request(url)` and per-delegate `QImage` properties). It would have to reimplement status tracking, cancellation on destroy and texture upload. QML cannot hold a `QImage` cheaply per cell without `QQuickImageProvider` anyway. It would also add a required object to `Main`. Adding a second `required property` would break every existing test that sets only the `document` initial property.
- **Rejected: a synchronous `QQuickImageProvider`** (`requestImage`). Qt runs it on its own reader thread with no cancellation and no pool control (REQ-F-030/031/033).
- **Engine cache off.** Delegates use `Image { cache: false }` so the provider's LRU is the only thumbnail cache. Otherwise `QQuickPixmap` would hold a second, unbounded-by-us copy and REQ-F-032 (at most 1000 entries) would not be testable.

### 1.4 New QML

Both files live under `apps/viewer/qml/grid/`. Each imports `QtQuick.Controls as Controls`, `Holonight.Core` and `Holonight.Controls` explicitly where used (REQ-C-003/004) and no style module. Both are registered in `QML_FILES`.

- **`ThumbnailGrid.qml`** is a `GridView` container with a busy overlay. It owns:
  - layout (§3.4);
  - selection state (`selectedUrl` and `selectedIndex`);
  - scroll-into-view;
  - rescan restoration;
  - the `activated(url)` and `selectionInteraction()` signals.

  It contains no key handling.
- **`ThumbnailCell.qml`** is the delegate, so it can be loaded standalone for narrow-layout tests, the same pattern as `FooterKeyHints`. It holds:
  - the tinted rounded background, drawn behind the image;
  - the ring, an inline `Rectangle` using `HnMetrics.focusBorderWidth`, `HoloniightPalette.borderFocus` and the same radius as the tint, with `objectName: "cellFocusRing"` (§0.3);
  - a thumbnail `Image` inside a fixed `ThumbnailMetrics.boxSize` square;
  - a placeholder `Rectangle` (`surfaceElevated`), visible while `image.status === Image.Loading` or before a source is set;
  - a broken-image `HnIcon` (`rendering: HnIcon.Semantic`, `source: "../icons/broken-image.svg"`), visible while `status === Image.Error`;
  - a single-line `HnLabel` (`elide: Text.ElideMiddle`, `maximumLineCount: 1`);
  - a `HoverHandler` and `Controls.ToolTip.text` (full name, shown only when the label is truncated or always; either satisfies REQ-F-010);
  - `TapHandler` for single and double taps;
  - accessibility: `Accessible.role: Accessible.ListItem`, `Accessible.name: fileName`, `Accessible.selected: isSelected`. Children (`Image`, `HnLabel`, ring, placeholder) set `Accessible.ignored: true`. Per the project memory this promotes their content, so the cell root is the only exposed node and the inner items must not re-expose text.

  Image sizing (REQ-F-009): the `Image` is centred in the box with `width: Math.min(implicitWidth, box)`, `height: Math.min(implicitHeight, box)` and `fillMode: Image.PreserveAspectFit`. It never upscales. The logical size is the source fitted into 256×256 without upscaling, measured in source pixels = logical pixels (the same as single view's 1:1). The provider tags the returned `QImage` with `devicePixelRatio = decodedWidth / logicalWidth` (≥ 1). A large source therefore reports logical 256 at any DPR, including fractional ones where `ceil` over-allocates (e.g. 282 px at 1.1). A 100×100 source at DPR 2 decodes to 100×100 px, is tagged DPR 1, and shows at 100×100 logical (REQ-F-009). `decodeThumbnail` returns the original source size alongside the image so the provider can compute `logicalWidth`. Verify in the first spike (§5, R-9) that `Image.implicitWidth` divides by the image's DPR for provider images. Fallback if it does not: put `pixelSize / scale` on a `Q_PROPERTY` via a status-only helper, or set width and height explicitly from the decoded size reported in the response id.

### 1.5 `Main.qml` changes

Add `import "grid"`.

- **State:** `property bool gridMode: false`, and readonly helpers
  ```
  canEnterGrid   = document.localPath.length > 0 && (document.scanning || document.folder.count > 0) && !modalActive
  canToggleGrid  = gridMode || canEnterGrid
  canInspect     = Ready && !modalActive && !gridMode        // edited
  canShowInformation = hasPath && !gridMode                   // new
  ```
  Editing `canInspect` alone disables, in one place, zoom, fit, 1:1, rotate/flip/reset, copy image, the drag and wheel handlers, the router's `imageReady` (so pan), and `playbackAvailable` (so Space). This covers REQ-F-026 and REQ-C-011/012 by construction. `imageInformation.enabled`, `informationButton.enabled` and its menu item move to `canShowInformation`. Copy Path stays enabled (not image-only, not listed in REQ-F-026).
- **Functions:**
  ```
  function enterGrid(): void   // canEnterGrid -> gridMode = true; grid.enter(document.url); actionsMenu stays as is
  function leaveGrid(): void   // gridMode = false; clearImageFocus()   (document is untouched: REQ-F-004)
  function toggleGrid(): void
  function browse(direction)   // if gridMode: grid.move(direction < 0 ? GridNavigation.Previous : GridNavigation.Next) else existing
  ```
  `browse` is made mode-aware so the `[`/`]` shortcuts and the Previous/Next menu items move the selection in grid mode instead of changing the open document (REQ-F-019, REQ-F-004). Their `enabled` conditions become `gridMode ? grid.canMove(dir) : document.canPrevious/canNext`.
- **Ctrl+G:**
  ```
  Shortcut { sequence: "Ctrl+G"; enabled: !window.modalActive && window.canToggleGrid; onActivated: window.toggleGrid() }
  ```
  The literal string `"Ctrl+G"` is used, not `StandardKey.FindNext`, so that Ctrl+Shift+G does not match (REQ-F-002). Blocked when no document or empty listing (REQ-F-003).
- **Escape precedence:** the existing `Escape` `Shortcut` becomes
  ```
  onActivated: { if (window.gridMode) { if (!window.actionsMenuOpen) window.leaveGrid() } else window.leaveFullscreen() }
  ```
  Grid closes first and fullscreen is untouched (REQ-F-005). Outside grid the behaviour is unchanged (REQ-F-006). Popups (help, information, dialog) are modal, so `Shortcut`s are blocked and the popups close on their own Escape. The `actionsMenuOpen` guard keeps a menu-closing Escape from also closing the grid.
- **Layout (`canvasArea`):**
  - `ImageCanvas` gets `visible: !window.gridMode`. It stays alive, so zoom and pan survive a round-trip.
  - `previousButton`, `nextButton`, `playPauseButton` and `detailsStrip` get `&& !window.gridMode` in their visibility.
  - `ThumbnailGrid { id: grid; anchors.fill: parent; visible: window.gridMode; model: window.document.folder }` is added.
  - `emptyStateGroup.visible` becomes `document.state === Empty || (gridMode && grid.showsEmpty)`. This reuses the existing glyph and hints (REQ-F-040) without touching its objectNames, which `tests/empty_state_test.cpp` uses.
  - In grid mode the `canvasArea` anchors and the header/footer visibility use the non-fullscreen rule (`viewerHeader.visible` and `viewerFooter.visible` add `|| window.gridMode`; `anchors.top`/`bottom` use `window.fullscreen && !window.gridMode`). The header and footer are therefore always reachable in fullscreen grid mode, since the canvas mouse-move that reveals them is gone.
- **Header title (REQ-F-024):** the `HnLabel` `rawText` becomes
  ```
  window.gridMode ? (document.scanning ? document.folderName
                     : qsTr("%1 — %n images", "", document.folder.count).arg(document.folderName))
                  : document.fileName || qsTr("HoloNight Viewer")
  ```
  `%n` gives plural-aware translation. The implementation uses singular and plural English source strings, both with the numerus argument, so the fallback is grammatical without a translation catalog. While scanning, `count` is the transient 1 (§0.9), so only the folder name is shown. The window `title` property is unchanged.
- **Footer:** `FooterKeyHints { gridMode: window.gridMode; animated: ... }` (§1.7).
- **Menu (REQ-F-028):** add after "Refresh":
  ```
  ViewerMenuItem { objectName: "gridToggleItem"; text: qsTr("Grid View"); checkable: true
                   shortcutKeys: [[Qt.Key_Control, Qt.Key_G]]; enabled: window.canToggleGrid
                   onTriggered: window.toggleGrid() }
  Binding { target: gridToggleItem; property: "checked"; value: window.gridMode }
  ```
  The `Binding` keeps `checked` in step after `checkable` toggles it, which would otherwise break a plain `checked:` binding. `shortcutKeys` is presentation-only and literal, per the memory lesson. Adding an item shifts `currentIndex` slots (separators take slots), so `tests/menu_layout_test.cpp` index assumptions need updating. Verify whether the provider `MenuItem` renders a checkable indicator. If not, add a check glyph inside `ViewerMenuItem.contentItem`, gated on `control.checkable && control.checked`, and set `Accessible.checked`.
- **Opening files (REQ-F-021/022):** `grid.onActivated: url => { window.document.openFromFolder(url); window.leaveGrid(); }`.
  - Ctrl+O and drop, on success (`portalFileChooser.onFinished`, `DropArea.onDropped`), set `gridMode = false` before `document.open()`, so a newly opened file shows in single view. A cancelled dialog leaves the grid alone. This is a design decision for a case the SPEC leaves open (§5, spec issue S-6).
- **Rescan:** `refreshFolder()` also does `++window.thumbnailGeneration`. That integer flows through `ThumbnailMetrics.sourceFor(url, dpr, generation)` into the thumbnail id, so Ctrl+R re-decodes thumbnails without a stat call per cache hit. Old-generation entries age out of the LRU.
- **Animation:** OR `window.gridMode` into the existing `suspendedByModal` `Binding` (`value: window.modalActive || window.gridMode`), so a hidden animated canvas does not keep decoding frames. Playback state is restored on leaving grid.

### 1.6 `WindowKeyRouter` changes (grid keys live in the router)

- **New:** `Q_PROPERTY(bool gridActive MEMBER m_gridActive)`, `signals: void gridMoveRequested(int move); void gridActivateRequested();`.
- **Mapping while `gridActive`** (after the modal check, the menu branch and the Tab branch, before the `m_imageReady` branch):
  - Left/H -> `Previous`; Right/L -> `Next`; Up/K -> `RowUp`; Down/J -> `RowDown`. These are `GridNavigation::Move` values.
  - Ctrl+D -> `PageDown` and Ctrl+U -> `PageUp`. The current early return `modifiers & ~(Shift|Keypad)` is relaxed only when `gridActive` and the modifiers are exactly Ctrl. Ctrl+Shift+D/U are ignored (REQ-F-017).
  - Return/Enter/keypad Enter -> `gridActivateRequested()`. The router skips auto-repeat and skips it while a header button is focused (`informationButton`, `fullscreenButton`, `actionsButton`, reusing `headerOrPlaybackButtonFocused()`), so the focused button activates (REQ-F-022/023). Space is never handled in grid mode, so a focused button keeps it.
  - Arrow keys never emit `panRequested` in grid mode (`imageReady` is false there because of `canInspect`).
- `Main.qml` handles them: `onGridMoveRequested: move => { grid.move(move); window.clearImageFocus() }` and `onGridActivateRequested: grid.activateSelection()`.

**Why the router and not `Keys` on the `GridView`:**

- Focus. This app deliberately parks focus on `neutralFocus`, and `clearImageFocus()` runs after almost every action, including menu close. A `Keys` handler on the grid would need to win focus back after each of those. The router works whatever item holds focus, exactly like today's pan keys, including when a header button is focused.
- Modality. A modal popup takes focus and `Shortcut`s stop. A grid `Keys` handler would have to check every modal flag itself, while the router already returns `false` for `m_modalActive` and forwards j/k for an open menu, so the menu keeps precedence.
- Tab and header buttons are already owned by the router. Enter/Space for header buttons (REQ-F-023) is the same "focused button wins" rule as for playback.
- Testability. Key behaviour is tested by sending `QKeyEvent`s to the window, as the existing tests do, and the maths are pure (§1.7 below).
- Cost: Ctrl+U/D need a small relaxation of the modifier filter in the router. It is limited to `gridActive` so single-view behaviour is untouched. `[` and `]` and Ctrl+G stay ordinary `Shortcut`s because they already exist there.

### 1.7 Navigation maths helper (pure C++)

`apps/viewer/grid_navigation.h`, a plain function with no Qt types on the hot path:

```cpp
namespace GridNavigation {
enum class Move : int { Previous, Next, RowUp, RowDown, PageUp, PageDown };
// Returns the new index in [0, count-1]; -1 when count <= 0. current outside [0,count) is treated as 0.
// columns < 1 is treated as 1. visibleRows < 0 is treated as 0.
constexpr int target(int current, Move move, int count, int columns, int visibleRows);
}
```

Rules, checked against the SPEC examples:

- `Previous`/`Next`: `clamp(current -/+ 1, 0, count-1)`, with no wrap (REQ-F-015/019).
- `RowDown`: if `current / columns == (count-1) / columns` (already in the last row), stay. Otherwise `min(current + columns, count - 1)`. Example, 7 items and 3 columns: index 5 (cell 6) gives index 6; index 6 stays.
- `RowUp`: if the row is 0, stay. Otherwise `current - columns` (REQ-F-016).
- `PageDown`: `rows = max(1, visibleRows / 2)`, giving `min(current + rows * columns, count - 1)`. With 30 items, 3 columns and 4 rows: index 0 gives 6 (item 7); index 24 gives 29 (item 30) (REQ-F-017).
- `PageUp`: `max(current - rows * columns, 0)`. Item 14 gives item 8; item 5 gives item 1 (REQ-F-018).

`class GridNavigation : QObject` (`QML_SINGLETON`, `Q_ENUM(Move)`) wraps it as `static Q_INVOKABLE int target(...)`, following `WindowState`.

### 1.8 Footer and help popup

- **`FooterKeyHints.qml`:** add `property bool gridMode: false`. `hints` selects a second array in grid mode. Names are stable ids for `objectName: "footerHint" + name`:
  - `GridNavigate` `[[Key_H],[Key_J],[Key_K],[Key_L]]`, "Move";
  - `GridPage` `[[Key_Control, Key_U],[Key_Control, Key_D]]`, "Page";
  - `GridOpen` `[[Key_Return]]`, "Open";
  - `GridToggle` `[[Key_Control, Key_G]]`, "Grid";
  - `Fullscreen`;
  - `Help` (last, always shown, as `fittingCount` requires).

  There is no Zoom, Fit, 100%, Rotate or Play/Pause entry. The single-view array is untouched, so `footer_key_hints_test` `kHints` still holds. No Ctrl+G hint is added to the single-view footer, because the SPEC does not require it and it would churn the existing test; discoverability comes from the menu and help.
- **`ShortcutHelpPopup.qml`:** add a `Grid` section (key `Grid`, label `qsTr("Grid")`) with rows for:
  - Ctrl+G ("Toggle grid view");
  - H/J/K/L, one row with four groups ("Move selection");
  - Ctrl+U ("Half page up") and Ctrl+D ("Half page down") as two rows;
  - Enter ("Open selected image").

  Update the Esc row to "Close dialog, grid or leave fullscreen". It is also asserted in `tests/shortcut_help_popup_test.cpp` `expectedSections()`, which must change with it. The popup is identical in both modes (REQ-F-029). Modal-blocking lesson: the popup already declares its own `?` `Shortcut` inside its content. No new key needs to work while it is open.

### 1.9 Selection preservation and the missing-file problem

- **Selection lives in QML (`ThumbnailGrid`)** as `selectedUrl` (source of truth) and `selectedIndex`, derived through `model.indexOfUrl(selectedUrl)`.
  - `GridView.currentIndex` is not used as state, because a model reset sets it to 0.
  - `lastIndex` is remembered on every selection change.
- **Restoration (REQ-F-039):** `ThumbnailGrid` declares `property bool scanning: document-supplied` (bound to `document.scanning`). Being a QML-declared property, it notifies only on value change (§0.9).
  - On `scanning` true -> false it runs `Qt.callLater(restoreSelection)`. The deferral swallows the transient false produced by a Ctrl+R during a scan, and the handler re-checks `!scanning`.
  - `restoreSelection()`: `i = model.indexOfUrl(selectedUrl)`. If `i < 0`, `i = clamp(lastIndex, 0, count-1)` and `selectedUrl = model.urlAt(i)`. Then it scrolls into view.
- **Missing file (REQ-F-040 and the deleted-open-file case):** `scanDirectory` records `result.missing = explicitUrl` when it had to append the explicit URL and that URL is not an existing regular file. `FolderGridModel::filterAcceptsRow` rejects the row whose URL equals `DirectoryModel::missingUrl()` (`setRecursiveFilteringEnabled` is not needed; it is a flat list). Single-view browsing, `position`, `count`, `canNext` and so on keep reading `DirectoryModel` directly. The current "file no longer exists" error flow is unchanged. An injected file that does exist (hidden file, unusual suffix) stays in the grid.

## 2. Data flow

### 2.1 Entering the grid
1. Ctrl+G (Shortcut), the menu item, or a triggered action calls `toggleGrid()`. If `gridMode` is false and `canEnterGrid`, then `gridMode = true`.
2. `grid.enter(document.url)` sets `selectedUrl`. `selectedIndex = model.indexOfUrl(url)`; if that is -1 it becomes 0 (missing file). It then schedules `Qt.callLater(scrollSelectionIntoView)`. The deferral is needed because `width` and `height` may be 0 on first layout (§5, R-6).
3. `keyRouter.gridActive` becomes true, `canInspect` becomes false, the canvas and its overlays hide, the header title, footer and menu check update, and the animation suspends.
4. `GridView` instantiates delegates for the viewport plus `cacheBuffer` (= one page height). Each `ThumbnailCell` binds `Image.source` to `ThumbnailMetrics.sourceFor(url, window.devicePixelRatio, window.thumbnailGeneration)`.

### 2.2 Navigating
`h/j/k/l/arrows/Ctrl+U/Ctrl+D` reach `WindowKeyRouter::eventFilter` (only when no modal is open and no menu is open). The router emits `gridMoveRequested(move)`. `Main` calls `grid.move(move)`, which does:
```
i = GridNavigation.target(selectedIndex, move, count, columns, visibleRows)
if (i !== selectedIndex) { selectedIndex = i; selectedUrl = model.urlAt(i); view.positionViewAtIndex(i, GridView.Contain) }
```
`[`/`]` Shortcuts go through `browse()`, which calls `grid.move(Previous/Next)`. `clearImageFocus()` then drops any header-button focus, so a following Enter opens the file. No decode happens on this path. The visible delegates already exist, and delegates newly revealed only kick off provider requests (a hash lookup and a pool `start`).

### 2.3 Cell to thumbnail
```
ThumbnailCell.Image.source = "image://thumbnail/<box>/<gen>/<base64url(path)>"
 -> QQuickPixmap -> ThumbnailProvider::requestImageResponse(id, requestedSize)        [GUI thread]
     parse id (box, gen, path); key = (path, box, gen)
     cache hit  -> response finishes via a queued call (never synchronously; the Image has not connected yet)
     cache miss -> ThumbnailTask (QRunnable) -> QThreadPool::start                    [pool, N = max(1, ideal/2)]
         worker: decoder(request, cancelledFlag) -> ThumbnailResult{image|error}      [worker thread]
         QMetaObject::invokeMethod(response, ..., Qt::QueuedConnection)               [back to GUI]
     GUI: if not cancelled and ok -> image.setDevicePixelRatio(box/kThumbnailBoxLogical);
          cache.insert(key, image); response.finished() -> Image.status = Ready
          else -> response.errorString(), Image.status = Error -> broken-image glyph
```
- Base64url of the UTF-8 path (`QByteArray::toBase64(Base64UrlEncoding | OmitTrailingEquals)`) is used because Qt passes the id to the provider percent-decoded, so `?`, `#` and `%` in file names would be ambiguous. The id contains no query.
- `decodeThumbnail`:
  - Open the file. For an `svg` suffix (the same extension dispatch `decodeImage` uses): `loadSvg`, then `rasterizeSvg` with `bound = box x box` (enlargement allowed, REQ-F-038).
  - Otherwise `HolonightImages::decode(file, {.limits = kRasterLimits, .bound = QSize(box, box)}, cancelled)`. It reads the header, rejects over-limit files before any pixel read (REQ-F-034), scales, and returns the first frame (REQ-F-037).
  - Convert to `Format_ARGB32_Premultiplied` on the worker. Any outcome other than `Success` becomes an error.
  - SVGs that link local raster files fail `rasterizeSvg` (`Unsupported`) and show the broken glyph. Single view renders these through its own path. Accepted as a documented limitation; thumbnails do not reuse the single-view renderer.

### 2.4 Cancellation
- **Delegate destroyed** (scrolled beyond `cacheBuffer`, model reset, grid closed): the `Image` is destroyed and `QQuickImageResponse::cancel()` runs.
- **Scrolled out but not destroyed:** not possible for `reuseItems: false`. Delegates outside `cacheBuffer` are destroyed.
- **`ThumbnailResponse::cancel()`:**
  1. Set the shared `std::atomic_bool`.
  2. Leave runnable ownership with the pool. When a queued task runs, it checks the shared cancellation flag and returns without invoking the decoder. No task pointer is retained by the response, avoiding `tryTake` races with auto-deletion (REQ-F-033).
  3. If the task already runs, the decoder sees `cancelled` at its next check (`HolonightImages::decode` checks between stages) and the result is discarded and never cached.
  4. Emit `finished` (queued) with a null factory so the pixmap reader releases the response.
- The worker holds shared cancellation state and delivers its result through a queued signal to the response. Destruction disconnects the response; late results are ignored.
- The pool is FIFO (default priority). Stale requests are marked by the destroy path and skipped before decoding. LIFO would decode the lookahead before the visible rows.
- Lookahead is `cacheBuffer` (REQ-F-033): with `cacheBuffer` equal to the viewport height, delegates exist for the visible cells plus roughly one page each side, and only existing delegates request thumbnails. A jump to the end destroys the old delegates, which cancels or drops their requests.

### 2.5 Enter opens a file
Router: `Return` -> `gridActivateRequested()` (skipped if a header button has focus) -> `grid.activateSelection()` emits `activated(selectedUrl)`. Double click: `TapHandler.onDoubleTapped` calls `select(index)` then the same `activated`. `Main` calls `document.openFromFolder(url)`, which does `select(url)` only if `directory_.indexOf(url) >= 0`, with no rescan and no-op if it is already `selected_url_`. Then `leaveGrid()`. `position` is `directory_.indexOf(url) + 1` (REQ-F-022's "position equals 7"). Decode errors for over-limit or corrupt files surface through the ordinary single-view error path (REQ-F-034/036).

### 2.6 Rescan (Ctrl+R in grid)
`Shortcut "Ctrl+R"` (unchanged, `localPath.length > 0`) -> `refreshFolder()`: `++thumbnailGeneration; document.refresh()`.
- `refresh()` re-selects the current document (reloads its image) and calls `directory_.scan`.
- The model resets to `{selected}` with `scanning` true. `ThumbnailGrid` shows `HnLoadingState { titleText: qsTr("Scanning folder…") }` and hides the view (REQ-F-041). `selectedUrl` and `lastIndex` are untouched because they are not derived from the transient model.
- The scan completes: the model resets to the filtered full list and `scanning` goes false. `restoreSelection()` runs (§1.9) and the delegates are recreated with the new generation. Nothing watches the filesystem (REQ-F-042).
- If the filtered list is empty, `ThumbnailGrid.showsEmpty` becomes true and `Main` shows `emptyStateGroup`.

## 3. Interfaces

### 3.1 C++ (new)

```cpp
// thumbnail_size.h
inline constexpr int kThumbnailBoxLogical = 256;                  // the one definition (REQ-C-002)
inline constexpr int kThumbnailCacheEntries = 1000;
inline constexpr qint64 kThumbnailCacheBytes = 256LL * 1024 * 1024;
constexpr int thumbnailBoxPixels(qreal dpr);                      // ceil(256*dpr - 1e-6), min 1
constexpr int thumbnailThreadCount(int idealThreadCount);         // max(1, ideal / 2)

// thumbnail_decoder.h
struct ThumbnailRequest { QString path; int boxPixels; };
struct ThumbnailResult { QImage image; QSize sourceSize; QString error; };   // image.isNull() => failed; sourceSize is the oriented original
ThumbnailResult decodeThumbnail(const ThumbnailRequest&, const std::atomic_bool& cancelled);

// thumbnail_cache.h
struct ThumbnailKey { QString path; int boxPixels; int generation; bool operator==(const ThumbnailKey&) const = default; };
size_t qHash(const ThumbnailKey&, size_t seed = 0);
class ThumbnailCache {                                            // GUI thread only
 public:
  explicit ThumbnailCache(qsizetype maxEntries = kThumbnailCacheEntries, qint64 maxBytes = kThumbnailCacheBytes);
  std::optional<QImage> find(const ThumbnailKey&);                // promotes to most-recent
  void insert(const ThumbnailKey&, QImage);                       // evicts LRU until both limits hold
  [[nodiscard]] qsizetype count() const;
  [[nodiscard]] qint64 bytes() const;
};

// thumbnail_provider.h
class ThumbnailProvider : public QQuickAsyncImageProvider {
 public:
  using Decoder = std::function<ThumbnailResult(const ThumbnailRequest&, const std::atomic_bool&)>;
  explicit ThumbnailProvider(Decoder decoder = decodeThumbnail,
                             int threadCount = thumbnailThreadCount(QThread::idealThreadCount()),
                             qsizetype cacheEntries = kThumbnailCacheEntries);
  ~ThumbnailProvider() override;                                  // cancels pending, waitForDone
  QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;
  [[nodiscard]] int maxThreadCount() const;                       // pool()->maxThreadCount()
  [[nodiscard]] const ThumbnailCache& cache() const;
  [[nodiscard]] int pendingCount() const;                         // queued + running, for tests
};
class ThumbnailResponse : public QQuickImageResponse {            // internal
  QQuickTextureFactory* textureFactory() const override;          // textureFactoryForImage(image)
  QString errorString() const override;
  void cancel() override;                                         // shared flag + queued finished
};

// grid_navigation.h: see §1.7          thumbnail_metrics.h: QML_SINGLETON
class ThumbnailMetrics : public QObject {
  Q_PROPERTY(int boxSize READ boxSize CONSTANT)
  static Q_INVOKABLE int devicePixels(qreal dpr);
  static Q_INVOKABLE QString sourceFor(const QUrl& fileUrl, qreal dpr, int generation);  // "image://thumbnail/<box>/<gen>/<b64url>"
};

// folder_grid_model.h
class FolderGridModel : public QSortFilterProxyModel {            // QML_UNCREATABLE
  Q_INVOKABLE QUrl urlAt(int row) const;
  Q_INVOKABLE int indexOfUrl(const QUrl&) const;                  // -1 if absent/hidden
 protected: bool filterAcceptsRow(int, const QModelIndex&) const override;   // hides DirectoryModel::missingUrl()
};
```

**Test hook:** a test constructs `new ThumbnailProvider(hook, /*threads*/ 2)` and adds it with `engine.addImageProvider("thumbnail", provider)` before loading `Main`. The hook is any callable that records `QThread::currentThread()`, counts calls, sleeps, or waits on a `QSemaphore` to block, and returns a synthetic `QImage`. It never has to read a file.

### 3.2 C++ (changed)

```cpp
// ImageDocument
Q_PROPERTY(QUrl url READ url NOTIFY changed)
Q_PROPERTY(QString folderName READ folderName NOTIFY changed)      // QFileInfo(localPath).absoluteDir().dirName(), "/" for the root
Q_PROPERTY(FolderGridModel* folder READ folder CONSTANT)
Q_INVOKABLE void openFromFolder(const QUrl& url);                  // select(url) if listed; no rescan; no-op if already selected

// DirectoryResult / DirectoryModel
struct DirectoryResult { QList<QUrl> urls; QString error; QUrl missing; };
QUrl DirectoryModel::missingUrl() const;

// WindowKeyRouter
Q_PROPERTY(bool gridActive MEMBER m_gridActive)
void gridMoveRequested(int move);   // GridNavigation::Move
void gridActivateRequested();
```

### 3.3 QML

```qml
// ThumbnailGrid.qml  (Item)
required property var model                       // FolderGridModel
property bool scanning: false                     // bound to document.scanning by Main
property int generation: 0                        // thumbnailGeneration
property real devicePixelRatio: 1                 // bound to window.devicePixelRatio
readonly property real cellWidth: ThumbnailMetrics.boxSize + 2 * cellPadding + cellSpacing
readonly property real cellHeight: cellPadding + ThumbnailMetrics.boxSize + labelGap + labelMetrics.lineSpacing + cellPadding + cellSpacing
readonly property int columns: Math.max(1, Math.floor((width - scrollBarReserve) / cellWidth))   // from the parent-driven width, never from the view
readonly property int visibleRows: height > 0 ? Math.max(1, Math.floor(height / cellHeight)) : 1
readonly property int count: view.count
property url selectedUrl
property int selectedIndex: -1
readonly property bool showsEmpty: !scanning && count === 0
signal activated(url fileUrl)
signal selectionInteraction()                     // a click: Main calls clearImageFocus()
function enter(url: url): void
function move(move: int): void                    // GridNavigation.Move
function canMove(move: int): bool
function select(index: int): void
function activateSelection(): void
function restoreSelection(): void

// ThumbnailCell.qml  (Item; delegate)
required property int index
required property string fileName
required property url url
property bool isSelected: false
property url source                               // computed by the grid
signal clicked()
signal doubleClicked()
// children (objectNames): cellBackground, cellFocusRing, cellThumbnail, cellPlaceholder, cellBrokenGlyph, cellLabel
```

Layout, in `ThumbnailGrid`:
- **T-010 implementation notes.** `columns = max(1, floor(width / cellWidth))` with no scroll-bar reserve, because REQ-F-008 requires exactly 2 columns at width 2W. The spacing is part of the cell (`ThumbnailCell.spacing` insets the tinted tile by half on each side), so `view.leftMargin = rightMargin = floor((width - columns * cellWidth) / 2)`: `GridView` derives its own column count from `width - margins`, and margins that also removed the spacing would make it lay out one column fewer than `columns`.
- `view.anchors.fill: parent`. `view.leftMargin = view.rightMargin = Math.floor((width - columns * cellWidth) / 2)`, so the content is centred (REQ-F-008, ±1 px) and the vertical scroll bar stays at the window edge.
- `cellWidth`/`cellHeight` are set on the `GridView`. `columns` is computed from the grid's own parent-assigned `width` and `scrollBarReserve`, never from `view.width`, to avoid a recursive layout (memory: "Layout self-width recursive rearrange"). The grid is not inside a `Layout`.
- The `GridView` uses `keyNavigationEnabled: false`, `activeFocusOnTab: false`, `reuseItems: false`, `cacheBuffer: height`, `Controls.ScrollBar.vertical: Controls.ScrollBar {}`, `Accessible.role: Accessible.List` and `Accessible.name: qsTr("Images")`.
- Scroll into view: `view.positionViewAtIndex(i, GridView.Contain)` after a move; on entry, `GridView.Center` deferred through `Qt.callLater`. Both leave the cell fully visible (REQ-F-013/014).

## 4. Key decisions and alternatives

| # | Decision | Rationale | Rejected alternatives |
|---|---|---|---|
| D-1 | `QQuickAsyncImageProvider` plus `QThreadPool` plus own LRU, `Image.cache: false` | Status, cancel and texture upload come free. One cache, testable. | QObject service (reimplements status/cancel; extra required property breaks tests); sync provider (no cancel, no pool control); the engine's pixmap cache (uncountable, not LRU-by-entries). |
| D-2 | Decode through `HolonightImages::decode`/`rasterizeSvg` with `bound = box x box` | One source of truth for limits, EXIF and format quirks (including the simple-WebP fallback). Header check precedes pixel read. | New `QImageReader` code (duplicates limits, `setScaledSize` does not work for GIF handlers). |
| D-3 | Apply EXIF orientation | Single view applies it (`OrientationPolicy::Apply` default). Otherwise a portrait photo would look sideways in the grid and upright once opened. The reader `autoTransform` cost is negligible. | Ignore orientation for speed (visible mismatch). |
| D-4 | Thumbnail id is `<boxPx>/<generation>/<base64url path>`; box px computed by `ThumbnailMetrics` with `ceil(256*dpr - eps)` | Deterministic ceil (Qt's `sourceSize * dpr` rounds instead), DPR change becomes a plain URL change, and a rescan needs no `stat`. | `sourceSize`-driven request (round, not ceil, no generation); a query string (percent-decoding ambiguity); validating (size, mtime) on each cache hit (GUI-thread I/O). |
| D-5 | Logical thumbnail size = source fitted into 256 without upscaling; `QImage` DPR tag = decoded / logical width | Matches REQ-F-009 and single view's logical 1:1 (user decision, 2026-09-29). Large sources are exactly 256 logical at any DPR. | Device-pixel 1:1 for small sources (50×50 logical at 2×; rejected by the user); fixed DPR tag from the window (256.36 logical artefacts at 1.1). |
| D-6 | Grid keys in `WindowKeyRouter`; `[`, `]`, Ctrl+G, Escape as `Shortcut`s; selection maths in a pure C++ helper | See §1.6. Consistent with pan and Space routing, focus-independent, modal-safe, and directly unit-testable. | `Keys` on `GridView` (focus fights with `neutralFocus`/`clearImageFocus`, must re-check every modal); a QML/JS helper (harder to test in this GoogleTest-only tree). |
| D-7 | Disable image-only features by adding `!gridMode` to `canInspect` (and a new `canShowInformation`) | One edit gates about a dozen actions, both handlers, the router's pan/Space and menu items, so it is hard to leave a gap. | Per-action `enabled` edits (drift-prone). |
| D-8 | Keep `ImageCanvas` alive, hidden | Zoom/pan and orientation survive the round-trip, and leaving is instant. | A `Loader`/`StackView` swap (state loss, rebuild cost). |
| D-9 | Selection in QML by URL, restored after `scanning` falls | `DirectoryModel` resets wholesale on rescan (via a transient one-row model). Only the URL and old index are stable across that. | Selection in `ImageDocument` (couples grid state to the document, and the document is not the grid selection); `GridView.currentIndex` (reset to 0 by model reset). |
| D-10 | `FolderGridModel` proxy hides a missing injected URL; `openFromFolder` avoids the rescan | Makes REQ-F-039/040 satisfiable without changing single-view browsing. Grid open keeps zoom/pan if it is the same file. | Change `scanDirectory` to drop missing files (breaks single-view recovery: `canNext` and `canPrevious` need the current index, and the "file no longer exists" flow); calling `open()` (rescans on every open). |
| D-11 | `reuseItems: false` | Simplest correct lifecycle: destroy means cancel. No stale-thumbnail flash or per-state reset in `onPooled`. | `reuseItems: true` (profile first; if needed, clear `source` in `GridView.onPooled` and reset ring/tooltip state). |
| D-12 | Header/footer always visible in fullscreen grid; grid anchors below and above them | The canvas mouse-move that reveals them is hidden. Avoids an overlay covering the top row. | Add a hover reveal to the grid (more state, new timers). |
| D-13 | Opening a new file (Ctrl+O, drop) leaves grid mode | Matches "open shows the image"; avoids the selection having to follow an unrelated document. | Stay in grid and reselect (unspecified, more reconciliation). |
| D-14 | Provider cache is bounded by 1000 entries and 256 MiB | At DPR 2, 512 x 384 x 4 B is about 786 KB, so 1000 entries would be about 0.8 GB. The byte cap is stricter than REQ-F-032 and does not contradict it. | Entries only (memory spike at HiDPI). |

## 5. Risks and mitigations

- **R-1 DPR change** (window moves between screens; `QT_SCALE_FACTOR` differs). `window.devicePixelRatio` is a binding. When it changes, every `Image.source` changes, so requests cancel and reload at the new box. Old-box entries age out of the LRU. Cell logical sizes do not change. Add a test that changes the bound DPR (via the `devicePixelRatio` property on the grid) and checks the new box size reaches the decoder. Manual check on a mixed-DPR setup (§6).
- **R-2 Very large folder** (2000+). `GridView` virtualises. `cacheBuffer` bounds instantiated delegates. `DirectoryModel::indexOf` is a linear `QList::indexOf`, and `FolderGridModel::indexOfUrl` uses it once per restore or entry. That is fine at 2000, and only entry and rescan use it, not navigation. The rescan reset recreates the viewport delegates. Watch memory: the LRU byte cap (D-14).
- **R-3 Delegate recycling / stale content.** `reuseItems: false` (D-11). If profiling later forces recycling, handle `GridView.onPooled`/`onReused` by clearing `source`.
- **R-4 Focus and Wayland.** All grid keys go through the router and never depend on which item has focus. Per project rules, native focus and pointer behaviour is not automated. The manual list in §6 covers header-button focus plus Enter/Space, Tab cycling, the actions menu with j/k, focus after the file dialog closes, and Escape in fullscreen grid. The existing test lessons apply: bare Wayland ignores resizes and `hyprctl` syntax changed; trust the `ctest` preset.
- **R-5 Modal blocking.** Help, information and dialog are modal, so `Shortcut`s (including Ctrl+G and Escape) and the router are blocked. No grid key needs to work inside them.
- **R-6 Clamping before the page size is known.** The first layout can have `width` or `height` 0. `visibleRows` returns 1 when `height <= 0`, `columns` is at least 1, and `GridNavigation::target` treats `columns < 1` as 1 and `visibleRows < 0` as 0 (page moves then use one row). Scroll-into-view on entry is deferred through `Qt.callLater` and re-run when `height` or `columns` first become valid.
- **R-7 Escape with the actions menu open.** `Shortcut`s act while the menu is open. The Escape branch checks `!actionsMenuOpen` before leaving grid. Covered by a test.
- **R-8 Transient scan states.** `document.count` is 1 during a scan; the title omits the count and the view is hidden while `scanning`. The restore is deferred so Ctrl+R during a scan cannot restore against an empty list. Navigation and activation are disabled until scanning completes, preserving the saved URL and index.
- **R-13 Threading (found in T-018).** `requestImageResponse` and `QQuickImageResponse::cancel()` run on the QML image-loading thread, not the GUI thread; `ThumbnailCache` is therefore used behind `ThumbnailProvider::cacheMutex_`, and the provider test `ConcurrentRequestsAndCancellationFinishOnTheirOwnThread` exercises that path.
- **R-9 Qt behaviour to verify in the first spike before building the rest:**
  1. `Image.implicitWidth` for a provider image divides by the returned `QImage`'s `devicePixelRatio`.
  2. `QQuickImageResponse::cancel()` is called when the delegate is destroyed and when `source` is cleared, and `finished` must still be emitted after it (we emit it queued).
  3. `cacheBuffer` produces about one page of delegates each side.
  4. The provider `MenuItem` renders `checkable`.
  5. `QGifHandler` ignores `setScaledSize`, so the library's final `scaled()` fallback applies (it does).

  Each has a fallback noted above, and none changes the component split.

  **T-001 spike outcome** (Qt 6.11.2, offscreen platform, standalone program with an async `QQuickImageProvider`, also run with `QT_SCALE_FACTOR=2`):
  1. **Does not hold.** A provider `QImage` of 200×100 px tagged `devicePixelRatio = 2` gives `Image.implicitWidth/Height = 200×100` and `sourceSize = 200×100`, with or without a scale factor. The image's DPR is not applied. **Use the fallback:** the cell sizes the `Image` explicitly from the logical size, not from `implicitWidth`. `ThumbnailProvider` therefore cannot rely on the DPR tag alone; the logical size (`decodedWidth / dpr`) must reach QML separately, e.g. encoded in the request id's response or exposed by a status-only helper. The DPR tag on the `QImage` is still set (harmless), but `PreserveAspectFit` inside an explicitly sized box is what maps device pixels to logical size. T-005, T-006 and T-009 must not assume `implicitWidth` is logical. **Resolved in T-006 without a side channel:** `ThumbnailResponse::textureFactory()` returns a `QQuickTextureFactory` subclass whose `textureSize()` is the logical size while `createTexture` uses the device-pixel image. Verified in a spike and in `ImageItemShowsLoadingThenReadyOrError`: a 320×240 image with a 4000×3000 source gives `Image.implicitWidth/Height = 256×192`. The logical size is derived as the `sourceSize` fitted into the logical box without enlargement (SVG `sourceSize` is already the enlarged rendering) and travels in the cached `QImage`'s `devicePixelRatio`. Cells can therefore keep `width: Math.min(implicitWidth, box)`.
  2. **Holds.** `cancel()` is called when the delegate is destroyed (Loader deactivated) and when `source` is cleared during loading. `finished` emitted (queued) after `cancel()` is accepted with no warnings, and the response is destroyed afterwards.
  3. **Holds, with one extra row.** A 3×2 viewport with 100 px cells and `cacheBuffer: 200` (two rows) at the top of the model instantiated 15 delegates: 6 visible plus 9 below (three rows, one more than the buffer). The S-7 bound `(visibleRows + 1) * columns + visibleRows * columns` still covers it.
  4. **Holds.** `Controls.MenuItem` with `checkable: true` exposes `checkable`, `checked` and a non-null `indicator` under the Basic style (checked visually in the Basic style only; the embedded HoloNight style is checked in T-015).
  5. **Holds.** For a 1×1 GIF, `QImageReader::setScaledSize(4×4)` returns a 4×4 image, so the scale is applied by the reader's own fallback path and the first frame comes out at the requested size.
- **R-10 SVG with local linked images** shows the broken glyph in the grid, although single view renders it. Accepted; documented.
- **R-11 Duplicate in-flight requests** for one key (a delegate destroyed and recreated while the first decode runs) decode twice. The first is cancelled at the next check. In-flight coalescing is deferred until measured.
- **R-12 Existing tests that move.** `menu_layout_test` (an item is added), `shortcut_help_popup_test` (Esc text, new section), `footer_key_hints_test` (unchanged for single view, extended for grid) and any test that counts header behaviour need updates. Called out in TASKS.

### Spec issues found

- **S-1** The summary counts are wrong. The file defines REQ-F-001..044 (44), REQ-NF-001 (1) and REQ-C-001..012 (12), which is 57 in total, but it says "Functional 42, Constraints 14".
- **S-2** REQ-F-044 cites a "Holonight.Controls focus ring primitive", but none exists (§0.3). Proposed wording: "the focus ring is drawn with `HnMetrics.focusBorderWidth` and `HoloniightPalette.borderFocus`, the same as other HoloNight controls". The test then checks the ring item, its border width and its colour.
- **S-3** REQ-F-040 and part of REQ-F-039 are unreachable with today's `scanDirectory`, which always re-adds the explicit URL. Resolved by D-10, but the SPEC should state that the missing current file is hidden in the grid only.
- **S-4** REQ-F-009 and REQ-F-011 conflict for sources smaller than the box at DPR > 1. A 100x100 source at DPR 2 decodes to 100x100 px; "displayed at its logical fitted size" is either 100x100 logical (2x upscaled in device pixels) or 50x50 logical (1:1). **Resolved:** the user chose logical size (100×100 logical); D-5 was updated and the SPEC is unchanged.
- **S-5** REQ-F-026 lists "the arrow keys" as unchanged, but REQ-F-015/016 make them move the selection in grid mode. Interpret it as "the arrow keys do not pan".
- **S-6** Unspecified: Ctrl+O or drop while in grid (decision D-13); the Previous/Next menu items in grid (routed to selection movement); Ctrl+G while a scan is in progress (allowed, shows busy).
- **S-7** REQ-F-033 "visible plus about one page" should be stated in terms of instantiated delegates (partial rows included). The test bound is `2 * (visibleRows + 1) * columns` (a partial row at each of the viewport and the `cacheBuffer`; measured 29 to 30 with 4 visible rows and 3 columns).
- **S-8** REQ-F-032 (1000 entries) allows about 0.8 GB at DPR 2. A byte cap was added (D-14).
- **S-9** REQ-F-022 says "Enter or Return". The keypad Enter is also accepted.
- **S-10** REQ-F-024's example title uses an em dash like the window title. During a scan the count is unreliable (R-8).

## 6. Test strategy

All automated tests join the single `viewer-smoke` binary (GoogleTest and QtTest), added to `tests/CMakeLists.txt`. There is no QtQuickTest. Fixtures are built in test code (temp dir of PNG/JPEG/GIF/SVG files via `QImage::save`, `tests/gif_fixture.h`, `tests/exif_fixture.h`), and the injected provider decoder needs no files for behavioural tests. Fixtures return `AssertionResult`, since `task tidy` lints tests.

| Test file (new unless noted) | Kind | Covers |
|---|---|---|
| `tests/grid_navigation_test.cpp` | C++ unit | REQ-F-015..019 (all SPEC examples, clamps, `columns`/`visibleRows` edge cases) |
| `tests/thumbnail_cache_test.cpp` | C++ unit | REQ-F-032 (1200 inserts stay at most 1000, LRU order, re-find promotes, byte cap) |
| `tests/thumbnail_decoder_test.cpp` | C++ unit with files | REQ-F-011 (320x240 at 1.25, 512x384 at 2, 256x192 at 1, 100x100 stays), 009, 034 (crafted oversized PNG header returns a limit error without allocating), 036 (corrupt JPEG), 037 (3-frame GIF, centre pixel red), 038 (viewBox 200x100 gives 256x128, non-transparent), EXIF orientation parity with `decodeImage` |
| `tests/thumbnail_provider_test.cpp` | C++, no QML | REQ-F-030 (decode thread is not the GUI thread; blocked decoder leaves the GUI responsive), 031 (`maxThreadCount` for ideal 16 and 1), 032 (cache hit does not call the decoder), 033 (cancel before start means the decoder is never called; cancel of a running decode does not populate the cache), 035 (status while blocked) |
| `tests/thumbnail_grid_test.cpp` | QML with `Main` plus injected provider, or standalone `ThumbnailGrid`/`ThumbnailCell` | REQ-F-007, 008 (widths 3W+W/2, 2W, W/2; margin equality within 1 px), 009, 010, 012, 013, 014 (2000 items, the SPEC key sequence), 020, 021, 033 (counting decoder over 2000 files, visible plus one page), 035, 036, 039, 040, 041, 043 (List/ListItem/selected via `QAccessible`; delegates reached with `grid.itemAtIndex`, since Repeater and view delegates are invisible to `findChild`, and Accessible goes on the delegate root), 044 |
| `tests/grid_mode_test.cpp` | QML with `Main` | REQ-F-001..006, 022, 023 (header-button focus keeps Enter/Space), 024, 026, 027, 028, REQ-C-011, C-012, and NF-001 (200 auto-repeat `j` presses, each under 16 ms via `QElapsedTimer`; the decoder hook records zero GUI-thread decodes) |
| `tests/footer_key_hints_test.cpp` (extend) | QML standalone (`StandaloneFooter`) | REQ-F-025 (entries present or absent per mode; single-view hints restored) |
| `tests/shortcut_help_popup_test.cpp` (extend) | QML | REQ-F-029 (Grid section in both modes; updated Esc text) |
| `tests/menu_layout_test.cpp` (update) | QML | REQ-F-002 (menu hint reads Ctrl+G), 028 (index shift for the new item; separators take `currentIndex` slots) |
| `tests/folder_browsing_test.cpp` (extend) | C++ | `DirectoryResult::missing`, `FolderGridModel` filtering, `openFromFolder` (no rescan, position), REQ-F-039/040 model side |
| `scripts/check-thumbnail-constant.py` plus CTest `viewer-thumbnail-constant` (new) | script | REQ-C-002 (literal 256 occurs once, in `thumbnail_size.h`, across grid and thumbnail sources); the runtime check that `ThumbnailMetrics.boxSize == kThumbnailBoxLogical` lives in `thumbnail_grid_test` |
| Existing `task` checks | tooling | REQ-C-003/004 (`qml-import-check` recurses into `qml/grid`), C-005 (`format-check` recurses), C-006, C-007 (`qmltypes-check` with the extended type list), C-008 (`qml-lint` follows `QML_FILES`), C-009 (`tidy` globs `apps/*.cpp` and `tests/*.cpp`), C-010 (`ctest --preset test`) |

Manual native checks (never automated; ask the user to run them, per project rules):
1. Focus and Wayland/Hyprland: Tab to the fullscreen and actions buttons in grid mode; Enter and Space activate them; after the actions menu or the file dialog closes, hjkl and Enter work again (REQ-F-022/023, R-4).
2. Escape in fullscreen grid closes the grid only, and a second Escape exits fullscreen (REQ-F-005/006). Header and footer visible in fullscreen grid (D-12).
3. Moving the window between screens of different scale changes the decode box and stays sharp (R-1); fractional scales 1.25, 1.5.
4. Holding `j`, Ctrl+D and Ctrl+U through a 2000-image folder feels smooth (NF-001 beyond the automated bound).
5. Accessibility tree in accerciser (REQ-F-043). Visual check of tint, ring and broken/placeholder cells in light and dark (`task visual-check` style capture).
6. Pointer: single click selects, double click opens (REQ-F-020/021) on a real pointer.

### Requirement to component traceability

| REQ | Components | Verified by |
|---|---|---|
| F-001 | `Main.qml` `Ctrl+G` Shortcut, `toggleGrid`/`canEnterGrid` | grid_mode_test |
| F-002 | literal `"Ctrl+G"` Shortcut; menu `shortcutKeys`; footer/help `Key_Control+Key_G` | grid_mode_test, menu_layout_test, footer/help tests |
| F-003 | `canEnterGrid` (`localPath`, `count`) | grid_mode_test |
| F-004 | `leaveGrid` leaves the document untouched; `browse` mode-aware; `[`/`]` route to the grid | grid_mode_test |
| F-005 | Escape Shortcut branch in `Main.qml` | grid_mode_test, manual 2 |
| F-006 | Escape Shortcut else-branch (`leaveFullscreen`) | grid_mode_test |
| F-007 | `FolderGridModel` (source order, no sort) bound to `ThumbnailGrid` | thumbnail_grid_test |
| F-008 | `ThumbnailGrid` `columns`/`cellWidth`/margins | thumbnail_grid_test |
| F-009 | `ThumbnailCell` image sizing; provider DPR tag | thumbnail_grid_test, thumbnail_decoder_test |
| F-010 | `ThumbnailCell` label elide/ToolTip/Accessible.name | thumbnail_grid_test |
| F-011 | `thumbnail_size.h`, `ThumbnailMetrics::devicePixels`, `decodeThumbnail` | thumbnail_decoder_test |
| F-012 | `ThumbnailCell` background and ring (tokens from holonight-qt; S-2) | thumbnail_grid_test, manual 5 |
| F-013 | `ThumbnailGrid.enter`, `positionViewAtIndex` | thumbnail_grid_test |
| F-014 | `ThumbnailGrid.move` with `GridView.Contain` | thumbnail_grid_test |
| F-015 | `GridNavigation::target` (Previous/Next), router H/L/arrows | grid_navigation_test, grid_mode_test |
| F-016 | `GridNavigation::target` (RowUp/RowDown), router J/K/arrows | grid_navigation_test |
| F-017 | `GridNavigation::target` (PageDown), router Ctrl+D | grid_navigation_test, grid_mode_test |
| F-018 | `GridNavigation::target` (PageUp), router Ctrl+U | grid_navigation_test, grid_mode_test |
| F-019 | `[`/`]` Shortcuts -> `browse` -> `grid.move` | grid_mode_test |
| F-020 | `ThumbnailCell` TapHandler -> `ThumbnailGrid.select` | thumbnail_grid_test, manual 6 |
| F-021 | `ThumbnailCell` double-tap -> `activated` -> `openFromFolder` | thumbnail_grid_test, manual 6 |
| F-022 | router `gridActivateRequested`, `openFromFolder` | grid_mode_test |
| F-023 | router header-button focus guard | grid_mode_test, manual 1 |
| F-024 | `Main.qml` header title, `ImageDocument::folderName` | grid_mode_test |
| F-025 | `FooterKeyHints.gridMode` | footer_key_hints_test |
| F-026 | `canInspect`/`canShowInformation` gating, router `imageReady` | grid_mode_test |
| F-027 | unchanged Shortcuts/Actions (Open, F, Q, ?, Ctrl+R), all outside the gated set | grid_mode_test |
| F-028 | `Main.qml` menu item plus `Binding` | grid_mode_test, menu_layout_test |
| F-029 | `ShortcutHelpPopup.qml` Grid section | shortcut_help_popup_test |
| F-030 | `ThumbnailProvider` pool + `ThumbnailTask` | thumbnail_provider_test |
| F-031 | `thumbnailThreadCount` | thumbnail_provider_test |
| F-032 | `ThumbnailCache`, `Image.cache: false` | thumbnail_cache_test, thumbnail_provider_test |
| F-033 | `cacheBuffer`, `ThumbnailResponse::cancel` and worker cancellation check | thumbnail_provider_test, thumbnail_grid_test |
| F-034 | `HolonightImages::decode` limits in `decodeThumbnail`, broken glyph | thumbnail_decoder_test, thumbnail_grid_test |
| F-035 | `Image.status`, placeholder in `ThumbnailCell` | thumbnail_grid_test |
| F-036 | `ThumbnailResponse::errorString`, broken glyph | thumbnail_decoder_test, thumbnail_grid_test |
| F-037 | `decodeThumbnail` (first frame) | thumbnail_decoder_test |
| F-038 | `decodeThumbnail` SVG path (`rasterizeSvg`) | thumbnail_decoder_test |
| F-039 | `ThumbnailGrid.restoreSelection`, `FolderGridModel` | thumbnail_grid_test, folder_browsing_test |
| F-040 | `FolderGridModel` plus `emptyStateGroup` gating (S-3) | thumbnail_grid_test |
| F-041 | `ThumbnailGrid` busy `HnLoadingState` on `scanning` | thumbnail_grid_test |
| F-042 | no watcher introduced; rescan only via `refreshFolder` | thumbnail_grid_test (list unchanged until Ctrl+R) |
| F-043 | `ThumbnailGrid` `Accessible.List`; `ThumbnailCell` `ListItem` | thumbnail_grid_test, manual 5 |
| F-044 | `ThumbnailCell` `cellFocusRing` (S-2) | thumbnail_grid_test |
| F-045 | D-13: `gridMode = false` on successful portal/FileDialog/drop open | grid_mode_test |
| F-046 | mode-aware `browse()` behind the Previous/Next menu items | grid_mode_test |
| F-047 | `canEnterGrid` permits entry during `scanning`; `ThumbnailGrid` busy state | thumbnail_grid_test |
| NF-001 | router, `GridNavigation`, provider (no GUI-thread decode) | grid_mode_test, manual 4 |
| C-001 | `apps/viewer/qml/grid/ThumbnailGrid.qml`, `ThumbnailCell.qml`; `Main.qml` uses `import "grid"` | file existence plus grep in `thumbnail_grid_test` |
| C-002 | `thumbnail_size.h`, `ThumbnailMetrics` | `check-thumbnail-constant.py`, thumbnail_grid_test |
| C-003 | grid QML imports `QtQuick.Controls as Controls` only | `task qml-import-check` |
| C-004 | explicit `import Holonight.Core`/`Holonight.Controls` in grid QML | `task qml-import-check` |
| C-005 | new files formatted | `task format-check` |
| C-006 | new QML | `task qml-import-check` |
| C-007 | `ThumbnailMetrics`, `GridNavigation`, `FolderGridModel` in the type check | `task qmltypes-check` |
| C-008 | new QML in `QML_FILES` | `task qml-lint` |
| C-009 | all new C++ | `task tidy` |
| C-010 | new tests registered in `viewer-smoke` | `ctest --preset test` |
| C-011 | `canInspect` gating; router pan branch used only outside grid | grid_mode_test |
| C-012 | router Space branch unchanged outside grid; `playbackAvailable` | grid_mode_test |
