# HoloNight Viewer

A standalone, keyboard-first HoloNight static-image viewer for native Wayland.
X11 and XWayland are outside the supported scope. Open one local image
with **Open…**, **Ctrl+O**, a file drop, or a command-line path. Images decode in
the background, honor embedded orientation, and fit the window. PNG, JPEG, BMP and WebP are
the release target; additional formats depend on installed Qt image plugins. Animated files
show their first frame only. Inspect with fit, actual size, zoom and pan, then
browse supported images in the containing folder.

Requires C++23, Qt 6.11+, CMake 3.25+, Ninja, Task, tomlplusplus, pkg-config, libwebp, and installed
HolonightQt::Core / HolonightQt::Controls. Tests use Qt Test and GTest. Checks need
clang-format, clang-tidy (run-clang-tidy), REUSE, desktop-file-utils, GIO and Python 3.
On Arch, the [CI Dockerfile](packaging/Dockerfile.ci) lists the packages.

```sh
task deps                 # builds sibling providers locally, without source changes
task build
task run
# Ctrl+O opens an image; f toggles fullscreen, Escape leaves fullscreen, q quits
task test
task build PRESET=release
task format-check
task tidy
task qml-lint
task license-check
task install-check
task desktop-check        # isolated development/packaged registration checks
task visual-check         # captures under build/visual for inspection
```

`--help` and `--version` are supported. Pass one local path (quote spaces), or a
local file URL. Use `--` before filenames beginning with a dash. Remote URLs and
multiple files are rejected. Missing, corrupt and unreadable images produce a
recoverable error in the window; Open remains available.

```sh
task run -- '/path/to/photo.jpg'
# Direct launch with the documented provider environment:
build/debug/apps/viewer/holonight-viewer -- './photo with spaces.png'
```

Native decorations belong to Qt/the compositor. Fullscreen restores the previous
normal/maximized state, including compositor-managed tiling. Viewer shortcuts pause while Open, Image Information or Shortcut Help is active.

| Image control | Action |
| --- | --- |
| `Page Up` / Previous, `Page Down` / Next | Browse siblings, stopping at folder boundaries |
| `F5` | Rescan the folder and reload the selected image, clearing the cache |
| `0` / Fit | Center the whole image and fit it as the window changes |
| `1` / Actual Size | Center at one source pixel per physical display pixel |
| `+` or `=` / `−` | Zoom in/out around the canvas center |
| Vertical wheel or trackpad scroll over the canvas | Zoom around the pointer |
| Left-button drag | Pan the image |
| Arrow keys with the canvas focused | Pan the viewed region in that direction |
| `R` / `Shift+R` | Rotate clockwise / counterclockwise by 90° |
| `H` / `V` | Flip horizontally / vertically relative to the displayed image |
| Actions → Reset Transform | Clear temporary rotation and flips |
| `Ctrl+C` | Copy the entire transformed image, including transparency |
| `Ctrl+Shift+C` | Copy the normalized absolute path, unquoted; retain symlink paths |
| `I` | Open selectable Image Information |
| `F1` | Open scrollable shortcut and gesture help |

Navigation, Refresh, transforms, Zoom, Fit and Actual Size focus the canvas so arrow-key panning works immediately.
Tab reaches the canvas and buttons; clicking the canvas also focuses it. Manual
zoom preserves magnification during resize/fullscreen and display-scale changes.
100% means physical pixels even at fractional display scaling. Zoom normally
ranges from 1% to 3200%, extending to include Fit for unusually small/large images.
Panning stops at image edges and centers axes that fit. Opening another image
resets to Fit; canceling Open preserves the view. Image controls pause while the
dialog is open. Zoom/pan reuse the decoded image and a canvas-sized rendering
surface without changing the original file.

