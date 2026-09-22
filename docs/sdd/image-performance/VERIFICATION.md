# Verification — 2026-09-23 (Europe/Kyiv)

Baseline production revision: `737d7a99849701f985e927a09f18c475231a6d1d`.
Only measurement tooling/tests and documentation changed. No production bottleneck
was demonstrated that warrants a correction; there is no candidate production delta
or claimed before/after speedup.

## Environment and method

Intel Core i9-14900HX, Linux 7.1.4, GCC 16.2.1, Qt 6.11.2, Release `-O3 -DNDEBUG`.
The clean build is `build/performance-acceptance`, with `BUILD_TESTING=ON`, `/usr`
install layout and the explicit project-local installed provider prefix.

Provider checkouts were clean at config `fe69a59e6b73167fd5349223a4d265d75386c139`,
Qt `863af4183bdf09ce05199b37e8f5dfb46a311ba1` and Images
`3633865d2f39e4f163f0159a0f252f88245379f0`. Existing provider builds use the same
compiler/Qt, Release and `BUILD_WAYLAND=OFF`; incremental builds reported no work.
All seven installed provider libraries matched their build artifacts by SHA-256.
Images production sources are unchanged from `efe3e78`; its later commit changes
only tests/documentation, so the existing provider artifact remains applicable.

Commands from the repository root:

```sh
cmake -S . -B build/performance-acceptance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps/prefix" \
  -DQML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml"
cmake --build build/performance-acceptance --parallel 2
python3 scripts/check-measure-release.py
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
  LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
  dbus-run-session --config-file=tests/fixtures/dbus-session.conf -- \
  build/performance-acceptance/tests/viewer-smoke \
  --gtest_filter='Document.*:Cache.*:Browsing.RapidRequestsForegroundPriorityCacheAndSilentPrefetch:Browsing.CacheHitAndMetadataInvalidation:AnimationController.*'
python3 scripts/measure-release.py build/performance-acceptance/tests/viewer-smoke \
  build/image-performance/baseline --scenario all
```

Clean Release build passed. All 45 focused cases and five Python runner tests passed.
The initial sandbox run could not bind the private D-Bus socket; the focused tests
and matrix then ran successfully outside the sandbox. No desktop input was automated.
All 25 matrix trials passed, sequentially, without competing builds/acceptance jobs.
Each scenario had five fresh processes. Output, exact binary/instrumentation hashes,
provider library hashes, CMake configuration, raw XML/logs, per-trial results and
20 ms RSS samples are in `build/image-performance/baseline/`.

## Measurements

Milliseconds unless stated otherwise; figures are median [minimum–maximum].

| Scenario / metric | Result |
|---|---|
| 32 MP PNG first open | 111 [111–112] |
| 32 MP PNG navigation | 111 [111–113] |
| PNG clipboard preparation (no receiver) | 614 [603–625] |
| Independent PNG encoding sample | 584 [579–593] |
| Large-workflow peak RSS, KiB | 423716 [423512–423792] |
| Navigation first open, 2048² PNG | 20 [20–20] |
| Twenty repeated selections within two-image working set | 333 [325–335] |
| Pressure traversal, 22 selections, cycle 1 | 691 [636–731] |
| Pressure traversal, cycle 2 | 716 [668–790] |
| Pressure traversal, cycle 3 | 671 [658–696] |
| Rapid burst to newest selected image | 30 [30–30] |
| Shutdown with pending work | 20 [20–21] |
| Navigation peak RSS, KiB | 123260 [114688–124352] |
| 32 MP GIF replaceFrame, nanoseconds | 9600 [6687–11414] |
| GIF playback GUI sampling gap | 11 [11–12] |
| GIF playback peak RSS, KiB | 548392 [548120–548412] |
| 200 MiB GIF open | 20 [20–30] |
| GIF frame count available | 50 [40–60] |
| GIF scanning GUI sampling gap | 10 [10–10] |
| GIF scanning peak RSS, KiB | 59360 [58844–59512] |
| 20,000-entry folder scan / initial open | 91 [90–91] |
| Large-folder next two images | 182 [171–202] |
| Large-folder peak RSS, KiB | 246068 [245628–246376] |

## Findings and limits

