# TIFF Format Support: Design

Status: Draft for review. Inputs: `SPEC.md` (same directory) and the code at `28b5bca`.
Scope: `holonight-viewer` only. No other repository is modified.

Notation: `REQ-*` IDs refer to SPEC.md. Evidence labels:

- **Read** means read in this repository at the commit above.
- **Probed** means observed by running throwaway programs (outside the repository, in the session scratchpad)
  against the installed Qt 6.11.2 with its `libqtiff` plugin and the system `libtiff.so.6`. The probes used
  `QImageReader` directly on hand-written files produced by a prototype of the fixture writer in section 5. They
  did not run the Viewer's `decodeImage()` or the Viewer's test-suite. Probed statements describe this machine, not
  CI.
- **Unverified** means neither read nor probed.

---

## 1. Findings that shape the design

| # | Fact | Evidence |
|---|------|----------|
| F1 | `decodeImage()` performs: stat checks, 256 MiB file cap, `ExifMetadata::read`, then `readImage()`, then `acceptableImage`, convert to `Format_ARGB32_Premultiplied`. `readImage()` builds a `QImageReader` on the `QFile`, sets `setAutoTransform(true)`, records `facts.format = reader.format().toUpper()`, checks `reader.size()` against `acceptableSize` before `read()`, and reads only if `size()` is valid. Nothing in it names a format except the WebP fallback. | Read `image_document.cpp:76-158` |
| F2 | Errors are chosen by `decoderError(readable, dimensions)`: not readable gives "unsupported or its header is damaged"; readable but size invalid gives "dimensions could not be read"; otherwise "damaged, unreadable, or exceeds the decode memory limit". The limit error is a separate string. | Read `image_document.cpp:64-75` |
| F3 | `ImageDocument::nameFilters()` and `scanDirectory()` both derive suffixes from `QImageReader::supportedImageFormats()`. `scanDirectory()` case-folds both sides, so `TIF` matches `tif`. There is no format list to edit. | Read `image_document.cpp:268-276`, `directory_model.cpp:63-76` |
| F4 | The installed `libqtiff` registers both `tif` and `tiff` as format keys, and `reader.format()` returns `tiff`, so the label is `TIFF`. | Probed: `supportedImageFormats()` contains `tif` and `tiff`; `format()` prints `tiff` |
| F5 | `ExifMetadata::payload()` recognises only JPEG (`FFD8`), PNG and RIFF/WEBP signatures and returns an empty payload for anything else; `parse()` returns a default `ExifDetails{}` for an empty payload. Today a TIFF therefore yields empty details with no error and no libexif call. Payload extractors read one block of at most `max_payload` (1 MiB); oversized blocks are skipped, never truncated. | Read `exif_metadata.cpp:14,45-51,252-268` |
| F6 | The release fixtures are produced by `scripts/format-fixtures.py <dir>`. Its `FORMATS` dict is written as `sample.<ext>`, and existing tests and the runtime probe iterate `png jpg bmp webp gif` over `sample.<ext>` expecting a 1x1 `(255,0,0,128)` pixel (opaque for `bmp` and `gif`). | Read `format-fixtures.py:91-103`, `release_formats_test.cpp:12-61`, `installed_runtime.cpp:48-54,76` |
| F7 | `tests/CMakeLists.txt` defines `release-fixtures` as an always-run custom target invoking the script into `${CMAKE_CURRENT_BINARY_DIR}/release-fixtures`; `viewer-smoke` and `installed-runtime-probe` depend on it and `viewer-smoke` gets `RELEASE_FIXTURE_DIR`. Nothing is committed under `tests/fixtures/images`; only `dark.toml`, `light.toml` and `dbus-session.conf` are committed there. | Read `tests/CMakeLists.txt` |
| F8 | The installed-runtime probe fails (exit 1) with "Missing required decoder: X" when `QImageReader::supportedImageFormats()` lacks a format in `{png, jpeg, bmp, webp, gif}`. `main.cpp` has no decoder check, so ordinary startup does not fail. | Read `installed_runtime.cpp:13-22`; `grep supportedImageFormats apps/viewer/main.cpp` finds nothing |
| F9 | `scripts/isolated-runtime.sh` is `set -euo pipefail`. It runs `python3 scripts/format-fixtures.py build/fixtures` and then the installed probe against `/opt/check/build/fixtures`. A non-zero probe exit aborts the script, so the `docker run --rm --network none viewer-runtime-check` CI step fails. `Dockerfile.runtime-check` is `FROM viewer-ci` and relies on it for "Qt format plugins". `prepare-runtime-check.sh` already copies `format-fixtures.py`. | Read those files and `.github/workflows/build.yml` |
| F10 | `qt6-imageformats` is already in `Dockerfile.ci`; the README already lists it as a dependency. | Read |
| F11 | No existing test mentions TIFF as a format. `exif_metadata_test.cpp` uses "tiff" only for the EXIF-internal TIFF block. | Read (`grep -i tif`) |

