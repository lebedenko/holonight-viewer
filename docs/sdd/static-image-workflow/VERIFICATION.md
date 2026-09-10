# Stage 4 verification

Implementation and **Stage 4 acceptance are complete as of 2026-09-09**.
The native Wayland clipboard gate was closed by subsequent automated native
pixel/transport results and the user's completed keyboard/menu, eight-orientation,
special/symlink path and in-progress navigation/shutdown walkthrough. See
[release evidence](../release-readiness/VERIFICATION.md). Clipboard loss after
exit without a persistence service is expected under the desktop-managed
persistence contract. Service qualification and native performance comparisons
remain Stage 5 gates. Earlier scaffold, image-opening and mixed-display native
gates remain open independently.
Requirements/design/tasks were recorded before implementation and authorized by
the supplied implementation plan. No provider sources were modified.

## Automated evidence (2026-09-08)

- `task deps`, `task build`, `task test`: local installed providers, debug build,
  and six CTest entries. `viewer-smoke` includes 35 tests; the native clipboard
  transfer and earlier opt-in folder resource benchmark skip under offscreen.
- `Workflow.OrientationsCompositionPaintingAndClipping`: independent pixel-index
  reference for all eight orientations; invocation order, inverses, swapped
  dimensions, alpha, original cache key, clipped painting, physical-pixel Actual
  Size at DPR 1, 1.25 and 1.5. Copy and painting agree with that reference.
- `Document.Orientation`: all eight embedded JPEG orientations, decoded metadata
  dimensions/format, then temporary transforms. Existing original-byte checks and
  `Workflow.InformationCacheRefreshAndUnchangedSource` check read-only behavior.
- Information tests cover cache round trips, transforms/reset, refreshed changed
  dimensions, deleted selections, unavailable/error facts and stale decode results.
- Clipboard controller tests cover immutable capture, one operation/no queue,
  both-command guards, preserved previous clipboard on preparation failure,
  filename feedback, primary-selection preservation when supported, and shutdown
  during preparation. Existing document shutdown test drains all three workers.
- `Viewer.StaticWorkflowControls` uses the production QML window: all new key
  commands, menu invocation, Fit/focus reset, F5 reset, modal shortcut suppression,
  text selection/copy, scrolling, Escape/fullscreen precedence, and reaching Help
  through the constrained minimum-size menu. Existing inspection/browsing tests
  cover pointer zoom, pan, navigation and stable Tab focus across document states.
- All seven Stage 4 XWayland tests pass, including production menu/dialog input
  and primary-selection preservation. `Workflow.ClipboardSeparateProcess` passes all eight orientations and
  alpha through a separate rendered Qt receiver, including navigation after
  capture and a normalized Unicode/space/symlink path. This uses platform MIME
  transfer rather than reading the sender's own QMimeData.

Generated logs/XML and screenshots are under `build/stage4-*` and `build/visual/`.
Run the native transfer explicitly with the normal provider environment:

```sh
QT_QPA_PLATFORM=xcb build/test/tests/viewer-smoke \
  --gtest_filter=Workflow.ClipboardSeparateProcess
```

The receiver is built as `build/test/tests/clipboard-probe`. Tests preserve their
fixtures under build/ or use automatically cleaned temporary directories there.

## Resource measurements

`Workflow.LargeCopyResponsiveness` uses an accepted 8000×4000 premultiplied image,
rotates clockwise, and waits while a 1 ms GUI timer runs. A Python subprocess
wrapper records `resource.getrusage(RUSAGE_CHILDREN).ru_maxrss`.

| Mode | Preparation/publication | Timer ticks | Peak RSS |
| --- | --- | --- | --- |
| Offscreen isolated copy | 40 ms | 4 | 294,024 KiB |
| XWayland copy and separate Qt receiver | 43 ms; 2,751 ms through transfer | 4 during preparation; 170 through transfer | 416,100 KiB |

The XWayland test process including setup took 2.95 seconds. Enable receiver
measurement with `VIEWER_COPY_TRANSFER=1` on a native platform. XML records
`copy_ms`, `copy_and_transfer_ms`, timer counts, snapshot and output byte sizes.
The snapshot and output each hold 128,000,000 bytes in this exercise. The initial
snapshot shares pixels; navigation can make it additional retained storage. The
existing 256 MiB display/cache bound excludes a retained copy snapshot (up to
128 MiB), transformed output (up to 128 MiB), and Qt/platform transport copies.
RSS is the maximum individual child-process high-water mark, not summed process
memory. Decoder/cache-filled production rendering and external clipboard-manager
storage are not included; no whole-process or desktop clipboard bound is claimed.
Clipboard persistence after exit is platform-managed. Qt MIME encoding/transport
may add GUI-thread work after worker preparation; timer progress is evidence of
continued event processing, not a hard maximum-stall guarantee.

## Quality and visual checks

Final local results: six CTest entries pass; all seven native XWayland Stage 4
tests pass. Tidy, QML lint, formatting, licensing, staged installation and desktop
checks pass. The separate Wayland clipboard gate was unresolved at this point;
the later results linked above close it.

Contributor checks: release build (`task build PRESET=release`, also exercised by
`task install-check`), formatting, tidy, QML lint, REUSE licensing, staged install,
and desktop registration separation. REUSE initially hit a sandbox restriction on
its multiprocessing socket; `task license-check` passed with desktop-host approval
(78/78 files with licensing/copyright metadata).

`bash scripts/check-visual.sh` exercises dark/light themes, 420×280 and 1000×700
windows at scale 1, 1.25 and 1.5. The Stage 4 captures cover the Actions menu,
Information and Help. Visual inspection found and corrected truncated menu labels
and minimum-window overflow: the menu is wide enough for current labels, uses an
in-window scrolling list, and restores focus on closure. Dialogs use the installed
style/fallback and HoloNight palette; scrollbars remain visible for overflow.
Generated contact sheets are inspection aids under build/visual/.

## Historical native clipboard failure (superseded)

The following records the original failure and required follow-up, not the current
acceptance status. Later native tests and human checks linked above supersede it.

Native Wayland production-window shortcut/dialog tests passed before the final
menu refinements. Automated separate-process clipboard attempts initially could
not read data with an unrendered receiver; a rendered receiver subsequently read
stale images instead of the newly published orientations. This suggests a native
input/focus/clipboard-ownership issue in the synthetic harness, but that cause is
**not established** and the failures are not counted as passes. Verify Copy Image
and Copy Path through real keyboard/menu input on Wayland, including a separately
focused receiving application, before accepting Stage 4. Do not infer native
Wayland interoperability from offscreen or XWayland success. Retain earlier native
desktop and mixed-display gates independently.
