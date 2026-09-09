# Stage 5: Source-release readiness

Approved scope: the user-supplied Stage 5 implementation plan authorizes these
requirements under CONTRIBUTING.md. Delivery is source plus CMake installation.
User clarification: native Wayland is the only supported desktop target; X11 and
XWayland are not release acceptance targets. No release publication, version bump, distribution package, bundled providers,
new viewing features, directory opening or single-instance forwarding.

- R1: When release validation runs, it shall require PNG, JPEG, BMP and WebP
  decoders and verify independently specified transparency, orientation and pixels.
  Missing codecs shall fail qualification without blocking ordinary startup.
  Other installed handlers shall remain best effort; animation uses only frame one.
- R2: When a desktop launcher opens a file, Viewer shall receive exactly one local
  path after `--`, including Unicode, spaces, percent and dash names. The installed
  entry shall advertise only the four guaranteed MIME types. Development registration
  shall retain explicit provider paths without changing default associations.
- R3: When installed-runtime validation runs, it shall install Viewer and separately
  built providers into standard container locations and run without source/build
  mounts or development runtime overrides. It shall check empty startup, the four
  formats, entry/icon installation and runtime search paths.
- R4: When document loading completes or fails, or clipboard work completes, Viewer
  shall announce the result through Qt accessibility. Zoom, pan and folder updates
  shall not generate status announcements. Controls, canvas, menus and dialogs shall
  expose names, roles, enabled state and keyboard focus with visible indicators.
  Dialog text shall remain selectable and copyable.
- R5: When native clipboard acceptance runs, a separate interactive receiver shall
  read on user activation and report pixels/dimensions or text. Native Wayland shall cover eight orientations, alpha, special/symlink paths, navigation
  during preparation and shutdown using real keyboard/menu input.
- R6: Before release acceptance, native portal selection/cancellation, file-manager
  drop, stacking decorations, mixed-scale movement, real Orca, committed-checkout CI,
  regression/visual checks and five-run release performance comparisons shall pass.
  Missing environments or unresolved provider/platform issues shall remain blockers.
  Evidence shall identify actual environments and retain image/cache limits.

## Approved Viewer fixes follow-up (2026-09-08)

The supplied “Viewer fixes and HoloNight dialog handoff” plan approves this
requirements/design/task extension. Acceptance is native Wayland only.

- R7: When copying an image, the worker shall transform and encode its captured
  snapshot as PNG; the GUI shall publish only explicit image/png and announce
  success after publication. Preparation or encoding failure shall preserve the
  previous clipboard; busy, navigation, path-copy and shutdown semantics remain.
- R8: When Qt cannot inspect or decode a simple static RIFF WebP with VP8/VP8L
  data, Viewer shall try libwebp based on content. Extended, metadata and animated
  containers shall stay on Qt's path. Input, axis, pixel and decoded limits shall
  apply before output allocation, decoding shall use a bounded buffer, cancellation
  checks and normalization shall remain, and source bytes shall remain unchanged.
- R9: Source builds and CI/runtime environments shall require private libwebp via
  pkg-config while retaining required Qt format-plugin qualification.
- R10: Viewer shall document the unresolved provider native-dialog delegation gap
  and its acceptance checks without provider edits or global environment changes.

## Approved visible-focus repair (2026-09-09)

The supplied restore-visible-keyboard-focus plan authorizes this R4 clarification:
when keyboard focus moves, buttons, the canvas, dialog text and Close controls shall
show a shared-token focus outline only on the focused target. Actions menu arrow
navigation shall visibly distinguish its enabled highlighted item. Tab/Shift+Tab,
shortcuts, dialog dismissal, input handling and image geometry shall remain intact.
Stable window-activation styling shall not suppress keyboard-focus indicators.
Native Open and provider changes remain outside this repair.
