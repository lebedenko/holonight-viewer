# Design

The decode worker owns the foreground reservation and cache allowance. GIF frame zero belongs to the budgeted LRU instead of an additional displayed snapshot. Prefetch only inserts into the remaining allowance. An injected playback clock/source and a small test budget exercise ownership without large allocations.

ClipboardController accepts an owned SVG source value. The existing worker rasterizes a self-contained source through HoloNight Images or loads a local linked document into a worker-local QSvgRenderer, applies the captured orientation and encodes PNG. Bounded dimensions are selected before allocation. Existing busy, feedback, failure and shutdown behavior is shared with raster copying.

All SVG loaders select disabled animation before loading. The SVG view remains vector-based.

Target-scoped ASan/UBSan options instrument Viewer-owned targets only. A separate preset and CI job run CTest with extended timeouts. CI pins the Arch base digest and one package snapshot; installed-runtime checks reuse that image. Explicit toolchain updates record qualification and versions.