### Probed behaviour of Qt 6.11.2 `libqtiff` on hand-written baseline files

| Input | `canRead` | `size()` | `read()` | Notes |
|-------|-----------|----------|----------|-------|
| RGB8 1x1 | true | 1x1 | ok, `Format_RGB32`, pixel (255,0,0) | |
| RGBA8 1x1, ExtraSamples=2 | true | 1x1 | ok, `Format_ARGB32_Premultiplied`, `pixelColor` (255,0,0,128) | unassociated alpha |
| Gray8 1x1 | true | 1x1 | ok, `Format_Grayscale8`, (128,128,128) | |
| Palette8 1x1 | true | 1x1 | ok, `Format_Indexed8`, (255,0,0) | ColorMap entry 2 |
| Two-IFD file | true | 1x1 | ok, page 1 colour; `imageCount()` is 1 | Qt's handler does not report the second page at all |
| Header declares 32769x1 | true | 32769x1 | not reached | `acceptableSize` rejects, limit error |
| Header declares 6000x6000 | true | 6000x6000 | not reached | rejected by the 32 million pixel rule |
| Strip data cut by 1 byte (IFD intact) | true | 1x1 | null | reaches "damaged, unreadable" branch |
| Cut inside IFD, cut at 6 bytes | true | invalid | null | reaches "dimensions could not be read" branch |
| Empty file | false | invalid | null | "unsupported or header damaged" |
| Every proper prefix of RGB/RGBA/Gray/Palette fixtures | | | null | only the full length decodes; no hang |
| Every prefix of the two-page file | | | ok once page 1 is complete | truncating only page 2 is not an error (consistent with REQ-F-005) |

Additional probed observations:

- Best-effort variants written by the prototype: 16-bit RGB (`Format_RGBX64`), float RGB, CMYK, Lab, PackBits,
  Deflate (Adobe, zlib) and a hand-made single-symbol LZW stream all decode to a non-null image. A tiled file
  and a BigTIFF file produced a null image with "Unable to read image data". Whether those two prototypes are
  themselves valid TIFF is **unverified**; they may be malformed. Either outcome satisfies REQ-F-010.
- Every TIFF open under Qt 6.11.2 prints one line to stderr of the form
  `foo: Not a TIFF or MDI file, bad magic number N (0xNN).`, including for a TIFF written by `QImage::save`. It is
  emitted by the Qt/libtiff layer, not by the fixtures, and does not affect the result. Cause **unverified**.
- With the plugin directory stripped of `libqtiff.so` (library paths overridden in the probe), `supportedImageFormats()`
  contains neither `tif` nor `tiff` and opening a TIFF returns "Unsupported image format" from the reader, which the
  Viewer maps to "The image format is unsupported or its header is damaged."

**Conclusion:** the spec's claim that no viewer change is needed holds for every guaranteed variant on this
machine. No defect was found that requires touching `readImage()` or `decodeImage()`.

---

## 2. Components

### Touched

| Component | Change | Reqs |
|-----------|--------|------|
| `scripts/format-fixtures.py` | Stdlib TIFF writer (section 5) and TIFF fixture outputs. | NF-001, F-001, F-005, F-011, C-003 |
| `tests/release_formats_test.cpp` | New `Release.Tiff*` tests; extend `RequiredIndependentFormats`. | F-001..F-003, F-005, F-007, F-010, F-011, C-002, C-003 |
| `tests/image_document_test.cpp` | New `Document.NameFiltersListTiffSuffixes`. | F-004 |
| `tests/folder_browsing_test.cpp` | New `Directory.ListsTiffSuffixesInAnyCase`. | F-006 |
| `tests/installed_runtime.cpp` | Require `tif`/`tiff` decoders; open `sample.tif`. | F-008 |
| `tests/CMakeLists.txt` | One determinism test (section 6). No other wiring change. | NF-001 |
| `apps/viewer/exif_metadata.cpp` | New TIFF branch in `payload()` and a TIFF-specific payload limit (section 3a). | F-012, F-013 |
| `tests/exif_metadata_test.cpp` | TIFF extraction and limit tests. | F-012, F-013 |
| `README.md` | Codecs line, EXIF sentence, "Supported formats and limits" (section 8). | F-009 |
| `docs/sdd/tiff-format-support/TASKS.md` | Created after approval of this design. | - |

### Explicitly untouched

`apps/viewer/*` except `exif_metadata.cpp` (no `image_document.cpp`, `directory_model.cpp`, `image_limits.h`, QML),
every `CMakeLists.txt` other than `tests/CMakeLists.txt`, `packaging/Dockerfile.ci`,
`packaging/Dockerfile.runtime-check`, `scripts/isolated-runtime.sh`, `scripts/prepare-runtime-check.sh`,
`.github/workflows/build.yml`, `Taskfile.yml`. The last three are untouched because the existing flow already
generates fixtures with `format-fixtures.py`, copies that script into the check context and runs the probe. All other
repositories are untouched.

