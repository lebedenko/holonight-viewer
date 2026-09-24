# Verification

## Local evidence — 2026-09-24

- `PYTHONDONTWRITEBYTECODE=1 python3 scripts/check-compare-performance.py`:
  8 tests passed, including nonzero process exit after passing XML and inconsistent
  peak-RSS rejection.
- `cmake --build build/maintenance-acceptance -j 2`: passed without compiler
  warnings. CTest passed 12/24 entries in the sandbox; the other 12 entries
  needed private D-Bus sockets. `ctest --test-dir build/maintenance-acceptance
  --rerun-failed --output-on-failure` passed all 12 outside the sandbox, covering
  all 24 entries. Logs: `build/maintenance-{build,ctest,ctest-unrestricted}.log`.
- `reuse lint`: passed outside the sandbox (multiprocessing sockets are blocked
  inside it). Changed C++ formatting and `git diff --check` passed.

## Fresh paired measurements

For each of `large` and `navigation`, ran:

```sh
python3 scripts/measure-release.py build/maintenance-acceptance/tests/viewer-smoke build/maintenance-final-baseline-SCENARIO --scenario SCENARIO
python3 scripts/measure-release.py build/maintenance-acceptance/tests/viewer-smoke build/maintenance-final-candidate-SCENARIO --scenario SCENARIO
python3 scripts/compare-performance.py build/maintenance-final-baseline-SCENARIO build/maintenance-final-candidate-SCENARIO build/maintenance-final-comparison-SCENARIO
```

All 20 measured processes passed, sequentially after acceptance builds and static analysis finished. Both comparisons succeeded without provenance overrides.
Raw XML/logs, sampled RSS, process exit/peak-RSS records, actual generated fixture
hashes and provenance are retained in those directories. Each trial uses private
HOME/XDG paths, offscreen/software rendering and scale 1. Private D-Bus required
unsandboxed execution.

Production binary SHA256: `f02c8c0f691fa8103952a1b0a446790b04ce970196c08395b04d484efa299720`.
Production sources, instrumentation, fixture identities, build settings and
provider artifact hashes match in each pair. Installed libraries were matched
byte-for-byte to local provider build outputs. Config `fe69a59`, Qt `863af41`
and Images `3633865` use Release, `/usr/bin/c++` and the consumer's Qt installation.
This validates the comparison tooling with identical production code; it does
not claim a performance improvement, timing gate or native rendering acceptance.

| Scenario | Dataset | report.json SHA256 |
|---|---|---|
| large | baseline | `96481b31fba309b0d426be10a120a814bbfc7c5e2305c2d6348f303c432b0190` |
| large | candidate | `748b07e9377037c13f5332603fb835ef3979af70023655878732e1d4dc718e58` |
| navigation | baseline | `ecf90390c0455cfbd0ea9fe769e48f2a2dc0a72e033755ae0242305f5df27393` |
| navigation | candidate | `ac1f6002e2312a504a4117ee25ee6f9edcb92a0dc95c50af39211bc91cbfefe6` |

## Full acceptance and correction

`task check` passed its builds, all 24 Debug CTest entries and formatting, then
reported one new clang-tidy diagnostic: missing braces in the fixture-manifest
helper. Added the braces and ran the affected translation unit with
`clang-tidy -p build/test -removed-arg=-mno-direct-extern-access
--config-file=.clang-tidy tests/release_performance_test.cpp`; it passed.
The full analysis covered all 39 translation units; no other actionable diagnostic
was reported. Logs: `build/maintenance-task-check.log` and
`build/maintenance-tidy-correction.log`.

Rebuilt `build/maintenance-acceptance`, then completed the remaining `task check`
components individually: `task qml-lint PRESET=debug`, `task license-check`,
`task install-check`, `task qml-import-check`, `task qmltypes-check`, and
`task format-check`. All passed. Complete logs:
`build/maintenance-correction-build.log` and `build/maintenance-remaining-checks.log`.
The above 20 final measurement trials were recollected after this correction;
the earlier `maintenance-{baseline,candidate,comparison}-*` evidence remains
preserved but is superseded for final instrumentation acceptance.
