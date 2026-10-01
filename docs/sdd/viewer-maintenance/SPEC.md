# Viewer maintenance

Approved scope: implementation plan accepted on 2026-10-01.

- Retained display, GIF playback and decoded-cache buffers shall fit the 256 MiB budget; neighbor prefetch shall not change the foreground allowance.
- Copy Image shall rasterize SVG at intrinsic dimensions, reducing oversized output proportionally to the existing decoded-image limits, and preserve transparency and captured orientation.
- Clipboard preparation shall run on its existing worker, preserve the previous clipboard on failure and remain independent of subsequent document selection. Local linked resources shall be read during preparation.
- SVG inspection, display, previews and copying shall disable animation consistently.
- Sanitizers shall be opt-in locally and required in a separate CI job. CI shall use a pinned base digest and dated Arch package snapshot.
- Existing QML interfaces and provider APIs shall remain unchanged. Structural refactoring and renderer replacement are excluded.

Temporary decode/conversion buffers, codec-private allocations, clipboard storage and canvas textures remain outside the retained-image budget.