Navigation's sampled RSS was exactly flat across all three pressure-cycle endpoints
in every trial: 115312, 115476, 115244, 115416 and 115304 KiB respectively. Review of
the existing cache confirms a two-entry cap as well as the 128 MiB byte cap; the
12-image traversal exceeds both aggregate working-set limits. Warm navigation,
pressure traversal, newest-selection handling and shutdown completed correctly.
Existing injected-decoder/cache tests also passed. These bounded runs provide no
evidence of sustained cache growth or a broken eviction/cancellation contract.

The measured clipboard preparation is largely explained by its independent PNG
encoding sample. Its GUI sampling gap stayed at 11 ms, as did static navigation.
Both GIF exercises met their unchanged <50 ms responsiveness assertions. There is
no demonstrated GUI-thread encoding/decode stall here that justifies an optimization.

Timings include QtTest polling/scheduler granularity; recorded zero-millisecond
transforms mean below this timer's resolution, not zero cost. RSS includes fixture
construction, codec/allocator retention and clipboard preparation. Kernel wait4
peak RSS and /proc samples use different accounting/sampling and are preserved as
reported. Shutdown RSS is sampled while the document/process still exists; it is
not a leak assertion. An empty Viewer cache does not imply a cold filesystem cache.
The generated solid-color PNGs are highly compressible; these workloads are not a
representative corpus of all photographs/codecs. No native rendering, clipboard transfer, mixed-display or new cross-machine
performance guarantee is claimed. Earlier native acceptance remains historical.

## Contributor acceptance

`task check` built Debug/Release/test and passed all 22 Debug CTest entries (smoke:
182 passed, eight opt-in/native skips, including the new opt-in navigation exercise).
It then stopped at the host's `/usr/bin/qmlformat` version 1.0. The existing supported
override `QMLFORMAT=/usr/lib/qt6/bin/qmlformat` selects Qt 6.11.2; `task format-check`
then passed. No formatter discovery or QML source was changed.

Continued the remaining acceptance stages individually. `task lint` completed its
full C++ tidy inventory and found three new benchmark style errors (two explicit
arithmetic-parentheses requirements and a short local variable name). Corrected
those without changing behavior. Rechecked the affected translation unit with the
same project runner/configuration:

```sh
run-clang-tidy -p build/test -removed-arg=-mno-direct-extern-access \
  -config-file=.clang-tidy -j 2 tests/release_performance_test.cpp
task qml-lint
task license-check
task install-check
task qml-import-check
task qmltypes-check
ctest --test-dir build/performance-acceptance --output-on-failure
```

All passed, including all 22 clean Release CTest entries and REUSE 238/238 files.
A preliminary direct clang-tidy invocation rejected the GCC-only option; the project
runner above correctly removes it. All remaining tidy diagnostics were suppressed
system-header/NOLINT notices. Build logs contained no compiler warnings. Expected
libpng read-error output belongs to existing deliberately damaged-input tests.
Final formatting, diff whitespace and local Markdown links passed.

After the style corrections, rebuilt Release and ran only the affected navigation
scenario again with identical workload/providers, five sequential processes and no
competing acceptance jobs:

```sh
python3 scripts/measure-release.py build/performance-acceptance/tests/viewer-smoke \
  build/image-performance/final-navigation --scenario navigation
```

All five passed. Final median [range]: warm 20 selections 325 [324–335] ms;
pressure cycles 677 [661–706], 656 [634–669], 681 [666–723] ms; newest selection
31 [30–41] ms; shutdown 20 [20–21] ms; GUI gap 11 [11–11] ms; peak RSS
123520 [113504–123940] KiB. Each trial again had identical RSS at all three pressure
checkpoints. The earlier full matrix is preserved above; final-navigation artifacts
capture the final instrumentation/binary hashes. No production speedup is claimed.

Logs: `/tmp/viewer-performance-{configure,build,final-build,focused,matrix,check,lint,`
`tidy-final,qmllint,license,install,imports,qmltypes,release-ctest,rebuild,format-final,`
`final-navigation}.log`; raw benchmark logs/XML/JSON remain under the two build
artifact directories. Full acceptance output and final diff were reviewed.
No new installed-runtime container is required because application code is unchanged.
Historical native/focus-automating harnesses were not used. No commits, publication,
CI queries or umbrella pin changes are part of this iteration.
