# Verification

Date: 2026-09-23. Automated implementation acceptance completed before publication. The subsequent user request authorizes commit and pinning; publication and exact pins are recorded in the umbrella ledger.

## Environment

GCC 16.2.1 (20260810), Qt 6.11.2, Ninja. Release providers, tests disabled and Wayland provider build disabled. Consumer acceptance uses project-local installed providers:

- Images `3633865d2f39e4f163f0159a0f252f88245379f0` (unchanged).
- Qt `863af4183bdf09ce05199b37e8f5dfb46a311ba1`.
- Config `fe69a59e6b73167fd5349223a4d265d75386c139`.

Provider sources are clean; Files revision ledger matches, Viewer `task deps` refreshed/verified installed artifacts. Provider Qt-private ABI warnings require the same Qt build at runtime; isolated acceptance uses the existing CI base image and installed payloads only.

## Checks

- `cmake --build build/test --parallel 4`: passed; focused regressions below passed after updating internal-result assertions.
- `cmake --preset release` then `cmake --build --preset release --clean-first --parallel 4`: passed, full clean Release compilation without compiler warnings.
- `CMAKE_BUILD_PARALLEL_LEVEL=4 task check`: clean Debug/Release builds and all 22 CTest entries passed. The initial format check resolved `/usr/bin/qmlformat` version 1.0; `QMLFORMAT=/usr/lib/qt6/bin/qmlformat task format-check` selected Qt 6.11.2 and passed. Completed `task lint` (39 translation units plus QML lint), `task license-check`, `task install-check`, `task qml-import-check`, and `task qmltypes-check` separately: all passed. Thus every full task-check stage passed without rerunning unaffected tests after the formatter selection. Main smoke: 188 passed and eight existing opt-in performance/native/clipboard/visual skips. The failed-read helper parameter names were also corrected before the successful tidy pass; rebuilt `viewer-smoke` and reran `ExifMetadata.PreservesDeviceIoFailure` successfully afterward.
- `bash scripts/prepare-runtime-check.sh`; `docker build --network none -t holonight-viewer-outcomes-runtime <generated-context>`; `docker run --rm --network none holonight-viewer-outcomes-runtime`: passed. No workspace mounts; installed payloads only.
- `git diff --check`: passed. Documentation links reviewed locally.

Focused command: `QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software dbus-run-session --config-file=tests/fixtures/dbus-session.conf -- build/test/tests/viewer-smoke --gtest_filter='Document.*:ExifMetadata.*:ImageInformationProperties.*:Workflow.InformationCacheRefreshAndUnchangedSource:Browsing.*:Cache.*'`.
This passed 46 tests; one pre-existing opt-in large-folder workload was skipped. The added malformed optional-tag regression passed separately. Full acceptance initially exposed five old format assertions expecting decoder text or EXIF values without status; updated them to assert typed outcomes. `--gtest_filter='Release.*'` then passed all 10 tests. No application correction was needed; clean Release and isolated-runtime results remain valid.

New tests cover exhaustive outcome presentation and silent cancellation/worker cleanup, raster adapter propagation, controlled metadata device I/O failure, metadata limits, success without facts, payload-only no-status parsing, malformed optional tags, silent prefetch and successful pixels with quiet metadata failures through cache reuse and selection reset. Existing rapid replacement, shutdown, SVG, animation and orientation tests remain active.

## Evidence and review

Complete local logs are retained under `build/verification/shared-image-outcomes/` (ignored build artifacts). Build, test, static-analysis and runtime logs reviewed; expected corrupt-fixture decoder diagnostics and suppressed external-header tidy warnings do not indicate consumer failures. Final source diffs and local documentation links reviewed. No application or provider changes followed clean Release/runtime acceptance; later corrections affected test assertions and parameter names only.

## Limitations

Automated checks only. Native sharp-preview T5, mixed-monitor qualification and other previously deferred gates remain open. No performance claims. Commit and pinning are now authorized; umbrella integration remains deferred.

## Current qualification continuation — 2026-09-23

The original implementation evidence above is historical. The approved single-monitor
plan resumes umbrella integration and Files T5 against current published pins;
physical second-monitor qualification remains deferred until hardware arrives and
is not a closure gate for this iteration. Clipboard-service, unrelated release and
unknown-dimension fixture deferrals remain unchanged. Qualification initially
left new changes local; the subsequent **“publish and pin”** request authorizes
publication and the umbrella checkpoint.

Current acceptance complete — 2026-09-23: [umbrella evidence](../../../../docs/initiatives/shared-image-outcomes/SINGLE-MONITOR.md)
records passing provider/consumer/installer checks and the user report
**“walkthrough passed”**. Files T5 is complete on the approved actual 1/1.25/1.6/2
matrix, with original scale restored. The user subsequently authorized publication
and pinning; the umbrella ledger records exact published revisions, its final
integration decision and the single CI snapshot. Existing second-monitor,
clipboard-service, release and unknown-dimension deferrals remain unchanged.
