# HoloNight Viewer GIF Animation Playback — Requirements Specification

**Status:** Requirements authorized. This specification defines animated GIF playback for HoloNight Viewer, including autoplay with frame-delay clamping, manual play/pause control via Space, a transient play/pause button, lazy frame-count discovery, frame-streaming decoding, and failure handling for truncated or corrupted frames.

---

## Overview

HoloNight Viewer is a Qt6/QML static image viewer (apps/viewer: Main.qml, C++ ImageDocument/ImageCanvas via QImageReader). This specification adds support for animated GIF playback, replacing the current behavior "animated files show first frame only" for GIF format exclusively. GIF becomes a guaranteed format alongside PNG, JPEG, BMP, and WebP (static). Animated WebP and APNG are unchanged.

The playback system shall:

1. **Autoplay and Timing** — GIFs autoplay on open, honoring file frame delays and loop count; delays ≤10 ms are clamped to 100 ms.
2. **Manual Control** — Space toggles play/pause; a transient 48 px circle button shows a play or pause glyph and shares visibility/fade with navigation arrows.
3. **Decoding Strategy** — Stream one frame at a time through one open QImageReader with one-frame look-ahead decoded off-thread; rewind via jumpToImage(0) on loop.
4. **State Persistence** — Pause/resume reuses the open reader without re-decoding; manual pause survives transforms; resume continues with the current frame's remaining delay.
5. **Transient Overlay** — Display "Animated", frame count (lazy), and playing/paused state; frame count appears once known or when playback wraps.
6. **Failure Handling** — Single-frame GIFs follow the static path; truncated/corrupt frames stop playback, paused, with a notice; frame 0 failures or oversized frames trigger the normal error state.
7. **Interaction Orthogonality** — Zoom, pan, rotate, flip apply to the current frame; copy puts the current frame on clipboard as PNG.

---

## Scope & Non-Goals

### In Scope

- Animated GIF format detection and playback (GIF87a, GIF89a).
- Frame streaming via QImageReader with one-frame look-ahead decoding off-thread.
- Autoplay on open with frame delay clamping (≤10 ms → 100 ms) and loop count preservation (infinite stays infinite).
- Manual play/pause via Space key; Space binding in shortcut help popup and footer key hints via shared semantic key hints.
- Transient 48 px play/pause button (circle, glyphs) sharing arrowsShown state and 2000 ms arrow timer (hover-hold behavior).
- Frame-fade behavior (150 ms opacity animation, faded button non-clickable and invisible to accessibility).
- Transient information overlay showing "Animated", frame count (lazy), and playing/paused state.
- Pause/resume without re-decoding current frame; resume continues with remaining delay.
- Zoom, pan, rotate, flip applying to current frame during playback without auto-pause; manual pause survives transforms.
- Copy current frame to clipboard as PNG (snapshot, non-pausing).
- Single-frame GIFs treated as static (no button, no "Animated" label, no Space binding).
- Failure after frame 0: stop on last good frame, paused, non-modal notice in details strip ("Playback stopped: damaged frame").
- Auto-pause on window hidden/minimized or modal open; resume if playing before (user pause never overridden).
- Reader/look-ahead cancellation on navigation/close/new open via generation tokens; no stale frame published.
- Transparency and disposal via Qt GIF plugin; frames composited over viewer background.
- Missing Qt GIF plugin: startup unaffected; opening GIF reports normal unsupported-format error; qualification fails per README.
- Per-frame limits: 32 million pixels, 32,768 px per axis, 128 MiB per decoded image (checked before decode from reader size).
- Fixed 256 MiB displayed-image plus decode-cache budget; encoded input cap 256 MiB unchanged; frame count does not grow budget.
- QML presentation in apps/viewer/qml; shared design primitives from holonight-qt; glyph provider per REQ-F-010.
- Integration into format-check, qml-import-check, qmltypes-check, qml-lint, and CTest acceptance.

### Out of Scope

- Animated WebP or APNG playback (GIF-only in this cycle).
- Scrubbing, frame-stepping, or speed control.
- Reduced-motion handling (no portable Qt signal).
- Exporting GIF via clipboard or file save.
- File watching (file changed/deleted during playback continues via open handle).
- A specialised texture-update path in image_canvas (per-frame image replacement is acceptable if measured within REQ-NF-003; otherwise raise it in design).
- Frame-stepping or seeking to a specific frame, so no requirement may depend on it.

---

## Glossary