REQ-C-001 and REQ-C-002 are therefore satisfied by construction: the diff contains no package, no CMake
dependency and no change to `readImage`/`decodeImage`.

---

## 3. Data flow when opening a TIFF (unchanged path)

1. `ImageDocument::open` -> worker -> `decodeImage(url, cancelled)`.
2. `QFile` opened; size checked against 256 MiB.
3. `ExifMetadata::read`: `payload()` sees the TIFF signature (`II*\0` or `MM\0*`) and, for a file of at most 64 MiB,
   returns `Exif\0\0` plus the whole file; `parse()` hands it to libexif (section 3a). A TIFF without EXIF tags gives
   `ExifDetails{}`; a TIFF above 64 MiB, a BigTIFF or an unreadable one gives empty details. No error in any case.
4. `readImage()`: `QImageReader` probes plugins by content and selects `libqtiff`. `canRead()` is true;
   `format()` is `tiff`, so `facts.format == "TIFF"`.
5. `reader.size()` comes from the first IFD. If it fails `acceptableSize`, the limit error is returned before any
   pixel is allocated. If invalid (truncated IFD), `read()` is skipped and the "dimensions could not be read"
   error is returned.
6. `reader.read()` decodes page 1 only; `setAutoTransform(true)` applies the orientation tag if present.
7. `acceptableImage` (dimension, pixel and 128 MiB decoded checks), convert to `Format_ARGB32_Premultiplied`,
   return with `information.format == "TIFF"` and the `exif` facts from step 3.

The 32-bit QImage allocation limit (`QT_IMAGEIO_MAXALLOC=128`) applies inside the reader as for other formats.
Observation, not a change: 16-bit TIFF decodes to an 8 bytes per pixel `QImage`, so a 16-bit file of more than about
16 million pixels will hit the 128 MiB rule as an error rather than a display. This is within "best-effort".

---

## 3a. TIFF EXIF branch (REQ-F-012, REQ-F-013)

A TIFF is itself the EXIF container: libexif expects `Exif\0\0` followed by a TIFF header, which is exactly a
TIFF file's first bytes. **Probed** (C program against the installed libexif): `Exif\0\0` + a raw TIFF loads and
returns Make and Model from IFD0, with IFD0 placed after the pixel data, in little- and big-endian files.
Exif-IFD and GPS-IFD parsing on TIFF is **unverified** until fixture 7 runs.

Change in `payload()`:

```cpp
if (signature.startsWith("II*\0") || signature.startsWith("MM\0*")) {
  return tiffPayload(source, cancelled);
}
```

`tiffPayload` returns an empty array when `source.size() > max_tiff_payload` (64 MiB) and otherwise reads the whole
file in bounded chunks, checking `cancelled` between chunks, and returns `exif_header + bytes`. `max_payload` stays
1 MiB for the other formats. The payload is a transient copy freed when `read()` returns, before `readImage()`
allocates, so the 256 MiB display-plus-cache budget is unaffected. A BigTIFF (`II+\0`, magic 43) does not match the
signature and yields empty details.

Why the whole file: metadata IFDs and their tag data can be anywhere and libexif needs valid offsets, so a truncated
read is unsafe, which is why the existing code skips oversized blocks instead of truncating them.

---

## 4. Decisions and alternatives

