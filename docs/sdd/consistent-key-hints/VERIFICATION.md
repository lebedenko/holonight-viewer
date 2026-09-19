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
