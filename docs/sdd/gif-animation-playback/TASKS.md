# SDD Tasks — gif-animation-playback

Test convention: tests are GoogleTest suites compiled into the single `viewer-smoke` executable (tests/CMakeLists.txt); new test files are added to its `add_viewer_application(viewer-smoke ...)` list. A check written as "suite `X` passes" means `viewer-smoke --gtest_filter='X.*'` exits 0 (run through `ctest -R viewer-smoke` or directly under the bus runner). Acceptance is `task check`.

- [x] T-001: Qt GIF characterisation test (T-Q1)
  - REQs: F-002, F-003, F-013, F-014, F-029, C-011
  - Check: frame_source_test.cpp is registered in tests/CMakeLists.txt; `viewer-smoke --gtest_filter='FrameSource.*'` passes with GIF plugin, pinning imageCount, loopCount, nextImageDelay, jumpToImage(0), per-read allocation limit, and GIF87a support; if any pinned behavior contradicts DESIGN.md section 9, stop and revise DESIGN/SPEC before continuing.

- [x] T-002: Extract image_limits.h from image_document.cpp
  - REQs: C-001, C-004, F-014, F-015
  - Check: image_limits.h exports acceptableSize(), kImageLimitBytes, kFileLimitBytes, configureDecodeLimits(); apps/viewer/image_document.cpp includes and uses it; `grep -rn "acceptableSize\|image_limit\|file_limit" apps/viewer/*.cpp | grep -v image_limits` finds no duplicates in .cpp files; `task check` passes.

- [x] T-003: Implement FrameSource interface, QtGifFrameSource, clampFrameDelay, and LoopPolicy
  - REQs: F-002, F-003, F-013, F-016, F-029, F-030
  - Check: frame_source.h exports FrameSource, FrameResult, SequenceInfo, makeQtGifFrameSource(), clampFrameDelay(), LoopPolicy; QtGifFrameSource::open() opens one QFile+QImageReader, readFrame() reads sequentially, rewind() calls jumpToImage(0), scan() queries imageCount/loopCount; code inspection confirms exactly one reader instance per source; clampFrameDelay(5) == 100 and clampFrameDelay(11) == 11; `task check` passes.

- [x] T-004: Implement PlaybackClock interface, QtPlaybackClock, and ManualPlaybackClock
  - REQs: C-011, F-001, NF-003
  - Check: playback_clock.h exports PlaybackClock with nowMs() and arm(delayMs), setWakeHandler(); QtPlaybackClock uses QElapsedTimer and QTimer; tests/playback_clock_test.cpp or animation_controller_test.cpp uses ManualPlaybackClock for deterministic timing without sleep; timing assertions pass on a fake clock with no real wall-clock dependency.

- [x] T-005: Implement FrameStreamer on animation thread
  - REQs: F-013, F-027, F-028
  - Check: FrameStreamer is a QObject on its own QThread; it accepts open/readNext/rewind/close as Qt::QueuedConnection slots; it posts opened/frame/end/failed/info results via QMetaObject::invokeMethod with generation token; results with stale generation are discarded before posting; `grep -n "invokeMethod.*GUI\|Qt::QueuedConnection" apps/viewer/frame_streamer.cpp | wc -l` shows async result posting; `task check` passes.

- [x] T-006: Implement AnimationController state machine and properties
  - REQs: F-001, F-004, F-005, F-007, F-011, F-012, F-021, F-022, F-023, F-025, F-026, C-008
  - Check: AnimationController is a QObject exported to QML with properties animated, playing, userPaused, canToggle, frameCount, frameIndex, failureNotice, suspendedByModal, suspendedByWindow; toggle() method exists; start(path, size) initiates priming; stop() clears state; playing reflects all pause reasons (userPaused && !suspendedByModal && !suspendedByWindow); frameChanged signal emits when frame swaps; `task check` passes.