| Term | Definition |
|------|-----------|
| Animated Image | A multi-frame image format where playback sequentially displays frames according to per-frame delays and a loop count. |
| Frame | A single image in a multi-frame sequence; each frame has a delay (in milliseconds) and may be subject to disposal/transparency rules. |
| Frame Count | The number of frames in the GIF; unknown at open, and known once the same reader reports it or playback first reaches the end of the sequence. |
| Frame Delay | Millisecond duration a frame is displayed before advancing to the next; ≤10 ms is clamped to 100 ms. |
| Look-Ahead Decoding | Background off-thread decoding of the next frame while the current frame is displayed; improves playback smoothness. |
| Loop Policy | How many times the sequence plays, derived from the file's loop field as reported by Qt: infinite, or a finite number of total plays (a GIF without a loop field plays once). The raw Qt convention is pinned by a characterisation test and confined to one adapter (DESIGN §9). |
| Play/Pause Button | A 48 px circular button showing a play glyph (paused) or pause glyph (playing); shares arrowsShown state and 2000 ms timer with navigation arrows. |
| arrowsShown | A boolean property reflecting visibility state of transient UI elements (arrows, play/pause button); independent of fade progress. |
| Playback Stopped Notice | A non-modal message in the details strip ("Playback stopped: damaged frame") when playback halts due to truncated or corrupted frames. |
| Reader | QImageReader instance for streaming GIF frames; one open handle per document. |
| Space Binding | Window-level Space key toggling play/pause; it yields to a keyboard-focused button, which activates instead. |
| User Pause | A pause initiated by the user (Space key or button click); survives transforms and modal auto-pause; resume is manual or on modal close. |
| Modal Active | State where a modal dialog (file chooser, help, info popup) is open; playback auto-pauses while modal is open. |
| Generation Token | A unique identifier incremented on navigation/close/new open; used to discard stale frames from look-ahead worker. |
| Truncated/Corrupt Frame | A frame that fails to decode (e.g., file ends mid-frame, pixel data is invalid, frame oversizes); playback halts without error dialog. |

---

## Functional Requirements

### F.1 Autoplay and Frame Timing

**REQ-F-001 (Event-driven — Autoplay on Open)**

When the user opens a GIF file and decoding frame 0 succeeds, the GIF shall immediately autoplay: the playback timer shall start, displaying frame 0 for its per-frame delay, then advancing to frame 1 after the delay.

**Acceptance Criteria:**
- An automated test drives an injectable clock (REQ-C-011) over a two-frame GIF with frame 0 delay 500 ms and frame 1 delay 300 ms; asserts frame 0 is shown at open, still shown at clock time 499 ms, and frame 1 is shown at 500 ms.
- A manual check opening a GIF through the Open action confirms autoplay starts without user input.

**REQ-F-002 (Ubiquitous — Frame Delay Clamping)**

Any frame with a per-file delay value of 10 milliseconds or less shall be clamped to 100 milliseconds; all other delays remain unchanged.

**Acceptance Criteria:**
- An automated test creates a GIF with frame delays [0 ms, 5 ms, 10 ms, 11 ms, 100 ms, 200 ms] and asserts, on the injectable clock, that each frame is displayed for exactly [100, 100, 100, 11, 100, 200] ms.
- QImageReader.nextImageDelay() returns the original value; clamping is applied only in playback timing.

**REQ-F-003 (Ubiquitous — Loop Count Preservation)**

A GIF's loop policy (from file metadata, as reported by QImageReader) shall be honored: an infinite loop repeats indefinitely; a finite policy of P total plays plays the sequence P times, then stops on the final frame, paused; a GIF with no loop field plays once.

**Acceptance Criteria:**
- An automated test loads a GIF whose file requests exactly one play; asserts playback stops on the final frame, does not return to frame 0, and the button then shows the play glyph.
- An automated test loads a GIF whose file requests an infinite loop; asserts playback is still running after twice the frame count of clock advances.
- A characterisation test pins how Qt reports each file case (no loop field, infinite, N repeats) and how it maps to total plays; the other loop tests derive their expectations from that table.
- A manual check with a finite-loop GIF (e.g. 3 repetitions) confirms playback stops after the final frame in the third repetition.

---

### F.2 Manual Play/Pause Control

**REQ-F-004 (Event-driven — Space Toggles Play/Pause)**

When the user presses Space while an animated image is loaded, no modal is open and no button has keyboard focus, playback shall toggle: if playing, pause (stop advancing frames); if paused, resume (continue from current frame). When the current image is not animated or is a single-frame GIF, Space shall have no effect.

**Acceptance Criteria:**
- An automated test opens a multi-frame GIF playing frame 0; after frame 0's delay elapses, presses Space; asserts playback is paused and the image remains on frame 0.
- A test presses Space again; asserts playback resumes and advances to frame 1 after the frame 1 delay.
- A test opens a static PNG and presses Space; asserts no state changes (no pause/play tracking).
- A test opens a single-frame GIF; presses Space; asserts no button and no pause/play state.

**REQ-F-005 (Event-driven — Play/Pause Button Click)**

When the user clicks the play/pause button while an animated image is loaded and no modal is open, playback shall toggle (same as Space).

**Acceptance Criteria:**
- An automated test opens a multi-frame GIF, waits for the button to appear (via arrowsShown), clicks the button; asserts playback is paused.
- A test clicks the button again; asserts playback resumes.

**REQ-F-006 (State-driven — Play/Pause Button Visibility)**

The play/pause button shall appear and disappear with the navigation arrows (previousButton, nextButton). The button's objectName shall be "playPauseButton". It shall use the same arrowsShown state and 2000 ms arrow timer (pointer movement, hover-hold) as the arrows; it shall fade over 150 ms (opacity animation); while faded (opacity ≈ 0), it shall not be clickable or exposed to accessibility.

**Acceptance Criteria:**
- An automated test opens a GIF, asserts playPauseButton exists with objectName "playPauseButton".
- After pointer movement over the canvas, arrowsShown becomes true within one frame and playPauseButton's opacity is 1.
- After 2 seconds without pointer movement, arrowsShown becomes false and playPauseButton's opacity animates to 0 over ~150 ms.
- The button's visibility is driven by the same arrowsShown flag and the same 150 ms opacity behavior as the arrows, so they fade in and out together.
- Clicking the button's former position 300 ms after the fade began (opacity ≈ 0) does not toggle playback.
- While faded, the accessibility tree does not expose a play/pause action node.

