# Shared thumbnail disk cache — design

`decodeThumbnail` remains the worker entry point. It opens the local file, inspects raster orientation and dimensions or loads and validates SVG bytes, then constructs the provider request from the absolute local URL, modification time, source size, expected output pixel dimensions, and content kind. The provider owns tier choice, metadata validation, and atomic writes. Viewer supplies no revision override so the provider's SVG policy marker applies.

The disk image is converted through Viewer's existing `finish` path before delivery. `ThumbnailProvider` still tags it with the logical size calculated from inspected source dimensions and inserts it into the in-memory LRU. The provider receives the existing cancellation flag. An invalid cache entry is a miss; decode errors and UI behavior remain Viewer's responsibility.

The provider is an installed CMake dependency. `scripts/prepare-deps.sh` builds it after its dependencies for standalone Viewer development. The umbrella installer will establish system installation order.

Implementation baseline: `holonight-thumbnails` revision `d27addc044f277686850588147ec825c40c0f252` (published on its canonical `origin/main`). Viewer started from revision `61a69092cb6ef0db592aff9ffacfba504f0fa9a1`.