| # | Decision | Rationale | Alternatives rejected |
|---|----------|-----------|-----------------------|
| D1 | No viewer code change. | Probed decode works for all four guaranteed variants; `nameFilters`, folder scan and the label are already derived from the plugin (F3, F4). REQ-C-002. | Adding a TIFF branch: no defect justifies it. |
| D2 | Hand-write TIFF bytes in Python `struct`, uncompressed, little-endian. | REQ-NF-001; independent of Qt and libtiff, so a Qt handler regression is not masked (spec risk 1). Bytes are a pure function of the script. | (a) Write fixtures with `QImage::save(..., "TIFF")`: uses the code under test, cannot detect a writer/reader symmetric regression, and cannot produce ExtraSamples/palette layouts deliberately. (b) `libtiff` via `ctypes` like the WebP fixtures use libwebp: contradicts REQ-NF-001 and adds a runtime dependency to the isolated container. (c) Pillow/tifffile: new dependency, violates REQ-C-001 spirit. |
| D3 | Generate at build time through the existing `release-fixtures` target; commit no binaries. | Matches how every other format is delivered (F7). One source of truth, reviewable as text, reused by `isolated-runtime.sh` in the container with no extra file copying. | Committing binary `.tif` files under `tests/fixtures/`: opaque diffs, REUSE annotation per binary (`REUSE.toml`), two sources of truth for the runtime container which regenerates anyway. |
| D4 | Name the RGBA fixture `sample.tif`; other fixtures `tiff-*.tif`. | `sample.<ext>` is already the convention that `RequiredIndependentFormats`, the mismatch-extension loop and the runtime probe iterate, and RGBA at alpha 128 matches their expected `(255,0,0,128)` pixel. Adding `"tif"` to those lists is one token each and buys the misleading-extension check for free. | A separate probe list for TIFF: duplicates logic. |
| D5 | Over-limit REQ-F-002 inputs are made by patching the width/height words of the RGB fixture in the test, not by extra fixture files. | The writer's layout puts `ImageWidth` at file offset 18 and `ImageLength` at 30 (section 5), fixed by design, so the patch is exact and documented, mirroring the existing WebP header patch (`release_formats_test.cpp:104-108`) and BMP header test. It keeps the fixture list at the six in the spec. | Extra generated files: enlarge the fixture set beyond the spec. |
| D6 | Truncated fixture cuts 2 bytes off the RGB fixture, inside the strip data, IFD intact. In addition the test sweeps every proper prefix of the RGB fixture (131 short files, tens of bytes each). | The strip-data cut is the only cut where `size()` is valid and `read()` fails, so it exercises the harder `decoderError` branch (spec risk 3 names three stages; the fixture covers one, the sweep covers header, IFD and strip in one loop without new files). Probed: all 131 proper prefixes produce a null image. | Only cutting inside the IFD: hits the cheaper branch and leaves `read()` failure untested. |
| D7 | Runtime check requires both `tif` and `tiff` keys and decodes `sample.tif` through the real UI path. | REQ-F-008. Reuses the existing `extensions` loop. | New separate probe binary: not needed. |
| D8 | Missing plugin remains a qualification failure only. Startup is unchanged. | Matches GIF policy in the README; F8. | Startup warning: new behaviour, out of scope. |
| D9 | TIFF EXIF: read the whole file up to 64 MiB, prefixed with `Exif\0\0`; above the limit, no EXIF. | The simplest correct approach given IFD placement (section 3a). 64 MiB covers typical camera and scanner TIFFs while keeping the transient copy well under the decode budget. | (a) Reuse the 1 MiB limit: most real TIFFs would silently get no EXIF. (b) Walk the IFD chain and rebuild a compact TIFF for libexif: much more code and its own bugs. (c) Memory-map the file: not needed on the worker thread. **The 64 MiB value is a choice for the user to confirm.** |

---

## 5. Hand-written TIFF fixture writer

Byte order little-endian (`II`, magic 42). All values below were exercised by the prototype against Qt 6.11.2.

### 5.1 File layout (single page)

```
offset 0   'I' 'I' 0x2A 0x00              header: byte order and magic
offset 4   uint32 = 8                     offset of first IFD
offset 8   uint16 N                       IFD entry count
offset 10  N x 12-byte entries, ascending tag order
           (tag u16, type u16, count u32, value-or-offset u32)
           uint32 next-IFD offset (0 for the last page)
           auxiliary arrays: BitsPerSample (spp > 1), ColorMap (palette)
           pixel data (one strip)
```

Entries are sorted by tag, so `ImageWidth` (256) is entry 0 at offset 10 and its value word is at offset 18;
`ImageLength` (257) is entry 1, value word at offset 30. SHORT values with count 1 are stored left-justified in the
value word (little-endian, so `value` then two zero bytes); arrays and any value over 4 bytes are stored at an
offset.

### 5.2 Entries per fixture

Common to all: 256 ImageWidth (LONG 1), 257 ImageLength (LONG 1), 258 BitsPerSample, 259 Compression = 1,
262 PhotometricInterpretation, 273 StripOffsets (LONG 1, offset of the pixel data), 277 SamplesPerPixel,
278 RowsPerStrip = height, 279 StripByteCounts = pixel byte count. PlanarConfiguration, resolution and
orientation tags are omitted (TIFF defaults; probed accepted).

| Fixture | Photometric | SamplesPerPixel | BitsPerSample | Extra entries | Pixel data | Expected |
|---------|-------------|-----------------|---------------|---------------|------------|----------|
| RGB8 `tiff-rgb8.tif` | 2 | 3 | SHORT[3] = 8,8,8 | none | `ff 00 00` | (255,0,0,255) |
| RGBA8 `sample.tif` | 2 | 4 | SHORT[4] = 8,8,8,8 | 338 ExtraSamples SHORT 1 = **2** (unassociated) | `ff 00 00 80` | (255,0,0,128) |
| Gray8 `tiff-gray8.tif` | 1 (BlackIsZero) | 1 | SHORT 1 = 8 (inline) | none | `80` | (128,128,128,255) |
| Palette8 `tiff-palette8.tif` | 3 | 1 | SHORT 1 = 8 (inline) | 320 ColorMap SHORT[768] | `02` | palette index 2 = (255,0,0,255) |