The header uses scalable, theme-tinted SVG icons; its Actions popup keeps a
visible inset from the window edges. The menu provides Open, view controls, navigation, temporary
transforms, copying, information, help and quit. The centered filename and wrapping
shortcut footer remain visible. Mouse movement reveals side navigation arrows for
five seconds; any keyboard input hides them immediately. An independent five-second
details strip appears after a new or refreshed image first renders. Mouse movement
restarts its timeout; ordinary repaints and keyboard inspection do not. The strip
shows transformed dimensions, decimal file size, physical-pixel scale (including
Fit), and folder position as separate labels with themed separators, wrapping at
narrow widths without resizing the canvas. Viewer-owned controls retain
keyboard focus and activation with visible keyboard-focus indicators. Window
activation alone does not change Viewer-owned styling. Typography
inherits the shared theme’s point-based font sizes.

Transforms compose in invocation order, reset zoom/pan to Fit, and do
not allocate another full-size image while viewing. Open, navigation and F5 clear
them; returning to a file does not restore them. Original files remain unchanged.
Information shows worker-collected format, encoded size, local modification time,
decoded dimensions after embedded orientation, and current transformed dimensions.
Facts follow cached image snapshots; F5 refreshes them. The path and Copy Path
remain available during loading/errors; unknown facts say “Unavailable.”
Information and Help support scrolling and text selection/copying. Escape closes
the dialog before leaving fullscreen; closing restores canvas focus.

Copy Image captures the image and orientation when invoked, ignoring zoom/pan.
A dedicated worker transforms and PNG-encodes one copy at a time with no queue.
The GUI publishes explicit image/png without Qt’s private image representation. Both copy commands
pause during preparation; browsing and inspection remain available. Filename-specific
feedback distinguishes the captured image from a later selection. Preparation or encoding
failure preserves the clipboard. The standard clipboard is used; primary selection
is untouched. Clipboard persistence after exit is managed by the desktop.

The 256 MiB display/cache bound excludes clipboard memory. A copy initially shares
its decoded snapshot, but after navigation may retain another image (up to 128 MiB).
A transformed output can add another 128 MiB; Qt, transport, receivers and clipboard
managers may retain additional storage. PNG encoding also needs temporary storage and its encoded payload. Updated five-run
measurements and native acceptance are recorded in the
[release verification](docs/sdd/release-readiness/VERIFICATION.md). On the local
32-million-pixel fixture, PNG copy preparation/publication takes about one second
with continuing GUI timer progress; native generic Qt alpha and compositor-input
navigation/shutdown checks pass. Earlier copy
measurements predate PNG publication and do not describe its latency.

Opening starts a nonrecursive folder scan without delaying the image. Supported
suffixes follow installed Qt handlers, case-insensitively. Hidden siblings are
excluded; the explicitly opened file stays included even if hidden, extensionless
or missing. Filenames use natural order (`image2` before `image10`), case-insensitive
text and original-name tie breaking. Symlink paths retain the folder you opened.
The position indicator shows scanning feedback until the snapshot is ready.
Navigation remains available while decoding or showing an image error. Broken
files keep their positions; F5 retains a deleted selection so you can navigate
away. Folder-read failures appear separately and preserve single-image viewing.
There is no live watcher: use F5 after additions, renames or external edits.
A local 20,000-entry exercise measured a 625 ms scan and 363 ms for two 6000×4000
image navigations, with GUI timer progress and 241,660 KiB peak RSS. These are
machine-specific decoder/cache measurements, excluding window rendering; see the
folder-browsing verification record for the method and limits.

Viewing limits are 256 MiB encoded input, 32 million pixels, 32,768 pixels on either
axis, and 128 MiB per decoded image. Unsupported dimensions and oversized images
produce an error instead of a lower-resolution substitute. Decoding/conversion and
canvas textures add temporary memory beyond that per-image bound; codec-private
allocations are not a whole-process memory guarantee. The decoded LRU holds at most two entries and 128 MiB, excluding the displayed
image; retained display plus cache is at most 256 MiB. Cache hits are checked on
the worker by absolute path, file size and modification time; edits preserving
both metadata fields require F5. Only the next neighbor in the latest direction
is prefetched, initially forward. There is one active decoder and only the newest
pending foreground request, which takes priority over queued prefetch. A separate
scan worker likewise retains one active and one newest pending scan. Closing keeps the event loop responsive and
waits for the active read to finish; a hung codec/filesystem has no hard exit deadline.
Original image files are opened read-only. Desktop MIME availability is declared
without changing the default image application.