- [x] T-007: AnimationController autoplay and frame timing test (T-U1)
  - REQs: F-001, NF-003
  - Check: animation_controller_test.cpp opens 2-frame GIF with delays [500, 300] on ManualPlaybackClock; frame 0 displays at 0 ms, frame 1 at exactly 500 ms, then stops at deadline + 300 ms; clamped delays produce exact timing per REQ-F-002 with clock.advance(); `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-008: AnimationController frame delay clamp and multiple-frame test (T-U2)
  - REQs: F-002
  - Check: animation_controller_test with fake FrameSource provides frames with delays [0, 5, 10, 11, 100, 200] ms; controller clamps to [100, 100, 100, 11, 100, 200]; frame timing on ManualPlaybackClock matches exactly; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-009: AnimationController loop policy and rewind test (T-U3)
  - REQs: F-003, F-016
  - Check: animation_controller_test opens 5-frame GIF with play-once policy; frame index cycles [0,1,2,3,4] then stops at 4 paused; separate test with infinite policy continues beyond 2*frameCount; rewind() is called via streamer.rewind(G) on loop; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-010: AnimationController pause/resume remaining delay test (T-U4)
  - REQs: F-017
  - Check: animation_controller_test pauses at frame 2 after 50 ms of a 200 ms delay; resume() sets deadline = now + remaining (≈150 ms); frame 2 displays until that deadline before advancing to frame 3; pause() never calls readFrame; code inspection confirms resume() does not re-read; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-011: AnimationController per-frame limits and damage test (T-U5)
  - REQs: F-014, F-023
  - Check: animation_controller_test with fake FrameSource returns valid frame then oversized frame; controller stops at frame N paused with failureNotice == "Playback stopped: damaged frame"; frame at exactly 32M pixels passes; code inspection confirms re-check after read() with acceptableSize; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-012: AnimationController generation token cancellation test (T-U6)
  - REQs: F-027, F-028
  - Check: animation_controller_test navigates to different image mid-read; generation increments; stale frame results (with old generation) are dropped and never published; no crash or assertion; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-013: AnimationController pause reason matrix test (T-U7)
  - REQs: F-025, F-026, C-010
  - Check: animation_controller_test verifies userPaused independent of suspendedByModal/suspendedByWindow; modal close does not resume if user-paused; all [playing, userPaused, suspendedByModal, suspendedByWindow] combinations are idempotent and order-independent; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-014: AnimationController memory accounting and leak test (T-U8)
  - REQs: F-015, NF-004
  - Check: animation_controller_test opens/closes 100-frame GIF 10 times on fake source; peak memory within 10% across runs; code inspection confirms no vector/list of frames, only displayed frame and one look-ahead; outstanding QImage count returns to zero after each close; `viewer-smoke --gtest_filter='AnimationController.*'` passes.

- [x] T-015: AnimationController threaded execution test (T-U9)
  - REQs: F-013, NF-003
  - Check: animation_controller_test with Execution::Threaded and real GIF fixture opens GIF, stops mid-decode, navigates; no crash or data race; shutdown() joins animation thread; `viewer-smoke --gtest_filter='AnimationController.*'` passes and sanitizer reports no leaks.

- [x] T-016: ImageDocument integration (start/stop/refresh/lifecycle)
  - REQs: C-008, F-020, F-021, C-012
  - Check: ImageDocument::complete() calls animation_.start(localPath(), image_.size()) when state==Ready && format=="GIF"; select() calls animation_.stop(); refresh() stops and starts; destructor cancels animation; animation.frameChanged() signal posts frame to GUI; single-frame GIF stays static (animated==false); `viewer-smoke --gtest_filter='Document.*'` passes.

- [x] T-017: ImageCanvas::replaceFrame implementation
  - REQs: F-018, F-019, NF-003
  - Check: ImageCanvas gains Q_INVOKABLE replaceFrame(const QImage&); if size==image_.size(), updates image_ without bumping generation/emitting imageChanged/calling setImage; otherwise falls back to setImage(); transforms do not snap canvas back to frame 0; the `ImageCanvas` suite passes and the image-changed signal fires only on actual document changes.

- [x] T-018: DecodedImageCache::setLimit implementation and memory budget
  - REQs: F-015, NF-004
  - Check: DecodedImageCache gains void setLimit(qint64 bytes); put() compares against limit_ not constant; GIF frame 0 decode calls setLimit(max(0, 256MiB - 2*frameSize)); cache is trimmed to fit; existing byteLimit stays for non-animated; memory test verifies cache shrinks on GIF display; `task check` passes.

- [x] T-019: Create play.svg and pause.svg icon assets
  - REQs: F-010, C-003
  - Check: apps/viewer/icons/play.svg and pause.svg exist; 24×24 px, viewBox 0 0 24 24, stroke #ffffff, REUSE SPDX header; play = outlined triangle M8 5 L19 12 L8 19 Z, pause = bars M9 6V18 and M15 6V18; license-check passes.

- [x] T-020: Register play and pause icons in CMake and format discovery
  - REQs: C-004, C-005
  - Check: apps/viewer/CMakeLists.txt foreach(icon ...) list includes "play pause"; `task license-check` passes with both SVGs (REUSE headers) and `task install-check` shows both installed under the icons directory; `task check` passes.

- [x] T-021: Implement WindowKeyRouter Space key handling
  - REQs: F-004, F-032
  - Check: WindowKeyRouter gains Q_PROPERTY(bool playbackAvailable) and signals void playbackToggleRequested(); in eventFilter Space key, if animating && !focused-button && !autoRepeat, emits signal; focused playPauseButton receives Space normally; `grep -n "case Qt::Key_Space" apps/viewer/window_key_router.h` finds the Space case; the `WindowKeyRouter` cases in animation_ui_test pass (Space toggles with no focused button, is not consumed with a focused header button, and is not consumed for static images).

- [x] T-022: Implement play/pause button in Main.qml (ViewerButton, glyphs, visibility, fade)
  - REQs: F-005, F-006, F-007, F-008, F-009, C-002, NF-002, NF-007
  - Check: Main.qml has playPauseButton (ViewerButton, objectName "playPauseButton", 48×48 px circle centered on canvas); icon.source bound to document.animation.playing ? "icons/pause.svg" : "icons/play.svg"; Accessible.name "Pause"/"Play"; enabled = canToggle && !modalActive; opacity animates 150 ms Behavior; visible = animated && (shown || opacity > 0); click calls window.togglePlayback(); hover pauses/resumes arrow timer; all colors from palette tokens; `task check` passes.

- [x] T-023: Implement animation strip labels (Animated, Playing/Paused, N frames)
  - REQs: F-011, F-012, NF-002, NF-005, NF-006
  - Check: Main.qml details strip metadata model adds animation tail using .concat; adds "Animated", playing ? "Playing" : "Paused", and when frameCount > 0: "%1 frames".arg(frameCount); all via qsTr(); HnLabel delegates have Accessible.role StaticText and Accessible.name; colors from palette tokens only; frameCount=0 means unknown; the existing footer_key_hints_test cases pass unmodified (7 rows when not animated).

- [x] T-024: Implement playback failure notice in details strip
  - REQs: F-023, F-024, NF-002, NF-005, NF-006
  - Check: Main.qml adds playbackNotice (HnLabel, objectName "playbackNotice", width=detailsFlow.width, wraps); visible when document.animation.failureNotice.length > 0; rawText = failureNotice; color = HoloniightPalette.warning; Accessible.role StaticText; Connections onFailureNotice calls revealDetails() and canvas.Accessible.announce(); notice cleared when animation.stop(); `task check` passes.

- [x] T-025: Add modal/window suspension bindings in Main.qml
  - REQs: F-025, F-026
  - Check: Main.qml adds Binding { target: document.animation; property: "suspendedByModal"; value: modalActive } and Binding { suspendedByWindow: !visible || visibility==Window.Minimized }; playing reflects all flags correctly; modal close resumes only if was playing before; manual pause overrides modal; `viewer-smoke --gtest_filter='AnimationUi.*'` passes.

- [x] T-026: Add frame replacement binding in Main.qml and keep image_ synchronized
  - REQs: F-017, F-018, F-019, NF-003
  - Check: Main.qml adds Connections { target: document; function onFrameChanged() { canvas.replaceFrame(document.image); } }; each frame replaces without resetting zoom/pan/fit; transforms continue playback without snapping; document.image === canvas.image always (no cache-key mismatch); `viewer-smoke --gtest_filter='AnimationUi.*'` passes.

- [x] T-027: Implement Space key hint in FooterKeyHints.qml
  - REQs: F-034, NF-006
  - Check: FooterKeyHints.qml adds animated property (bound from Main.qml); hints model includes when animated: { name: "PlayPause", keyGroups: [[Qt.Key_Space]], label: qsTr("Play/Pause") } before Help row; objectNames footerHintPlayPause*; all in qsTr(); static 7-row hints when not animated; `viewer-smoke --gtest_filter='FooterKeyHints.*'` passes unchanged.

- [x] T-028: Add Playback section to ShortcutHelpPopup.qml
  - REQs: F-033, NF-006
  - Check: ShortcutHelpPopup.qml sections model adds { key: "Playback", label: qsTr("Playback"), rows: [{ keyGroups: [[Qt.Key_Space]], description: qsTr("Toggle play/pause"), keycap: true }] }; objectNames shortcutHelpSectionPlayback, shortcutHelpRowPlayback0, Keycap, Description; all in qsTr(); `viewer-smoke --gtest_filter='ShortcutHelpPopup.*'` updated with new section.

- [x] T-029: Update and extend test suite for animation
  - REQs: F-001, F-020, F-021, F-022, C-009, C-012
  - Check: tests/image_document_test.cpp FirstAnimationFrameOnly rewritten to open 2-frame GIF with 500/300 ms delays and assert autoplay to frame 1; refresh() restarts at frame 0; single-frame GIF stays static; frame 0 failures trigger Error state; copy while playing copies current frame and preserves state; existing transform/navigation tests pass; `viewer-smoke --gtest_filter='Document.*'` passes.

- [x] T-030: Create animation_ui_test.cpp for QML integration
  - REQs: F-005, F-006, F-007, F-008, F-009, F-018, F-019, F-020, F-032
  - Check: tests/animation_ui_test.cpp with offscreen QML and fake AnimationController verifies button objectName/enabled/visible/fade/click, Space key vs focused header button vs open card vs static image, strip labels, failure notice, transforms do not pause, footer and help sections; all QML accessibility roles present; registered in tests/CMakeLists.txt; `viewer-smoke --gtest_filter='AnimationUi.*'` passes.

- [x] T-031: Update shortcut_help_popup_test.cpp for new Playback section
  - REQs: F-033, C-006, C-009
  - Check: tests/shortcut_help_popup_test.cpp asserts objects shortcutHelpSectionPlayback and shortcutHelpRowPlayback0 exist with a Space keycap and description "Toggle play/pause", and every previously asserted section still passes; `viewer-smoke --gtest_filter='ShortcutHelpPopup.*'` passes.

- [x] T-032: Update installed_runtime.cpp for GIF format qualification
  - REQs: F-031, C-006
  - Check: tests/installed_runtime.cpp required format list adds "gif"; GIF87a and GIF89a fixtures added to extension checking; qualification fails when Qt GIF plugin missing, passes when present; `ctest -R viewer-runtime-probe` passes with plugin.

- [x] T-033: Add GIF fixtures to format-fixtures.py
  - REQs: F-029, F-030, F-031
  - Check: scripts/format-fixtures.py generates 2-frame GIF87a and GIF89a fixtures with known delays [100, 200] ms, loopCount values, and transparency/disposal variations; fixtures are used by installed_runtime.cpp; `python3 scripts/format-fixtures.py --help` documents GIF fixtures.

- [x] T-034: Update README with GIF animation support documentation
  - REQs: F-031, C-007, NF-001, NF-002, NF-006
  - Check: README "Supported formats" section lists GIF as guaranteed animated format; "Keyboard" table adds Space → Play/Pause row; "Overlays" section documents Animated label, Playing/Paused state, N frames count, and damaged-frame notice; "Design notes" section documents frame replacement and memory budget (256 MiB display + decode cache); "GIF support" limitation documents transparency/disposal via Qt, codec-private memory disclaimer, and "Missing codecs fail qualification"; `grep -c 'first frame only' README.md` finds no claim that GIF shows only its first frame; the Keyboard table has a Space row, and the Supported formats table/limits text mention GIF playback and the 2-frame memory accounting.

- [x] T-035: Native performance test — frame swap timing (T-P1)
  - REQs: NF-003
  - Check: release_performance_test.cpp measures ImageCanvas::replaceFrame time on 32 MP GIF; main thread stall duration < 50 ms per frame; test runs on real clock without injection; log output recorded with frame size and timing; `viewer-smoke --gtest_filter='ReleasePerformance.*'` passes.

- [x] T-036: Native performance test — GIF scan timing (T-P2)
  - REQs: F-012, NF-003
  - Check: release_performance_test.cpp measures FrameSource::scan time on 256 MiB GIF fixture; GUI thread never blocks > 50 ms during open; look-ahead of frame 2 may wait for scan but does not block GUI; test recorded and reviewed for scan delay impact; `viewer-smoke --gtest_filter='ReleasePerformance.*'` passes.

- [x] T-037: Manual native test — window minimise/restore (T-M1, user-performed)
  - REQs: F-026
  - Check: User opens multi-frame GIF playing; minimises window; verifies playback paused via accessibility tree or manual observation; restores window; verifies playback resumes if was playing, stays paused if was paused; repeats with pre-paused GIF.

- [x] T-038: Manual native test — glyph crispness at 1x/2x device pixel ratio (T-M2, user-performed)
  - REQs: F-010, NF-001
  - Check: User visually inspects play and pause glyphs in 48 px button at 1× and 2× DPR; both centered and crisp; >= 8 px padding inside button; glyph stroke visible and not anti-aliased to excess.

- [x] T-039: Manual native test — 420×280 minimum layout and GIF properties (T-M3, user-performed)
  - REQs: NF-001, F-030, F-029
  - Check: User opens GIF at 420×280 minimum window; verifies play/pause button visible and clickable; details strip fits without horizontal scroll; transparency and disposal render correctly (transparent areas show background, disposal frames composite as expected); multi-frame GIF with disposal variations play back correctly.

- [x] T-040: Final acceptance — task check and build verification
  - REQs: C-001, C-005, C-006
  - Check: `task check` exits 0 (both build presets, tests, format-check, lint, license-check, install-check, qml-import-check, qmltypes-check); the complete build logs contain no new warnings; every task T-001..T-036 is `[x]`, and T-037..T-039 are either `[x]` or explicitly recorded as pending user-performed manual checks.

## Implementation notes

- T-005: `FrameStreamer` is a plain class driven through `QMetaObject::invokeMethod` on a `QObject` living on the animation thread (commands are queued functor invocations rather than declared slots); results are delivered as `StreamEvent`s carrying the generation.
- T-001/T-003: Qt refuses `jumpToImage(0)`, so `QtGifFrameSource::rewind()` reopens the same `QFile`; see DESIGN 9.1.
- T-016: the `ImageDocument` controller-injection overload was not needed; controller behaviour is tested directly with `Execution::Inline`.
- T-021: `WindowKeyRouter` handles Space in the existing `imageReady` switch, so the check's `grep` for `case Qt::Key_Space` matches.
- T-035/T-036 (opt-in, `VIEWER_PERFORMANCE=1`): 32 MP GIF (8000x4000) playing: `replaceFrame` 14 us, worst GUI timer gap 11 ms (< 50 ms). 200 MiB GIF: opened in 20 ms, animated at 30 ms, frame count known at 40 ms (Qt skips comment sub-blocks quickly), worst GUI gap 10 ms.
- T-037..T-039 were performed natively by the user (2026-09-21), who reported one issue, a binding loop on `playPauseButton` icon size, since fixed with a single `glyphSize` property.
- T-040 (2026-09-21): `task check` exit 0 (debug, release and test builds; ctest 20/20; format-check; clang-tidy; qml-lint; reuse lint; install-check; qml-import-check; qmltypes-check). The log contains only clang-tidy summary lines, no compiler warnings. The first run failed clang-tidy on new headers and tests; all were fixed and the whole task rerun.
- Tooling: `/usr/bin/qmlformat` on this machine is not Qt's; format and format-check need `QMLFORMAT=/usr/lib/qt6/bin/qmlformat`.
