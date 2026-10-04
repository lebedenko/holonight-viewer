# Local CI Rehearsal

Baseline: 2f388a4a5f46c318644f4225ab4f5cff054c3cda.

`task ci` and GitHub invoke the same repository launcher and standard, sanitizer
and REUSE 6.2.0 lanes. Preserve `task check`, all ten dark/light scale pixel
checks, ASan/UBSan with uninstrumented providers, staged installation and the
second installed-runtime container without workspace mounts or networking.

The build image is immutable; supplemental archives are version/checksum pinned.
Providers retain Config 03fa635, Images ac11f23 and Thumbnails d27addc.
Qt is corrected from 61d0c16 to published f10e8c8ba57282e953f8ddc4a6b0c1109e05a3bf:
Viewer already sets `lowercaseLetters` on HnKeyHint/HnKeySequenceLabel in three
production components; the old pin lacks that API. Native acceptance exposed 14
failing CTest registrations. f10e8c8 first adds both properties and is on origin/main.
Build each provider in Debug with Wayland disabled as before. Application trees are fresh for every lane.

Snapshot current tracked edits/deletions and non-ignored new files, preserve modes
and symlinks, and report new inputs. Clone independent Git objects without checkout
or hardlinks, then index current inputs in that disposable clone; retain real HEAD
for versioning and include new files in Git-based static checks. Host source is
read-only, build state disposable, and logs/evidence host-owned under build/ci.

The host launcher builds/runs the captured installed-runtime context. Never expose
the Docker daemon to the build container. Runtime supplements are reverified and
installed offline; the runtime image ID is saved. Every required failure propagates.
No publication, registry upload or umbrella pin updates.

Native acceptance requires owned diagnostics to pass with clang 23. The correction
renames 21 internal field identifiers (181 references across 33 files); QML property
names, callable APIs and literal values remain stable. The grid keyboard regression
compares exact image size/pan relative to the viewport center because footer hints
can resize the viewport by one pixel. No tolerance or retry hides incorrect routing.
Twenty consecutive private-bus native regression runs pass. Qt tool resolution
prefers the configured lib/qt6/bin directory over unrelated executables in bin.

Five test helper headers also receive strict-check corrections: typed pixel storage
with QImage fill, explicit GIF byte casts, named ignored parameters, signed/unsigned
comparison and an explicit private enum base. A standalone baseline/current probe
confirms identical bytes for five frame identities and forty GIF encodings, including
identical buffer cleanup/peak counters (`build/ci/helper-equivalence/result.log`).
All existing check families and fixture expectations remain enabled.

## Verification — 2026-10-04

- Full `task ci` exits 0: `build/ci/20261004T141254Z-k945rnzt/` retains
  standard/sanitizer/licensing logs, immutable image identities, provider/tool
  provenance and results. Standard and ASan/UBSan each pass all 25 CTest checks,
  including all ten style/scale pixel checks (20 captured images per lane).
  Standard also passes complete task check and the separate installed-runtime
  container: six image formats, opening edge cases and desktop launch.
- The launcher audit confirms 11,496 source/development files retain their bytes,
  modes, modification times and symlink targets. Host-owned evidence is isolated
  below ignored build/ci; snapshots include current edits and new inputs.
- Native exact-provider `task check` passes with GCC 16.2.1, Qt 6.11.2 and
  clang-tidy 23.1.1 (`native-workspace/native-check-complete.log`). Subsequent
  helper rebuild/retest passes all 25 tests (`native-final-helper-retest.log`);
  all test translation units pass tidy (`native-tests-tidy-final.log`). The
  final math-parentheses correction passes focused tidy and all ten pixel checks
  (`native-menu-tidy-final.log`, `native-menu-tests-final.log`).
- A read-only pinned clang-tidy 22 audit covered all 58 owned translation units;
  its single additional math-parentheses diagnostic was corrected before the
  passing complete container run. No check family was disabled.
- Five launcher and three tooling regressions pass, covering edited/deleted/new/
  ignored/space-containing inputs, executable bits, symlinks, real HEAD/index,
  failure propagation, missing runtimes, rootless Podman arguments, explicit
  compile contexts and Qt executable priority. Actual Podman is unavailable.
- The CI runtime Dockerfile now gives its immutable image argument a matching
  default. Runtime-only rebuild/check evidence is in
  `build/ci/runtime-warning-followup/`; normal developer runtime packaging stays
  unchanged. REUSE and final diff review cover the completed documentation.

No pushes, remote acceptance claims, system installation or umbrella pin changes.
