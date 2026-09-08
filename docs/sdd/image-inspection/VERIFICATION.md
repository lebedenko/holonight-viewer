# Stage 2 verification

Verified locally on 2026-09-08, Arch Linux / Qt 6.11.2. Requirements, design
and tasks were approved by the user. Implementation and local verification are
complete; native mixed-scale display acceptance remains pending. No sibling or
provider sources changed.

## Automated coverage

ViewGeometry tests verify numeric Fit/Actual Size rectangles, physical display
ratios, center preservation, pointer anchoring across 100 zoom round trips,
edge constraints, letterbox centering, zoom bounds for large and one-pixel images,
image replacement, invalid inputs and temporary zero-size viewports. A painting
test draws one opaque source pixel over transparency at 100%/125%/150% display
scaling and asserts exactly one physical output pixel remains opaque/red.

Production-window tests send real Qt wheel, drag, button and keyboard events.
They cover angle and pixel wheel deltas, delivered inverted-scroll signs,
horizontal-only wheel rejection, zoom anchoring, Tab/Backtab canvas focus,
keyboard pan, geometry changes during a drag, resize/fullscreen, modal guards,
cancel preservation, Fit reset on replacement and error state. A layout check
ensures switching Fit to a percentage does not resize the canvas. Existing
opening, orientation, alpha, resource, shutdown and unchanged-source tests remain
part of the same test suite.

## Resource measurement

A 6000×4000 premultiplied image is rendered into a 1000×700 canvas with the
software backend. The test performs 60 pan/update/grab iterations while a 1 ms
GUI timer runs. Separate processes measure Fit and 3200% zoom using
`os.wait4(...).ru_maxrss`; the generated source stays shared and the canvas
size stays fixed. On an Intel Core i9-9900K, the final measurements were:

| Mode | 60 iterations | GUI timer ticks | Peak RSS |
| --- | --- | --- | --- |
| Fit | 270 ms | 59 | 149,244 KiB |
| 3200% | 126 ms | 63 | 149,304 KiB |

Both processes passed. The 60 KiB RSS difference supports canvas-sized rendering
storage rather than allocating a magnified image. Records are
`build/stage2-memory.json`, `build/stage2-memory-{fit,max}.log` and corresponding
XML files. Run the following with MODE set to fit or max, recording child RSS
with Python os.wait4 and elapsed wall time with time.monotonic:

```sh
env QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  VIEWER_BENCHMARK_MODE="$MODE" build/test/tests/viewer-smoke \
  --gtest_filter=Viewer.LargeImageInspection \
  --gtest_output="xml:build/stage2-memory-$MODE.xml"
```

This measures one backend/machine, not a whole-process bound across all codecs
or graphics drivers. The large-image test records timer progress and timing;
it does not impose a cross-hardware latency guarantee.

## Desktop acceptance

Native movement between displays with different scaling remains pending.
Synthetic ratio tests and offscreen scale captures do not establish that desktop
behavior. Stage 1 native portal and external file-manager drop checks, and stage 0
CI/clean-checkout/stacking-compositor checks remain open independently.

## Commands and outcomes

| Check | Result | Requirements |
| --- | --- | --- |
| `task deps`, `task build`, baseline `task test` | Passed before implementation; baseline 6/6 CTest checks | Integration, VIEW-15 |
| `task test` | Passed: 6/6 CTest checks, including 20 GTest cases; final CTest run 9.40 s | VIEW-01–15 |
| `task build PRESET=release` via `task install-check` | Passed | Integration |
| `task format`, `task format-check` | Passed | Quality |
| `task tidy` | Passed for all eight application/test translation units, no project diagnostics | Quality |
| `task qml-lint` | Passed without diagnostics | VIEW-12–13 |
| `task license-check` | Passed outside sandbox after REUSE's worker socket was denied; final serial lint also passed | Licensing |
| `task install-check` | Passed: staged installation and PNG/JPEG/CLI recovery using installed providers | VIEW-15 |
| `task desktop-check` | Passed: isolated debug/release registration and packaged entry checks | Integration |
| `task visual-check` | Passed all six theme/scale combinations, including new inspection captures | VIEW-01–04, VIEW-07–10, VIEW-13 |
| Live Wayland inspection | Passed: production-window input/lifecycle case in 3.637 s | VIEW-02, VIEW-04–09, VIEW-12–15 |
| `git diff --check` | Passed | Quality |

