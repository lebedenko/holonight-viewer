# Stage 5 verification

Status: **not release-ready; acceptance remains blocked**. This iteration does
not publish, tag, bump a version, package a distribution or change provider sources.
The supplied implementation plan approved SPEC, DESIGN and TASKS scope under
CONTRIBUTING.md. Generated logs, fixtures, screenshots and benchmark checkouts are
under `build/` and are not source-release contents.

Current fixes follow-up: compact WebP and the native generic Qt clipboard alpha
matrix now pass. Earlier failures below are retained as historical evidence and
are superseded by [the fixes results](#viewer-fixes-follow-up-2026-09-08).
The [HoloNight dialog delegation handoff](../../holonight-qt-dlg-delegation-missed.md)
is an explicit unresolved provider blocker.

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
