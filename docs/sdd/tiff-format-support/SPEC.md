# TIFF Format Support Specification

## Context

HoloNight Viewer guarantees static PNG, JPEG, BMP and WebP, and animated GIF. TIFF already opens through Qt's
`libqtiff` plugin (`qt6-imageformats`, already a dependency) but only on a best-effort basis, with no fixtures,
tests or documentation. This specification promotes TIFF to a guaranteed format. Its one viewer behavior change is
reading EXIF from TIFF files (REQ-F-012, REQ-F-013); any other defect that testing finds is fixed as a bug.

## Scope

- **Guaranteed:** single-page, 8-bit, uncompressed TIFF in RGB, RGBA, grayscale and palette. A multi-page file
  shows page 1 only, as animated WebP and APNG do today.
- **Best-effort, must decode or fail cleanly (error shown, no crash, no hang):** 16-bit and float samples, CMYK,
  Lab, tiled layout, BigTIFF, compressed files (LZW, Deflate, PackBits), and truncated or corrupt files.
- **EXIF:** camera, exposure and location facts are read from TIFF files with the existing libexif, through a new
  TIFF branch in `ExifMetadata::payload()`. Because a TIFF's metadata IFDs can sit anywhere in the file, the whole
  file is passed to libexif, so a TIFF-specific payload limit applies (REQ-F-013).
- **Limits:** the existing decode limits apply unchanged (256 MiB input, 32 million pixels, 32,768 px per axis,
  128 MiB decoded, 256 MiB displayed plus cache). There is no TIFF-specific decode path.
- **Dependencies:** none new.

## Non-goals

- Orientation behavior changes. Qt applies the TIFF orientation tag through `setAutoTransform(true)` today; no
  orientation fixture or test is added.
- Reading EXIF from BigTIFF files (libexif cannot parse them).
- Multi-page navigation or UI.
- 16-bit handling or conversion.
- Guarantees for compressed, tiled, BigTIFF, 16-bit, float, CMYK or Lab variants.
- New codecs, dependencies or TIFF-specific decode limits.
- SVG, ICO, TGA or any other new format.

## Fixtures

`scripts/format-fixtures.py` gains hand-written, stdlib-only TIFF fixtures. They use no Qt and no libtiff, and
their bytes are deterministic. All are baseline uncompressed:

1. RGB8, 1x1, known colour.
2. RGBA8, 1x1, known colour and alpha.
3. Grayscale8, 1x1, known value.
4. Palette8, 1x1, known palette entry.
5. Two-page file whose pages differ (page 1 red, page 2 green).
6. One truncated copy of a valid fixture.
7. RGB8 1x1 with EXIF: camera Make and Model in IFD0, an ISO value in the Exif IFD and GPS coordinates in the GPS
   IFD, with IFD0 placed after the pixel data as libtiff-style writers do.

## Requirements

### Ubiquitous

**REQ-F-001: Decode guaranteed variants.** The Viewer shall decode single-page, 8-bit, uncompressed TIFF in RGB,
RGBA, grayscale and palette.
- Acceptance: for each of fixtures 1-4, decoding succeeds and the image has the fixture's dimensions and known
  pixel colour.

**REQ-F-002: Existing limits apply.** The Viewer shall apply the existing input, pixel, dimension and decoded-size
limits to TIFF files without a TIFF-specific code path.
- Acceptance: a TIFF whose header declares more than 32,768 px on one axis, or more than 32 million pixels, is
  rejected with the text "This image exceeds the viewing limit (32 million pixels or 128 MiB decoded)."

**REQ-F-003: Format label.** The Viewer shall report the format of a decoded TIFF as "TIFF".
- Acceptance: the `ImageInformation.format` for a decoded TIFF fixture equals `"TIFF"`.

**REQ-F-004: Listed as supported.** The Viewer shall list `tif` and `tiff` among its supported suffixes.
- Acceptance: `ImageDocument::nameFilters()` contains both `*.tif` and `*.tiff`.

**REQ-NF-001: Deterministic independent fixtures.** The fixture script shall generate TIFF fixtures using only the
Python standard library.
- Acceptance: two runs of the script produce byte-identical TIFF files, and the script imports neither Qt nor
  libtiff bindings.

**REQ-C-001: No new dependency.** TIFF shall be decoded only through the existing `libqtiff` plugin from
`qt6-imageformats`.
- Acceptance: `CMakeLists.txt` files and package lists (`packaging/Dockerfile.ci`, README) gain no new package.

**REQ-C-002: Existing decode path.** TIFF shall be decoded through the existing `QImageReader` path and the same
limit checks (`acceptableSize`, `acceptableImage`) as other still formats.
- Acceptance: the diff adds no TIFF-specific branch to `readImage` or `decodeImage`, unless a defect fix requires
  one and is recorded in the design.

