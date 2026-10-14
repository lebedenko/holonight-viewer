# External dependency management — holonight-viewer

Work package: EDM-014
Baseline: `2b696a328ec109bd11fe6e1b67e37ea727c8bff5`
Umbrella audit: `38146d8f474317f3f8d72699f60e6d16a71dd165`
Status: Done locally; accepted handoff checkpoint `741e7e9ce6d180c89671b3111dc4be10832020ce`.
Implementation baseline: `74eee36c7484b37dcece55b5cdf71d84f21c93bb` (clean canonical origin/main confirmed 2026-10-14).

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall explicitly prepare supplied local providers and check installed external prerequisites.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG and any GMock targets they use.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/holonight-viewer.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Preserve existing dependency interfaces and component switches; adopt installed GTest CONFIG discovery where tests use it.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, installed-package consumers and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and manual checks.
- [x] Make a repository-local commit only after local verification; publish only when authorized.

Implementation and clean automated acceptance are complete locally. Publication and pinning remain separate authorized handoffs.

## EDM-014 implementation contract — 2026-10-14

Only Viewer is assigned by umbrella checkpoint `741e7e9ce6d180c89671b3111dc4be10832020ce`, following `9aa9460751a05a27e662a8591c1fa3f10fbd9b99`.
The original audit baseline above remains authoritative for the audit. Canonical origin/main was confirmed for
Viewer and each provider before implementation. No publication or gitlink change is included.

Preparation order and exact revisions:

| Provider | Revision | Options |
|---|---|---|
| Config | `0ccd25f323662f50a47bb048be636160f2b7c135` | Debug, BUILD_TESTING=OFF |
| Qt | `7c3ce074a8c2e02e4ee8cad8cd19eb8627aa4f68` | Debug, tests/demo/gallery OFF, final BUILD_WAYLAND=ON |
| Images | `ae827d78db9cc21f9537c27f50f49baf879324da` | Debug, BUILD_TESTING=OFF |
| Thumbnails | `c9ae0ad9a64e8ce28ee669b5463ef1bec74bc16d` | Debug, BUILD_TESTING=OFF |

Canonical declarations and bundle 2.0.0 are copied into this standalone repository. Viewer acceptance explicitly
requires WebP fixture encoding, GIO launching, AppStream/desktop validation, Task, format/tidy and REUSE.
Images continues to own codec implementation dependencies. Runtime image-format plugin requirements remain explicit.
CMake requires installed GTest >=1.14 CONFIG and public Qt >=6.11 with exact matching private Qt.
Internal packages use only approved prefixes; external prefixes and system discovery remain supported.
Normal configure/build/test never prepare providers or download sources. Existing presets are preserved.

`task ci:prepare` acquires both immutable images and exact provider sources, then runs `task deps PRESET=test`
in the qualified environment offline. `--provider-source-root ..` clones exact commits from local canonical
repositories, ignoring dirty working-tree contents. `task deps` remains the explicit developer preparation command.
Prepared files, permissions and symlinks are hash validated before acceptance; incompatible state is archived.
A contract refresh may seed intact providers only when image, platform and provider revisions match; canonical
workflow source/toolchain/options/external checks decide whether to reuse them.

All three original lanes remain: standard, sanitizer and licensing. Acceptance is offline (`--network=none`,
`--pull=never`); runtime construction uses `--network=none --pull=false`. Hosted Licensing acquires the pinned
REUSE image explicitly before offline use. Active supplement bootstrap/extraction scripts are removed;
`scripts/ci/packages.json` remains historical audit evidence. Build and runtime Dockerfiles select the same
qualified full Arch digest. Acceptance sees installed providers and revision records, without provider source/build
mounts. Runtime staging copies the verified provider prefix, production Release payload and qualification-only
probe, preserving `/usr/bin/hn-viewer`, desktop metadata and development-QML isolation. No application API/CLI changes.

Strict tidy uses the explicit test compiler database, scoped coverage validation and one invocation per source.
Synchronization preserves the existing configured `lib/qt6/bin` lookup precedence, now also corrected canonically.
The shared empty-brace trailing-comma policy is retained with all other checks strict.

The first clean acceptance exposed two pre-existing test-only `QDragEnterEvent` calls passing `QPointF` where
qualified Qt 6.11.2 requires `QPoint`. Both use the existing integer point directly; coordinates, event dispatch
and assertions are preserved. This compatibility correction is verified separately before final clean acceptance.

Fresh native desktop review, performance campaigns, Debian qualification, host installation and application
restarts are outside EDM-014. Manual native ecosystem review remains part of EDM-022.

Qualified Clang 22 also requires behavior-preserving explicit raw-pointer null comparisons in grid selection/
ViewerController, a designated InformationSection initializer and an explicit defaulted ThumbnailCache destructor
(the other four special members were already deleted). No checks are disabled. The complete diagnostic pass
covered all 64 owned translation units; corrected sources and all reported header consumers receive focused
strict verification before clean acceptance. These six source/header/test files form separate compatibility commit
`b2aaa5f5b019c59ce88ccb36f2c03532e6835f1a` (`fix(build): preserve compatibility with qualified Qt and Clang`).