ColorMap is three planes of 256 uint16 (all reds, then greens, then blues). Entry 2 has red 0xFFFF and green/blue 0;
all other entries are 0. ExtraSamples must be 2 (unassociated). With 1 (associated alpha) the stored bytes would have
to be pre-multiplied and the expected colour changes, so the fixture avoids it.

### 5.3 Two-page file `tiff-two-page.tif`

Header points to IFD 1 at offset 8. The writer builds a page as a function of its base offset:
`page(base, colour, next_ifd)` returns the IFD, its auxiliary arrays and its pixel data with every offset computed
from `base`. File = header + `page(8, red, next=8+len(page1))` + `page(8+len(page1), green, next=0)`. Page 1 is RGB8 red
`ff 00 00`, page 2 is RGB8 green `00 ff 00`. Probed: Qt reports `imageCount() == 1` and decodes page 1 (red).

### 5.4 Truncated fixture `tiff-truncated.tif`

`tiff-rgb8.tif` bytes with the last 2 bytes removed (strip data 3 -> 1 byte). IFD complete, so `size()` is 1x1 and
`read()` fails.

### 5.4a EXIF fixture `tiff-exif.tif` (REQ-F-012) and its big-endian copy `tiff-exif-be.tif`

RGB8 1x1 red, laid out as libtiff-style writers do: header, pixel data, then IFD0, then the tag data. IFD0 holds the
common tags plus Make (271, ASCII), Model (272, ASCII), ExifIFD pointer (34665, LONG) and GPSIFD pointer (34853,
LONG). The Exif IFD holds ISOSpeedRatings (34855, SHORT); the GPS IFD holds GPSLatitudeRef/Latitude and
GPSLongitudeRef/Longitude (rationals). Exact strings and values are chosen by the writer and asserted by the tests
against what `camera()`, `iso()` and `location()` format. The writer takes a byte-order parameter (`'<'` or `'>'`)
so the big-endian copy shares the code. Every value is written by `struct`; nothing depends on libtiff or libexif.

### 5.5 Script integration

- New pure functions `tiff_entry`, `tiff_page`, `tiff_file` near the GIF helper; only `struct` and `zlib` (already
  imported) are used. No new import. The docstring of the module is updated to say "stdlib TIFF".
- Module-level dict `TIFF_FIXTURES` (like `GIF_FIXTURES`), written by the `__main__` block. `FORMATS` gains
  `'tif': <rgba8 bytes>` so `sample.tif` is emitted by the existing loop.
- Determinism: no timestamps, no dict-order dependence beyond insertion order, entries sorted by tag; two runs of the
  prototype were byte-identical (probed).

### 5.6 Best-effort variants (see decision U1)

If approved, a second dict `TIFF_BEST_EFFORT_FIXTURES` adds nine one-pixel files, all red, stdlib only:
`tiff-be-rgb16.tif` (bps 16), `tiff-be-float.tif` (bps 32, SampleFormat 3), `tiff-be-cmyk.tif` (photometric 5, spp 4),
`tiff-be-lab.tif` (photometric 8), `tiff-be-tiled.tif` (tags 322-325 replace 273/278/279, 16x16 tile),
`tiff-be-bigtiff.tif` (magic 43), `tiff-be-lzw.tif` (compression 5, single-symbol stream: clear code, literal bytes
as 9-bit codes, end code), `tiff-be-packbits.tif` (32773) and `tiff-be-deflate.tif` (8, `zlib.compress`). Probed: seven decode, tiled and
BigTIFF fail cleanly; tiled/BigTIFF prototype validity is unverified and must be checked against libtiff docs during
implementation, and either outcome is acceptable to the test.

---

## 6. Fixture delivery to tests and CMake wiring

No new wiring for delivery:

1. `add_custom_target(release-fixtures ...)` runs `format-fixtures.py` into
   `${CMAKE_CURRENT_BINARY_DIR}/release-fixtures` on every build of `viewer-smoke` or `installed-runtime-probe`
   (F7). The TIFF files appear next to `sample.png` and friends.
2. Tests read them through `RELEASE_FIXTURE_DIR`.
3. In the isolated container the same script writes `build/fixtures` and the probe reads
   `/opt/check/build/fixtures` (F9). `prepare-runtime-check.sh` already copies the script.

One new CTest for REQ-NF-001. A CMake-only comparison would need a helper `.cmake` file, so the check is a small
stdlib mode of the script itself: `format-fixtures.py --self-check` builds every TIFF fixture twice in memory, exits
non-zero if the two byte strings differ, and exits non-zero if `sys.modules` contains any Qt or TIFF binding
(`PyQt*`, `PySide*`, `PIL`, `tifffile`, `libtiff`). It is registered as:

