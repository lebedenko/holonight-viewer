# Tasks

Approved through the supplied implementation plan after SPEC.md and DESIGN.md.
Implementation and release acceptance are separate; unchecked gates block Stage 5.

- [x] T1a (R1): Four-format declarations, mandatory decoder checks and independent
  PNG/JPEG/BMP/WebP fixtures, pixels/orientation, mismatches and original-byte checks.
- [x] T1b (R1): Resolve the compact WebP decoder failure; unchanged regression passes.
- [x] T2 (R2): Desktop metadata, development registration, GIO substitution/argument
  checks and staged installed-entry opening, without changing MIME defaults.
- [x] T3a (R3): Installed-runtime probe/context scripts, CI wiring and source/custom
  prefix installation documentation; probe validated locally.
- [x] T3b (R3): Execute isolated installed runtime container with no workspace mounts.
- [ ] T3c (R6): Final committed-tree hosted CI.
- [x] T4a (R4): Accessibility names/focus and completion/error/clipboard announcements;
  automated tree, focus, event and dialog text-copy checks.
- [x] T4b (R4): Basic real Orca/AT-SPI smoke and load/error/clipboard speech.
- [ ] T4c (R4): Native Orca reading order, full menu/dialog names, keyboard-only and
  visible-focus acceptance through the actual accessibility bridge.
- [x] T5a (R5): Interactive clipboard receiver reads on activation and reports/saves
  image dimensions/pixels or exact path text; retain automatic regression mode.
- [x] T5b (R5): Compositor-input Wayland PNG transfer, eight orientations/alpha and
  special/symlink paths using wl-paste and an interactive receiver.
- [x] T5c1 (R5/R7): Resolve generic Qt image alpha loss; compositor-input matrix,
  navigation during confirmed preparation and quit during preparation pass.
- [ ] T5c2 (R5): Physical human keyboard/menu and full native transfer acceptance.
- [x] T6a (R6): Debug/release builds, format, tidy, QML lint, licensing, staged install,
  desktop checks, offscreen visual inspection and five-run local performance tools.
- [ ] T6b (R6): Full release qualification, native portal/drop,
  stacking decorations, mixed displays, rendering/transport performance comparison.
- [x] T7 (R1–R6): README/backlog and verification records distinguish local evidence
  from remaining blockers. No release publication or Stage 5 acceptance.

## Approved fixes tasks (2026-09-08)

- [x] F1 (R7): Worker PNG publication and payload/lifecycle tests.
- [x] F2 (R8–R9): Bounded simple WebP fallback, regressions, build/runtime dependencies.
- [x] F3 (R10): Provider dialog handoff linked as an unresolved blocker.
- [x] F4a (R5–R9): Contributor checks, installed runtime and five-run baseline comparison.
- [x] F4b (R5/R7): Native generic receiver/wl-paste matrix with compositor keyboard
  activation, exact Qt text reception, navigation and quit during preparation.
- [ ] F4c (R5/R6): Human/native rendering, transfer/manager costs and hosted-CI gates
  remain open as recorded in VERIFICATION; no release readiness declaration.
