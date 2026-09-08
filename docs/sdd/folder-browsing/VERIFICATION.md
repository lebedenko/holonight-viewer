# Folder browsing verification

Verified locally on 2026-09-08, Arch Linux / Qt 6.11.2. The user-supplied
implementation plan authorized this cycle's requirements, design and tasks.
Stage 3 implementation and its local acceptance checks passed. Earlier native
acceptance gates remain pending independently. No provider sources changed.

## Requirement evidence

| Requirements | Evidence |
| --- | --- |
| BROWSE-01–03, 07 | Directory tests exercise natural ordering, leading-zero/case ties, arbitrarily long ASCII digit runs, Unicode/spaces, uppercase suffixes, hidden exclusion/explicit inclusion, explicit extensionless/missing files, regular-file and directory/dangling symlinks, an empty folder, enumeration failure, model roles/contracts, 100 superseding scans and stale result rejection. |
| BROWSE-04–06 | Document tests cover boundaries, immediate requested-index changes, rapid navigation and reversal, errors without skipping, deletion, additions/renames and Refresh retaining a missing selection; folder and image errors remain separate. |
| BROWSE-08–09 | Injected decoder tests count calls to prove cache hits without decoding, newest foreground priority behind an active prefetch, silent prefetch errors, one attempt per selection and refresh bypass. Cache tests cover count/byte eviction, LRU order, oversize rejection, size and timestamp invalidation, missing files, normalized paths and distinct symlink paths. |
| BROWSE-10 | Directory shutdown and existing nonblocking decode-shutdown tests pass. The stress test refreshes then shuts down with scanning/decoding dispatched and waits asynchronously for both workers. |
| BROWSE-04–06, 11 | Production Main.qml tests deliver actual Qt Page Up/Down, Previous/Next clicks and F5. They assert Fit reset, canvas focus, immediate zoom/arrow pan, continued navigation from an error, and no document changes while Open is active. |

The normal suite includes 28 GTest cases: 27 pass and the opt-in resource case
is skipped. The resource case passes separately. All six CTest checks pass.
Fixtures, captures, staging directories and logs stay under build/.

## Resource measurement

`Browsing.LargeFolderAndImages` creates 20,000 supported-suffix entries, including
three accepted 6000×4000 PNG images (96,000,000 decoded bytes each). It starts a
1 ms GUI timer, opens the first image, waits for the snapshot and image, navigates
twice, then refreshes and shuts down. Test setup and file cleanup are included in
whole-process elapsed time and peak RSS, but excluded from scan/navigation times.
On this Intel Core i9-9900K machine:

| Measurement | Result |
| --- | --- |
| Folder scan / snapshot publication | 625 ms |
| Both initial image and snapshot ready | 625 ms |
| Two subsequent image navigations | 363 ms |
| GUI timer ticks during open/navigation | 98 |
| Whole test process elapsed | 3.795 s |
| Peak RSS (`os.wait4`, `ru_maxrss`) | 241,660 KiB |

Evidence: `build/stage3-memory.json`, `build/stage3-memory.xml` and
`build/stage3-memory.log`. Reproduce the case with:

```sh
env QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  VIEWER_BROWSE_BENCHMARK=1 build/test/tests/viewer-smoke \
  --gtest_filter=Browsing.LargeFolderAndImages \
  --gtest_output=xml:build/stage3-memory.xml
```

Launch this command as a child and collect `os.wait4(child, 0).ru_maxrss` for
peak memory. XML properties record timings and timer progress. These results
establish progress on this machine, not a cross-hardware latency guarantee.
The decoder/cache exercise does not include a rendered production window;
rendering has separate production-window and existing large-canvas tests.
Retained displayed-plus-cached image storage is bounded by 256 MiB. Codec
allocation, conversion, temporary results and rendering storage are additional.
Metadata-preserving edits require F5. Running filesystem/codec calls can delay
cooperative cancellation; there is no hard shutdown deadline.

## Contributor checks

| Command | Result |
| --- | --- |
| `task deps`, baseline `task build`, `task test` | Passed before implementation |
| Final `task test` | Passed all 6 CTest checks, 10.75 s |
| `task build PRESET=release` / `task install-check` | Passed release build, staged runtime, CLI PNG/JPEG and recovery checks |
| `task format`, `task format-check` | Passed |
| `task tidy` | Passed all 11 application/test translation units after simplifying natural comparison and descriptive names |
| `task qml-lint` | Passed without QML diagnostics |
| `task license-check` | Passed outside sandbox after REUSE worker socket creation was denied |
| `task desktop-check` | Passed isolated development and packaged registration checks |
| `task visual-check` | Passed all 6 appearance/scale combinations, including production browsing controls |
| Live Wayland browsing test | Passed production-window keyboard, buttons, focus and dialog suppression |
| `git diff --check` | Passed |

Logs use `build/stage3-*.log`. Ninja emitted a recoverable premature-log-end
warning during several incremental builds; final builds, checks and tests passed.

## Visual and native acceptance

Inspected all six `build/visual/{dark,light}-{1,1.25,1.5}-browse-small.png`
captures at 420×280 logical pixels. Controls, disabled boundary state, position
and keyboard hints are readable and unclipped. Existing empty/opening/inspection
captures also passed at small and large sizes. At minimum size the added controls
and wrapped hints leave a shallow, functional canvas; larger windows provide
more inspection space.

The live check uses:

```sh
env QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  VIEWER_CAPTURE_PREFIX="$PWD/build/visual/live-stage3" \
  build/test/tests/viewer-smoke --gtest_filter=Viewer.FolderBrowsingControls
```

It passed in 846 ms; inspected `build/visual/live-stage3-browse-small.png`.
The compositor chose a larger tiled window, so this capture is live input/render
evidence, not a native minimum-size assertion. The harness sends synthetic Qt
input and uses the fallback dialog. Native portal/file-manager drops (stage 1),
cross-display mixed scaling (stage 2), and clean-checkout/CI/stacking-compositor
acceptance (stage 0) remain explicitly pending. Stage 3 evidence does not close them.

## Canvas focus warning correction (2026-09-08)

A user reported `QQuickItem: Cannot set activeFocusOnTab to false once item is
the active focus item.` while browsing. Main.qml bound the canvas's Tab focus
policy to image readiness, so navigation/Refresh changed it to false while the
canvas already had focus. The canvas now has a stable `activeFocusOnTab: true`;
existing readiness/dialog guards continue to govern inspection input (BROWSE-05/06).
Directory opening remains out of scope: open one image to browse its siblings.

The existing production browsing test reproduced the warning three times before
the correction. After rebuilding, both `Viewer.FolderBrowsingControls` and
`Viewer.InspectionControlsAndLifecycle` passed with zero console warnings,
covering navigation, errors, Refresh, immediate zoom/pan and dialog suppression.
These runs explicitly set `QT_FORCE_STDERR_LOGGING=1` and
`QT_LOGGING_RULES='*.warning=true'` so platform logging could not hide the warning.
Evidence is `build/focus-warning-before.log` and `build/focus-warning-after.log`.

Contributor verification: `task deps`, debug/release builds, `task test` (6/6,
10.75 s), formatting, QML lint, licensing and staged install passed. Licensing
used the previously authorized unsandboxed REUSE worker socket. Static analysis
also passed for all 11 translation units. Logs are `build/focus-warning-*.log`. No layout or native
integration changes were made; earlier desktop acceptance remains pending.
