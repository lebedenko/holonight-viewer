# SDD Tasks — thumbnail-grid-view

- [x] T-001: Spike — verify R-9 Qt behaviours and record in DESIGN.md
  - REQs: None (verification task)
  - Check: DESIGN.md §5 R-9 items 1–5 verified (Image.implicitWidth DPR division, cancel on destroy, cacheBuffer size, MenuItem checkable, GifHandler scaled fallback) and outcome notes added to DESIGN

- [x] T-002: Implement thumbnail_size.h constant and check script
  - REQs: REQ-C-002, REQ-F-011, REQ-F-031
  - Check: `scripts/check-thumbnail-constant.py` finds kThumbnailBoxLogical 256 exactly once and `ctest -R viewer-thumbnail-constant` (a separate add_test) passes

- [x] T-003: Implement GridNavigation pure maths helper and unit tests
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019
  - Check: `viewer-smoke --gtest_filter` for the `grid_navigation_test.cpp` suites passes all cases: Previous/Next no-wrap, RowUp/RowDown clamp to last row, PageUp/PageDown with half-visible-rows, and edge cases for columns < 1 and visibleRows < 0

- [x] T-004: Implement ThumbnailCache LRU with entry and byte bounds and unit tests
  - REQs: REQ-F-032
  - Check: `viewer-smoke --gtest_filter` for the `thumbnail_cache_test.cpp` suites passes: 1200 inserts stay at most 1000 entries, least-recently-used evicted first, 512×512 ARGB inserts respect 256 MiB byte cap, cache hit lookup succeeds

- [x] T-005: Implement decodeThumbnail function and unit tests covering DPR, limits, EXIF, SVG, first frame and corrupt files
  - REQs: REQ-F-009, REQ-F-011, REQ-F-034, REQ-F-036, REQ-F-037, REQ-F-038
  - Check: `viewer-smoke --gtest_filter` for the `thumbnail_decoder_test.cpp` suites passes: DPR 1.25 yields 320×240 for 4000×3000, 100×100 stays unmagnified, over-limit PNG header rejection finds no pixel decode, corrupt JPEG returns error, 3-frame GIF centre pixel is red, SVG viewBox 200×100 at DPR 1 gives 256×128 non-transparent

- [x] T-006: Implement ThumbnailProvider async image provider with thread pool, response cancellation and cache integration and tests
  - REQs: REQ-F-030, REQ-F-031, REQ-F-032, REQ-F-033, REQ-F-035, REQ-F-036
  - Check: `viewer-smoke --gtest_filter` for the `thumbnail_provider_test.cpp` suites passes: decode thread is not GUI thread, maxThreadCount equals 8 for ideal 16 and 1 for ideal 1, cache hit calls decoder zero times, cancel before pool execution prevents decode, cancel of running decode does not cache, Image.status is Loading then Ready/Error

- [x] T-007: Implement ThumbnailMetrics singleton and register ThumbnailProvider in main.cpp and test infrastructure
  - REQs: REQ-C-002, REQ-C-007
  - Check: CMake adds ThumbnailMetrics to qmltypes check, `ThumbnailMetrics.boxSize` equals 256 at runtime, `ThumbnailMetrics.devicePixels(1.25)` is 320, tests inject provider before loadFromModule and exit clean

- [x] T-008: Implement DirectoryResult::missing, FolderGridModel proxy filtering, ImageDocument url/folderName/folder/openFromFolder and folder_browsing tests
  - REQs: REQ-F-039, REQ-F-040
  - Check: `viewer-smoke --gtest_filter` for the `folder_browsing_test.cpp` suites passes: deleted current file is hidden in grid only, selection restored to same URL after rescan, selection moves to old index if URL missing, position equals indexOf for openFromFolder (no rescan)

