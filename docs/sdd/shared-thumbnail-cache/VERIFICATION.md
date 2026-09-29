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

## CI checkout correction

The first published Viewer run at `5d812e9f7087a4beacaea5a8d2ab4ddc962d35f9` stopped in `task deps`: the workflow did not check out `holonight-thumbnails`, so `/work/holonight-thumbnails` was absent. The prior baseline [run #36594178287](https://github.com/lebedenko/holonight-viewer/actions/runs/36594178287) had already failed because its Images pin lacked `holonight_images/svg.h`; the old Qt pin also lacks `HnControlSize.Xs` used by Viewer. The correction checks out exact published Config `03fa635cedc506e101a148fc54f7eb46d17c6de5`, Qt `61d0c16af689e960367fd0f631f2c88afa169441`, Images `ac11f23e9ff2d70b1c142b643bc2abb6dd69f6c1`, and Thumbnails `d27addc044f277686850588147ec825c40c0f252`, matching the local acceptance stack. It also includes Thumbnails in release measurement provenance. The failed first Viewer run is [Build and checks #36614565141](https://github.com/lebedenko/holonight-viewer/actions/runs/36614565141); Licensing passed in that run.

Before the corrective commit, a GitHub API lookup returned each exact provider commit from its canonical repository. YAML parsing confirmed the four checkout paths and revisions. Local `task deps`, `python3 scripts/check-measure-release.py` (5 tests), `reuse lint`, and `git diff --check` passed. The earlier full application acceptance remains valid because this correction changes CI checkout revisions and measurement provenance only; those revisions match the provider sources used for that acceptance. Hosted CI for the corrective commit is pending publication.