Provider defaults are ../holonight-config and ../holonight-qt. Override their
locations with HOLONIGHT_CONFIG_SOURCE and HOLONIGHT_QT_SOURCE for `task deps`.
Builds and the staging prefix live under build/deps. Set JOBS to control provider
parallelism. Override HOLONIGHT_DEPENDENCY_PREFIX and HOLONIGHT_QML_IMPORT_PATH as
Task variables for an existing installation. Direct CMake users can set those
cache variables with `cmake --preset debug -D...`; CMAKE_PREFIX_PATH can supply
additional installed packages. The debug/release/test presets default to the
local prefix. Task run/test set the installed QML and library search paths.

Install with `DESTDIR=/your/stage cmake --install build/release` (prefix /usr).
The application package installs its executable, desktop entry, icon, and license;
it depends on separately installed HoloNight provider libraries and QML modules.
A system installation discovers providers via Qt's normal module paths. A custom
provider prefix needs QML_IMPORT_PATH=<prefix>/lib/qt6/qml and
LD_LIBRARY_PATH=<prefix>/lib. No source-tree imports are embedded in the binary.

See [contributor workflow](CONTRIBUTING.md), [project brief](docs/PROJECT_BRIEF.md),
[backlog](docs/BACKLOG.md), and [scaffold verification](docs/sdd/project-scaffold/VERIFICATION.md), and
[image-opening verification](docs/sdd/image-document/VERIFICATION.md), and
[image-inspection verification](docs/sdd/image-inspection/VERIFICATION.md), and
[folder-browsing verification](docs/sdd/folder-browsing/VERIFICATION.md), and
[static-workflow verification](docs/sdd/static-image-workflow/VERIFICATION.md).
Licensed GPL-3.0-or-later; see LICENSE.

`task run` registers the selected development build's desktop entry and icon under
`${XDG_DATA_HOME:-~/.local/share}` before launch, so the host portal can resolve
`org.holonight.Viewer`. Use `task desktop-install` to register without opening a
window. The entry launches the absolute build executable with its provider paths;
rerun the task after moving the checkout or switching builds. This user entry takes
precedence over a system installation; remove its applications/org.holonight.Viewer.desktop
file when switching to a system package.

Stage 5 prepares a **source release with CMake install**; it is not release-ready.
See [release qualification](docs/sdd/release-readiness/VERIFICATION.md) for remaining
explicit user performance review and final evidence-head hosted CI status.
Native rendering/clipboard measurements and clean-checkout/installed-runtime
validation have passed; candidate hosted CI succeeded. Native Open/provider-dialog acceptance, physical mixed-monitor
and mixed-scale movement, and clipboard persistence-service behavior/costs are
explicitly deferred to the next release, not passed. No release
has been published and no distribution packages or bundled providers are supplied.

The guaranteed target formats are static **PNG, JPEG, BMP and WebP**. Install Qt
Base's PNG/BMP support and JPEG plugin, plus Qt Image Formats' WebP plugin (Arch:
`qt6-base qt6-imageformats`). Other installed handlers remain best effort. All
animated formats display the first frame only. Missing codecs fail qualification,
not ordinary startup. Qt remains the primary decoder. A private libwebp fallback
handles simple static RIFF VP8/VP8L files that Qt cannot inspect or decode,
including the compact Qt 6.11.2 regression. Extended containers, metadata and
animation stay on the Qt path. Source builds require pkg-config and libwebp
(Arch: `pkgconf libwebp`); installed runtimes need libwebp and the Qt plugins.