## Verified local acceptance — 2026-10-14

Final command `JOBS=8 python3 scripts/ci/run.py` passes all three lanes at
`build/ci/20261014T135150Z-mc5g60en`. Standard includes clean Debug and Release, all 27 CTest registrations,
private-bus portal cases, ten dark/light fractional-scale separator combinations, Fusion controls, strict
format/tidy, QML import/type/lint contracts, REUSE, staged installation and the second installed-runtime container.
Strict tidy checks all 64 owned translation units using the explicit test database (167 compiler contexts,
139 files including generated sources; only the 64 owned sources are analyzed). The complete sanitizer
ASan/UBSan suite passes 27/27; full test logs contain no AddressSanitizer, LeakSanitizer or UBSan reports,
including intentional startup-failure cases. Licensing passes separately with pinned REUSE 6.2.0.

Main smoke in each lane runs 379 cases: 371 pass and the eight existing opt-in/native-only cases stay skipped.
The retained skips are five ReleasePerformance exercises (including native Wayland), native clipboard persistence,
large-folder resource measurement and FooterKeyHints.VisualCapture requiring VIEWER_CAPTURE_PREFIX. Exact names
are in VERIFICATION.json. No new skip or weaker assertion is introduced; automated pixel capture still passes.

Installed runtime image `sha256:a3d922871e58eb553c277facf4a6faeda70761c95a7dcb85c4497a2639365c76`
is constructed and run offline. It has no workspace mount or development QML/library environment; ELF runtime
paths contain no development directories. The qualification probe decodes PNG, JPEG, BMP, WebP, GIF and TIFF;
CLI Unicode/spaces/dash/rejection/recovery contracts and GIO launch for all seven formats pass. Desktop and
AppStream validation, including negative controls, pass. Production `/usr/bin/hn-viewer` and qualification-only
`/usr/libexec/holonight-viewer/installed-runtime-probe` retain their existing staging boundaries.

Focused verification passes 13 launcher, 3 tidy-context, 12 real-CMake dependency and 32 canonical tooling tests.
Both corrected drag/drop cases pass independently. Canonical/copied JSON and CMake declarations match;
bundle drift, generated installer-profile consistency, Python AST, shell and YAML syntax, and whitespace checks pass.
All 68 umbrella installer fixtures pass in the qualified offline container after correcting the fixture's stale
QMarkdown version assertion to the canonical 1.1 requirement already present at the starting checkpoint.
The initial host fixture attempt additionally encountered isolated-shell startup interference; no host shell
configuration was modified. This is a fixture correction, not an external package contract change.

Preparation commands and compatible refresh logs are preserved at
`build/ci/20261014T132239Z-n7ahc24p` and `build/ci/20261014T132517Z-cpej0555`.
The initial failed acceptance at `build/ci/20261014T132615Z-6dst6pdo` and incremental diagnostic/correction
logs remain available. All known corrections were batched before final clean acceptance. Compatible providers
were reused only after exact image/revision/options/toolchain/content checks; no provider acceptance build is
claimed from an incompatible cache. The first preparation's Taskfile syntax failure occurred before provider builds.

Complete logs were reviewed. Remaining notices are provider private-Qt ABI notices (one exact Qt/image is used),
optional Vulkan headers unavailable on existing software/OpenGL paths, suppressed external-header/existing NOLINT
counts, expected negative-fixture diagnostics and deliberate libpng read/write errors in passing damaged-image/
forced-encoding-failure tests. There are no actionable owned compiler, tidy, QML or sanitizer diagnostics.

Environment: full Arch image digest `37ca051c39d1688d6413c47d94b8c0f221152dac6981ec67d155f2c43ea17b40`,
local ID `sha256:51bb1aed43c0ad4e9336ed49962862ec78293f4380fd05e73b954e88c6c479c7`;
Qt 6.11.2, GCC 16.2.1, CMake 4.4.3, Ninja 1.13.2, Clang format/tidy 22.1.8, Task 3.53.1,
GTest 1.18.0, Python 3.14.7 and REUSE 6.2.0. The full package list, exact provider options/source hashes,
compiler identities, accepted implementation hashes, manifest and log hashes, runtime image ID and retained
skips are in [VERIFICATION.json](VERIFICATION.json).

Documentation/evidence REUSE closure passes at `build/ci/20261014T141446Z-ma3xhh2f`; syntax and drift/whitespace checks also pass. Application
acceptance is not repeated for documentation-only changes. Viewer commits remain local; no publication,
current hosted CI success, gitlink update or ecosystem integration is claimed. The initiative stays Accepted.
Fresh user-operated native review remains EDM-022; no host installation/restart or new performance campaign ran.
