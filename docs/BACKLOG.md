# Backlog

The supplied roadmap sets this order. Each numbered stage is a separate SDD
cycle: approve requirements, approve design/tasks, implement, verify, document.
Do not mark a stage complete while acceptance checks remain pending.

| Stage | Deliverable | Acceptance gate |
| --- | --- | --- |
| 0. Close the scaffold | CI, stacking-compositor decorations, development versus installed registration | Clean-checkout build and installed launch; all scaffold checks closed |
| 1. Open an image | One local file via CLI, Open dialog, or drop; asynchronous decoding, orientation, fit, loading/errors | Common static formats; useful failures; no stale results during rapid replacement |
| 2. Inspect comfortably | Fit, actual size, pointer-centered zoom, mouse drag, keyboard pan | Predictable resize/fullscreen behavior, fractional scaling, oversized images |
| 3. Browse a folder | Supported siblings, natural filename order, previous/next, position, bounded cache/prefetch | Responsive large folders; rapid navigation and unreadable/disappearing files |
| 4. Complete the static-image workflow | Temporary rotate/flip, basic information, copy image/path, shortcut help | Composable transforms with unchanged originals; clipboard interoperability |
| 5. Release readiness | Supported-format declarations, desktop opening, installation, accessibility, performance | Packaged workflow on supported desktops without development paths |

First usable checkpoint: after stage 3, collect everyday static-browsing friction.
First stable release: after stage 5, explicitly scoped to static images.

Stage 0 retains the approved [scaffold cycle](sdd/project-scaffold/SPEC.md).
[Stage 1 requirements](sdd/image-document/SPEC.md),
[design](sdd/image-document/DESIGN.md), and [tasks](sdd/image-document/TASKS.md)
are approved. Image opening is implemented; see its verification record for
automated evidence and remaining native desktop acceptance.

[Stage 2 requirements](sdd/image-inspection/SPEC.md),
[design](sdd/image-inspection/DESIGN.md) and
[tasks](sdd/image-inspection/TASKS.md) are approved. Image inspection is implemented
with local automated and live Wayland checks passed; see its
[verification record](sdd/image-inspection/VERIFICATION.md) for evidence and the
pending native mixed-scale display check. Stage 0 and stage 1 acceptance remain
open independently.

[Stage 3 requirements](sdd/folder-browsing/SPEC.md),
[design](sdd/folder-browsing/DESIGN.md) and
[tasks](sdd/folder-browsing/TASKS.md) are approved through the implementation plan.
Folder browsing is implemented and its local acceptance checks pass: natural
supported-sibling snapshots, Page Up/Down and buttons, boundaries, error positions,
F5 refresh, bounded cache/prefetch, production input, and dark/light scale checks.
The 20,000-entry exercise measured a 625 ms scan, 363 ms for two large-image
navigations and 241,660 KiB peak RSS with continuing GUI timer progress. See the
[verification record](sdd/folder-browsing/VERIFICATION.md) for measurement scope
and limitations. Earlier native-desktop gates remain pending independently.
This reaches the first usable checkpoint.

[Stage 4 requirements](sdd/static-image-workflow/SPEC.md),
[design](sdd/static-image-workflow/DESIGN.md), and
[tasks](sdd/static-image-workflow/TASKS.md) are approved through the supplied plan.
Temporary composable transforms, information snapshots, asynchronous image copying,
path copying, Actions and shortcut help are implemented. Automated pixel/input/
metadata/lifecycle tests and XWayland separate-process clipboard transfer provide
local evidence. Stage 4 is **not accepted**: native Wayland clipboard transfer
remains unresolved. Earlier native-desktop gates remain pending independently.
See the [verification record](sdd/static-image-workflow/VERIFICATION.md) for measured
memory/latency, visual checks, and the exact remaining acceptance boundary.

## After the first release

1. GIF/WebP animation: playback, pause/resume, frame timing, bounded resources.
2. Command mode using established shared action definitions.
3. Mouse-revealed controls with predictable keyboard behavior, accessible focus,
   and discoverable shortcuts.
4. A separate trash workflow with confirmation/recovery and explicit failures.
5. Richer metadata and formats driven by actual usage.

Keep loading/document state in C++, presentation/input feedback in QML, and add
the directory model at stage 3. Use one local-only opening path; bound decoding
and cache memory and reject stale asynchronous results. Maintain fixtures for
orientation, transparency, large dimensions, corrupt data, Unicode and missing
files. Check responsiveness, appearance, scaling, keyboard use and installed
runtime at each relevant stage. Advertise only packaged, supported MIME types.

Adaptive CSD/SSD remains a shared holonight-qt initiative and must not block core
viewing. No compositor-name heuristics or Viewer-specific decoration policy.
Image editing, albums, tagging, photo databases, HoloNight Files and Quick Look
are outside this roadmap. Crop remains an uncommitted later possibility.

Stage 5 follows the approved [release-readiness cycle](sdd/release-readiness/SPEC.md).
Source installation qualification, four-format declarations, file-aware desktop
integration, accessibility announcements and interactive clipboard tooling are
implemented. **Stage 5 is not accepted**: full native desktop/accessibility/mixed-scale,
human clipboard lifecycle/performance and hosted CI gates remain open.
The approved Viewer fixes add explicit PNG clipboard publication and a bounded
simple-WebP fallback; compact decoding and the compositor-input generic Qt alpha
matrix now pass. Default HoloNight native Open remains an unresolved
[provider delegation blocker](holonight-qt-dlg-delegation-missed.md).
Installed-only container validation and basic native Orca/PNG clipboard checks pass. See the
[verification record](sdd/release-readiness/VERIFICATION.md) for actual evidence.

Fullscreen restoration follow-up: Viewer now changes only the fullscreen state
flag, preserving compositor-managed tiling. The native Hyprland regression passes;
see the [scaffold verification](sdd/project-scaffold/VERIFICATION.md) for evidence
and the separate stacking-desktop acceptance limitation.
