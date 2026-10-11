# Viewer review fixes requirements

Approved source: the supplied implementation plan (2026-10-11).

- REQ-NF-001: When only an animation frame changes, the Viewer shall retain existing metadata delegates.
  Acceptance: deterministic frame advances preserve delegate identity while the strip is visible and hidden; pause, resume, frame-count discovery, failure and reset update values.
- REQ-NF-002: When the next browsing neighbor is an SVG, the Viewer shall defer decoding that neighbor until foreground selection.
  Acceptance: lowercase and uppercase SVG neighbors have zero speculative calls and one foreground call; raster/GIF neighbors remain prefetched.
- REQ-F-001: If a loading result becomes stale or is cancelled, then the Viewer shall prevent that result from replacing the selected document.
  Acceptance: barriers exercise active/queued replacement, refresh, paste cancellation and shutdown; accepted paste commits once without a second decode.
- REQ-C-001: The Viewer shall preserve its existing decoded-cache and playback memory limits.
  Acceptance: cache reuse, source invalidation, eviction, GIF reservation and tentative paste budget regressions pass.
- REQ-C-002: The Viewer shall keep image, browsing, thumbnail and platform implementation in separate non-installed private targets.
  Acceptance: a fresh external build with explicit provider/QML paths passes; staged payload remains unchanged.

Preserve ImageDocument QML API and injected constructors. No provider changes, SVG caching, codec timeout, browsing/presentation facade or unrelated cleanup.
