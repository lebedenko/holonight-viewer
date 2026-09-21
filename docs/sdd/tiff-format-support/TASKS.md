# SDD Tasks — tiff-format-support

- [x] T-001: Implement TIFF base fixture writer functions and generate 8 guaranteed-variant fixtures
  - REQs: NF-001, F-001, F-005, F-011, C-003
  - Check: `python3 scripts/format-fixtures.py build/test` creates tiff-rgb8.tif, sample.tif, tiff-gray8.tif, tiff-palette8.tif, tiff-two-page.tif, tiff-truncated.tif, tiff-exif.tif, tiff-exif-be.tif with correct byte structure and contents per DESIGN section 5.

- [x] T-002: Extend TIFF fixture writer with nine best-effort variant fixtures (design U1)
  - REQs: F-010
  - Check: `python3 scripts/format-fixtures.py build/test` generates tiff-be-rgb16.tif, tiff-be-float.tif, tiff-be-cmyk.tif, tiff-be-lab.tif, tiff-be-tiled.tif, tiff-be-bigtiff.tif, tiff-be-lzw.tif, tiff-be-packbits.tif, tiff-be-deflate.tif.

- [x] T-003: Implement --self-check determinism mode for fixture writer and register CTest (design U3)
  - REQs: NF-001
  - Check: `ctest -R viewer-tiff-fixtures-deterministic` passes and `python3 scripts/format-fixtures.py --self-check` exits 0 with no Qt/TIFF modules imported.

- [x] T-004: Add TIFF guaranteed variant decode and multi-page tests to release_formats_test.cpp
  - REQs: F-001, F-003, F-005, F-007, C-003
  - Check: `ctest -R 'Release.TiffGuaranteedVariantsDecode|Release.TiffMultiPageShowsFirstPage'` passes with pixel values matching fixture specifications.

- [x] T-005: Add TIFF size limit and truncation tests to release_formats_test.cpp (including prefix-sweep per design U6)
  - REQs: F-002, F-011, C-002
  - Check: `ctest -R 'Release.TiffLimitsApply|Release.TiffTruncatedIsAnError'` passes with 32769px and 6000x6000 rejections, and prefix sweep of 131 truncated variants all producing errors.

- [x] T-006: Add TIFF best-effort variant tests to release_formats_test.cpp (design U1)
  - REQs: F-010
  - Check: `ctest -R Release.TiffBestEffortVariantsNeverCrashOrHang` passes with all nine variants producing image or error within timeout.

- [x] T-007: Implement tiffPayload branch in ExifMetadata::payload() and add EXIF tests to exif_metadata_test.cpp
  - REQs: F-012, F-013
  - Check: `ctest -R 'ExifMetadata.ReadsTiffCameraExposureAndLocation|ExifMetadata.SkipsTiffOverSizeLimit'` passes with EXIF extracted from tiff-exif.tif and big-endian variant, and skipped above 64 MiB.

- [x] T-008: Add TIFF name-filter test to image_document_test.cpp
  - REQs: F-004
  - Check: `ctest -R Document.NameFiltersListTiffSuffixes` passes with ImageDocument::nameFilters() containing `*.tif` and `*.tiff`.

- [x] T-009: Add TIFF folder-browsing test to folder_browsing_test.cpp
  - REQs: F-006
  - Check: `ctest -R Directory.ListsTiffSuffixesInAnyCase` passes with scanDirectory() listing a.tif, b.TIF, c.tiff, d.TIFF.

- [x] T-010: Update installed_runtime.cpp to require TIFF plugin and open sample.tif fixture
  - REQs: F-008
  - Check: `installed_runtime.cpp` contains `"tif"` and `"tiff"` in `required` array and `"tif"` in `extensions` loop, opening sample.tif with matchesFixture verification.

- [x] T-011: Update README documentation (Codecs line, Supported formats section, EXIF sentence)
  - REQs: F-009
  - Check: `rg -n 'TIFF|tiff' README.md` shows Codecs line includes "TIFF", Supported formats describes single-page guarantee and best-effort variants, EXIF sentence names TIFF and its 64 MiB limit.

- [x] T-012: Run full local acceptance build with fixture determinism, test suite, and diff verification
  - REQs: C-001, C-002
  - Check: `task check` passes with no unresolved warnings, all TIFF tests pass, and `git diff` shows no new package dependencies in CMakeLists.txt/Dockerfile.ci/README and no TIFF-specific branch in readImage/decodeImage.

- [x] T-013: Perform manual Docker missing-plugin negative check (design U5, user-approved)
  - REQs: F-008
  - Check: Build viewer-runtime-check image with libqtiff.so deleted, run isolated-runtime.sh probe, verify exit code 1 and stderr message "Missing required decoder: tif"; record result in TASKS.md verification row.

## Verification

- 2026-09-22, `task check` (QMLFORMAT=/usr/lib/qt6/bin/qmlformat): exit 0; all 21 CTest entries passed, including
  `viewer-smoke`, `viewer-tiff-fixtures-deterministic` and `viewer-runtime-probe`; no compiler or tool warnings.
- 2026-09-22, T-013 manual Docker check (local, image `viewer-runtime-check` built from `task runtime-context`):
  - With the plugin: `docker run --rm --network none viewer-runtime-check` exited 0 and the installed probe printed
    "Decoded tif: 1x1".
  - With `/usr/lib/qt6/plugins/imageformats/libqtiff.so` deleted in a throwaway layer: exit 1 and stderr
    "Missing required decoder: tif". The message names the format key, not the plugin or package.
  - Hosted CI was not run and is not claimed.
