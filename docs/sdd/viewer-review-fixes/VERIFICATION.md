# Viewer review fixes verification

Implemented the supplied plan in three separately reviewable Conventional Commits on 2026-10-11. The earlier `docs/sdd/review-fixes`
cycle is preserved. Evidence is under `build/viewer-review-fixes/`.

## Results

| Check | Result | Evidence |
| --- | --- | --- |
| Stage 1 focused regressions | 21 passed | `viewer-stage1-tests.log` |
| Stage 2 loader/document/directory/playback/UI regressions | 75 passed | `viewer-stage2-current-providers.log` |
| Fresh external configure/build, BUILD_TESTING=ON | Passed | `viewer-stage3-external-configure.log`, `viewer-stage3-external-final-build.log` |
| External full CTest | All 27 entries passed | `viewer-stage3-external-tests.log` |
| Final external focused regressions | All 75 passed | `viewer-stage3-external-final-focused.log` |
| Final task check | Debug/Release/test builds, all 27 CTest entries and formatting passed; stopped at unchanged grid_selection_controller.cpp:10 lint | `viewer-review-fixes-check-final.log` |
| ASan/UBSan | Final task sanitizer-check passed all 27 entries (247.87 seconds), with leak detection and halt-on-error enabled | `viewer-review-fixes-sanitizer.log`, `viewer-review-fixes-sanitizer-final.log` |
| QML lint/imports/types, REUSE, staged install | Passed on final code | `viewer-review-fixes-remaining-final.log` |
| Changed-code clang-tidy | Passed | `viewer-final-document-tidy.log`, `viewer-final-loader-tidy.log`, `viewer-final-loader-tests-tidy.log`, `tidy/FINAL-SUMMARY.txt` |
| Full C++ lint | Existing diagnostics at four unchanged source sites affect 11 of 64 translation units | `tidy/FINAL-SUMMARY.txt` |

The complete installed providers at `/tmp/hn-refactor-prefix` were reused through
explicit `HOLONIGHT_DEPENDENCY_PREFIX`, `HOLONIGHT_QML_IMPORT_PATH` and
`LD_LIBRARY_PATH`. The default `build/deps/prefix` lacks the existing UI's
HnPopupCard; six existing Main.qml tests failed there and passed with the complete
prefix. No provider sources or host installation were changed. Test execution
outside the sandbox was necessary for private D-Bus sockets and offscreen render
contexts.

## External build and boundaries

```sh
cmake -S . -B /tmp/viewer-review-fixes-external -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH=/tmp/hn-refactor-prefix \
  -DQML_IMPORT_PATH=/tmp/hn-refactor-prefix/lib/qt6/qml
cmake --build /tmp/viewer-review-fixes-external -j 8
LD_LIBRARY_PATH=/tmp/hn-refactor-prefix/lib \
  ctest --test-dir /tmp/viewer-review-fixes-external --output-on-failure
```

The external build began without an existing build directory. Compile commands
confirmed private target include directories: images only; browsing plus images;
thumbnails plus images; platform only. None includes the former common
`apps/viewer` directory. That directory is private to the application executables
for their controllers, QML registration and presentation types. Svg is private to
images/thumbnails; GuiPrivate, Wayland and generated protocol headers remain
private to platform. Core/DBus are platform's header dependencies.

The final staged `/usr` payload is `build/release/install-check.jOKnhl`, created
with DESTDIR. It retains the executable, desktop entry, icon, AppStream metadata
and three license files, with no private archives or headers installed. Installed
CLI opening and GIO launches for all seven supported formats passed.

## Coverage

- Deterministic clocks separate frame-index signals from metadata state; visible
  and hidden strip delegates retain their QObject identities across frame changes.
  Existing UI checks cover pause/resume, count discovery, damage notice and reset.
- Lowercase and uppercase SVG neighbors have no speculative decoder calls and
  one call on selection; existing raster/GIF prefetch and cache tests remain.
- Loader barriers cover active/pending replacement, GUI completion identity and
  busy state, reentrant submission ordering, blocked shutdown and accepted paste
  adoption before subsequent work. Document integration covers refresh epochs,
  stale/cancelled foreground and paste results, failure preservation and one-decode
  paste acceptance.
- Existing cache reuse, source invalidation, eviction, GIF reservation, tentative
  paste allowance and playback memory regressions pass.

## Acceptance limitations

The supplied plan defers unrelated lint cleanup. Full lint reports:

- `grid_selection_controller.cpp:10`: implicit pointer-to-bool conversion.
- `viewer_controller.cpp:77,80,118,142,148`: implicit pointer-to-bool conversions.
- `image_information_formatter.cpp:86`: designated-initializer suggestion.
- `thumbnails/thumbnail_cache.h:29`: missing explicit destructor alongside deleted
  copy/move operations; this diagnostic recurs in multiple translation units.

All four files are byte-for-byte unchanged from commit `0893c2b` (accounting for
ThumbnailCache's move). No checks were disabled. New loader request scalars were
initialized and test callbacks changed to const references after full lint found
those scoped issues; final targeted checks pass. Consequently task check's overall
result cannot be reported as passing until the deferred baseline lint is resolved.

The user confirmed all requested native Wayland checks passed on 2026-10-11:
Open/cancel, navigation, playback, pasted paths and exit. Native pointer movement and focus were not automated; the automated
UI suites use the offscreen platform. README was updated for the private targets.