**REQ-F-007 (State-driven — Button Enabled Only for Animated)**

The play/pause button shall be enabled (clickable, accessible name visible) only when the ImageDocument is Ready and the current image is animated (multi-frame). For static or single-frame images, the button shall be disabled (not clickable, grayed out or hidden per accessibility).

**Acceptance Criteria:**
- An automated test opens a multi-frame GIF; asserts playPauseButton is enabled (can be clicked).
- A test opens a static PNG; asserts playPauseButton is disabled.
- A test opens a single-frame GIF; asserts playPauseButton is disabled.
- A test with no image open; asserts playPauseButton is disabled.

**REQ-F-008 (State-driven — Button Appearance)**

The play/pause button shall be a 48 px circle, centered in the canvas area. While paused, it shall display a play glyph; while playing, a pause glyph (the glyph shows the action available). Both glyphs shall be rendered at a consistent size relative to the button; the glyph source is decided in design (REQ-F-010). The button shall use the same floating-button surface as the previous/next arrows.

**Acceptance Criteria:**
- A manual visual check loads a multi-frame GIF; asserts the button is a 48×48 px circle, centered in the canvas area, and shows the pause glyph (playing) — the play glyph appears after pausing.
- After pressing Space to pause, the glyph changes to the play symbol.
- After resuming, the glyph returns to the pause symbol.
- The button surface matches previousButton's floating surface (same palette tokens, verified in QML).

**REQ-F-009 (Ubiquitous — Accessible Name)**

The play/pause button shall have an accessible name that is the action it offers: "Play" while paused, "Pause" while playing. When the image is not animated the button is not exposed.

**Acceptance Criteria:**
- An automated accessibility test finds the button with role Button and Accessible.name "Play" while paused.
- After pressing Space to pause, Accessible.name stays "Play"; after resuming it is "Pause" (the name is the available action, matching the glyph in REQ-F-008).
- With a static image, Accessible.name is empty or the button's Accessible.ignored is true.

---

### F.3 Playback State and Glyphs

**REQ-F-010 (Ubiquitous — Glyph Source)**

Play and pause glyphs shall come from one provider chosen in the design stage: the shared icon repository if it already ships suitable glyphs, otherwise viewer-owned assets in apps/viewer/icons (as fullscreen.svg and information.svg are today), following the same HnIcon usage as existing viewer icons.

**Acceptance Criteria:**
- The DESIGN.md records the chosen provider and why.
- Both glyphs are rendered through the same HnIcon path as the existing viewer icons, at a size that leaves at least 8 px padding inside the 48 px button.
- A visual check at 1× and 2× device pixel ratio shows both glyphs crisp and centered.

---

### F.4 Information Overlay

**REQ-F-011 (State-driven — Overlay Content)**

While an animated image is loaded, the details strip shall include an "Animated" label, the playing/paused state, and, once known, the frame count as "N frames". The strip keeps its existing reveal and 3-second conceal rules and its 150 ms fade.

**Acceptance Criteria:**
- An automated test opens a multi-frame GIF and reveals the strip; asserts it contains "Animated" and "Playing".
- After Space, the strip shows "Paused" and still shows "Animated".
- Before the frame count is known, no "frames" text appears; after it is known, "N frames" appears with the exact count.
- The strip for a static image or single-frame GIF contains none of these labels.
- The strip's timers and fade are the existing detailsTimer/opacity behavior (no new timer introduced).

**REQ-F-012 (Event-driven — Lazy Frame Count)**

The frame count shall be obtained from the single open reader without a second reader and without blocking the GUI thread: from the same reader, obtained off the GUI thread (a GIF count may require scanning the whole file), or otherwise when playback first reaches the end of the sequence. Playback shall never wait for the count.

**Acceptance Criteria:**
- An automated test on a 10-frame GIF asserts the overlay has no count at open, and shows "10 frames" once the injectable clock has advanced through one full sequence (or immediately, if the reader reports it).
- A test with a 1,000-frame GIF asserts the GUI thread event loop is never blocked longer than 50 ms during open.
- A code review confirms only one QImageReader exists per document (REQ-F-013).

---

### F.5 Decoding Strategy and Reader Management

**REQ-F-013 (Ubiquitous — One-Reader, Frame-Streaming Architecture)**

Animated GIF playback shall use exactly one open QImageReader instance per document, reading one frame at a time sequentially. A second thread (or async worker) shall decode the next frame (look-ahead) before it is displayed, to improve playback smoothness.

**Acceptance Criteria:**
- A code inspection confirms only one QImageReader is open per ImageDocument instance during GIF playback.
- The look-ahead worker creates no additional QImageReader instances; it decodes asynchronously; each decoded frame is a fresh image handed to the GUI thread. The reader used to decode frame 0 for display is closed before the animation reader opens, so no two readers are open at once.
- Playback does not stall waiting for frame decode (look-ahead ensures frame is ready when timer fires).

**REQ-F-014 (State-driven — Per-Frame Limits)**

Each frame shall be subject to existing per-frame limits before decoding: 32 million pixels (width × height ≤ 32M), 32,768 px per axis (width ≤ 32K, height ≤ 32K), and 128 MiB per decoded image (size on GPU/memory). The size is checked from the reader before frame 0 is decoded, and every decoded frame is re-checked against the same limits (all GIF frames share the logical-screen size, so an over-limit frame can only be frame 0).

