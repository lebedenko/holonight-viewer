# Verification: Consistent key hints

Date: 2026-09-20. Provider: `8fe24ff83f8108631c2b7b0351994f7e513acfcd`
(published and pinned before consumer implementation). Host Qt: 6.11.2.

## Local checks

- `task deps`: built and privately installed the accepted Release provider.
- Debug, Release and test builds from `task check`: passed.
- Fresh Release acceptance: `cmake --preset release -B /tmp/key-hints-viewer-clean`
  and `cmake --build /tmp/key-hints-viewer-clean -j2`: passed, no compiler warnings.
- Focused footer/menu/help tests exposed and corrected menu ListView navigation
  when badges make the menu scroll. The final full acceptance run,
  `ctest --test-dir build/test --output-on-failure`, passed **20/20 entries**,
  including viewer-smoke, both styles and fractional-scale menu rendering.
  The isolated D-Bus fixture required an elevated run outside the sandbox.
- `QMLFORMAT=/usr/lib/qt6/bin/qmlformat task format-check`, `task lint`,
  `task qml-import-check`, `task qmltypes-check`, `task license-check`: passed.
  REUSE required permission for its local multiprocessing socket.
- `task install-check`: passed staged packaging, installed startup, opening
  supported formats and installed GIO desktop launch.
- `task runtime-context`, the repository's runtime Dockerfile and isolated
  runtime script: passed with Qt 6.11.2, no network and no workspace mounts.
  PNG/JPEG/BMP/WebP decoding, CLI opening and installed GIO launch all passed.
  The matching `viewer-ci` image was built locally for this check.
- After the final compact-menu test correction, the repository's configured
  `run-clang-tidy` command was rerun on `tests/image_inspection_test.cpp`: passed.
- Offscreen footer and help captures at scales 1, 1.25 and 1.5; help checks at
  12 and 18 pt in normal and compact windows. Reviewed
  [footer](footer-1.25.png) and [18 pt help](help-18pt-1.25.png): badges fit,
  alternatives and combinations wrap, and action labels remain aligned.

## Review and handoff

The shared provider's verification covers all symbols, punctuation, legacy text,
accessibility, disabled state and the 8/12/18 pt multi-font matrix. Action bindings
are unchanged. The scrolling list defers key navigation to Controls.Menu so that
separator/disabled-item skipping is preserved at every viewport height.

Native ecosystem interaction remains a user-performed integration check. This
repository has not been published or pinned by this consumer work.

## KH-007 refinement — 2026-09-20

Viewer baseline: `746acb2f00e427eab97743feda7492fbc40c751e`.
Provider: `9723cef7371a4ca5f2967d869b26ee7ff1e3c789`, published and pinned
at umbrella checkpoint `a79866f` before implementation. Earlier KH-003 evidence
above remains historical. Qt 6.11.2, GNU C++ 16.2.1.

### Implementation and regressions

- Main.qml now uses the frameless HnKeySequenceLabel for menu sequences, bound
  to the adjacent label's complete font and enabled textMuted / disabled
  textDisabled colors. Semantic arrays, navigation, actions and scrollbar
  clearance are unchanged. CI checks out the corrected provider revision.
- FooterKeyHints.qml binds badge point size to the description's resolved point
  size, preserving the provider's monospace family. The runtime regression covers
  point sizes 8, 18, pixel size 32, then point size 12. Copying the QML pixel-size
  getter initially exposed rounding at 8 pt (10 versus 11 resolved pixels); using
  the resolved point-size binding preserves the intended size in both unit modes.
- Menu tests verify no background/padding, full font parity including bold,
  italic and letter spacing, readable names and live state colors. Existing
  geometry, disabled-item/separator skipping and action tests remain intact.
- The compact footer leaves more room for the empty-state glyph at the minimum
  window size. Its old test assumed it must be hidden at 420×280; the corrected
  test verifies bounds there and forces a shorter test window to verify that
  the glyph gives way while text keeps its size. Product empty-state code is unchanged.

### Acceptance

- `task deps`: updated the private Release provider from the exact accepted
  source revision. Installed HnKeyHint.qml and HnKeySequenceLabel.qml match source.
- Focused footer/menu/help run passed except for the initial footer rounding
  regression, corrected above. Final targeted empty-state/typography checks:
  **5/5 passed**. Visual matrix tests also pass the help geometry and menu hairline checks.
- `QMLFORMAT=/usr/lib/qt6/bin/qmlformat task check`: Debug, Release and test builds
  passed. Initial CTest was **19/20**, with only the old empty-state assumption
  failing. After its correction, `ctest --test-dir build/test -R '^viewer-smoke$'
  --output-on-failure` passed; every one of the **20 CTest entries** now has a
  passing result. Unaffected entries were not repeated. Native clipboard coverage
  remains skipped by the existing offscreen fixture.
- Continued the remaining acceptance steps individually: `task format-check`,
  `task lint`, `task qml-import-check`, `task qmltypes-check`, `task install-check`.
  All passed. Clang-tidy and qmllint report no user-code diagnostics. The isolated
  D-Bus fixture and installation checks required execution outside the sandbox.
- Fresh Release acceptance: `cmake --preset release -B /tmp/holonight-kh007-clean`
  and `cmake --build /tmp/holonight-kh007-clean -j4`: passed. Complete build logs
  reviewed; no compiler warnings. No full application rebuild was needed after
  the test-only empty-state correction.
- `task runtime-context`, `docker build -t holonight-viewer-kh007-runtime
  build/runtime-check.SvtVPh`, and `docker run --rm --network none
  holonight-viewer-kh007-runtime`: passed. Container has no workspace mount and
  uses the existing matching Qt 6.11.2 viewer-ci image. Installed PNG/JPEG/BMP/WebP,
  CLI handling and GIO desktop launch all passed.
- Final `reuse lint` passed (198/198 files); `git diff --check` passed.

### Visual review and remaining boundaries

Reviewed footer/menu/help captures at scales 1, 1.25 and 1.5 with 12 and 18 pt
appearance settings. Help additionally exercises 12/18 pt badge fonts and normal
and compact windows. R, 1 and Ctrl+0 remain centered; menu sequences have matching
label typography without badges, muted enabled ink and disabled ink. Help wraps
within its column, including at 18 pt. All captures use offscreen rendering;
menu hairlines use the production RHI OpenGL backend. Xvfb was unavailable, so
captures used the same offscreen backend as the passing acceptance matrix.

Evidence: [footer](compact-footer-1.25.png), [menu](frameless-menu-1.25.png),
[18 pt help](compact-help-18pt-1.25.png). Native user interaction, publication and
pinning, and final ecosystem integration remain separate handoff steps.
