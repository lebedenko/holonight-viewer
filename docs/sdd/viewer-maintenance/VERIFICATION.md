# Verification — 2026-10-01

## Regression evidence

- Before the SVG fix, both new regressions failed: Copy Image never became busy, and the GUI renderer did not disable animation.
- Before the GIF fix, the injected 48-byte budget reproduced a restored 128 MiB cache allowance, an extra retained frame zero, and retention after selecting the neighbor.
- Final focused GIF/SVG regressions passed 7/7, including frame-zero cache reuse, captured orientation/navigation, alpha, bounded geometry, local linked images, failure preservation and static SVG policy.

## Acceptance

| Check | Result |
| --- | --- |
| Local standard CTest | 25/25 entries passed |
| Local formatting, QML lint/import/type metadata, licensing and staged install | Passed |
| Local clang-tidy | One missing test-fixture initializer was found; corrected fixture and cache-reuse test passed targeted clang-tidy. All other translation units passed the original full run. |
| Local sanitizer CTest | 25/25 entries passed with leak detection enabled and no suppressions |
| Final local sanitizer smoke | 327 cases: 319 passed, 8 existing opt-in cases skipped |
| Clean pinned-image `task deps; task check` | Passed against the exact workflow provider revisions |
| Pinned-image `task sanitizer-check` | 25/25 entries passed; final updated smoke: 319 passed, 8 skipped |
| Installed-runtime image, no mount/network | Passed format decoding, CLI opening/rejection/recovery and installed GIO desktop launch |

The pinned base is `archlinux@sha256:eb8f6dcc89a38977c9735f10fcf6ae4afe496283e7008eb7a3420cdba31fbd04`, with Arch snapshot `2026/09/30` and Qt 6.11.2. Clean qualification used temporary source copies and archived workflow provider revisions, preserving sibling working trees. Installed-runtime qualification used the same toolchain image through `VIEWER_CI_IMAGE`.

Local evidence is under `build/maintenance-*.log`. Clean container package/compiler/CMake/Qt provenance is under the temporary CI workspace recorded in `build/maintenance-ci-workspace-path`; future CI runs upload the same provenance and CTest logs as artifacts.

## Pending native acceptance and limits

Run `task run -- /path/to/image.svg` and manually check SVG copy/paste, rotation and transparency; check GIF playback/navigation and SVG zoom. Native clipboard persistence, optional performance/large-folder scenarios and visual captures were not exercised by these offscreen runs. Project AGENTS.md requires manual native interaction and forbids automated desktop focus/pointer movement.

Providers and Qt remain uninstrumented. Memory accounting covers retained decoded buffers, not whole-process RSS. Temporary decode/conversion buffers, codec allocations, clipboard preparation/storage and canvas textures remain excluded. SVG local linked resources are read during worker preparation rather than snapshotted. No system installation or publication was performed.