Logs are `build/stage2-*.log`. Final staged install:
`build/install-check.7uJ3MH`; installed opening fixtures:
`build/opening-check.vy78w0be`; isolated registration:
`build/desktop-check.5sywibyc`. These are generated local evidence, not committed
files or runtime dependencies.

## Visual and live evidence

The visual script captures Fit, Actual Size, pan and maximum zoom at 420×280 and
1000×700, in dark/light themes at 100%/125%/150%. New capture names are
`build/visual/{dark,light}-{1,1.25,1.5}-inspect-{fit,actual,pan,max}-{small,large}.png`.
The unchanged empty/opening captures run alongside them. Fixtures and images stay
under build/.

Inspected the final light 125% Fit/minimum, dark 150% Actual Size/minimum, light
150% maximum zoom/large, and live Actual Size/minimum captures. Controls, status
and wrapped keyboard hints are readable and unclipped. The image stays inside the
canvas; Actual Size shows consistent source-pixel spacing, also established by
physical-pixel painting assertions. A dense checkerboard naturally aliases when
reduced to a small Fit rectangle with the current smooth raster sampler.

Live command:

```sh
env QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  VIEWER_CAPTURE_PREFIX="$PWD/build/visual/live-stage2" \
  build/test/tests/viewer-smoke \
  --gtest_filter=Viewer.InspectionControlsAndLifecycle
```

The live harness opens a real window but sends synthetic Qt input and forces the
fallback file dialog. It does not establish physical trackpad behavior, native
portal behavior or cross-display scaling. Initial live failures exposed test
assumptions about compositor sizing: a 1600×1200 image fit completely on this
desktop and correctly refused to pan. A larger accepted fixture and settled
configure events corrected the test. Visual review separately caught percentage
text wrapping into a new row during zoom; a dedicated status row and minimum-size
regression assertion resolved that layout issue.

Native cross-display acceptance remains open as stated above; this record does
not mark the whole roadmap stage or earlier stages fully accepted.

## Open shortcut warning follow-up (2026-09-08)

A user reported Qt's warning that StandardKey.Open has multiple platform key
bindings. Main.qml now uses `sequences: [StandardKey.Open]` instead of the singular
sequence property. This preserves the approved Open behavior (IMG-11, VIEW-15)
while registering all standard platform bindings.

Re-ran `task deps`, `task build`, `task test` (6/6 CTest checks, including existing
Ctrl+O/dialog tests), `task build PRESET=release`, `task format`,
`task format-check`, `task tidy`, `task qml-lint` and `task install-check`: passed.
Licensing passed with `reuse --no-multiprocessing lint`; whitespace checks passed.
A two-second offscreen production startup stayed alive with empty console output;
neither startup nor the test log contained the Shortcut warning. Logs are
`build/shortcut-*.log`. Native platform shortcut variants were not separately
exercised by this follow-up.

## Keyboard pan focus follow-up (2026-09-08)

A user reported that opening an image and zooming with + left arrow-key panning
inactive until a mouse drag. Zoom shortcuts changed the view without moving
keyboard focus from the previously focused control to the canvas. Shared QML
Fit, Actual Size and zoom actions now explicitly focus the canvas; wheel zoom
does likewise. Arrows remain scoped to the canvas, and Tab can still move focus
back to controls (VIEW-07, VIEW-12–13).

The production-window regression first failed against the old behavior: both +
and = left focus on Open and a Down key did not move the zoomed image. The updated
test passes without any prior canvas click or drag. It also checks focus after
Actual Size, wheel zoom and keyboard zoom-out, and verifies that an arrow does
not pan when Tab has deliberately moved focus back to a control. Existing dialog
guards and drag behavior remain covered.

Debug/release builds and all 6 CTest checks (20 GTest cases) passed; the final
CTest run took 9.45 s. `task deps`, `task format`, `task format-check`, `task tidy`,
`task qml-lint` and `task install-check` also passed. Licensing passed with
`reuse --no-multiprocessing lint`; `git diff --check` passed. This follow-up used
offscreen production-window tests and did not repeat native desktop acceptance.
Logs are `build/pan-focus-*.log`, including the failing pre-fix regression in
`build/pan-focus-before.log`.
