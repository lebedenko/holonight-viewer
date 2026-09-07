# HoloNight Viewer

A standalone, keyboard-first HoloNight static-image viewer. Open one local image
with **Open…**, **Ctrl+O**, a file drop, or a command-line path. Images decode in
the background, honor embedded orientation, and fit the window. PNG and JPEG are
required; additional formats depend on installed Qt image plugins. Animated files
show their first frame only. Zoom and folder browsing are later roadmap stages.

Requires C++23, Qt 6.11+, CMake 3.25+, Ninja, Task, tomlplusplus, and installed
HolonightQt::Core / HolonightQt::Controls. Tests use Qt Test and GTest. Checks need
clang-format, clang-tidy (run-clang-tidy), REUSE, and desktop-file-utils.
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
normal/maximized state. Global fullscreen/quit shortcuts pause while Open is active.

Viewing limits are 256 MiB encoded input, 32 million pixels, 32,768 pixels on either
axis, and 128 MiB per decoded image. Unsupported dimensions and oversized images
produce an error instead of a lower-resolution substitute. Decoding/conversion and
canvas textures add temporary memory beyond that per-image bound; codec-private
allocations are not a whole-process memory guarantee. There is one active decoder
and only the newest pending request. Closing keeps the event loop responsive and
waits for the active read to finish; a hung codec/filesystem has no hard exit deadline.
Original image files are opened read-only. Desktop MIME association is deferred to
release readiness.

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
[image-opening verification](docs/sdd/image-document/VERIFICATION.md).
Licensed GPL-3.0-or-later; see LICENSE.

`task run` registers the selected development build's desktop entry and icon under
`${XDG_DATA_HOME:-~/.local/share}` before launch, so the host portal can resolve
`org.holonight.Viewer`. Use `task desktop-install` to register without opening a
window. The entry launches the absolute build executable with its provider paths;
rerun the task after moving the checkout or switching builds. This user entry takes
precedence over a system installation; remove its applications/org.holonight.Viewer.desktop
file when switching to a system package.