```cmake
add_test(NAME viewer-tiff-fixtures-deterministic
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/format-fixtures.py" --self-check)
```

This turns REQ-NF-001 into a checked statement. The script's libwebp `ctypes.CDLL` block currently runs at import
time, so the `--self-check` branch must be evaluated before it (or the WebP block moved under a function). That small
refactor is a risk (section 11) and decision U3.

---

## 7. Test plan

All tests are gtest cases in `viewer-smoke` (run via `ctest`), or the installed-runtime probe. "Decode" means
`decodeImage()` on the fixture path, as in `Release.SimpleWebPContentCorruptionAndLimits`. Pixel checks use
`pixelColor(0, 0)` on the returned `QImage`.

| Requirement | Test / check | Assertion |
|-------------|--------------|-----------|
| REQ-F-001 | `Release.TiffGuaranteedVariantsDecode` (`release_formats_test.cpp`) | For `tiff-rgb8.tif`, `sample.tif`, `tiff-gray8.tif`, `tiff-palette8.tif`: image non-null, `size() == 1x1`, error empty, `pixelColor` equals (255,0,0,255) / (255,0,0,128) / (128,128,128,255) / (255,0,0,255). |
| REQ-F-001 (UI path) | `Release.RequiredIndependentFormats` extended with `"tif"` | Opens `sample.tif` through `ImageDocument`, state `Ready`, 1x1 `(255,0,0,128)`. Its mismatch-extension copy (`mismatch-tif.png`) also opens (content detection; unverified until run). |
| REQ-F-002 | `Release.TiffLimitsApply` | Copy `tiff-rgb8.tif`, write width 32769 at offset 18: `decodeImage` returns null image and error equal to the limit text "This image exceeds the viewing limit (32 million pixels or 128 MiB decoded)."; likewise width 6000/height 6000 (offsets 18 and 30). Also assert file-size cap is not TIFF-specific by not adding a case. |
| REQ-F-003 | `Release.TiffGuaranteedVariantsDecode` | `information.format == "TIFF"` for each of the four fixtures. |
| REQ-F-004 | `Document.NameFiltersListTiffSuffixes` (`image_document_test.cpp`) | `ImageDocument::nameFilters().first()` contains `*.tif` and `*.tiff`. |
| REQ-F-005 | `Release.TiffMultiPageShowsFirstPage` | `tiff-two-page.tif` decodes to (255,0,0,255) and not (0,255,0,255). |
| REQ-F-006 | `Directory.ListsTiffSuffixesInAnyCase` (`folder_browsing_test.cpp`) | With `writeFile` creating `a.tif`, `b.TIF`, `c.tiff`, `d.TIFF` (placeholder bytes are enough, the scan is by suffix), `scanDirectory` lists all four in natural order. |
| REQ-F-007 | `Release.TiffGuaranteedVariantsDecode` | For fixtures 1-4: `information.exif == ExifDetails{}` and `error.isEmpty()`. Also a direct `ExifMetadata::read` assertion on a QBuffer of fixture 1 equal to `ExifDetails{}` (a TIFF with no EXIF tags). |
| REQ-F-012 | `ExifMetadata.ReadsTiffCameraExposureAndLocation` (`exif_metadata_test.cpp`) | For `tiff-exif.tif` and `tiff-exif-be.tif`: `ExifMetadata::read` returns the known camera, ISO and location; `decodeImage` carries them in `information.exif`. |
| REQ-F-013 | `ExifMetadata.SkipsTiffOverSizeLimit` | Copy `tiff-exif.tif` into a temp file resized (sparse) to exactly 64 MiB: facts present. Resize to 64 MiB + 1: `read` returns `ExifDetails{}` and `decodeImage` still returns the image with an empty error. Also a BigTIFF header and a cancelled read return empty details. |
| REQ-F-008 | `installed-runtime-probe` (`tests/installed_runtime.cpp`), run by `viewer-runtime-probe` and by `scripts/isolated-runtime.sh` | `required` gains `"tif"` and `"tiff"`; `extensions` gains `"tif"`, so `sample.tif` is opened via the live QML `Main` and must match a 1x1 (255,0,0,128) pixel. Missing plugin -> "Missing required decoder: tif" and exit 1 -> `set -e` -> container non-zero -> CI job fails. Ordinary startup: `main.cpp` has no check. The negative case is not permanent CI; it is run once manually (see section 9) and the result recorded in TASKS. |
| REQ-F-009 | Review checklist item, plus `rg -n 'TIFF' README.md` showing the three edits of section 8 | Manual/documentation check. |
| REQ-F-010 | `Release.TiffBestEffortVariantsNeverCrashOrHang` (only if U1 approved) | For each `tiff-be-*.tif`: `decodeImage` returns non-null image or non-empty error, and the loop finishes inside the 60 s `viewer-smoke` timeout. Variants that cannot be produced are listed as untested. |
| REQ-F-011 | `Release.TiffTruncatedIsAnError` | `tiff-truncated.tif`: null image, non-empty error, no crash. Plus prefix sweep of `tiff-rgb8.tif` from 0 to size-1 bytes written to a temp file: every result has null image and non-empty error. |
| REQ-NF-001 | `viewer-tiff-fixtures-deterministic` (section 6) | Byte-identical builds and no Qt/tiff module imported. |
| REQ-C-001 | Diff review: `git diff --stat` touches no `CMakeLists.txt` dependency line, no `packaging/Dockerfile.ci`, and README gains no package name. | Recorded in the verification row. |
| REQ-C-002 | Diff review: `git diff -- apps/viewer` is empty. | Recorded in the verification row. |
| REQ-C-003 | `Release.TiffGuaranteedVariantsDecode` (RGBA case) | `image.pixelColor(0,0) == QColor(255, 0, 0, 128)`, the same equality the existing WebP/PNG tests use. |

