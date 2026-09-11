# Empty-window decoration verification

Date: 2026-09-11. Scope approved through the supplied implementation plan.
Uses the installed providers under build/deps/prefix; no sibling sources changed.
Spark was assigned the isolated SVG implementation but was unavailable due to its
model usage limit; the main agent completed the asset and integration.

## Automated evidence

- `task deps`: passed; log `build/empty-window-decoration/deps.log`.
- `task check`: passed (exit 0); final log
  `build/empty-window-decoration/check-final.log`. Debug/release/test builds, all
  seven CTest entries, formatting, C++ tidy, QML lint, REUSE licenses, staged
  four-format installed launch and uninstall checks passed. The successful run
  required sandbox escalation because REUSE binds a local multiprocessing socket.
  An earlier run ended with signal 143; another found three test-only arithmetic
  parentheses lint issues, corrected before this final successful run.
- Focused smoke checks passed: exact square side formula, canvas-relative centering,
  padding and containment at 420×280, 1000×700, 480×900, 1600×500 and offscreen
  fullscreen. Checks installed HnIcon sourceSize follows displayed dimensions and
  the resource loads without an error.
- Empty canvas accessibility name and ignored/nonfocusable decoration pass.
  Opening/drop checks use a fully transparent PNG and verify the decoration is
  hidden for Ready, Loading and Error. Existing visible-focus, keyboard, dialog,
  image lifecycle and fullscreen regressions remain part of verification.
- Live dark → light → dark passes via the installed provider file watcher in a
  child process initialized with an isolated complete appearance TOML. Tint equals
  the surface token, the provider render URL changes, screenshots differ, and
  HnIcon reports no error. `build/empty-window-decoration/live.log`.
- SVG validation: all four paths and sun coordinates match the packaged original;
  square viewBox, one neutral opaque color, no gradients or opacity layers.
  `build/empty-window-decoration/svg-validation.txt`. Packaged icon unchanged.
- Visual script matrix passed (8 test selections in each of 8 theme/scale runs): `build/empty-window-decoration/visual.log`.
  Command: `QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml"
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" bash scripts/check-visual.sh`.
  This is the capture command used by `task visual-check`, after the test build.
  Captures are under `build/visual/`, covering dark/light at 1, 1.25, 1.5 and 2×.

Initial regressions caught an unresolved tint-provider resource URL and anchor
rounding that reduced fractional padding. The implementation now resolves the URL
and uses exact x/y centering bindings. Test fixture issues (attached-property
lookup and incomplete appearance TOML) were corrected before final verification.

## Subjective rendered inspection

Agent inspection of dark/light default and minimum windows, fractional portrait,
2× wide landscape, and live-theme captures finds the original photo/sheet motif
recognizable and subordinate to the controls. The selected surface role is accepted
for this local visual inspection. Square proportions and clear surrounding space
are retained, with transparent panel interiors and smooth contours after resizing.
No visible empty-state title remains. Final matrix inspection also includes light
minimum size and light 2× fullscreen; local subjective visual acceptance passes.

## Acceptance boundary

These are offscreen Qt software-rendering and simulated scale checks, not physical
mixed-monitor movement or human screen-reader acceptance. Native compositor,
physical scaling and Orca walkthroughs were not repeated for this change; existing
next-release deferrals remain unchanged. No host installation was performed.

## Console-warning follow-up

The user observed transient provider warnings during `task run`, while the
decoration rendered correctly. Native default and simulated 2× startup probes
were quiet here (`startup-native.log` and `startup-native-2.log` under the evidence
directory); their timeout exits are intentional. The exact reported startup
sequence has therefore not been reproduced.

Provider investigation identified its 1024-pixel request limit. A new 2000×1600
window regression failed with HnIcon.hasError before the repair
(`warnings-large.log`) and passed afterward (`warnings-fixed.log`). Viewer now
keeps its original displayed side and caps only the provider raster request at
1024 physical pixels, dividing the logical source size by window DPR. Above that size the raster is enlarged, so full-resolution rasterization
is not claimed. It also leaves the source empty while the computed side is zero.
The smoke test captures transient provider warnings in addition to checking final
image status. No provider source or installed package was modified.

The 2000×1600 bounded-raster capture remains visually acceptable for this subtle
decoration. `task check` passed (`warnings-check.log`). The DPR correction landed
while that check was running, so the final source was subsequently rebuilt and
all seven test entries passed again (`warnings-final-test.log`), followed by
format checking (`warnings-format.log`) and the complete eight-run visual matrix
(`warnings-visual-final.log`). The final matrix includes the 2000×1600 case and
transient-warning assertions at every theme/scale combination. All logs are under
`build/empty-window-decoration/`; captures remain under `build/visual/`.

The first cap passed at 1× but failed the 1.25× large-window test because Qt scales
provider requests by DPR. The final cap is floor(1024 / window DPR), with unchanged
logical display geometry. The final fractional/2× reruns passed, as recorded above.
