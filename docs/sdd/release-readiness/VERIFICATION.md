# Stage 5 verification

Status: **qualification measurements/checks complete; user performance acceptance pending**. This iteration does
not publish, tag, bump a version, package a distribution or change provider sources.
The supplied implementation plan approved SPEC, DESIGN and TASKS scope under
CONTRIBUTING.md. Generated logs, fixtures, screenshots and benchmark checkouts are
under `build/` and are not source-release contents.

Current fixes follow-up: compact WebP and the native generic Qt clipboard alpha
matrix now pass. Earlier failures below are retained as historical evidence and
are superseded by [the fixes results](#viewer-fixes-follow-up-2026-09-08).
The [HoloNight dialog delegation handoff](../../holonight-qt-dlg-delegation-missed.md)
is unresolved and explicitly deferred to the next release.

## Current pending gates (approved scope, 2026-09-09)

The supplied source-qualification plan supersedes historical pending/blocker
statements below. Historical failures and human acceptance evidence are retained.

- Explicit user acceptance of the completed native latency/memory measurements.
- Final-head hosted CI status is recorded in [draft PR #1](https://github.com/lebedenko/holonight-viewer/pull/1)
  after the evidence commit. Candidate hosted CI has passed; the PR checklist is
  authoritative for the later documentation-only head.

Five native trials per revision, the committed clean-checkout contributor/visual
checks and the fresh installed-only runtime container have passed their current
acceptance boundaries, as detailed in the source-qualification execution below.
Baseline incorrect alpha remains a recorded failure.

Native Open/provider-dialog acceptance, physical mixed-monitor/mixed-scale movement,
and clipboard persistence-service behavior and memory/transport costs are **deferred
to the next release, not passed** (TASKS N1–N3). They do not block this release.
No persistence service will be launched; its unmeasured costs are not zero.

Viewer focus/Orca, human clipboard image/path/lifecycle, four-format file-manager
drop and labwc window behavior passed. Stage 4 is accepted; Stage 5 remains pending
until the current gates above pass. No publication or version change is authorized.

## Environment and baseline

Local checks: 2026-09-08, Arch Linux, kernel 7.2.3-arch1-3, Intel Core i9-9900K
(16 logical CPUs), Qt 6.11.2. The desktop session is Hyprland on native Wayland (`wayland-1`),
but the checks below use Qt offscreen/software rendering unless
explicitly stated. Native Wayland is the sole desktop acceptance target following user clarification.
Historical XWayland results do not satisfy or block that acceptance.
This is not evidence for every Wayland compositor.

Baseline commit: `de7ef473eaca2e5d4e2de0b0084dbc691412c473`. Before implementation,
`task deps`, `task build`, `task test` passed (six CTest entries). Provider sources
were built only into Viewer `build/deps`; CI dependency revisions are unchanged.
Qt Image Formats was initially missing. With user authorization,
`sudo pacman -S --needed --noconfirm qt6-imageformats` installed version 6.11.2-1.
The initial failing required-decoder check established the missing-plugin gate.

## Automated results

| Requirement | Check and evidence | Result |
| --- | --- | --- |
| R1 | `Release.RequiredIndependentFormats`: mandatory PNG/JPEG/BMP/WebP handlers; stdlib PNG/BMP, fixed EXIF-6 JPEG, libwebp-encoded alpha fixture; known pixels/dimensions, mismatched supported extensions and unchanged bytes | Pass |
| R1 | Existing corruption, unreadable/special input, encoded/pixel/dimension limits, orientation, first-frame GIF and stale-request tests | Pass |
| R1 | `Release.CompactWebPDecoderGate`, compact 36-byte lossless WebP | **Fail**, retained release blocker |
| R2 | `task desktop-check`: debug/release provider paths and icon separation; real GIO launcher records `--` and exact Unicode/space/percent/dash arguments, including percent in executable path | Pass |
| R2/R3 | `task install-check`: staged CMake files, empty startup, CLI recovery and four normal formats through staged installed desktop entry with PATH lookup | Pass; uses development provider overrides, not isolation |
| R3 | `viewer-runtime-probe` in CTest and `bash scripts/prepare-runtime-check.sh` | All four probe decodes pass locally; installed container payload prepared |
| R3 | Second container, without workspace mounts or development overrides | **Pass** after installing Docker and fixing the Arch task-runner alias; see follow-up below |
| R6 | Final committed-tree hosted CI | **Pending**; no hosted run for this uncommitted change |
| R4 | Qt accessibility interfaces: canvas/buttons/menu-item names, roles, disabled/focused states, keyboard menu activation, Tab movement, Help text selection/copy, dialog close focus restoration; announcement events on load/error/image/path copy, silence during transform/zoom | Pass |
| R5 | Native Wayland compositor-input matrix: wl-paste and interactive Qt receiver | All eight orientations and alpha pass with explicit PNG; generic Qt image reception **fails alpha** |
| R6 | Visual script: 30 production-window test executions; image minimum/default windows inspected in dark/light at 1, 1.25, 1.5 | Pass offscreen; native decorations/mixed displays still pending |

`task test` is expected to remain red on this Qt build: the compact WebP gate
fails rather than being skipped or reclassified as supported. Six other CTest entries
pass; viewer-smoke reports 35 passing tests, one failure and three explicitly
opt-in/native skips. Logs live in `build/release-readiness/` (`test.log`, `install.log`,
`desktop.log`, `format-check.log`, `qml-lint.log`, `tidy.log`,
`tidy-final-tools.log`, `tidy-accessibility.log`, `license.log`). Debug/release builds, formatting, QML lint
and REUSE pass. Full tidy passes, with focused follow-up checks for final tool edits.
The REUSE checker required host execution because sandbox policy blocks its
multiprocessing socket. License coverage uses the repository's REUSE.toml (92/92 files covered).

### Compact WebP reproducer

`python3 scripts/format-fixtures.py build/release-readiness/fixtures` produces a
36-byte valid 1×1 lossless RGBA WebP with a red pixel at alpha 128. Independent
libwebp `WebPGetInfo` returns 1×1. A standalone Qt QImageReader probe reports
`canRead=true`, `size=(-1,-1)` and fails to read. The same image with a valid
optional RIFF `JUNK` chunk reads as 1×1. The normal fixture includes that chunk;
`compact.webp` preserves the failing original. No Viewer or provider workaround
alters input bytes. Viewer now reports unreadable decoder dimensions instead of
incorrectly claiming this tiny image exceeds the size limit. Resolution of this
installed-handler defect is required before declaring the four-format guarantee.

## Isolated runtime reproduction

CI's first container builds the committed checkout and separately installs the
providers. It includes `qt6-imageformats`, GIO, desktop-file-utils and contributor
tools. `prepare-runtime-check.sh` creates a disposable context under build/ with
only installed `/usr` files and diagnostic scripts. The second image uses the
same CI image's Qt ABI, receives no workspace mount, runs with `--network none`,
unsets development QML/library/style paths, checks installed runtime search paths,
loads the real UI empty and with all four fixtures, and launches the actual
installed entry through GIO. The probe is an opt-in `qualification` CMake install
component, excluded from normal installation.

Use the workflow commands in `.github/workflows/build.yml`; the runtime context
path is recorded in `build/runtime-check-context`. The staged host check does not
substitute for this gate. The full workflow stops on the compact WebP test until
that required gate is resolved. Clean extraction of the baseline was built locally;
a clean committed checkout of the final change and a hosted run are still required.

## Performance method

`ReleasePerformance.LargeWorkflow` creates two 8000×4000 PNGs outside the timed
region, opens the first, navigates to the second, applies eight transform commands
and prepares a rotated copy. A 1 ms GUI timer records the maximum observed gap.
`measure-release.py` runs five fresh processes and uses Linux wait4 resource usage
for per-process peak RSS, preserving XML and JSON under build/.

Both binaries are Release builds against the same installed providers. The
baseline source was extracted with `git archive` under build/ and received only
the identical benchmark test and its CMake target_sources line. Production baseline
sources were unchanged. Configure both with BUILD_TESTING=ON and explicit
HOLONIGHT_DEPENDENCY_PREFIX / HOLONIGHT_QML_IMPORT_PATH pointing to build/deps/prefix.
Run sequentially with those QML/library paths in the environment:

```sh
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software python3 scripts/measure-release.py \
  build/release-readiness/baseline/tests/viewer-smoke build/release-readiness/performance-before
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software python3 scripts/measure-release.py \
  build/release-readiness/performance/tests/viewer-smoke build/release-readiness/performance-after
```

These measurements exclude production-window rendering, platform clipboard transport
and external receiver/clipboard-manager storage. They do not close the full native
performance gate. QtTest polling affects timer sampling; the reported maximum is
an observed gap, not a guaranteed upper bound. Transform command timings below
1 ms are not evidence of GPU render latency. Image/cache limits remain unchanged;
clipboard snapshots, transformed output and transport are outside the 256 MiB
retained display/cache bound. Stage 4's separate transport/memory evidence remains
historical, not a new five-run transfer comparison.

| Measurement (five runs in order) | Baseline | Stage 5 |
| --- | --- | --- |
| Open (ms) | 232, 232, 242, 232, 232 | 232, 232, 232, 232, 232 |
| Navigate (ms) | 242, 242, 243, 242, 242 | 242, 242, 242, 242, 241 |
| Eight transform commands (ms) | 0, 0, 0, 0, 0 | 0, 0, 0, 0, 0 |
| Copy preparation/publication (ms) | 51, 41, 50, 41, 30 | 41, 30, 41, 31, 31 |
| Maximum observed GUI gap (ms) | 11, 11, 11, 11, 11 | 11, 11, 11, 11, 11 |
| Peak RSS (KiB) | 421204, 421120, 421452, 421040, 421144 | 420980, 421212, 421564, 421240, 421256 |

## Remaining native acceptance

Record compositor/session, Qt/provider versions, scale/monitor configuration,
receiver names/versions and actual results for each row. Do not infer passes from
offscreen, synthetic events or a different platform.

- Orca with the real Qt AT-SPI bridge: canvas and control names/roles/enabled state,
  reading order, visible focus, Actions menu, native Open, Information and Help;
  loading/error/image-copy/path-copy announcements and quiet zoom/pan/folder updates.
  Orca was subsequently installed; the basic native smoke and announcements pass
  as recorded below. Comprehensive reading order/visible-focus acceptance remains open.
- Keyboard-only native Open selection and cancellation, browsing, inspection,
  eight orientations/reset, image/path copy and dialog navigation/text selection.
- Native Wayland (`QT_QPA_PLATFORM=wayland`, without an XCB fallback): invoke Copy through real keys/menu, activate
  `clipboard-probe --interactive image build/received.png` or
  `--interactive text build/received.txt`, then click/Enter/Ctrl+V. Verify all eight
  orientations, alpha and exact Unicode/space/symlink paths; navigate during
  preparation, close during preparation and test persistence after exit. Repeat
  with an independent native Wayland receiving application. No cause is assigned
  to the earlier native Wayland failures until these real-input checks run.
- Native portal file selection/cancellation, external file-manager drop, native
  decorations on a stacking compositor, mixed-scale display movement and focus.
- Five-run release opening/navigation/rendering/copy/transfer comparison including
  timer gaps and RSS, with external manager storage reported separately.
- Committed clean-checkout contributor checks, no-mount installed runtime and hosted
  CI results. No release acceptance until every required gate above passes.


## Follow-up after installing native test tools

Installed: Orca 50.2-1, wl-clipboard 2.3.0-1, Docker 29.7.2-1; speech-dispatcher
0.12.1-3 and espeak-ng 1.52.0-1 are available. No additional package is needed for
the clipboard/accessibility checks described here. Docker access for the current
user is denied by its socket permissions; execution through sudo succeeds.

### Native Wayland clipboard

Environment: Hyprland 0.56.2, Qt 6.11.2, `QT_QPA_PLATFORM=wayland`, DISPLAY unset.
Both test windows were checked in Hyprland's client list with `xwayland=false`.
Compositor-delivered keys used the installed Hyprland 0.56 Lua dispatcher API,
targeted only by test process PID. This exercises real Wayland protocol input
and clipboard requests; it is not a claim of a human physical-keyboard session.
Dispatcher syntax follows [Hyprland's CLI documentation](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Using-hyprctl/).

An independently generated asymmetric 3×2 PNG contains alpha 64, 128 and 255.
The eight orientations were reached using R/H shortcuts on the production release
Viewer. Each Copy Image used Ctrl+C; the separate interactive receiver read only
after focus and Return. A second receiver, wl-paste, requested image/png; GTK's
GdkPixbuf decoded the saved pixels for comparison against independent matrix
transformations. All generated inputs/results remain under
`build/release-readiness/native-wayland/`.

- Explicit PNG: **8/8 pass** through both wl-paste and the Qt receiver's new `png`
  diagnostic mode, including every pixel, alpha and transformed dimensions.
- Generic Qt `QClipboard::image()`: dimensions/colors reflect all eight current
  orientations, but **8/8 fail alpha**, returning 255 for every pixel.
- Copy Path: **pass**, exact UTF-8 absolute symlink path containing Unicode, spaces,
  a percent sign and a dash-prefixed basename. Original PNG bytes are unchanged.
- The offered private `application/x-qt-image` payload starts `42 4d` (BMP) and
  carries no alpha, while the simultaneously offered image/png preserves alpha.
  This identifies a concrete private-format interoperability problem rather than
  attributing this run to stale images or lack of a Wayland input serial. The
  generic receiver mode remains unchanged; selecting `png` is diagnostic evidence,
  not a fix or a waiver of the failing gate. Provider/Qt sources were not modified.

Evidence: `qt-generic-results.json`, `qt-generic-check.log`, `results.json`,
`qt-png-check.log`, `qt-private-image.bin`. Navigation during preparation, shutdown,
large native transfer timing and human keyboard/menu acceptance remain pending.

### Real Orca and portal smoke

Orca ran as a separate native process with its real speech system and Qt's AT-SPI
bridge enabled. Native accessibility snapshots include canvas/control names,
roles and enabled states, Help and Information. Orca debug output confirms speech
for load, image-copy, path-copy and image-error announcements; AT-SPI's
`object:announcement` events contain the corresponding messages. The test Orca
and Viewer instances were stopped afterward. Evidence: `orca-debug.log`,
`a11y-tree.json`, `a11y-events.json`, `accessibility.log` under the native directory.
This closes the basic real screen-reader smoke, not the full human reading-order,
visible-focus or keyboard-only acceptance matrix.

The session defaults to `QT_QPA_PLATFORMTHEME=holonight`. The first attempt did
not find a separate native Open dialog under that setting, so default-theme
portal behavior remains unqualified. With the installed standard
`QT_QPA_PLATFORMTHEME=xdgdesktopportal`, the GTK portal presented a native Wayland
`Open image` window. Escape cancellation passed; Ctrl+L/path paste/Return selected
an existing corrupt PNG, and Orca spoke Viewer's decoding error. This verifies
native portal selection/cancellation specifically with that theme configuration.
No session theme setting was changed persistently.

Only one active physical monitor is available (3840×2160, scale 1), so mixed-scale
physical-display movement remains untested. No external file-manager or stacking
Wayland compositor executable was found among Thunar, Nautilus, Dolphin, PCManFM,
Nemo, labwc or Weston; those environment-dependent acceptance checks remain open.

### Installed-only container

The new Docker run exposed that Arch's go-task package installs `/usr/bin/go-task`,
while the CI workflow invokes `task`. Dockerfile.ci now provides the conventional
`/usr/local/bin/task` symlink; no host command or package was changed by this fix.
The corrected image successfully built a fresh snapshot of Viewer source and the
unchanged, read-only provider checkouts at the exact pinned CI revisions:
`fe69a59e6b73167fd5349223a4d265d75386c139` and
`22ded7815727ce483fd91e82a9cc04bfe252ec3b`.

The second container ran `scripts/isolated-runtime.sh` with `--network none`, no
workspace mount, and no development QML/library/plugin overrides. **Pass**:
installed entry/icon, runtime-path scan, empty UI creation, all four normal fixture
decodes with pixels/dimensions, CLI failures/recovery, and all four formats opened
through the installed GIO desktop entry. Logs: `container-image.log`,
`container-build.log`, `container-runtime-image.log`, `container-runtime.log`.
The Viewer runtime source matches the working tree, but this is a fresh source
snapshot, not a hosted run from a final committed tree. The compact WebP gate and
native generic-image alpha gate continue to block release readiness.

## Viewer fixes follow-up (2026-09-08)

Approved plan: “Viewer fixes and HoloNight dialog handoff”, recorded before
implementation in SPEC/DESIGN/TASKS (R7–R10). The existing working-tree Stage 5
changes were preserved. Provider sources and pinned revisions are unchanged.
Local libwebp is 1.6.0 through pkg-config. Build dependency is required/private;
CI explicitly installs libwebp/pkgconf, and the installed-runtime image inherits
libwebp and mandatory Qt codecs from viewer-ci. No public runtime API changes.

### Implementation and automated evidence

The clipboard worker transforms and encodes the captured image; only PNG bytes
cross back for GUI publication via QMimeData::setData. Success feedback follows
setMimeData. Null preparation, exceptions and PNG writer failure clear the new
payload and preserve the old clipboard. The internal encoding helper permits
failed-device tests without a public runtime hook. Snapshot, busy/path-copy,
navigation and shutdown behavior is retained. Local tests inspect explicit PNG
bytes; the separate receiver's generic QClipboard::image() mode is unchanged.

Qt decoding remains primary. Fallback accepts an exact RIFF with one padded
VP8/VP8L chunk; extended/ancillary/animated containers remain on Qt's path. It
checks libwebp dimensions and limits before allocating RGBA8888 and calls the
bounded WebPDecodeRGBAInto API, then uses the normal premultiplied normalization.
The encoded limit is 256 MiB, pixel limit 32 million, per-axis limit 32768 and
output limit 128 MiB; WebP's 14-bit dimensions are already below the axis limit.
No input bytes are rewritten. Source references:
[Qt QMimeData](https://doc.qt.io/qt-6/qmimedata.html) and
[libwebp decode API](https://raw.githubusercontent.com/webmproject/libwebp/main/src/webp/decode.h).

- `task deps`, contributor debug/release builds and `task test`: pass. Seven CTest
  entries pass; viewer-smoke has 40 passing cases and three opt-in/native skips.
  `Release.CompactWebPDecoderGate` is unchanged and now passes.
- New tests cover compact alpha/content detection under a misleading extension,
  each truncated prefix, invalid signature/chunk size, oversized VP8L dimensions,
  cancellation, original bytes, and Qt equivalence for ordinary, extended, lossy
  and two-frame animated WebP. The existing first-frame GIF and general file,
  dimension, pixel and memory limits remain covered.
- Clipboard tests cover all eight PNG orientations/pixels/alpha/dimensions,
  exclusive image/png publication, captured-image changes, navigation, busy/path
  suppression, null/exception preparation failure and shutdown. Failed/read-only
  output devices verify PNG encoding failure; real out-of-memory injection is not
  claimed.
- Installed-only runtime: pass, rebuilt prepared payload container with no workspace
  mounts and `--network none`; four formats, installed entry/icon/runtime paths,
  CLI recovery and GIO launch pass. Logs: `build/fixes-runtime-image.log`,
  `build/fixes-runtime.log`; payload context `build/runtime-check.n9i0fp`.
  This uses the existing local viewer-ci image (which already contains libwebp),
  not a fresh hosted build of the edited CI Dockerfile.

### Native Wayland clipboard

Same Qt 6.11.2 / Hyprland 0.56.2 environment and installed providers as above,
DISPLAY unset, QT_QPA_PLATFORM=wayland. Both windows report xwayland=false.
Compositor-delivered Ctrl+C and R/H shortcuts, followed by focus/Return in the
unchanged interactive **image** receiver, pass **8/8** for every pixel, alpha and
dimension. Independent wl-paste PNG reception also passes **8/8**. Offered MIME
list is exactly image/png; application/x-qt-image is absent. A separate interactive
Qt text receiver and wl-paste both preserve the exact Unicode/space/percent/dash
absolute symlink path. Original PNG bytes remain unchanged.

Evidence: `build/fixes-native.log`, `build/release-readiness/fixes-native-wayland/`
(results JSON, receiver logs and received payloads), generated driver
`build/release-readiness/fixes-native-check.py`. These are real Wayland protocol
requests with compositor-delivered keyboard activation, not a physical human
keyboard/menu session. The explicit PNG-only diagnostic did not replace generic
Qt reception in these checks.

The separate 8000×4000 native lifecycle exercise confirmed the previous text
clipboard was still present after Copy (PNG preparation active), then navigated
with Page Down. The generic receiver got the original red/alpha-128 snapshot
while Copy Path confirmed selection had moved to the second image. A second
confirmed in-progress copy followed by Q exited cleanly. Evidence:
`build/fixes-native-lifecycle.log` and
`build/release-readiness/fixes-native-lifecycle/`. The first driver attempt read
an unfinished receiver output file; the corrected driver waits for the receiver's
success report before decoding. No application change was needed for that test race.
Post-exit clipboard-manager persistence is not established by this check.

### Final contributor checks

`task format-check`, `task qml-lint`, `task license-check`, `task desktop-check`
and `task install-check` pass. Full clang-tidy passes after fixing new-test naming
and refactoring decoder error handling (no lint suppressions). The final equivalent
commands were `cmake --build build/test --target tidy` and `ctest --preset test`;
all seven CTest entries pass again. REUSE required host execution because its
multiprocessing socket is blocked in the sandbox. Native tools and Docker likewise
ran with approved host access; no automatic approval rejection was bypassed.
Logs: `build/fixes-{test,final-test,format,qml,license,desktop,install,tidy}.log`;
debug/release build logs and dependency output are also under build/.

### Five-run release comparison for the fixes

Repeated sequentially after builds/lint completed on the same machine and installed
providers, with the unchanged production baseline at
`de7ef473eaca2e5d4e2de0b0084dbc691412c473`. Both baseline and current Release test
binaries received the same benchmark source. Generated runner:
`build/run-fixes-performance.sh`; raw XML/logs/JSON:
`build/release-readiness/fixes-performance-{before,after}/`.

| Measurement (five runs in order) | Baseline | PNG publication |
| --- | --- | --- |
| Open (ms) | 242, 232, 232, 232, 232 | 242, 232, 232, 232, 242 |
| Navigate (ms) | 242, 242, 242, 242, 242 | 242, 242, 242, 242, 242 |
| Eight transform commands (ms) | 0, 0, 0, 0, 0 | 0, 0, 0, 0, 0 |
| Copy preparation/publication (ms) | 40, 41, 41, 41, 40 | 998, 1007, 999, 1029, 988 |
| Separate PNG encoding sample (ms) | 956, 967, 975, 964, 966 | 971, 960, 955, 954, 957 |
| Maximum observed GUI timer gap (ms) | 11, 11, 11, 11, 11 | 11, 11, 11, 11, 11 |
| Peak process RSS (KiB) | 421672, 421752, 421572, 421684, 421828 | 421448, 421516, 421440, 421436, 421644 |

PNG publication costs approximately one second for this 32-million-pixel fixture,
versus 40–41 ms for the old transform/setImage preparation boundary. Encoding is
now completed before publication; the old preparation number omitted lazy platform
serialization. This is not a full end-to-end transport comparison. Timer gaps
remain 11 ms in all runs; peak process RSS is approximately 412 MiB in both builds.
No claim of statistically significant memory improvement is made.

The standalone encoding sample encodes the current decoded snapshot on another
worker after workflow timer sampling stops; it measures PNG encoding directly,
not an exact subtraction of the rotated copy's worker latency. Peak RSS covers
the whole fresh process including this sample. No receiver or clipboard manager
runs in these offscreen measurements: their transport/storage costs are excluded,
not reported as zero. Native rendering, large transfer/receiver and manager RSS
comparisons remain pending. Polling affects observed timer gaps; this is not a
hard latency guarantee. Memory bounds remain unchanged and exclude temporary
encoding buffers and clipboard storage as documented in README.

### Remaining release gates

The [provider native-dialog blocker](../../holonight-qt-dlg-delegation-missed.md)
remains unresolved. The per-process xdgdesktopportal comparison in the previous
record is retained; no global environment setting or provider source changed.
Full human keyboard/menu and Orca reading-order/focus acceptance, clipboard
persistence/manager costs after exit, native rendering/transfer performance,
file-manager drop, stacking-compositor decorations, physical mixed-scale displays,
final committed-checkout checks and hosted CI remain open. X11/XWayland are
outside acceptance. These Viewer fixes do not establish release readiness.

## Visible keyboard-focus repair (2026-09-09)

The supplied plan records a confirmed manual failure: keyboard navigation worked,
but Viewer-owned focused controls lacked visible indicators. Earlier focus-state
and capture evidence did not establish visible-focus acceptance. The mockup
requirement is clarified to preserve stable window-activation styling while
retaining keyboard-focus feedback. Native Open remains outside this repair.

Buttons now use visualFocus, dialog text uses activeFocus, and the canvas has an
inset, decorative outline. Each uses the installed shared focus color/width.
Actions menu enabled highlighted items receive the same outline; existing disabled
text and mouse feedback remain. Layout, focus order, image geometry and provider
APIs are unchanged.

The new Accessibility.VisibleKeyboardFocus regression uses Tab/Shift+Tab, menu
Down/Up, F1/I, Space on Close and Escape. It samples actual window pixels at
indicator edges and checks departure, dialog restoration, normal/minimum sizes.
The existing accessibility regression also checks disabled menu indicator state.

Native compositor-input repetition (`build/focus-compositor.py`,
`build/focus-compositor.log`) passed on Hyprland/Wayland: forward/backward Tab
reached Information, Fullscreen, Actions and canvas; F1/I reached selectable text
and Close; menu Down/Down/Up identified Open/Fit/Open; Escape restored canvas.
AT-SPI focused roles/names were recorded without launching or replacing Orca.
This is compositor-driven automation, not a physical human walkthrough.

First-pass integration caught a missing text-area id and corrected it. Pixel-test
refinements account for the installed one-pixel focus token, actual capture/window
scale, and native channel quantization (maximum five channel levels). These do not
relax the requirement that the focus color be rendered and disappear on departure.

Final contributor validation passes: `task deps`, debug/release builds (also
rebuilt by QML lint and install checks), `task format`, `task format-check`,
`task qml-lint`, `task license-check`, `task install-check`, and full `task tidy`.
After the final pixel-test adjustments, focused clang-tidy on
`tests/accessibility_test.cpp` and `ctest --preset test` pass (7/7 CTest entries,
22.81 s; three opt-in performance/native-clipboard cases remain skipped normally).
Logs: `build/focus-{build,release,format,format-check,qml-lint,license,install,tidy}.log`,
`build/focus-tidy-final-test.log`, `build/focus-test-final.log`. REUSE required
approved host execution because the sandbox blocked its multiprocessing socket.

Native Wayland QTest keyboard/rendered-indicator runs pass in both themes at
1000×700 and 420×280 logical sizes on the current 1.5-scale display. Captures are
under `build/focus-native/`; command/log are `build/focus-native.sh` and
`build/focus-native.log`. These capture Qt window rendering, not compositor
screenshots. Inspected native dark/light canvas, button, dialog text and Close
captures show identifiable outlines. No native Open path was exercised.

At completion of automation, human keyboard-only visible-focus acceptance and
full Orca reading-order checks remained open (T4c/K3). Neither QTest nor
compositor-injected input establishes physical human acceptance; no Stage 5
acceptance or provider-dialog resolution is claimed by this repair.

Final `bash scripts/check-visual.sh` passes all six dark/light × 1/1.25/1.5
combinations (42 test executions), including keyboard indicator assertions at
1000×700 and 420×280. Log: `build/focus-visual.log`; captures:
`build/visual/*-keyboard-*.png`. Inspected dark/light normal/minimum canvas,
Actions button/menu, help/information text and Close captures, including 1.25/1.5
scaling, show visible indicators without changing image geometry or layout.

### Human focus acceptance (2026-09-09)

After the guided physical keyboard walkthrough covering forward/backward Tab,
Actions menu arrows, Help/Information text and Close controls, dismissal and
canvas restoration, the user reported “focus pass”. This closes K3. The guide
also requested dark/light, normal/minimum sizes and fractional scaling; the user
did not specify exact configurations, so no additional configuration-specific
human evidence is claimed. At this point Orca acceptance (T4c), native Open and
other release gates remained open.

### Human Viewer Orca acceptance (2026-09-09)

The user reported “orca pass” after the guided Viewer walkthrough: forward and
backward reading order, control/canvas names, Actions menu names and available
state feedback, Help/Information text and Close controls, dialog dismissal and
focus restoration, and image-copy/path-copy speech. Together with the focus pass,
this closes the Viewer T4c walkthrough. No new Orca version/session details were
provided. Native Open was explicitly excluded and remains a separate unresolved
provider/native release gate. Physical clipboard transfer and other release
acceptance are not established by this report.

### Human clipboard transfer, partial acceptance (2026-09-09)

The user reported that the first four guided steps passed: keyboard image copy
and visual paste checks (dimensions/appearance/transparency), rotated image copy,
keyboard Actions menu copy, and exact full-path paste into a text editor. The
final step failed: after copying, waiting for completion and quitting Viewer,
clipboard content did not survive. Receiving application names/versions and
clipboard-manager configuration were not supplied. This is human visual evidence,
not a new independent pixel comparison or full eight-orientation/lifecycle matrix.
T5c2 remains open, with post-exit persistence recorded as failed in this session.

Initial source inspection finds PNG publication through QClipboard and no explicit
clipboard clear in ClipboardController shutdown. A limited process-name check
found no wl-paste, cliphist, copyq, clipman, klipper or clipse process; this does not
exclude another clipboard service. Missing desktop persistence is a hypothesis,
not an established Viewer defect or a waived acceptance gate.

Subsequent clarification: the user tried PCManFM-Qt file copy → Viewer Ctrl+V
and GIMP region copy → Viewer Ctrl+V; neither produced an action. The same GIMP
region pasted into a new GIMP image successfully. Viewer currently implements
Copy Image/Copy Path, with no paste action or Ctrl+V binding. These incoming-paste
attempts do not test Viewer clipboard export or post-exit persistence. The earlier
reported export results required direction-specific confirmation at that point.

The user then explicitly confirmed Viewer Ctrl+C → GIMP Ctrl+Shift+V works while
Viewer remains open. Closing Viewer before pasting causes GIMP to report that
there is no image in the clipboard. This resolves the transfer-direction ambiguity
and confirms post-exit image unavailability with GIMP as receiver. GIMP version
and clipboard-manager configuration remain unspecified. A follow-up process check
also found no wl-clip-persist process or executable on PATH. This limited check
does not establish the absence of all desktop persistence services. No persistence
cause has been established, and T5c2 remains open.

The user subsequently confirmed that no clipboard persistence service is used.
Post-exit image loss is therefore classified as expected for this session without
desktop persistence, consistent with README's desktop-managed persistence contract,
rather than a confirmed Viewer defect. The observed GIMP result remains recorded;
persistence with a service and its storage/transfer costs remain untested. No
service was installed or configured. T5c2 remains open for the rest of the full
human transfer/lifecycle matrix.

The user then reported “8 orientations pass” for the guided Viewer → GIMP
sequence: Reset Transform, initial copy/paste, three successive R rotations with
copy/paste after each, H with copy/paste, then three more R rotations with
copy/paste after each. The guide requested comparison of orientation, dimensions
and transparency with Viewer kept open. This records a human visual pass for all
eight orientations; it does not add an independent pixel measurement. Special
and symlink paths and confirmed in-progress navigation/shutdown remain pending
for the human walkthrough.

The user reported “path pass” for the prepared symlink
`build/manual-acceptance/- Фото 100%.png`. The guided check compared the pasted
normalized absolute path exactly, including the leading dash in the filename,
Unicode, spaces and percent sign, without added quoting/escaping or resolution to
the target `source.png`. Human special/symlink path acceptance passes. Confirmed
in-progress navigation/shutdown checks remain pending.

The user subsequently reported “navigation pass / quit pass” for the prepared
8000×4000 fixtures: copy the red image, navigate to the green image during
“Preparing…”, then paste the original red/transparent snapshot into GIMP; start
another copy and quit during preparation without a hang or crash. This completes
the guided physical image/path transfer and lifecycle walkthrough (T5c2) for the
user's session. Local `gimp --version` reports 3.2.4; no new receiver backend
measurement was made. Human visual evidence complements the existing automated
native pixel/transport checks. Desktop-service persistence and transport/manager
performance qualification remain open under T6b/F4c; no service was enabled and
no release acceptance is claimed.

### Human file-manager drop acceptance (2026-09-09)

The user reported “drop pass - all four formats” after the guided PCManFM-Qt →
Viewer single-file drag-and-drop checks for PNG, JPEG, BMP and WebP. The guide
requested correct image/filename updates without hangs or unexpected dialogs.
This closes the human file-manager drop portion of T6b. Native Open, stacking
decorations, physical mixed-scale displays, performance qualification and final
committed-tree/hosted-CI gates remain separate.

### Human labwc window acceptance (2026-09-09)

Following the guided stacking-Wayland checks (native title bar, move/resize,
maximize/restore, fullscreen entry/exit and dialog placement), the user reported
logging into labwc and that Viewer “works as expected”. This records a human pass
for the labwc window-behavior portion of T6b. No labwc version or per-operation
capture was supplied; this does not resolve the separate native Open blocker.

The user explicitly has no second monitor and requested that two-monitor tests
remain pending. Physical mixed-scale display movement/rendering/focus/dialog
acceptance is therefore unavailable and remains open, not passed or waived.

## Source qualification execution (2026-09-09)

Approved plan: “Qualify the current source release”. Branch:
`qualify/source-release`. Existing uncommitted Stage 4 documentation reconciliation
is included. No production CLI/API, provider sources/revisions or version changed.
Spark owned the initial performance-test and receiver changes; it hit its usage
limit during integration corrections. Main completed the corrections locally.

Tooling pilot on labwc 0.20.2/wlroots 0.20.2, Qt 6.11.2, native Wayland, OpenGL,
basic render loop, 1000×700 logical window at device scale 1.5 passed independent
4000×8000 green/alpha-128 reception. This debug pilot is not a release measurement.
The first pilot failed empty reception because the owner had no native input serial;
the driver now sends native compositor input before clipboard publication. The
receiver reads after its activation and wtype Return. Its completion timestamp
precedes save/hash work; process RSS includes the full process lifetime. Timing
uses the monotonic Qt clock and updated-canvas synchronization/frameSwapped.

A first fresh-image attempt failed while traversing generated, root-owned build
payloads. `.dockerignore` now excludes build/ and .git/ from source contexts.
The successful retry, clean-checkout, measurement and hosted results follow below.

Initial hosted Build and checks failed on `0b6bc436dce936aa1c59cb1a3b277c60a4d8ab4e`
([PR run](https://github.com/lebedenko/holonight-viewer/actions/runs/34397129680)):
`Document.UnreadableAndSpecialFiles` decoded a chmod-000 fixture because Docker
ran the contributor tests as root. The same unchanged test passes locally as the
ordinary user. CI now runs the build/check container with the runner's UID/GID,
so the mandatory unreadable-file regression exercises real permission denial.
The test and production handling remain unchanged. Licensing passed on this head
([run](https://github.com/lebedenko/holonight-viewer/actions/runs/34397129674)).

### Clean checkout and installed runtime

Full contributor sequence passed at committed source `0b6bc436dce936aa1c59cb1a3b277c60a4d8ab4e`
in `build/qualification/clean`: task deps, build, build PRESET=release, test (7/7
CTest entries, 22.31 s), format-check, tidy, qml-lint, license-check, desktop-check,
install-check and visual-check. Provider sources were separate clean clones beneath
build/qualification at CI-pinned Config `fe69a59e6b73167fd5349223a4d265d75386c139`
and Qt `22ded7815727ce483fd91e82a9cc04bfe252ec3b`, installed only into the clean
Viewer checkout's build/deps/prefix. All three source trees were clean afterward.
The checkout was then fast-forwarded to `0f137cf5b1efe1046c07f6932b454663a90ffb1a`;
that change touches only CI UID/GID and documentation, not production/test sources.

Logs: `build/qualification/clean-checks.log` and
`build/qualification/clean/build/qualification-logs/`. The visual script passed
42 test executions across dark/light × 1/1.25/1.5. Inspected captures include normal
canvas/button, minimum-size menu/Help/Close and normal Information in all six
combinations: visible outlines, bounded dialogs, scrollable minimum-size text and
unchanged image geometry. These offscreen captures do not pass deferred physical
mixed-monitor movement. Captures: `build/qualification/clean/build/visual/`.

Fresh CI image built with `docker build --pull --no-cache -t viewer-ci -f
packaging/Dockerfile.ci .` (image `c3ae167bbd65`). Its fresh installed-runtime image
used `build/qualification/clean/build/runtime-check.1gLZuX`, followed by
`docker run --rm --network none viewer-runtime-check` with **no mounts**. Pass:
empty startup, PNG/JPEG/BMP/WebP decode and CLI recovery, four-format GIO installed
launcher, installed entry/icon, and no development runtime paths/overrides. Logs:
`build/qualification/{ci-image,runtime-image,runtime}.log`. This is a local isolated
runtime result; hosted final-head validation remains separately required.

### Native five-trial comparison

Candidate `0f137cf5b1efe1046c07f6932b454663a90ffb1a` versus production baseline
`de7ef473eaca2e5d4e2de0b0084dbc691412c473`. Both use identical performance-test and
receiver source (SHA-256 recorded), Release builds and the same separately installed
CI-pinned providers. Baseline adds only test-target wiring and this instrumentation;
its production sources are unchanged. No builds or competing benchmarks ran during
the five baseline trials followed by five candidate trials. Every trial used fresh
Viewer and receiver processes, the production Main QML window, and the same fixtures,
renderer, compositor, size and scale.

Environment: Arch Linux, kernel 7.2.4-arch1-2, Intel i9-9900K (16 logical CPUs),
labwc 0.20.2/wlroots 0.20.2, Qt Base 6.11.2-3, Declarative/Wayland/Image Formats
6.11.2-1, Mesa 26.2.2-1, libwebp 1.6.0-2, wtype 0.4-2. Native wayland-0,
DISPLAY unset, HoloNight platform theme, OpenGL (actual renderer API 3), basic
render loop, 1000×700 logical window, effective device scale 1.5 in every trial.
QT_SCALE_FACTOR=1 retains that compositor scale; it does not mean a physical scale
of 1. Provider QML/library paths point only at the same clean installed prefix.

The existing independent red/green RGBA(255,0,0,128)/(0,255,0,128) 8000×4000 PNGs
were copied unchanged into `build/qualification/fixtures`, excluding unrelated
historical received files from folder prefetch. SHA-256:
`5275d874e1c0a6f63d0b7631b555324f84ed14a7c074b145ceedea8960f23681` (1.png),
`6af74058a17d0a7b0b40b76e28e0d3bdfb24c8b49e137e7f2c653ffee3de9428` (2.png).
Independent GdkPixbuf validation compares every received pixel to specified green
alpha-128 and 4000×8000 quarter-turn dimensions. These uniform fixtures establish
quarter-turn dimensions/color/alpha; they do not independently distinguish mirror
handedness. The earlier asymmetric eight-orientation native matrix remains the
handedness evidence. Source bytes remain unchanged.

**Candidate output: 5/5 pass. Baseline output: 0/5 pass** — all baseline receptions
have alpha 255 instead of 128, though dimensions and green color match. Baseline
quicker copy numbers are measurements of incorrect output, not equivalent successful
transfers. All ten workflows/receivers completed without hangs or crashes.

| Measurement | Baseline median [min–max] | Candidate median [min–max] |
| --- | --- | --- |
| Open to updated Qt frame (ms) | 151 [141–152] | 163 [162–175] |
| Navigation to updated Qt frame (ms) | 211 [210–221] | 216 [215–227] |
| Eight rendered transforms (ms) | 144 [129–155] | 127 [108–132] |
| Copy preparation/publication (ms) | 38 [37–38] | 967 [959–980] |
| Copy initiation to receiver completion (ms) | 432 [404–450] | 1359 [1333–1372] |
| Receiver read/conversion only (ms) | 243 [239–257] | 242 [238–250] |
| Maximum observed GUI timer gap (ms) | 131 [128–149] | 82 [63–105] |
| Viewer process peak RSS (KiB) | 918736 [917732–918924] | 709136 [708592–709828] |
| Receiver process peak RSS (KiB) | 604352 [604316–604572] | 508812 [508548–509040] |

All five values below are in trial order; raw per-transform timings, GUI tick counts,
window metadata, XML/logs, captured window and received PNG/pixel results are retained
under `build/qualification/native-{baseline,candidate}/run-{1..5}`. Summary/environment
JSON and source/environment fingerprints live alongside those directories. Runner:
`build/qualification/run-native.sh`; build recipe: `build/qualification/build-performance.sh`.

| Measurement | Baseline trials 1–5 | Candidate trials 1–5 |
| --- | --- | --- |
| Open to updated Qt frame (ms) | 141, 152, 141, 152, 151 | 165, 163, 175, 162, 163 |
| Navigation to updated Qt frame (ms) | 211, 210, 211, 221, 211 | 215, 227, 227, 216, 216 |
| Eight rendered transforms (ms) | 152, 129, 144, 155, 137 | 128, 132, 122, 108, 127 |
| Copy preparation/publication (ms) | 37, 38, 38, 38, 38 | 959, 979, 959, 967, 980 |
| Copy initiation to receiver completion (ms) | 424, 432, 404, 450, 438 | 1333, 1359, 1342, 1369, 1372 |
| Receiver read/conversion only (ms) | 243, 243, 239, 257, 246 | 238, 242, 240, 249, 250 |
| Maximum observed GUI timer gap (ms) | 128, 131, 130, 149, 133 | 78, 82, 105, 63, 82 |
| Viewer process peak RSS (KiB) | 917732, 918840, 918720, 918736, 918924 | 708660, 709828, 708592, 709136, 709388 |
| Receiver process peak RSS (KiB) | 604316, 604352, 604328, 604552, 604572 | 508852, 508812, 508548, 509040, 508800 |

Boundaries and tradeoffs:

- Open/navigation start at the document command and finish at frameSwapped after
  synchronization sees the updated image. Each transform likewise completes an
  updated frame; the aggregate covers eight separate operations, not only command
  dispatch. Qt frame completion is not physical display latency. Results are this
  warmed filesystem/session and fixture corpus, not cold-disk or universal timings.
- Copy preparation includes worker preparation and publication; the completion poll
  has roughly 10 ms granularity. End-to-end copy includes launching the receiver,
  native compositor activation and its generic QClipboard image read/conversion.
  A shared monotonic timestamp ends the measurement before PNG save/hash work.
  The owner receives a native input serial first; the receiver reads on wtype Return
  after confirmed activation. This is compositor automation, not human input timing.
- GUI gaps use a 1 ms requested timer under QTest event-loop polling. The interval
  includes rendering, the pre-copy capture, publication and transfer through observed
  receiver completion; it is not a hard responsiveness guarantee or an attribution
  of each gap to clipboard code. Candidate gaps range 63–105 ms versus 128–149 ms.
- RSS is Linux wait4/ru_maxrss for each separate child lifetime, including screenshot
  capture and receiver save/hash validation. These are kernel resident high-water
  values, not periodic RSS samples or exact allocation/GPU peaks. They cannot be
  added into a simultaneous combined peak. Candidate medians are 692.5 MiB Viewer
  and 496.9 MiB receiver versus 897.2/590.2 MiB baseline; no universal memory bound
  or statistically significant improvement is claimed from five trials.
- Correct PNG publication costs a median 967 ms versus baseline's 38 ms preparation,
  and 1359 ms versus 432 ms through reception. The baseline loses alpha in every
  run. Candidate open/navigation medians are 12/5 ms higher and transform aggregate
  17 ms lower; no numeric pass threshold or significance claim is invented.
- Clipboard persistence-service behavior and memory/transport costs remain deferred,
  not zero. No persistence service was launched. Existing documented decode/cache
  limits remain unchanged; rendering/encoding/platform transport add temporary memory.

Measured tradeoffs were presented to the user for explicit review. Performance
acceptance remains pending until the user responds; successful measurements alone
do not close R12/Q3. Final hosted PR-head CI remains a separate gate.

### Hosted CI after the permission fix

Both workflows passed on candidate `0f137cf5b1efe1046c07f6932b454663a90ffb1a`:
[Build and checks](https://github.com/lebedenko/holonight-viewer/actions/runs/34401736186)
and [Licensing](https://github.com/lebedenko/holonight-viewer/actions/runs/34401736183).
Build and checks includes its freshly built CI image, all contributor checks and
the installed-only runtime container; every step passed. The first failure remains
recorded above and the unreadable-file regression is unchanged.

[Draft PR #1](https://github.com/lebedenko/holonight-viewer/pull/1) remains unmerged.
The final evidence-only commit must also receive both hosted workflows. Its exact
head SHA, run URLs and outcomes are recorded in the PR description after those
runs finish, avoiding a self-referential commit claiming its own future CI result.
No native rerun is required for unchanged production/tooling sources.
