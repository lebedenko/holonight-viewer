# Stage 1 verification

Verified 2026-09-08 on Arch Linux, Qt 6.11.2, with installed HoloNight packages.
Requirements and design/tasks were separately approved by the user. Image opening
is implemented. Native portal/file-manager acceptance below remains pending, so
this record does not claim complete desktop acceptance or close stage 0.

| Check | Result | Requirements |
| --- | --- | --- |
| `task deps`, `task build`, baseline `task test` | Passed before implementation; provider output stayed under build/deps | Integration |
| `task test` | Passed: 6/6 CTest checks; viewer-smoke contains 14 GTest cases | IMG-01–12 |
| `task build PRESET=release` via `task install-check` | Passed: static internal UI module linked into executable | Integration |
| `task format`, `task format-check` | Passed for all application/test C++ and Main.qml | Integration |
| `task tidy` | Passed for all five project translation units, no project diagnostics | Integration |
| `task qml-lint` | Passed without diagnostics | IMG-01, IMG-04–06, IMG-11 |
| `task license-check` | Passed outside sandbox after REUSE worker socket was denied inside it | Licensing |
| `task install-check` | Passed: staged files, desktop validation, sustained empty startup and generated PNG/JPEG opening using installed providers | IMG-02, IMG-07, IMG-10–12 |
| `task desktop-check` | Passed for debug/release registration in isolated XDG data home; packaged entry remains independent | Integration |
| `task visual-check` | Passed: dark/light, 100%/125%/150%, empty/image, minimum and initial sizes | IMG-01, IMG-05–06, IMG-11 |
| Live Wayland smoke | Passed: both production-window keyboard and opening/adapter tests; captures inspected | IMG-01–06, IMG-11–12 |
| `git diff --check` and final serial REUSE lint | Passed after documentation updates | Integration |

## Automated coverage

The real Main.qml and the same statically linked document/canvas implementation
are used by the executable and window tests. The tests activate Ctrl+O, select a
Unicode/space-named file by double-clicking its Qt fallback-dialog delegate, cancel
another dialog without changing the image, and send actual Qt drag-enter/drop
events to the window. Single-file drops open; multiple/remote drops produce errors
and clear the canvas; a subsequent valid drop recovers. Existing fullscreen,
Escape, maximized-state restoration, quit, native window flags, empty-state layout
and configured HoloNight style tests remain enabled.

Real decoder fixtures cover PNG alpha, JPEG, Unicode/spaces, extensionless content,
all eight EXIF orientations (rotation and reflection), GIF first-frame-only
behavior with a different second frame, missing/corrupt/unsupported files,
directories, unreadable files and a special device. Tests compare original file
bytes after opening. Oversized-axis, excessive-pixel-count and sparse encoded-file
fixtures exercise the documented rejection policy. Mandatory PNG/JPEG handlers
are asserted; GIF coverage reports a skip when unavailable (it ran here).

A held fake decoder receives 1,000 replacements: only the first and newest
request execute, and only one Ready presentation occurs. Separate coverage checks
stale failures after a newer validation error, successful recovery, cancellation,
and shutdown with pending work. Shutdown keeps processing GUI events while the
held operation finishes, discards its result and signals after the worker stops.

`viewer-opening` runs `scripts/check-opening.py` against the built executable. The
same check runs against the staged executable with only installed provider paths.
It generates PNG/JPEG fixtures, exercises relative Unicode/space and dash-prefixed
paths, rejects remote/multiple CLI input, and verifies that corrupt/missing paths
report translated errors while keeping the window/process alive for recovery.
Success processes are observed for one second and then terminated by the harness;
this subprocess check supplements, rather than replaces, document Ready/pixel
assertions in the window/decoder tests. Packaging still advertises no MIME types.

## Responsiveness and memory

A generated 6000×4000 PNG decodes successfully while a 1 ms GUI timer continues
receiving events; an immediate replacement followed by shutdown drains safely.
A standalone run of `Document.LargeImageAndShutdown` took 0.744 seconds, with peak
child RSS 137,612 KiB (about 134 MiB), including fixture generation and Qt startup.
Measured with Python `time.monotonic()` and `resource.getrusage(RUSAGE_CHILDREN)`;
output is `build/stage1-memory.log`. This is one local measurement, not a performance
guarantee across codecs, hardware or graphics backends.

The 128 MiB policy bounds each accepted decoded image, not process RSS. Image
conversion/orientation may hold extra buffers, and backing textures depend on
canvas/device size. Codec-private allocations are not covered by the Qt reader
limit. Work remains bounded to one active decode and one pending URL. Mid-read
cancellation is unavailable: closing does not block the event loop, but process
exit can wait for a codec/filesystem operation with no hard deadline.

## Visual and runtime evidence

Generated artifacts remain under build/. Logs are `build/stage1-*.log`; the final
staged prefix was `build/install-check.nmXrce`, with opening artifacts under
`build/opening-check.jrpt25r2`. Desktop registration artifacts are under
`build/desktop-check.3a37emz8`. These paths are local evidence, not installed runtime
dependencies and not committed artifacts.

`task visual-check` reproduces captures under `build/visual/`: names use
`{dark,light}-{1,1.25,1.5}-{small,large,image-small,image-large}.png`.
Inspected light 125% image/minimum, dark 150% image/minimum, light 150% empty/large,
and live Wayland image/minimum and image/large captures. Text, fitted image and
controls are readable and unclipped. The solid-red asymmetric-aspect fixture makes
fit boundaries explicit; alpha/orientation pixels are verified separately.

Live check command (with installed provider QML/library paths in the environment):

```sh
QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=software \
  VIEWER_CAPTURE_PREFIX="$PWD/build/visual/live-stage1" \
  build/test/tests/viewer-smoke \
  --gtest_filter=Viewer.WindowAndKeyboard:Viewer.OpeningCanvasAndAdapters
```

The two live cases passed in about 1.3 seconds. Tests force Qt's fallback file
dialog, including on Wayland; compositor-owned decorations are not in the client
surface captures.

## Remaining desktop acceptance

- Select and cancel an image with the host's native portal dialog, including
  keyboard focus and shortcut suppression. The fallback-dialog result does not
  establish portal behavior.
- Drag a real file from the desktop file manager into the running application.
  Synthetic Qt drops establish the application adapter, not cross-process desktop
  transfer behavior.
- Retain stage 0's CI execution, committed clean-checkout and stacking-compositor
  server-side titlebar inspection checks. No provider, sibling or umbrella source
  changes were made; no publish/commit action was performed.

These manual checks remain explicit in TASKS.md. Stages 2–5 and their approval
cycles are outside this implementation.
