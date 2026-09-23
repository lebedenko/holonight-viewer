# Design

Depend on unchanged HolonightImages `3633865d2f39e4f163f0159a0f252f88245379f0`. Keep typed outcomes in internal worker values and format raster errors at UI acceptance. Metadata status belongs to the existing EXIF value, travels with facts, and never creates a notice.

Viewer DecodeResult has an optional raster outcome, leaving consumer-owned error text intact. Raster cache hits carry Success; ImageInformation retains EXIF status through cache reuse. Cancelled completions still release the worker and dispatch pending work.

## Changed files

- `apps/viewer/exif_metadata.cpp`
- `apps/viewer/exif_metadata.h`
- `apps/viewer/image_document.cpp`
- `apps/viewer/image_document.h`
- `tests/exif_metadata_test.cpp`
- `tests/image_document_test.cpp`
- `tests/release_formats_test.cpp`