**Acceptance Criteria:**
- An automated test opens a GIF whose logical screen is 10000×10000 (100M pixel); frame 0 is over the limit, so the document enters the normal error state (REQ-F-022).
- A unit test with a fake frame source returns a valid frame followed by a frame over the limits; playback stops after the valid frame, paused, with the notice.
- A test creates a frame at exactly the limits (32M pixels, 32K×1 aspect); decodes successfully and displays.

**REQ-F-015 (Ubiquitous — Fixed Memory Budget)**

The combined 256 MiB budget for displayed image plus decode cache shall remain fixed across all frame counts. Frame count does not increase this budget. While an animated image is displayed, the displayed frame plus one look-ahead frame count against it, and the decode cache limit is reduced accordingly. Encoded input file size cap of 256 MiB is unchanged.

**Acceptance Criteria:**
- An automated memory-profiling test opens a 10-frame and a 100-frame GIF with similar image dimensions; asserts peak memory is within 10% (proportional to frame size, not count).
- A code review confirms no growable vector/list of decoded frames: at most the displayed frame and one look-ahead frame are held.
- A unit test confirms the decode cache limit shrinks to fit displayed plus look-ahead frames within 256 MiB and is restored for other formats.

**REQ-F-016 (Event-driven — Reader Rewind on Loop)**

When the playback sequence reaches the final frame and the loop policy allows another play, the reader shall rewind to frame 0 via jumpToImage(0) and resume. If the plays are complete, playback shall stop on the final frame, paused.

**Acceptance Criteria:**
- An automated test opens a GIF with 5 frames whose policy is two total plays; asserts frame index cycles [0,1,2,3,4,0,1,2,3,4] then stops at frame 4.
- A test with an infinite policy asserts cycling continues beyond 2× frame count.

**REQ-F-017 (Ubiquitous — Pause/Resume Without Re-Decode)**

When the user pauses playback (Space or button), the current frame remains displayed and the reader's state is preserved. When resumed, playback continues with the current frame's remaining delay (if any) then advances to the next frame. No re-decoding of the current frame occurs.

**Acceptance Criteria:**
- An automated test opens a GIF, pauses at frame 2 after 50 ms of a 200 ms delay; resumes; asserts the remaining delay is honored (frame 2 still displayed until ~150 ms have elapsed since frame started) before advancing to frame 3.
- A code inspection confirms resume() does not re-call QImageReader::read() for the current frame.

---

### F.6 Interaction and Transform Orthogonality

**REQ-F-018 (Event-driven — Transforms Do Not Auto-Pause)**

When the user initiates a transform (zoom via Ctrl++, Ctrl+-, Ctrl+0, 1 or the wheel; pan via arrow keys or left-button drag; rotate via R or Shift+R; flip via X or Shift+X) while playback is running, the transform shall apply to the current frame and playback shall continue without pausing.

**Acceptance Criteria:**
- An automated test opens a GIF playing frame 2; applies Ctrl++ (zoom in); asserts playback continues and frame advances normally after frame 2's delay.
- A test applies R (rotate) and X (flip); asserts each applies to the displayed frame and playback continues.
- A test pans via arrow keys; asserts playback continues.

**REQ-F-019 (State-driven — Manual Pause Survives Transforms)**

If the user has paused playback (via Space or button), applying a transform shall not resume playback. The paused state shall be preserved after the transform.

**Acceptance Criteria:**
- An automated test opens a GIF, presses Space to pause; applies Ctrl++ (zoom); asserts playback is still paused after the zoom.
- A test applies R (rotate) and X (flip) while paused; asserts playback remains paused.

**REQ-F-020 (Event-driven — Copy Current Frame as PNG)**

When the user presses Ctrl+C (copy image) while an animated image is playing or paused, the currently displayed frame, with the current rotation and flips applied, shall be copied to the clipboard as PNG (snapshot, not the entire GIF); Ctrl+Shift+C (copy path) is unchanged. Copying does not affect playback state (playing remains playing, paused remains paused).

**Acceptance Criteria:**
- An automated test opens a GIF playing frame 3; presses Ctrl+C; asserts the clipboard contains a PNG image matching frame 3's appearance.
- A test pauses at frame 2; presses Ctrl+C; asserts the clipboard contains frame 2 and playback remains paused.
- A test rotates the view, then copies; asserts the clipboard image is the current frame rotated the same way.

---

### F.7 Single-Frame GIFs and Failure Handling

**REQ-F-021 (State-driven — Single-Frame GIF as Static)**

A GIF file with exactly one frame shall be treated identically to a static image (PNG, JPEG): no play/pause button, no "Animated" label in overlay, no Space binding, no playback tracking. The frame displays normally via the static image path.

**Acceptance Criteria:**
- An automated test opens a single-frame GIF; asserts playPauseButton is not visible or is disabled.
- A test asserts the overlay does not display "Animated".
- A test presses Space; asserts no state change (Space has no effect).

**REQ-F-022 (State-driven — Frame 0 Success Required)**

If frame 0 fails to decode (file corrupted at start, truncated before frame 0 complete, frame oversizes, or GIF plugin missing), the image shall be treated as an unsupported format: the normal ImageDocument Error state shall activate, displaying the error feedback UI (existing behavior).

**Acceptance Criteria:**
- An automated test opens a truncated GIF (invalid at byte 1); asserts ImageDocument enters Error state and displays error feedback.
- A test opens a GIF with frame 0 requiring 50M pixels (over limit); asserts error state.
- A test with QImageReader unable to read GIF (missing plugin or invalid file magic) asserts error state.

