# Information-preview DPR audit

Baseline: `06b0061a20ab4175f6f1546c8316132e31271bce`. Initial scope was local, uncommitted Viewer work and automated verification only.
On 2026-09-23 the user reported manual checks done and authorized commit and pin.

- R1: Reproduce screen/window DPR divergence before any production correction.
- R2: Preview follows its owning window DPR, using 1 before attachment, including
  transitions through 1, 1.25, 1.5 and 2 and close/reopen without recreation.
- R3: Preserve fitted logical bounds, orientation, preview source and visibility;
  physical magnification selects smooth downsampling below 1× and sharp enlargement.
- R4: No provider, public API, decoding, cache or main-canvas changes. Provider pins
  and all deferred native acceptance (including Files T5) remain unchanged.
- R5: Focused red/green evidence, surrounding regressions, clean Release build,
  task check and isolated installed-runtime verification are required for a fix.
  Manual qualification subsequently passed by user report on 2026-09-23.
