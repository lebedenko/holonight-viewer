# Verification — 2026-09-23

## Baseline and environment

Viewer baseline: `06b0061a20ab4175f6f1546c8316132e31271bce` (initially clean).
Provider checkouts are clean and unchanged:

- holonight-config: `fe69a59e6b73167fd5349223a4d265d75386c139`
- holonight-qt: `863af4183bdf09ce05199b37e8f5dfb46a311ba1`
- holonight-images: `3633865d2f39e4f163f0159a0f252f88245379f0`

Existing provider builds use GCC 16.2.1 (20260810), Qt 6.11.2, Release,
BUILD_TESTING/BUILD_TESTS=OFF and BUILD_WAYLAND=OFF. Each provider build reported
no work to do. Installed image/palette archives match the build artifacts byte for
byte. The existing `viewer-ci` image (`cc0049424d63`) reports the same Qt and GCC
versions. No dependency pins or provider sources were modified.

## Red/green regression

Commands run from Viewer (log paths below are local evidence):

```sh
cmake --build build/test --target viewer-smoke -j2
env QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  dbus-run-session --config-file=tests/fixtures/dbus-session.conf -- \
  build/test/tests/viewer-smoke \
  --gtest_filter=ImageInformationPopup.PreviewTracksWindowDprAndPhysicalSampling
```

Before the QML edit, the regression failed: screen DPR stayed 1, canvas DPR stayed
1 on attachment to a DPR-1.5 window and throughout later transitions. At DPR 1.5,
physical magnification was incorrectly 0.734375 instead of 1.1015625; at DPR 2 it
was 0.734375 instead of 1.46875. SmoothPixmapTransform remained enabled and the
painted checker contained 19,320 and 34,596 blended interior pixels respectively.
The 94×94 fitted logical bounds stayed correct. Log: `/tmp/viewer-dpr-red.log`.

After replacing the screen binding with `Window.window ? Window.window.devicePixelRatio : 1`,
rebuilt and ran the same private-bus command with filter
`ImageInformationPopup.*:ImageCanvas.*:ViewGeometry.*`: **19/19 passed**, including
the new regression. Log: `/tmp/viewer-dpr-green.log`.

The regression checks fallback before attachment, initial attachment at DPR 1.5,
live transitions 1 → 1.25 → 1.5 → 2, and close/change-to-1.25/reopen. The same real
canvas retains its source, orientation, visibility and fitted bounds. Physical
magnification equals 94 × DPR / 128. Painted output contains blends only below
1× magnification; enlargement uses exact checker colors. No QML warnings occurred.
The new fixture never shows a native window or injects input.

The first sandboxed invocation could not create the private D-Bus socket and did
not run a test. The command was rerun with sandbox escalation. Surrounding existing
input tests run only on the offscreen platform; no desktop input was automated.

## Acceptance

```sh
cmake -S . -B build/information-preview-dpr-acceptance -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps/prefix" \
  -DQML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml"
cmake --build build/information-preview-dpr-acceptance -j2
QMLFORMAT=/usr/lib/qt6/bin/qmlformat task check
bash scripts/prepare-runtime-check.sh
docker build --network none -t holonight-viewer-information-preview-dpr-runtime \
  build/runtime-check.MFweQc
docker run --rm --network none holonight-viewer-information-preview-dpr-runtime
```

- Fresh Release configure/build passed; complete logs inspected, no warnings.
  Logs: `/tmp/viewer-dpr-release-configure.log`, `/tmp/viewer-dpr-release-build.log`.
- `task check` built all presets and passed all 22 CTest entries (107.75 s),
  including the Fusion controls suite. Main smoke: 183 passed, eight existing
  opt-in performance/clipboard/visual skips. Formatting passed.
- The full tidy scan completed all 39 files but stopped task check on four
  identifier-length diagnostics in the new pixel loops. Renamed `x`/`y` to
  `column`/`row`; the affected-file tidy recheck passed with no actionable warnings.
  The compiler and tidy logs were reviewed; remaining tidy counts are suppressed
  non-user-code/NOLINT diagnostics. Logs: `/tmp/viewer-dpr-task-check.log`,
  `/tmp/viewer-dpr-tidy-recheck.log`.
- Rebuilt the affected test and reran popup/canvas/geometry: 19/19 passed again
  (`/tmp/viewer-dpr-final-build.log`, `/tmp/viewer-dpr-final-tests.log`).
- Continued remaining acceptance steps individually: format-check, qml-lint,
  license-check (242/242 files), install-check, qml-import-check and qmltypes-check
  all passed. Thus all required check components passed, although the initial
  aggregate command exited at the subsequently corrected tidy failure. The
  test-only rename does not invalidate the clean Release or installed-runtime
  evidence; unaffected expensive checks were not repeated.
- Final diff whitespace check and SDD relative-link checks passed.
- Isolated installed runtime: passed (exit 0), including PNG/JPEG/BMP/WebP/GIF/TIFF
  decoding, CLI recovery and installed GIO desktop launch; `/tmp/viewer-dpr-runtime-build.log`,
  `/tmp/viewer-dpr-runtime.log`. Staging uses the existing script and only installed
  payloads, with no workspace mounts or runtime network. Image ID:
  `4bd11f2c99ae7d25d5dc6e7fe7fbe103f4ffaff5e5d93e241299334710b13849`.

Commands for the correction and remaining checks:

```sh
run-clang-tidy -p build/test -removed-arg=-mno-direct-extern-access \
  -config-file=.clang-tidy -j 2 tests/image_information_popup_test.cpp
cmake --build build/test --target viewer-smoke -j2
# Rerun the private-bus command above with the popup/canvas/geometry filter.
QMLFORMAT=/usr/lib/qt6/bin/qmlformat task format-check
task qml-lint
task license-check
task install-check
task qml-import-check
task qmltypes-check
git diff --check
```

## Manual acceptance and handoff — 2026-09-23

The user reported “manual checks done” and requested “commit and pin”. This closes
manual visual qualification for this popup DPR correction by user report; no
additional agent-driven native checks were performed. It does not close Files T5,
physical mixed-monitor acceptance or other previously deferred gates.

The automated evidence above is unchanged. Documentation closure and publication
require no repeat of the unaffected application build or installed-runtime checks.
Commit/publication and the umbrella pin are authorized by the follow-up request;
exact published revision and the one-time CI snapshot are recorded in the umbrella
coordination ledger. No host installation or compositor changes were performed.