- [x] T-009: Implement ThumbnailCell.qml delegate with thumbnail image, label, background tint, focus ring, placeholder, broken glyph and broken-image.svg icon
  - REQs: REQ-F-009, REQ-F-010, REQ-F-012, REQ-F-020, REQ-F-021, REQ-F-035, REQ-F-036, REQ-F-043, REQ-F-044, REQ-C-001, REQ-C-003, REQ-C-004
  - Check: `task qml-lint` reports no ThumbnailCell errors, cell loads standalone under 420px width, background/ring appear on select with no thumbnail obscuring, placeholder visible while status=Loading, broken glyph on Error, label middle-elided, Accessible.ListItem with Accessible.name=fileName, `qml-import-check` passes

- [x] T-010: Implement ThumbnailGrid.qml container with layout, column math, selection tracking, scroll-into-view, busy state during scan and empty state
  - REQs: REQ-F-007, REQ-F-008, REQ-F-013, REQ-F-014, REQ-F-024, REQ-F-035, REQ-F-041, REQ-F-043, REQ-F-047, REQ-C-001, REQ-C-003, REQ-C-004
  - Check: `viewer-smoke --gtest_filter` for the `thumbnail_grid_test.cpp` suites covers layout: 3W+W/2 width gives 3 columns with margins equal ±1px, current file selected and scrolled into view on enter, selection stays visible after each nav, HnLoadingState shown while scanning, List role and Contain positioning verified

- [x] T-011: Add Main.qml grid mode state, properties (gridMode, canEnterGrid, canToggleGrid, canInspect edit), Ctrl+G Shortcut literal and header title folder name + count display
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-026, REQ-F-027, REQ-F-024, REQ-C-011, REQ-C-012
  - Check: Ctrl+G toggles gridMode true/false, Ctrl+Shift+G does nothing, header shows "folder — N images" in grid and "fileName" in single view, zoom/pan/fit/rotate/Space disabled in grid (canInspect false), Ctrl++/−/0/1/R/X/I work in single view only

- [x] T-012: Add Main.qml Escape precedence (grid closes before fullscreen exits), canvas/overlays hiding in grid, header/footer always visible in fullscreen grid and animation suspend on gridMode
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-045
  - Check: Fullscreen grid Escape closes grid not fullscreen, grid Escape outside fullscreen still exits fullscreen, Ctrl+O success or drop success exits grid, drop cancel keeps grid, canvas ImageCanvas hidden while gridMode, animated image stops decoding frames while grid is shown

- [x] T-013: Add WindowKeyRouter grid key routing for h/j/k/l/arrows/Ctrl+U/D/Enter with header-button focus guard and gridActive property
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-022, REQ-F-023, REQ-NF-001
  - Check: `viewer-smoke --gtest_filter` for the `grid_mode_test.cpp` suites covers navigation: h/l/arrows move ±1, j/k/arrows move ±row, Ctrl+U/D move ±page, Enter opens if no button focused (Tab focus guard verified), header button Tab-focus activates with Enter/Space not grid, 200 auto-repeat j presses each under 16ms with zero GUI-thread decodes

