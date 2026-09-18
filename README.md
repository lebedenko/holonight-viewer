# HoloNight Viewer

A standalone, keyboard-first static-image viewer for native Wayland, built on the
HoloNight theme and controls ([holonight-config](https://github.com/lebedenko/holonight-config),
[holonight-qt](https://github.com/lebedenko/holonight-qt)). X11 and XWayland are
outside the supported scope.

Open one local image, inspect it with fit, actual size, zoom, pan and temporary
transforms, then browse the supported images in its folder. Images decode in the
background, honor embedded orientation, and fit the window. Original files are
opened read-only and never changed.

## Requirements

- **Build:** C++23, Qt 6.11+ (including Qt DBus and the Qt GUI private module),
  CMake 3.25+, Ninja, Task, tomlplusplus, pkg-config, libwebp, libexif,
  wayland-client, wayland-protocols, wayland-scanner, and installed
  HolonightQt::Core / HolonightQt::Controls.
- **Tests:** Qt Test, GTest and `dbus-run-session`.
- **Checks:** clang-format, clang-tidy (run-clang-tidy), REUSE, desktop-file-utils,
  GIO and Python 3.
- **Codecs:** Qt Base's PNG/BMP support and JPEG plugin, plus Qt Image Formats' WebP
  plugin (Arch: `qt6-base qt6-imageformats`). See
  [Supported formats](#supported-formats-and-limits).

On Arch, the [CI Dockerfile](packaging/Dockerfile.ci) lists all packages.

## Quick start

```sh
task deps                 # build sibling providers locally, without source changes
task build
task run                  # Ctrl+O opens an image; F toggles fullscreen, Q quits
task test
task check                # sequential builds, tests, formatting, lint, licenses, staged install
task --list               # individual checks and build commands
```

`task deps` expects the providers in `../holonight-config` and `../holonight-qt`; see
[Providers and presets](#providers-and-presets) to change that.

## Usage

### Opening images

Open one local image with **Open…**, **Ctrl+O**, a file drop, or a command-line path.
Open… and Ctrl+O use the desktop's xdg-desktop-portal file picker, attached to the
Viewer window. Viewer calls the portal FileChooser itself, so Open does not depend on
the platform theme; Qt's built-in dialog appears only when no portal can serve the
request. Escape, closing the picker, or its Cancel button cancels and restores the
previously focused control if it is still available; the current view is preserved.

On the command line, pass one local path (quote spaces) or a local file URL, and use
`--` before filenames beginning with a dash. Remote URLs and multiple files are
rejected. Existing local paths are resolved before URL parsing, so `frame:1.png`
opens when present; a missing `frame:1.png` is rejected as a URL, while
`./frame:1.png` reports the normal missing-file error. `--help` and `--version` are
supported.

```sh
task run -- '/path/to/photo.jpg'
# Direct launch against the local providers from task deps:
QML_IMPORT_PATH=build/deps/prefix/lib/qt6/qml LD_LIBRARY_PATH=build/deps/prefix/lib \
  build/debug/apps/viewer/hn-viewer -- './photo with spaces.png'
```

Missing, corrupt and unreadable images produce a recoverable error in the window,
reported once per attempt (each retry or refresh reports again); Open remains
available. With no image open, the canvas shows a subtle, theme-tinted Viewer icon
with "No image open" and "Ctrl+O to open · or drop an image here". The icon shrinks,
then hides, on short windows while the text keeps its size, and it disappears once
an image opens, including a transparent one.

### Keyboard and mouse

| Control | Action |
| --- | --- |
| `[` / Previous, `]` / Next | Browse siblings, stopping at folder boundaries |
| `Ctrl+O` / Open… | Choose an image with the desktop file picker |
| `Ctrl+R` / Refresh | Rescan the folder and reload the selected image, clearing the cache |
| `Ctrl+0` / Fit | Center the whole image and fit it as the window changes |
| `1` / Actual Size | Center at one source pixel per physical display pixel |
| `Ctrl++` or `Ctrl+=` / `Ctrl+-` | Zoom in/out around the canvas center |
| Vertical wheel or trackpad scroll over the canvas | Zoom around the pointer |
| Left-button drag | Pan the image |
| Arrow keys | Pan the viewed region from anywhere in the window |
| `R` / `Shift+R` | Rotate clockwise / counterclockwise by 90° |
| `X` / `Shift+X` | Flip horizontally / vertically relative to the displayed image |
| Menu → Reset Transform | Clear temporary rotation and flips |
| `Ctrl+C` | Copy the entire transformed image, including transparency |
| `Ctrl+Shift+C` | Copy the normalized absolute path, unquoted; retain symlink paths |
| `I` | Open or close the Image Information card |
| `?` | Open or close the Shortcut Help card |
| `F` / Fullscreen | Toggle fullscreen; `Escape` leaves it |
| `Q` / Quit | Quit Viewer |

Viewer shortcuts pause while Open, Image Information or Shortcut Help is active.
Escape closes a card or dialog before leaving fullscreen; closing clears the focus
ring. Fullscreen restores the previous normal/maximized state, including
compositor-managed tiling. Native decorations belong to Qt and the compositor.

Tab and Shift+Tab cycle through the enabled header buttons (Information, Fullscreen,
Menu); the empty window skips Information. Image actions clear the focus ring. In the
menu, J/K and Down/Up move between items, skipping separators; single-key shortcuts
still act while the menu is open and close it. The menu groups Open/Refresh,
Previous/Next, view controls, transforms, copying, Image Information/Shortcut Help and
Fullscreen/Quit, with each item's shortcut shown right-aligned.

### Zoom, pan and transforms

100% means physical pixels, even at fractional display scaling. Zoom normally ranges
from 1% to 3200%, extending to include Fit for unusually small or large images.
Manual zoom is preserved across resize, fullscreen and display-scale changes.
Panning stops at image edges and centers axes that fit. Opening another image resets
to Fit.

Transforms compose in invocation order and reset zoom/pan to Fit. Open, navigation
and Ctrl+R clear them; returning to a file does not restore them.

### Overlays

The header shows the centered filename and scalable, theme-tinted icons; a wrapping
shortcut footer stays visible below the canvas.

- **Navigation arrows** appear on mouse movement for two seconds (held while the
  pointer rests on an arrow). Any keyboard input hides them immediately.
- **Details strip** appears for three seconds after a new or refreshed image first
  renders and after zoom, Fit, Actual Size, rotate, flip or mouse movement; resizing
  does not reveal it. It shows transformed dimensions, decimal file size,
  physical-pixel scale (including Fit) and folder position, wrapping at narrow widths
  without resizing the canvas.

Both fade in and out.

### Image Information and Shortcut Help

**Image Information** (`I`) is a card over a lightly dimmed image. Its fixed header
shows a preview (hidden below 360 px wide), the filename, a
`format · W × H · MP · size` summary, a "Rotated view W × H" line while a quarter turn
swaps the dimensions, and the locale's short modification date and time. Below it
scroll these sections:

- **CAMERA:** camera; lens, focal length and aperture; shutter and ISO
- **LOCATION:** coordinates and altitude
- **FILE:** the path with the home directory shown as `~`, selectable and copyable

Missing facts and empty sections are omitted, and "Details unavailable" replaces an
empty summary. EXIF is read from JPEG, PNG and WebP files with libexif; damaged
metadata is omitted. Facts follow cached image snapshots and the open card follows
document changes; after closing it, `Ctrl+R` also refreshes the cached facts.

**Shortcut Help** (`?`) is a matching card with a fixed "Shortcuts" header and
scrolling NAVIGATION, VIEW, TRANSFORM, IMAGE, APPLICATION and MOUSE sections. Long
key hints wrap within their column at larger text sizes.

Close either card with its key, Escape, its × button or a click outside.

### Folder browsing

Opening starts a nonrecursive folder scan without delaying the image; the position
indicator shows scanning feedback until it finishes. Supported suffixes follow
installed Qt handlers, case-insensitively. Hidden siblings are excluded, but the
explicitly opened file stays included even if hidden, extensionless or missing.
Filenames use natural order (`image2` before `image10`), case-insensitive text and
original-name tie breaking. Symlink paths retain the folder you opened.

Navigation remains available while decoding or showing an image error. Broken files
keep their positions, and Ctrl+R retains a deleted selection so you can navigate
away. Folder-read failures appear separately and preserve single-image viewing.
There is no live watcher: press Ctrl+R after additions, renames or external edits.

### Copying

**Copy Image** captures the image and orientation when invoked, ignoring zoom and pan,
and publishes it as `image/png`. **Copy Path** copies the absolute path and remains
available during loading and errors. Both commands pause while a copy is being
prepared; browsing and inspection remain available. Feedback names the copied file,
so it is distinguishable from a later selection. A failed copy leaves the clipboard
untouched.

Only the standard clipboard is used; the primary selection is untouched. Clipboard
persistence after exit is managed by the desktop.

## Supported formats and limits

The guaranteed formats are static **PNG, JPEG, BMP and WebP**. Other installed Qt
image handlers work on a best-effort basis, and animated files show their first frame
only. Missing codecs fail qualification, not ordinary startup.

Qt is the primary decoder. A private libwebp fallback handles simple static RIFF
VP8/VP8L files that Qt cannot inspect or decode, including the compact Qt 6.11.2
regression; extended containers, metadata and animation stay on the Qt path.
Installed runtimes need libwebp, libexif and the Qt plugins.

| Limit | Value |
| --- | --- |
| Encoded input | 256 MiB |
| Decoded pixels | 32 million, and 32,768 on either axis |
| Decoded image | 128 MiB |
| Displayed image plus decode cache | 256 MiB |

Oversized images and unsupported dimensions produce an error instead of a
lower-resolution substitute. See [Design notes](#design-notes) for what these bounds
exclude.

## Installation

Version **0.1.0** is a source release with CMake installation; no distribution
packages, portable binaries or bundled providers are supplied. The
[release notes](docs/releases/v0.1.0.md) list tested provider revisions.

### System install

Build the HoloNight providers against the same Qt build used by Viewer, using the
dependency revisions in [CI](.github/workflows/build.yml): their private Qt API use
ties the runtime ABI to that Qt build. Providers must already be installed under
`/usr`, including QML modules under `/usr/lib/qt6/qml`; `task deps` supplies only
development providers.

```sh
task install
```

`task install` configures Release in `build/system-install` with system provider
paths, builds, runs `sudo cmake --install`, then refreshes
`/usr/share/applications`. A failed build stops before installation, and development
build caches remain separate. It installs the executable, desktop entry, icon and
license files. A system installation discovers providers through Qt's normal module
paths; no source-tree imports are embedded in the binary.

The desktop entry appears in application menus, opens one file per process through
`hn-viewer -- %f`, and advertises PNG/JPEG/BMP/WebP. Installing does not change the
default image application.

### Uninstall and reinstall

```sh
task uninstall
task install
```

`task uninstall` uses sudo to remove only `/usr/bin/hn-viewer`, the legacy
`/usr/bin/holonight-viewer`, the desktop entry and icon, and the two license files.
It removes the license directory only if empty, preserves providers and user data,
and needs neither configuration nor an install manifest. Missing files are harmless.
It then refreshes the desktop database if the applications directory exists;
command failures propagate. It always targets `/usr`, ignoring `DESTDIR`, so
custom-prefix installations require manual removal.

### Staged and custom-prefix installs

`DESTDIR` stages the install layout without changing the host:

```sh
DESTDIR=/your/stage cmake --install build/release                   # prefix /usr
DESTDIR="$PWD/build/system-stage" cmake --install build/system-install
DESTDIR="$PWD/build/system-stage" bash scripts/uninstall.sh
```

For a custom prefix, set `CMAKE_INSTALL_PREFIX` and provider paths when configuring,
then expose its `bin` on `PATH`, `share` on `XDG_DATA_DIRS`, and the providers with
`QML_IMPORT_PATH=<prefix>/lib/qt6/qml` and `LD_LIBRARY_PATH=<prefix>/lib` (adapt
`lib` to your platform).

### Upgrading from older checkouts

The executable is now `hn-viewer`, with no compatibility alias; the application
identity and settings paths are unchanged. If an earlier checkout registered a
development entry, remove
`${XDG_DATA_HOME:-$HOME/.local/share}/applications/org.holonight.Viewer.desktop` and
the matching `icons/hicolor/scalable/apps/org.holonight.Viewer.svg`, then run
`update-desktop-database` on that applications directory. Otherwise the old user
entry shadows the system entry.

## Development

See the [contributor workflow](CONTRIBUTING.md) for conventions.

### Providers and presets

`task deps` builds providers from `../holonight-config` and `../holonight-qt`;
override them with `HOLONIGHT_CONFIG_SOURCE` and `HOLONIGHT_QT_SOURCE`, and set `JOBS`
to control parallelism. Builds and the staging prefix live under `build/deps`.

To use an existing installation, override `HOLONIGHT_DEPENDENCY_PREFIX` and
`HOLONIGHT_QML_IMPORT_PATH` as Task variables. Direct CMake users can set the same
cache variables with `cmake --preset debug -D...`, and `CMAKE_PREFIX_PATH` can
supply additional packages. The debug, release and test presets default to the local
prefix.

`task run` builds the configured preset and launches it with the checkout's provider
overrides and CLI arguments; use `PRESET=release` for Release. `task run` and
`task test` set the installed QML and library search paths. Neither creates desktop
entries or icons in user directories.

### Checks

`task check` (alias `task verify`) runs, in sequence: debug and release builds, tests,
format checking, C++/QML lint, license checking, staged installation verification
and isolated uninstall checks. Each step is also available on its own: `test`,
`format-check`, `tidy`, `qml-lint`, `lint`, `license-check`, `install-check` and
`uninstall-check`. `task format` applies C++ and QML formatting.

Run `viewer-smoke` through ctest: it starts the tests on a private D-Bus session with
a mock portal. Run directly on a desktop session, it exits with an explanation
instead of reaching the real portal.

CMake `format` and `format-check` cover application C++ headers and application/test
translation units; Task also formats and checks QML. QML formatting uses
`QMLFORMAT` when set (an executable path or command name, without arguments), then
`qmlformat`/`qmlformat-qt6` on `PATH`, `qtpaths6` installation directories, and
`/usr/lib/qt6/bin/qmlformat`. An invalid explicit override fails instead of falling
back. Temporary check output stays under `build/`.

```sh
QMLFORMAT="/opt/Qt 6/bin/qmlformat" task format-check
```

`task clean` removes only `build/debug`, `build/release`, `build/test` and
`build/system-install`; local provider installs and verification artifacts stay
under `build/`.

### Screenshots and visual checks

`task screenshot` starts Viewer through `task run`, waits for its window, captures it
to `build/screenshot.png`, and closes that instance. `--delay N` waits N seconds after
the window appears (to open a menu or show a focus ring), and `--margin N` adds N
pixels on every side. It currently requires a Hyprland session, `hyprctl` and `grim`.

```sh
task screenshot -- --delay 5 --margin 16
```

`task visual-check` captures dark/light renders at normal and fractional scale under
`build/visual` for manual inspection.

### CI and Docker reproduction

CI runs `task deps` and `task check` with the required codecs and contributor tools,
stages Viewer and providers under `/usr`, then launches a second container with the
installed payloads only: no workspace mount, no network and no development runtime
overrides. To reproduce with Docker and the provider checkouts used by CI:

```sh
docker build -t viewer-ci -f packaging/Dockerfile.ci .
# First run CI's Build and verify step in the parent checkout layout:
# task deps, task check, then task runtime-context, which writes
# build/runtime-check-context and the disposable context under build/.
docker build -t viewer-runtime-check "$(cat build/runtime-check-context)"
docker run --rm --network none viewer-runtime-check
```

### Native clipboard qualification

For real native clipboard checks, start the probe on the same native platform as the
receiver under test:

```sh
build/test/tests/clipboard-probe --interactive image build/received.png
build/test/tests/clipboard-probe --interactive text build/received.txt
```

Copy through Viewer's keyboard or menu, focus the probe, then click or press
Enter/Ctrl+V. It reports image dimensions, the first pixel and a canonical RGBA
digest, or the exact path text, saves the received data, and stays open for repeated
activation. Automatic mode serves regression tests and does not establish native
input acceptance.

Qualification keeps the generic `QClipboard::image()` receiver mandatory; explicit
PNG reception is a separate diagnostic. Follow the native matrix, including Orca, in
the [release verification](docs/sdd/release-readiness/VERIFICATION.md).

### Native performance measurement

Opt-in tooling measures the production QML window against a separate interactive
generic Qt receiver on labwc. Build the test target in Release mode, configure the
installed provider paths and the red/green 8000×4000 alpha fixtures, then run:

```sh
python3 scripts/measure-native-release.py build/performance/tests/viewer-smoke \
  build/performance/tests/clipboard-probe build/release-readiness/fixes-native-lifecycle \
  build/qualification/native-candidate --revision COMMIT_SHA
```

Run both revisions sequentially with identical instrumentation and environment; the
default is five fresh processes. `wtype` activates reception through native
compositor input, so let the temporary windows keep focus. Artifacts stay under
`build/`.

- Frame timing ends at Qt frame completion, not physical display.
- Transfer includes receiver startup/activation and ends after read/conversion,
  before save/hash validation; separate process RSS includes that validation.
- `--allow-incorrect-baseline` records a historical baseline; incorrect pixels still
  fail.

Performance acceptance requires explicit user review of the measured tradeoffs.

## Design notes

- **Decoding:** one active decoder plus only the newest pending foreground request,
  which takes priority over queued prefetch. Only the next neighbor in the latest
  direction is prefetched, initially forward. A separate scan worker likewise keeps
  one active and one newest pending scan.
- **Cache:** the decoded LRU holds at most two entries and 128 MiB, excluding the
  displayed image; display plus cache stays within 256 MiB. Hits are checked on the
  worker by absolute path, file size and modification time, so edits that preserve
  both need Ctrl+R.
- **Memory beyond the bounds:** decoding/conversion and canvas textures add temporary
  memory, and codec-private allocations are not covered, so the limits are not a
  whole-process guarantee. Zoom, pan and transforms reuse the decoded image and a
  canvas-sized surface without allocating another full-size image.
- **Clipboard memory** is outside the 256 MiB bound. A copy first shares its decoded
  snapshot but may retain another image (up to 128 MiB) after navigation; a
  transformed output can add another 128 MiB, and PNG encoding needs temporary
  storage plus its payload. Qt, transport, receivers and clipboard managers may
  retain more. A dedicated worker transforms and encodes one copy at a time, with no
  queue, and publishes explicit `image/png` without Qt's private image representation.
- **Shutdown** keeps the event loop responsive and waits for the active read to
  finish; a hung codec or filesystem has no hard exit deadline.
- **Measurements** (local, machine-specific): PNG copy of the 32-million-pixel fixture
  takes about one second with continuing GUI timer progress. A 20,000-entry folder
  scanned in 625 ms, and two 6000×4000 navigations took 363 ms with 241,660 KiB peak
  RSS, excluding window rendering. Method and limits are in the verification records.

## Release status

Stage 5 **source-release acceptance with CMake install** was recorded on 2026-09-10,
including the user's explicit acceptance of measured performance. Native
rendering/clipboard measurements, clean-checkout and installed-runtime validation,
and hosted CI passed; see [release qualification](docs/sdd/release-readiness/VERIFICATION.md)
and [PR #1](https://github.com/lebedenko/holonight-viewer/pull/1) for CI results on
the final acceptance commit. For 0.1.0, native Open/provider-dialog acceptance,
physical mixed-monitor and mixed-scale movement, and clipboard persistence-service
behavior and costs are explicitly deferred to the next release, not passed.

Since 0.1.0, unreleased source opens files through the portal picker; its Hyprland
acceptance is recorded in release qualification, and the Orca walkthrough is still
open. The default HoloNight theme's missing dialog delegation remains a
[next-release provider issue](docs/holonight-qt-dlg-delegation-missed.md) for other
applications.

See the [publication record](docs/sdd/source-publication/VERIFICATION.md) for
validation and the
[GitHub release page](https://github.com/lebedenko/holonight-viewer/releases/tag/v0.1.0)
for source and checksum assets.

## Documentation

- [Project brief](docs/PROJECT_BRIEF.md) and [backlog](docs/BACKLOG.md)
- Verification records:
  [project scaffold](docs/sdd/project-scaffold/VERIFICATION.md),
  [image opening](docs/sdd/image-document/VERIFICATION.md),
  [image inspection](docs/sdd/image-inspection/VERIFICATION.md),
  [folder browsing](docs/sdd/folder-browsing/VERIFICATION.md),
  [static-image workflow](docs/sdd/static-image-workflow/VERIFICATION.md),
  [empty-window decoration](docs/sdd/empty-window-decoration/VERIFICATION.md),
  [UI polish](docs/sdd/viewer-ui-polish/VERIFICATION.md),
  [release readiness](docs/sdd/release-readiness/VERIFICATION.md)
- [Review-fixes cycle](docs/sdd/review-fixes/SPEC.md)

## License

GPL-3.0-or-later; see [LICENSE](LICENSE).