**REQ-F-023 (State-driven — Failure After Frame 0)**

If any frame after frame 0 fails to decode (truncated mid-frame, corrupted pixel data, oversized, or GIF is malformed after frame 0), playback shall halt on the last successfully decoded frame, paused. A non-modal notice shall appear in the details strip: "Playback stopped: damaged frame". The viewer shall not show an error page or modal dialog.

**Acceptance Criteria:**
- An automated test opens a 10-frame GIF; injects truncation after frame 5; asserts playback stops at frame 5 (last good), paused, and the notice "Playback stopped: damaged frame" appears in detailsStrip.
- A test asserts no error modal is shown and the UI remains responsive.
- A test navigates to the next image after the notice appears; asserts the notice is cleared and the next image loads normally.

**REQ-F-024 (Event-driven — Notice Appearance and Dismissal)**

When a frame decode fails after frame 0, the notice "Playback stopped: damaged frame" shall appear in the details strip with an appropriate visual style (e.g., warning color, icon). The notice belongs to the details strip: it is included whenever the strip is revealed, is revealed automatically when the failure occurs, and is announced to assistive technology; it is cleared when the user navigates away, refreshes, or opens a new image.

**Acceptance Criteria:**
- A manual check with a truncated GIF confirms the strip reveals with the notice at the failure and shows it again on every later reveal.
- The notice uses a palette token, not a color literal.
- After navigating to the next image, the notice is cleared and does not carry to the next image.

---

### F.8 Modal and Window State Interaction

**REQ-F-025 (State-driven — Auto-Pause on Modal)**

When Open, Image Information or Shortcut Help becomes active (the existing modalActive state), playback shall auto-pause if currently playing. The playback state (playing/paused) shall be recorded; if the modal closes, playback shall resume to its recorded state only if it was playing before the modal opened. A user-initiated pause (Space or button) is never overridden by modal auto-pause; resuming after modal close respects the manual pause.

**Acceptance Criteria:**
- An automated test opens a GIF playing frame 3; opens the Shortcut Help card; asserts playback is paused while the modal is open.
- A test closes the Help card; asserts playback resumes.
- A test opens a GIF playing; pauses manually (Space); opens a modal; asserts playback is paused; closes the modal; asserts playback remains paused (manual pause not overridden).
- A test opens a GIF paused; opens a modal; closes it; asserts playback remains paused.

**REQ-F-026 (State-driven — Auto-Pause on Window Hidden/Minimized)**

When the viewer window is hidden or minimised, playback shall auto-pause if playing. Loss of keyboard focus alone (for example switching to another window that leaves Viewer visible) shall not pause playback. When the window is shown again, playback shall resume if it was playing before (respecting manual pause as in REQ-F-025).

**Acceptance Criteria:**
- A manual test opens a GIF playing; minimizes the window; asserts (via accessible object inspection or native check) that playback is paused.
- After restoring the window, playback resumes.
- If the user manually paused before minimizing, playback remains paused after restore.

---

### F.9 Reader and Worker Cancellation

**REQ-F-027 (Event-driven — Generation Token Cancellation)**

When the user navigates to a different image (previous or next), refreshes with Ctrl+R, or opens a new image, any ongoing look-ahead decode operation shall be cancelled via generation token increment. The next frame decoded by the look-ahead worker shall check the current generation token and discard stale results.

**Acceptance Criteria:**
- A code inspection confirms a generation token (atomic counter or similar) is incremented on document change/close/open.
- The look-ahead worker captures the generation at decode start and compares it before publishing results; stale frames are discarded.
- An automated test opens a slow-to-decode GIF, immediately navigates to the next image; asserts no crash or assertion from stale frame publication.

**REQ-F-028 (Unwanted Behaviour — No Stale Frame Publication)**

The look-ahead worker shall never publish a decoded frame for a closed or replaced document. If the generation token changes between decode start and completion, the frame shall be discarded.

**Acceptance Criteria:**
- An automated test opens a GIF, waits for look-ahead to begin, immediately opens a different image, waits for look-ahead to complete; asserts no crash and that only the new image's frames are displayed.
- A code review confirms the worker checks generation before calling ImageCanvas::setImage() or similar.

---

### F.10 GIF Format Support and Plug-In Dependency

**REQ-F-029 (Ubiquitous — GIF87a and GIF89a Support)**

