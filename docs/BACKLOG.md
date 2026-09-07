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