### Event-driven

**REQ-F-005: Multi-page.** When a multi-page TIFF is opened, the Viewer shall display only the first page.
- Acceptance: for fixture 5, the decoded image has page 1's known colour and not page 2's.

**REQ-F-006: Directory browsing.** When a folder is scanned, the Viewer shall include files with the suffixes
`tif`, `tiff`, `TIF` and `TIFF`.
- Acceptance: a folder test containing `a.tif`, `b.TIF`, `c.tiff` and `d.TIFF` lists all four.

**REQ-F-007: EXIF-less TIFF.** When a TIFF without EXIF metadata is opened, the Viewer shall open it without an
error and without crashing.
- Acceptance: the fixtures 1-4 decode successfully with an empty `ImageInformation.exif`, and the result carries
  no error string.

**REQ-F-012: TIFF EXIF.** When a TIFF that carries EXIF metadata is opened, the Viewer shall read camera, exposure
and location facts from it as it does for JPEG, PNG and WebP.
- Acceptance: for fixture 7, `ExifMetadata::read` returns the fixture's known camera string, ISO and location, and
  `decodeImage` carries them in `ImageInformation.exif`. The same holds for a big-endian copy of the fixture.

**REQ-F-013: TIFF EXIF size limit.** When a TIFF is larger than the TIFF EXIF limit (64 MiB), the Viewer shall
skip EXIF reading for it, still open the image, and show no error.
- Acceptance: a valid fixture-7 file padded to 64 MiB + 1 byte decodes with the image present, empty
  `ImageInformation.exif` and no error string; the same file at exactly 64 MiB yields the fixture's facts.

### State-driven

**REQ-C-003: Pixel fidelity.** While decoding a guaranteed variant, the Viewer shall preserve the pixel's colour
and alpha.
- Acceptance: the RGBA fixture's decoded pixel, read with `QImage::pixelColor`, equals the fixture's known colour
  and alpha.

### Conditional

**REQ-F-008: Plugin required for qualification.** Where the Viewer is qualified for a release or an installed
runtime, the TIFF plugin shall be present and shall decode a TIFF file.
- Acceptance: the installed-runtime check (`tests/installed_runtime.cpp`, run through
  `scripts/isolated-runtime.sh`) opens a TIFF fixture and fails if it does not decode; `qt6-imageformats` remains
  in `packaging/Dockerfile.ci`.
- Acceptance: with the TIFF plugin missing, the qualification check fails, and ordinary startup does not.

**REQ-F-009: Documentation.** Where the README lists guaranteed formats, it shall name TIFF.
- Acceptance: the README "Supported formats and limits" section and the Codecs line name TIFF, and describe
  multi-page files as showing the first page only and the other variants as best-effort; the README sentence
  about EXIF names TIFF and its 64 MiB limit.

### Unwanted behaviour

**REQ-F-010: Best-effort variants.** If a 16-bit, float, CMYK, Lab, tiled, BigTIFF or compressed TIFF is opened,
then the Viewer shall either decode it or show an error, and shall not crash or hang.
- Acceptance: for each variant that the test suite can produce, opening it either yields a non-null image or a
  non-empty error string, and the call returns within the test timeout.

**REQ-F-011: Truncated file.** If a truncated TIFF is opened, then the Viewer shall show an error and shall not
crash or hang.
- Acceptance: for fixture 6, the result has a null image and a non-empty error string, and the call returns
  within the test timeout.

## Open items

1. Resolved: libexif parses raw TIFF bytes prefixed with `Exif\0\0`, including IFD0 after the pixel data and
   big-endian files (probed with a C program on libexif 0.6.x). Exif-IFD and GPS-IFD parsing on TIFF is still to be
   confirmed by fixture 7 (REQ-F-012).
2. Check how the runtime-check container and qualification fail when the plugin is missing (REQ-F-008), and
   whether the failure names the plugin.
3. Confirm which best-effort variants (REQ-F-010) the tests can produce without libtiff; unproducible variants
   are recorded as untested, not as passing.

## Risks

- Qt 6.11.2 has had handler regressions (WebP). The independent fixtures are intended to expose any TIFF
  regression.
- The fixtures prove uncompressed decoding only. Compressed TIFF depends on `libtiff` through Qt and stays
  best-effort.
- Passing a whole TIFF to libexif reads up to 64 MiB into memory transiently, on the worker thread, before the
  decode. This is unlike the JPEG/PNG/WebP paths, which read one small block. The limit value is a design
  choice to confirm.
- Truncation can fail at different decode stages (header, IFD, strip data); the truncated fixture covers one
  point.