Test timeout: the existing per-test `viewer-smoke` TIMEOUT 60 covers "within the test timeout". The prefix sweep decodes
131 files of about 130 bytes each and is expected to take well under a second (the same sweep ran in the probe
program with no measurable delay; not timed).

Acceptance follows the project workflow: `task check` (format-check, clang-tidy, tests, staged install), then the
isolated runtime check through `task runtime-context` and the CI container. New test code follows existing style
(`QTemporaryDir` under `RELEASE_FIXTURE_DIR`, `ASSERT_*` before dereferencing, no raw loops over unsigned indices
that clang-tidy flags; see the dev-environment notes in memory for tidy rules).

---

## 8. README changes

1. Codecs line (README line 22): "plus Qt Image Formats' WebP plugin" becomes "plus Qt Image Formats' WebP, GIF and
   TIFF plugins". (The GIF plugin comes from the same package and is already required but unnamed here.)
2. Image Information paragraph (line 166): "EXIF is read from JPEG, PNG and WebP files with libexif;" becomes "EXIF is
   read from JPEG, PNG, WebP and TIFF files (TIFF up to 64 MiB) with libexif;".
3. "Supported formats and limits" (lines 204-208) becomes:

   > The guaranteed formats are static **PNG, JPEG, BMP, WebP and TIFF**, and **GIF** (GIF87a and GIF89a, including
   > animation). TIFF is guaranteed for single-page, 8-bit, uncompressed RGB, RGBA, grayscale and palette files; a
   > multi-page TIFF shows its first page only. Other TIFF variants (16-bit, float, CMYK, Lab, tiled, BigTIFF and
   > compressed files) and other installed Qt image handlers work on a best-effort basis: they either open or show
   > an error. Other animated files (APNG, animated WebP) show their first frame only. Missing codecs, including the
   > Qt GIF and TIFF plugins, fail qualification, not ordinary startup.

   The limits table is unchanged; no TIFF-specific limits exist.

---

## 9. Qualification and runtime-check changes

- `tests/installed_runtime.cpp`: add `"tif"`, `"tiff"` to the `required` array and `"tif"` to `extensions`. The existing
  `matchesFixture` needs no change because `sample.tif` is defined to be RGBA red at alpha 128.
- No change to `isolated-runtime.sh`, `Dockerfile.runtime-check`, `prepare-runtime-check.sh` or `Dockerfile.ci`.

**What happens when the plugin is missing (Open item 2, answered from code and a probe):**

| Where | Behaviour | Names the plugin? |
|-------|-----------|-------------------|
| Installed-runtime probe | stderr "Missing required decoder: tif" (first missing key; `tiff` would be next), exit 1 | Names the format key, not `libqtiff` or `qt6-imageformats`. Same as GIF today. |
| `isolated-runtime.sh` / `docker run` | `set -e` aborts at the probe line; the container exits non-zero; the CI step "Isolated installed runtime" fails | No additional message. The probe's stderr appears in the CI log. |
| `viewer-smoke` in `task check` | `Release.RequiredIndependentFormats` fails at its `supportedImageFormats().contains` assertion ("Required decoder: tiff") | Format key only. |
| Ordinary application startup | Unaffected: `main.cpp` has no format check. Opening a TIFF shows "The image format is unsupported or its header is damaged." | No. |

Caveat: the container inherits plugins from `viewer-ci` (F9), which installs `qt6-imageformats`. Whether libtiff is
present transitively there (it is needed by `libqtiff.so`) is unverified; locally `libtiff.so.6` is installed. If the
plugin loaded but libtiff were missing, Qt would not register the plugin and the outcome would be the same as above.