- [x] T-014: Implement mode-aware browse function and Previous/Next menu items moving selection in grid mode, Ctrl+O/drop leaving grid mode
  - REQs: REQ-F-019, REQ-F-021, REQ-F-022, REQ-F-045, REQ-F-046
  - Check: Grid mode bracket shortcuts ]/[ move selection ±1 not document, Previous/Next menu items move selection ±1 in grid not document, Ctrl+O dialog cancel keeps grid, successful open/drop sets gridMode=false before document.open(), mode-aware browse detection tested in grid_mode_test

- [x] T-015: Add Actions menu "Grid View" toggle item after Refresh with shortcutKeys literal Ctrl+G, checkable state and update menu_layout_test currentIndex
  - REQs: REQ-F-001, REQ-F-028
  - Check: Menu shows "Grid View" with Ctrl+G hint, checked when gridMode true, unchecked when false, triggering toggles grid, item is disabled when canToggleGrid false, menu_layout_test adjusted for item index shift via separators

- [x] T-016: Add footer grid hints for hjkl/Ctrl+U/Ctrl+D/Enter navigation and update footer_key_hints_test for grid mode
  - REQs: REQ-F-025
  - Check: Grid mode footer shows GridNavigate/GridPage/GridOpen/GridToggle hints (hjkl/Ctrl+U/Ctrl+D/Enter/Ctrl+G), single-view hints unchanged, no Zoom/Fit/Rotate/Play/100% entries in grid, footer_key_hints_test verifies grid and single-view arrays independently

- [x] T-017: Add help popup Grid section with Ctrl+G/hjkl/Ctrl+U/Ctrl+D/Enter rows and update Esc row text and shortcut_help_popup_test expectedSections()
  - REQs: REQ-F-029, REQ-F-002
  - Check: Press ? in grid or single view, Grid section present with 5 rows (Ctrl+G toggle, Move selection, Page up/down, Open selected), Esc row text says "Close dialog, grid or leave fullscreen", `viewer-smoke --gtest_filter` for the `shortcut_help_popup_test.cpp` suites passes updated assertions

- [x] T-018: Create thumbnail_grid_test covering layout math, selection tracking, clicks, navigation scroll, restore, accessibility tree and placeholder/error states
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-020, REQ-F-021, REQ-F-030, REQ-F-033, REQ-F-035, REQ-F-036, REQ-F-039, REQ-F-040, REQ-F-041, REQ-F-042, REQ-F-043, REQ-F-044, REQ-C-002
  - Check: `viewer-smoke --gtest_filter` for the `thumbnail_grid_test.cpp` suites passes: 3-column layout ±1px margins, 2000-item selection restore via URL after add and delete, lookahead cancel with 2000 synthetic files drops stale requests, cells click to select and double-click to activate, placeholder on Loading and broken glyph on Error, Accessible.List/ListItem structure, ring visible after nav

- [x] T-019: Create grid_mode_test covering toggle, navigation keys, Escape priority, fullscreen interaction, feature gating, menu/help integration, header/footer and focus responsiveness
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-026, REQ-F-027, REQ-F-028, REQ-F-041, REQ-F-045, REQ-F-046, REQ-F-047, REQ-NF-001, REQ-C-011, REQ-C-012
  - Check: `viewer-smoke --gtest_filter` for the `grid_mode_test.cpp` suites passes: Ctrl+G toggle (single view ↔ grid), empty/no-doc blocks entry, nav keys h/j/k/l/Ctrl+U/D move selection, Escape closes grid only in fullscreen, header title updates, zoom/pan/Space remain disabled, menu item checked state follows gridMode, Enter opens file, 200 auto-repeat j under 16ms, focus stays in window across Nav, Ctrl+R during scan shows busy

- [x] T-020: Tooling pass: run format-check, qml-import-check, qmltypes-check, qml-lint, tidy and ctest with all constraints
  - REQs: REQ-C-002, REQ-C-003, REQ-C-004, REQ-C-005, REQ-C-006, REQ-C-007, REQ-C-008, REQ-C-009, REQ-C-010
  - Check: `task format-check` reports zero grid QML format errors, `task qml-import-check` reports no grid import issues, `task qmltypes-check` includes ThumbnailMetrics/GridNavigation/FolderGridModel, `task qml-lint` zero warnings on grid QML, `task tidy` passes C++ files, `ctest --preset test` shows 0 test failures

- [x] T-021: Manual native checks list provided to user (never automated)
  - REQs: REQ-F-020, REQ-F-022, REQ-F-023, REQ-F-044, REQ-NF-001
  - Check: User confirms Wayland/Hyprland Tab/Enter/Space focus on buttons, Escape in fullscreen grid vs single view, double-click opens and single-click selects on real pointer, accerciser a11y tree shows List/ListItem, ring renders with palette/metrics tokens, multi-DPR pointer behavior and 2000-item scroll smoothness subjectively good
  - Result: checks 1, 2, 5 and 6 confirmed by the user. Checks 3 (multi-DPR / fractional scale) and 4 (2000-image scroll smoothness) are deferred: not possible to run at this time.