Both GIF87a and GIF89a formats shall be supported via Qt's QImageReader GIF plugin. Transparency and disposal methods shall be handled per Qt's implementation (typically composited over the viewer's background).

**Acceptance Criteria:**
- An automated test opens a GIF87a file; asserts frame 0 decodes and displays.
- An automated test opens a GIF89a file; asserts decoding and playback work.
- A manual test opens a GIF with transparency; asserts transparent areas show the viewer's background color (not opaque black or white).

**REQ-F-030 (State-driven — Disposal and Transparency)**

Frame disposal (dispose-none, dispose-to-background, dispose-to-previous) and transparency shall be handled by Qt's GIF plugin; the viewer shall display the composited result directly without custom disposal logic.

**Acceptance Criteria:**
- A manual test opens a GIF with disposal-to-background; asserts subsequent frames show the correct background (viewer background, not retained previous frame).
- A manual test opens an animated GIF with transparency; asserts transparency is preserved (not opaque).

**REQ-F-031 (State-driven — Missing Plugin Fallback)**

If the Qt GIF plugin is not available (missing libqgif.so or equivalent), opening a GIF file shall report the normal unsupported-format error. Qualification (format support reporting) shall fail per README "Missing codecs fail qualification".

**Acceptance Criteria:**
- The installed-runtime qualification (tests/installed_runtime.cpp with its format fixtures) lists GIF as a required format and fails when the plugin is missing.
- README's supported-formats section lists GIF as a guaranteed animated format.
- A test with the plugin disabled asserts opening a GIF file triggers the error state (same as opening an unsupported format).

---

### F.11 Space Key and Keyboard Routing

**REQ-F-032 (Event-driven — Space Key Routing)**

Space shall be handled at window level, alongside the other window shortcuts, and shall toggle playback only when an animated image is loaded, no modal is active and no button has keyboard focus. When a header button (information, fullscreen, actions) has keyboard focus, Space shall activate that button as it does today. With the actions menu open, Space is not intercepted.

**Acceptance Criteria:**
- An automated test asserts a window-level Space handler exists and drives the same toggle as the play/pause button.
- A test with no focused button presses Space; asserts playback toggles.
- A test with the fullscreen button focused presses Space; asserts fullscreen toggles and playback does not.
- A test with a card open presses Space; asserts playback does not toggle.
- A test with a static image presses Space; asserts the event is not consumed.

**REQ-F-033 (Ubiquitous — Space in Shortcut Help)**

The Shortcut Help popup (ShortcutHelpPopup.qml or existing help UI) shall display Space as a shortcut: in a PLAYBACK or equivalent section, show "Space" (keycap) with description "Toggle play/pause".

**Acceptance Criteria:**
- An automated test enumerates Shortcut Help rows; finds "Space" key paired with "Toggle play/pause" or similar description.
- A manual check confirms Space appears in the help popup with the correct description.

**REQ-F-034 (Ubiquitous — Space in Footer Key Hints)**

The footer key hints (FooterKeyHints.qml) shall include Space as one of the displayed shortcuts when an animated image is loaded. The hint text shall match the semantic key hint format (e.g., "Space" as a keycap).

**Acceptance Criteria:**
- An automated test opens an animated GIF; enumerates footer hints; asserts the shared semantic key hint for Space appears with the label "Play/Pause".
- An automated test opens a static image; asserts Space does not appear (or appears disabled).

---

## Non-Functional Requirements

**REQ-NF-001 (Minimum Window Size Compatibility)**

Playback controls (play/pause button) and information overlay shall render correctly at the minimum window size (420×280) without horizontal overflow or vertical stacking issues.

**Acceptance Criteria:**
- A manual test at 420×280 opens an animated GIF; asserts the play/pause button is visible and clickable on top of the canvas.
- The details strip with "Animated" and frame count labels fits within the window width.

**REQ-NF-002 (Theme Awareness)**

All colors for the play/pause button, overlay text and failure notice shall come from the existing palette tokens used by the surrounding QML (as previousButton and detailsStrip do). No hard-coded color literals shall be used.

**Acceptance Criteria:**
- A search of new QML finds no color literals for playback UI; all colors come from palette tokens.
- Switching theme (light/dark) while a GIF plays updates button colors, overlay text, and notice style in real time.

**REQ-NF-003 (Performance and Responsiveness)**

Playback shall not stall the GUI thread: decoding runs off-thread and the event loop is never blocked for more than 50 ms by playback. On an injectable clock, frame advances occur exactly at the clamped delays.

**Acceptance Criteria:**
- An automated test with a 100-frame GIF and the injectable clock asserts the displayed frame index equals floor(clock / delay) at every step.
- A manual native check with a real clock plays a 100 ms-delay GIF for 30 s and observes no visible stutter.
- A performance profiler confirms the main thread does not block during frame display (decode happens off-thread).

**REQ-NF-004 (Memory Stability)**

Peak memory usage shall not grow with frame count beyond the fixed 256 MiB budget. Repeated open/close of GIFs shall not leak memory.

**Acceptance Criteria:**
- An automated memory test opens and closes a 100-frame GIF 10 times; asserts peak memory does not increase across repetitions.
- A valgrind or sanitizer run with playback of multiple GIFs reports no leaks.

**REQ-NF-005 (Accessibility)**

The play/pause button shall have an accessible name ("Play" / "Pause") and expose a toggle action. The "Animated" label, frame count, and failure notice shall be exposed to the accessibility tree as static text or informational roles.

**Acceptance Criteria:**
- An automated accessibility test finds the play/pause button with Accessible.role Button and Accessible.name matching current state.
- The accessibility tree exposes "Animated" text and frame count as StaticText nodes.
- The failure notice is exposed as StaticText with appropriate context.

**REQ-NF-006 (Translation Support)**

All user-visible strings (play/pause button accessible names, "Animated", "N frames", "Playback stopped: damaged frame", and footer/help descriptions) shall be wrapped in qsTr().

**Acceptance Criteria:**
- A search of new QML finds no user-visible string assigned to `text`, `label`, or `Accessible.name` outside qsTr().

**REQ-NF-007 (Fade Animation Smoothness)**

The play/pause button and information overlay fade animations (150 ms) shall be smooth and glitch-free; opacity shall transition linearly from 1 to 0 (or vice versa).

**Acceptance Criteria:**
- A manual visual check opens a GIF, watches the button fade out; observes smooth opacity transition without steps or flicker.
- An automated test asserts the button uses the same 150 ms NumberAnimation on opacity as the arrows.

---

## Constraint Requirements

**REQ-C-001 (Qt6 and holonight-qt Stack)**

All implementation shall use Qt6 QML and C++ with the existing holonight-qt primitives already used by the viewer. No external animation libraries or UI frameworks beyond Qt6 shall be introduced.

**Acceptance Criteria:**
- The build system (CMake) compiles with no new external dependencies.
- qml-lint and qmltypes-check produce no new warnings.

**REQ-C-002 (Presentation in apps/viewer/qml)**

Playback UI (play/pause button, overlay updates) shall be implemented in apps/viewer/qml (Main.qml or new component files). Backend state (reader, playback loop, look-ahead worker) shall be in C++ within apps/viewer.

**Acceptance Criteria:**
- The diff shows new/modified QML in apps/viewer/qml; decoding logic in apps/viewer/*.cpp.
- New C++ classes are registered with Qt meta-object system (Q_OBJECT, qmlRegisterType if necessary).

**REQ-C-003 (Icon Provider)**

Play and pause glyphs shall follow REQ-F-010: if the shared icon repository lacks them, they are viewer-owned assets; no change to another repository is made by this cycle.

**Acceptance Criteria:**
- The chosen provider matches DESIGN.md and no other repository is modified.
- Any viewer-owned asset is installed and license-checked like the existing icons.

**REQ-C-004 (CMake and QML Module Registration)**

Any new QML files (e.g., PlayPauseButton.qml, if split out) shall be registered in apps/viewer/CMakeLists.txt within the qt_add_qml_module() block.

**Acceptance Criteria:**
- New .qml files are listed in CMakeLists.txt.
- The QML module builds successfully (cmake, ninja steps pass).

**REQ-C-005 (Format Checks and Taskfile)**

New QML, C++ and asset files shall be covered by the existing checks (format-check, qml-import-check, qmltypes-check, qml-lint, license-check) without weakening them; any file lists those checks rely on are updated.

**Acceptance Criteria:**
- `task check` passes with the new files present.
- The QML format discovery check finds every new QML file.

**REQ-C-006 (CTest Acceptance)**

Playback functionality shall be covered by CTest test cases (existing test infrastructure). Tests shall execute via `task check` or direct ctest invocation.

**Acceptance Criteria:**
- New test files are added to tests/ and registered in tests/CMakeLists.txt.
- Tests are registered in CMakeLists.txt (add_test).
- `task check` or `ctest` runs and passes all playback tests.

**REQ-C-007 (Ownership)**

Implementation shall be confined to this repository and shall reuse existing holonight-qt controls and tokens instead of duplicating them. If a needed primitive is missing from a provider, the design shall raise it as a separate cross-repository work package rather than modify the provider here.

**Acceptance Criteria:**
- The diff touches only this repository.
- Button and label styling reuse existing holonight-qt primitives.

**REQ-C-008 (ImageDocument and Reader Integration)**

The playback system shall integrate with ImageDocument's existing state machine (Empty, Loading, Ready, Error). The open QImageReader shall be owned by ImageDocument or a delegate class (e.g., PlaybackController); on close/navigate, the reader is closed and the look-ahead worker is cancelled.

**Acceptance Criteria:**
- A code review confirms the reader lifecycle is tied to ImageDocument's document state.
- On ImageDocument destruction or state reset, the reader is closed and no dangling threads remain.

**REQ-C-009 (Existing Test Suite Compatibility)**

The implementation shall not break existing tests; tests referencing image loading, navigation, transforms, or error handling shall pass without modification (except tests that explicitly verify old behavior, e.g., "non-animated GIFs show frame 1 only").

**Acceptance Criteria:**
- Existing CTest suite passes (ctest on unmodified test files).
- Any test explicitly expecting non-animated GIF behavior is updated or marked for deprecation.

**REQ-C-010 (Auto-Pause Triggers)**

The only automatic pause triggers are a modal becoming active and the window being hidden or minimised (REQ-F-025, REQ-F-026). Transforms and focus changes never pause or resume playback.

**Acceptance Criteria:**
- An automated test applies all transform actions while a GIF plays; asserts playback continues.
- Only modal open or window hide causes auto-pause.

**REQ-C-011 (Injectable Clock)**

Frame timing shall be driven by a clock or timer that tests can replace, so timing assertions are deterministic. Production code shall use the real timer.

**Acceptance Criteria:**
- Playback tests advance a fake clock and never sleep or depend on wall-clock time.
- The production build constructs the real timer with no test-only branches on hot paths.

**REQ-C-012 (Refresh Reloads Playback)**

When Refresh (Ctrl+R) reloads an animated image, the previous reader and worker shall be cancelled and the reloaded image shall start from frame 0 playing, subject to the same rules as opening it.

**Acceptance Criteria:**
- An automated test refreshes a GIF paused at frame 3; asserts playback restarts at frame 0 and no stale frame from the earlier reader is displayed.

---

## Open Questions & Risks

1. **Frame-Count Wording:** Settled as "N frames" (REQ-F-011); localisation wording is reviewed in design.

2. **Play/Pause Glyph Source:** Whether the shared icon repository already ships suitable glyphs, or viewer-owned assets are needed (REQ-F-010); design decides.

3. **Injectable Clock Design:** REQ-C-011 requires it; design chooses the seam (timer factory or clock interface).

4. **QImageReader::setAllocationLimit(128) Interaction:** Setting process-wide QImageReader::setAllocationLimit(128 MiB) affects all readers; if other code opens readers with larger frame limits, coordination is needed. No change required if existing allocation limit is already set.

5. **image_canvas Frame Replacement:** Whether replacing the canvas image per frame is fast enough or needs a dedicated update path; design measures and decides.

6. **Modal Auto-Pause Edge Cases (resolved in design: independent pause flags):** If a modal opens very quickly after autoplay starts (e.g., user opens File dialog immediately), playback state may be uncertain. REQ-F-025 requires recording and restoring; edge case handling (race conditions) is deferred to implementation testing.

7. **Look-Ahead Decode Scheduling:** Whether look-ahead uses a dedicated thread, thread pool, or async task (QThread, QtConcurrent, std::async) is an implementation detail; only constraint is off-thread, cancellable via generation token.

8. **Single-Frame Detection (blocks REQ-F-021):** The design must decide how a single-frame GIF is told apart before the button and "Animated" label appear — for example decoding frame 1 ahead of display as the look-ahead frame — and define the initial state until that is known.

---

## Revision Notes

Amended after the design stage (DESIGN.md §13, P1–P12): loop policy expressed as total plays (P1); accessible-name criterion corrected (P2); frame-count scan moved off the GUI thread (P3); per-frame limits tests reworked (P4); memory and reader wording aligned with the code (P5, P6); notice persistence defined through the strip (P7, chosen by the user); test paths and qualification corrected (P8); button fade and centering wording (P10, P11); README and qualification updates named (P12). P9 (compositor-initiated minimise on Wayland) remains a documented residual risk verified manually.

## Traceability List

| ID | Requirement | Acceptance Criteria Count |
|----|----|---|
| REQ-F-001 | Autoplay on Open | 2 |
| REQ-F-002 | Frame Delay Clamping | 2 |
| REQ-F-003 | Loop Count Preservation | 4 |
| REQ-F-004 | Space Toggles Play/Pause | 4 |
| REQ-F-005 | Play/Pause Button Click | 2 |
| REQ-F-006 | Play/Pause Button Visibility | 6 |
| REQ-F-007 | Button Enabled Only for Animated | 4 |
| REQ-F-008 | Button Appearance | 4 |
| REQ-F-009 | Accessible Name | 3 |
| REQ-F-010 | Glyph Source | 3 |
| REQ-F-011 | Overlay Content | 5 |
| REQ-F-012 | Lazy Frame Count | 3 |
| REQ-F-013 | One-Reader, Frame-Streaming Architecture | 3 |
| REQ-F-014 | Per-Frame Limits | 3 |
| REQ-F-015 | Fixed Memory Budget | 3 |
| REQ-F-016 | Reader Rewind on Loop | 2 |
| REQ-F-017 | Pause/Resume Without Re-Decode | 2 |
| REQ-F-018 | Transforms Do Not Auto-Pause | 3 |
| REQ-F-019 | Manual Pause Survives Transforms | 2 |
| REQ-F-020 | Copy Current Frame as PNG | 3 |
| REQ-F-021 | Single-Frame GIF as Static | 3 |
| REQ-F-022 | Frame 0 Success Required | 3 |
| REQ-F-023 | Failure After Frame 0 | 3 |
| REQ-F-024 | Notice Appearance and Dismissal | 3 |
| REQ-F-025 | Auto-Pause on Modal | 4 |
| REQ-F-026 | Auto-Pause on Window Hidden/Minimized | 3 |
| REQ-F-027 | Generation Token Cancellation | 3 |
| REQ-F-028 | No Stale Frame Publication | 2 |
| REQ-F-029 | GIF87a and GIF89a Support | 3 |
| REQ-F-030 | Disposal and Transparency | 2 |
| REQ-F-031 | Missing Plugin Fallback | 3 |
| REQ-F-032 | Space Key Routing | 5 |
| REQ-F-033 | Space in Shortcut Help | 2 |
| REQ-F-034 | Space in Footer Key Hints | 2 |
| REQ-NF-001 | Minimum Window Size Compatibility | 2 |
| REQ-NF-002 | Theme Awareness | 2 |
| REQ-NF-003 | Performance and Responsiveness | 3 |
| REQ-NF-004 | Memory Stability | 2 |
| REQ-NF-005 | Accessibility | 3 |
| REQ-NF-006 | Translation Support | 1 |
| REQ-NF-007 | Fade Animation Smoothness | 2 |
| REQ-C-001 | Qt6 and holonight-qt Stack | 2 |
| REQ-C-002 | Presentation in apps/viewer/qml | 2 |
| REQ-C-003 | Icon Provider | 2 |
| REQ-C-004 | CMake and QML Module Registration | 2 |
| REQ-C-005 | Format Checks and Taskfile | 2 |
| REQ-C-006 | CTest Acceptance | 3 |
| REQ-C-007 | Ownership | 2 |
| REQ-C-008 | ImageDocument and Reader Integration | 2 |
| REQ-C-009 | Existing Test Suite Compatibility | 2 |
| REQ-C-010 | Auto-Pause Triggers | 2 |
| REQ-C-011 | Injectable Clock | 2 |
| REQ-C-012 | Refresh Reloads Playback | 1 |

**Total Requirements:** 53 (34 functional, 7 non-functional, 12 constraint)
**Total Acceptance Criteria:** 143

---

**End of Specification**