**Negative check (manual, one-off):** to confirm REQ-F-008's second acceptance, build the runtime-check image and run it
with `libqtiff.so` deleted, for example a throwaway Dockerfile layer `RUN rm /usr/lib/qt6/plugins/imageformats/libqtiff.so`
after `FROM viewer-runtime-check`. Expect exit code 1 and the message above. Not run yet (Docker was not used; the
equivalent was confirmed only at the `QImageReader` level in the probe). Record the result in TASKS. It is not turned
into a permanent CI job to keep the container flow unchanged.

---

## 10. Answers to the spec's Open items

1. **`ExifMetadata::read` for TIFF.** Resolved by bringing EXIF into scope (REQ-F-012, REQ-F-013, section 3a).
   Verified by reading: today `payload()` returns empty for a TIFF. Probed: libexif parses `Exif\0\0` + raw TIFF,
   including IFD0 after the pixel data and big-endian. Exif-IFD and GPS-IFD parsing on TIFF is unverified until
   fixture 7 runs.
2. **Runtime check without the plugin.** See section 9. The check fails hard in qualification and names the format
   key only. Startup does not fail. The container-level negative run is unverified.
3. **Producible best-effort variants.** All nine variants named in REQ-F-010 can be written with the stdlib (LZW as
   a valid single-symbol stream, Deflate via `zlib`, PackBits by hand). Probed: the first seven decode; tiled and
   BigTIFF prototypes returned clean errors but may be malformed themselves. Nothing is "unproducible", but the
   tiled and BigTIFF fixtures need correctness review before their results can be claimed as decode-or-error rather
   than plain rejection of a bad file. The design records them as such until then.

---

## 11. Risks

| Risk | Handling |
|------|----------|
| Whole-file EXIF read (up to 64 MiB, transient, worker thread) | Bounded by the limit, chunked with cancellation checks, freed before decode (section 3a). Above the limit EXIF is skipped, not truncated. |
| libexif on hostile TIFFs | Same libexif and silent log as the other formats; the existing `max_records`/cancellation pattern is followed. Fuzzing not planned. |
| Qt handler regression (spec risk) | Independent bytes (D2). The four decoders each check dimensions and colour. |
| Truncation stage variety (spec risk) | Strip-data fixture plus prefix sweep covers header, IFD and strip stages in one test (D6). |
| Refactor of `format-fixtures.py` for `--self-check` could disturb the WebP fixtures (encoder runs at import) | Keep the refactor minimal; the existing `RequiredIndependentFormats` and WebP tests act as regression checks. If the refactor is judged too invasive, drop the `--self-check` and demonstrate REQ-NF-001 once by running the script twice and comparing directories in the verification row (weaker: not repeatable in CI). |
| The Qt/libtiff stderr line on every open | Noted as observed behaviour; unaffected by the design. Cause unverified. |
| `sample.tif` alpha semantics | Qt returns premultiplied `ARGB32` and the viewer converts; probed pixel (255,0,0,128). Rounding on other alpha values is not tested. |
| `mismatch-tif.png` content detection | Assumed to work like other formats; verify when the test runs. |
| Best-effort variants prototype validity (tiled, BigTIFF) | Treat as clean-failure evidence only until validated. |
| Local probes ran on Qt 6.11.2 with local libtiff | CI uses Arch rolling packages and may differ; hosted CI is the acceptance check. |
| Hangs cannot be excluded for arbitrary hostile TIFFs | Spec limits REQ-F-010 to producible variants; existing README caveat on codec hangs applies. |

---

## 12. Decisions needing user input

- **U1. Best-effort fixtures.** SPEC lists six fixtures. The prototype shows nine best-effort variants can also be
  generated with the stdlib; REQ-F-010's acceptance applies to "each variant that the test suite can produce".
  Recommendation: add them (small, one test). Otherwise REQ-F-010 has no automated evidence and every variant is
  recorded as untested.
- **U2. TIFF EXIF. Resolved: in scope.** The user chose to bring it in. Remaining decision: the 64 MiB TIFF payload limit (D9) is a proposed value to confirm.
- **U3. `--self-check` refactor of the fixture script (section 6)** versus a manual one-time determinism check.
  Recommendation: `--self-check`, provided the WebP block is wrapped without changing its output.
- **U4. Missing-plugin message.** The probe names the format key only. Optionally extend the message to name
  `qt6-imageformats`; recommendation: leave as is for consistency with GIF.
- **U5. Manual container negative check.** Approve running the one-off Docker check in section 9, or accept that
  the missing-plugin behaviour rests on the code reading and the `QImageReader`-level probe.
- **U6. Truncation coverage.** Approve the prefix sweep over `tiff-rgb8.tif` (test-time only, no new fixture
  file) in addition to the single truncated fixture.
- **Informational, no action proposed:** every TIFF open prints a libtiff/Qt line to stderr
  (`foo: Not a TIFF or MDI file, ...`); the Qt 6.11.2 TIFF handler reports one page for a multi-page file;
  16-bit files above ~16 million pixels will be rejected by the 128 MiB decoded limit.
