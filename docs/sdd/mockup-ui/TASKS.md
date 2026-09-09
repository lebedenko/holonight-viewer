# Mockup UI tasks

Approved through the supplied implementation plan.

- [x] R1/R4: Header, footer, menu and responsive overlay layout.
- [x] R2/R3: Passive input observation, independent timers and generation-safe rendering.
- [x] R4: Snapshot size formatting and transformed/physical scale display.
- [x] R5: Local window-activation-independent presentation with keyboard-focus indicators and retained interactions.
- [x] R1–R5: Update tests, run required checks, inspect theme/scale captures and record limitations.

Local implementation and acceptance checks are complete. Native compositor and
physical mixed-DPI acceptance in earlier cycles remains independent; see
VERIFICATION.md for scope and skipped opt-in checks.

- [x] R6: Audit Viewer font-size assignments and remove the sole pixel-size override.
- [x] R6: Verify build, runtime warning absence and required quality checks.

- [x] R7: Add resource-backed SVG header icons and inset popup placement.
- [x] R7: Verify runtime resource loading, menu bounds, themes/scales and required checks.

- [x] R8: Render reactive metadata sections with a Repeater and HnSeparator.
- [x] R8: Run required checks and inspect normal/narrow overlay captures.
