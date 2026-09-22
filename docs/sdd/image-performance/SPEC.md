# Image performance

Approved implementation plan, 2026-09-22. Baseline: `737d7a99849701f985e927a09f18c475231a6d1d`.

- R1: Measure real Viewer document operations in five sequential fresh Release processes per scenario, with private D-Bus, offscreen rendering and explicit providers.
- R2: Cover large images, GIF playback/scanning, large folders, cache reuse/pressure, rapid navigation and asynchronous shutdown; verify selected identity and decoded pixels before timing completion.
- R3: Preserve raw logs/XML, per-process peak and sampled RSS, latency summaries and reproducible source/toolchain provenance. Failed, missing, skipped or malformed measurements must fail qualification.
- R4: Correct only demonstrated Viewer-local bottlenecks, with deterministic regressions and identical before/after instrumentation. No universal new timing thresholds.
- R5: Keep changes local and preserve providers, application APIs, native acceptance history and umbrella state. No native pointer/focus automation, publication or commits.