For a system installation, build the separately installed HoloNight providers
against the same Qt build used by Viewer. Use the dependency revisions in
[CI](.github/workflows/build.yml); provider private Qt API use ties runtime ABI to
that Qt build. Configure Viewer against the provider installation, then install:

```sh
cmake --preset release -DHOLONIGHT_DEPENDENCY_PREFIX=/usr -DHOLONIGHT_QML_IMPORT_PATH=/usr/lib/qt6/qml
cmake --build --preset release
sudo cmake --install build/release
sudo update-desktop-database /usr/share/applications
```

`DESTDIR` stages the same layout without installing on the host. For a custom
prefix, set `CMAKE_INSTALL_PREFIX` and provider paths when configuring; expose its
`bin` on `PATH`, `share` on `XDG_DATA_DIRS`, and provider QML/library paths through
`QML_IMPORT_PATH` and `LD_LIBRARY_PATH` (adapt `lib` to your platform). Custom-prefix
overrides are distinct from the standard-location isolated qualification below.

The desktop entry offers one file per process through `holonight-viewer -- %f`
and advertises PNG/JPEG/BMP/WebP only. Registering availability and refreshing the
desktop database do not select a default image application. Before switching from
`task run` to the installed application, remove the generated development entry
`${XDG_DATA_HOME:-$HOME/.local/share}/applications/org.holonight.Viewer.desktop`
and its matching user icon if no longer needed, then refresh that applications
directory with `update-desktop-database`. The user entry otherwise shadows `/usr`.

CI builds the committed checkout with required codecs and contributor tools, stages
Viewer and providers under `/usr`, then launches a second container with installed
payloads only, no workspace mount, no network and no development runtime overrides.
To reproduce on a machine with Docker and the provider checkouts used by CI:

```sh
docker build -t viewer-ci -f packaging/Dockerfile.ci .
# Run the Build and verify command from CI in the parent checkout layout first.
# It writes build/runtime-check-context and the disposable context under build/.
docker build -t viewer-runtime-check "$(cat build/runtime-check-context)"
docker run --rm --network none viewer-runtime-check
```

For real native clipboard checks, start `build/test/tests/clipboard-probe
--interactive image build/received.png` (or `text build/received.txt`) with the
same native platform as the receiver under test. Copy through Viewer's keyboard
or Actions menu, focus the receiver, then click or press Enter/Ctrl+V. It reports
image dimensions, first pixel and a canonical RGBA digest, or exact path text,
and saves the received data. It remains open for repeated activation. Automatic
mode remains available for regression tests; it does not establish native input
acceptance. Follow the native matrix in the verification record, including Orca.

Native Wayland clipboard qualification retains the generic QClipboard::image()
receiver as mandatory; explicit PNG reception is a separate diagnostic. See the
release verification for current results and remaining human input checks.
Native portal selection/cancellation passed with the per-process
`QT_QPA_PLATFORMTHEME=xdgdesktopportal` comparison. The default HoloNight theme's
missing delegation remains a [next-release provider issue](docs/holonight-qt-dlg-delegation-missed.md).

Opt-in native performance tooling uses the production QML window and a separate
interactive generic Qt receiver on labwc. With the existing red/green 8000×4000
alpha fixtures and installed provider paths configured, run:

```sh
python3 scripts/measure-native-release.py build/performance/tests/viewer-smoke \
  build/performance/tests/clipboard-probe build/release-readiness/fixes-native-lifecycle \
  build/qualification/native-candidate --revision COMMIT_SHA
```

Build the test target in Release mode first. Run both revisions sequentially with
identical instrumentation and environment; the default is five fresh processes.
`wtype` activates reception through native compositor input; let these temporary
windows retain focus during the run. Artifacts remain under `build/`. Frame timing
ends at Qt frame completion, not physical display. Transfer includes receiver
startup/activation and ends after read/conversion, before save/hash validation.
Separate process RSS includes that later validation work. The historical baseline
may be recorded with `--allow-incorrect-baseline`; incorrect pixels remain failures.
Performance acceptance requires explicit user review of measured tradeoffs.
