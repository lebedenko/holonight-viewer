# Unreleased

Current source retains version 0.1.0. The 0.1.0 tag and its release notes describe
historical source, executable and qualification; they do not qualify this candidate.

Viewer now launches as `hn-viewer`. It opens local paths and file URLs, accepts file
drops, uses the desktop portal picker, browses folders in natural order, and offers
a thumbnail grid. Fit, physical-pixel actual size, zoom, pan, fullscreen, temporary
rotate/flip, image information, shortcut help and image/path copying preserve originals.
Animated GIFs play automatically and support pause/resume.

Guaranteed formats are PNG, JPEG, BMP, WebP, TIFF, SVG and GIF. TIFF guarantees cover
single-page 8-bit uncompressed RGB, RGBA, grayscale and palette images; multiple
pages show the first. Other TIFF variants and additional installed handlers are best
effort. APNG and animated WebP show the first frame. GIF87a/GIF89a animation is
supported. Static self-contained SVG renders as vectors; local linked images work
in single view but are unavailable in grid previews. SVGZ and SVG animation are
unsupported. See the README for the shared SVG resource policy.

Raster input is bounded at 256 MiB, 32 million decoded pixels, 32,768 per axis and
128 MiB per decoded image. Display plus decode cache is bounded at 256 MiB; GIF
keeps the displayed and look-ahead frames within that budget. SVG input is capped
at 10 MiB. Thumbnail cache holds at most 1000 entries and 256 MiB. Temporary decode,
render and clipboard/platform allocations are additional memory, not an RSS bound.

Build requirements: C++23, Qt 6.11+ Core/Gui/GuiPrivate/DBus/Quick/Qml/QuickControls2/Svg,
CMake 3.25+, Ninja, Task, tomlplusplus, pkg-config, libwebp, libexif, wayland-client,
wayland-protocols and wayland-scanner. Tests require Qt Test, GTest and dbus-run-session.
Checks require Python 3, clang-format, clang-tidy/run-clang-tidy, REUSE,
desktop-file-utils, GIO and AppStream. Runtime needs native Wayland, the same Qt build
used for providers, libwebp/libexif, Qt Base PNG/BMP/JPEG/GIF support, Qt Image Formats
WebP/TIFF plugins and the four installed providers.

Install providers in dependency order: holonight-config, holonight-qt,
holonight-images, holonight-thumbnails. CI pins their revisions in
[scripts/ci/lane.sh](../../scripts/ci/lane.sh); `task deps` builds sibling checkouts.
Do not reuse the historical release's two-provider instructions for current source.

Configure against an explicit compatible installed prefix (adapt `lib` as needed):

```sh
provider_prefix=/path/to/installed/providers
export QML_IMPORT_PATH="$provider_prefix/lib/qt6/qml"
export LD_LIBRARY_PATH="$provider_prefix/lib"
cmake -S . -B build/current -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_PREFIX_PATH="$provider_prefix" -DQML_IMPORT_PATH="$QML_IMPORT_PATH"
cmake --build build/current
cmake --build build/current --target format-check qml-import-check qmltypes-check qml-lint
ctest --test-dir build/current --output-on-failure
DESTDIR="$PWD/build/current-stage" cmake --install build/current
build/current-stage/usr/bin/hn-viewer -- '/path/to/photo.png'
```

Installation includes the existing desktop ID/icon and seven MIME declarations plus
AppStream metadata. Project licensing remains GPL-3.0-or-later; only the metainfo
XML is CC0-1.0. No host installation, release publication or tag changes are part
of this qualification. Native Open With (TIFF/SVG/GIF), portal selection/cancellation,
GIF controls, grid navigation, fullscreen and clipboard transfer need manual checks.
Physical mixed-monitor qualification remains deferred.
