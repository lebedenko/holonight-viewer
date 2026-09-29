# Shared thumbnail disk cache — Viewer verification

Date: 2026-09-29. Provider baseline: `holonight-thumbnails` `d27addc044f277686850588147ec825c40c0f252`. Viewer baseline: `61a69092cb6ef0db592aff9ffacfba504f0fa9a1`.

| Check | Result |
| --- | --- |
| `task deps` | Passed; rebuilt config, Qt, images, and thumbnails in Viewer's local prefix. |
| Focused `ThumbnailDecoder.*:ThumbnailProvider.*:ThumbnailGrid.*` | 45 passed. A subsequent decoder-only run passed 15/15 after adding the cache write failure case. Coverage includes shared raster reuse, aspect-aware and EXIF-oriented writes, SVG resource rejection, cache write failure fallback, and >1024-pixel disk bypass. |
| Debug, Release, and Test builds | Passed against the local provider prefix. |
| `ctest --preset test` | 25/25 passed outside the sandbox; D-Bus tests require local sockets. |
| `task format-check`, `task qml-import-check`, `task qmltypes-check`, `task qml-lint` | Passed. |
| `task tidy` | Passed after addressing two diagnostics in new code; final run had no user-code errors. The final added decoder test passed targeted clang-tidy. |
| `reuse lint` | Passed outside the sandbox; its workers require local sockets. |
| `task install-check` | Passed; staged `/usr` payload, CLI opening, and installed GIO launch. |
| `task runtime-context` and `docker run --rm --network none viewer-thumbnail-runtime` | Passed; installed payload contains the shared thumbnail provider and runs without workspace mounts or network. |

Manual native Files/Viewer comparison of matching raster and SVG thumbnails is pending with the user and tracked by the umbrella coordinator.
