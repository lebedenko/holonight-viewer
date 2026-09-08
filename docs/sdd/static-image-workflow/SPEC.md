# Stage 4: Static-image workflow

Approved through the supplied implementation plan (2026-09-08); this record
precedes implementation. GPL-3.0-or-later; repository REUSE metadata applies.

- S4-1: When R/Shift+R/H/V is invoked with a ready image, the viewer shall
  compose a clockwise/counterclockwise quarter turn or screen-relative flip,
  reset inspection to Fit, and focus the canvas. Actions shall offer Reset
  Transform. Selection/open/refresh shall reset orientation; files stay unchanged.
- S4-2: The viewer shall render all eight orientations without allocating a
  transformed image, retaining clipping, physical-pixel Actual Size and pan/zoom.
- S4-3: When I is invoked for a local selection, a modal scrollable Information
  dialog shall show selectable normalized absolute path, detected format, encoded
  size, local modification time, decoded and transformed dimensions. Worker
  snapshots shall follow request identity and cache entries; unavailable facts
  shall read Unavailable. F5 shall refresh the snapshot.
- S4-4: Ctrl+C shall copy the entire captured transformed image with alpha through
  the standard Qt clipboard. A dedicated worker shall prepare one operation with
  no queue; both copy commands shall be disabled until completion. Navigation
  shall not change the capture. Feedback shall identify its filename. Failure
  shall preserve the clipboard; shutdown shall cancel publication and drain.
- S4-5: Ctrl+Shift+C shall copy the unquoted normalized absolute local path,
  preserving symlinks, including during loading/errors.
- S4-6: One Actions button beside Open shall expose shared translated QML Actions.
  F1 shall open modal scrollable shortcut/gesture help. Dialogs shall suppress
  viewer shortcuts, permit text copying, consume Escape before fullscreen, and
  restore canvas focus. Footer shall provide a compact F1 hint.
- S4-7: Verification shall cover orientation/pixel agreement, production input,
  information lifecycle, separate-process clipboard transfer, responsiveness,
  memory, dark/light minimum/default windows at 1/1.25/1.5 scales, and contributor
  quality/install/desktop checks. Unresolved checks shall remain explicitly open.

Non-goals: directories, save/export, persistent edits, undo, animation, EXIF
photographic metadata, provider changes. Existing display/cache limits remain;
clipboard snapshot/output/platform storage is additional memory.
