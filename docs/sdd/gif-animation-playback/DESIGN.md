# GIF Animation Playback: Design

Status: Draft for review. Inputs: `SPEC.md` (same directory) and the code at `2ba1e52`.
Scope: `holonight-viewer` only (REQ-C-007). No other repository is modified.

Notation: `REQ-*` IDs refer to SPEC.md. "Verified" means read in this repository; "Qt behaviour (to pin)"
means recalled Qt 6.11.2 GIF-handler behaviour that cannot be read here (no Qt sources installed) and that
the characterisation test T-Q1 in section 12 must pin before implementation relies on it.

---

## 1. Findings that shape the design (code as read)

| # | Fact | Source |
|---|------|--------|
| F1 | `decodeImage()` opens a `QFile`, reads EXIF, builds one temporary `QImageReader`, calls `reader.read()` once (first frame), converts to `Format_ARGB32_Premultiplied`, sets DPR 1 and returns; the reader dies with the function. | `image_document.cpp` `readImage`, `decodeImage` |
| F2 | Limits live in an anonymous namespace: `acceptableSize` (32,768 per axis, 32,000,000 px), `image_limit` = 128 MiB (checked on `image.sizeInBytes()`), `file_limit` = 256 MiB. `reader.size()` is checked before `read()`. `canRead()` only says the header parses; it is not a size check. | `image_document.cpp` |
| F3 | The process-wide allocation limit is already set: the `ImageDocument` constructor calls `qputenv("QT_IMAGEIO_MAXALLOC","128")` and `QImageReader::setAllocationLimit(128)` (MiB) before `thread_.start()`. | `image_document.cpp:234-235` |
| F4 | `ImageCanvas::setImage()` returns early only when `cacheKey` is equal; otherwise it bumps `generation_`, calls `view_.setImage()` which calls `fit()` (resets zoom and pan), and emits `imageChanged()`. `paint()` emits `firstRendered()` whenever `painted_generation_ != generation_`. | `image_canvas.cpp`, `view_geometry.cpp` |
| F5 | In `Main.qml`, `canvas.image` is bound to `window.document.image`, which re-evaluates on every `ImageDocument::changed()` (also emitted by `transform()`). `onImageChanged` does `++inputEpoch`, `rendered = false`, `concealDetails()`; `onFirstRendered` does `revealDetails()`. | `Main.qml:652,657-665` |
| F6 | Decoded cache: `DecodedImageCache`, worker-thread only, LRU keyed by URL, at most two entries, static `byteLimit` = 128 MiB. The worker also holds `displayed_` (shares the `QImage` buffer with `image_`). It stores whole images, not frames. | `decoded_image_cache.{h,cpp}`, `image_document.cpp:349-380` |
| F7 | Clipboard copy takes `image_` and `orientation_` at invocation and transforms/encodes on its own worker. | `ImageDocument::copyImage`, `clipboard_controller.cpp` |
| F8 | `WindowKeyRouter` is an event filter on the window's `KeyPress`. It returns `false` when `modalActive`, has a `menuOpen` branch (J/K only), a Tab branch that finds the three header buttons by objectName, and an `imageReady` arrow-key branch. Space is not handled anywhere. Shortcuts are `Shortcut`/`Controls.Action` items in `Main.qml`. | `window_key_router.h` |
| F9 | Icons are viewer-owned 24x24 SVGs (`stroke="#ffffff"`, REUSE header) in `apps/viewer/icons/`, listed in the `foreach(icon ...)` in `apps/viewer/CMakeLists.txt` and consumed as `icon.source: "icons/x.svg"` on `ViewerHeaderButton`, or via `HnIcon` (`empty-viewer.svg`, tinted with `normalColor`). | `Main.qml`, `CMakeLists.txt` |
| F10 | `../holonight-icons` (HoloNight icon theme) contains no play/pause/media-playback glyph (searched `*play*`, `*playback*`, `*pause*`; only unrelated `insync-paused` and `multimedia-player-ipod`). | read-only `find` |
| F11 | `HnKeySequenceLabel` in holonight-qt already renders `Qt.Key_Space` (label "Space"). `HoloniightPalette.warning` exists. | `../holonight-qt/qml/controls/HnKeySequenceLabel.qml`, palette usage |
| F12 | The only existing GIF test, `Document.FirstAnimationFrameOnly` in `tests/image_document_test.cpp`, builds a 2-frame GIF (delay bytes `0100` = 10 ms) and asserts the first frame persists after 50 ms. It verifies the old behaviour. | `tests/image_document_test.cpp:384` |
| F13 | Format qualification is `tests/installed_runtime.cpp` (`required{"png","jpeg","bmp","webp"}`) plus `scripts/format-fixtures.py` fixtures; there is no "File > Image Information" qualification UI. | those files |
| F14 | `ImageDocument::workerFinished()` emits `shutdownFinished` when `++finished_workers_ == 3` (decode thread, directory scan, clipboard). | `image_document.cpp:426` |

---

## 2. Components and ownership

All new C++ stays in `apps/viewer` (REQ-C-002). Pure, QML-free pieces go in the `viewer-private` static library so
unit tests link them cheaply; QML-visible types go in the `qt_add_qml_module` `SOURCES` list (same split as
`ClipboardController`).

| Component | Files (new unless noted) | Owns | Reqs |
|-----------|--------------------------|------|------|
| `image_limits.h` | `image_limits.h` (extracted from `image_document.cpp` anon. namespace) | `acceptableSize()`, `kImageLimitBytes`, `kFileLimitBytes`, `configureDecodeLimits()` (the `qputenv`+`setAllocationLimit(128)` pair, idempotent). Called by `ImageDocument` ctor and `QtGifFrameSource`. | F-014, F-015 |
| `FrameSource` (interface) and `QtGifFrameSource` | `frame_source.{h,cpp}` (viewer-private) | The single `QFile`+`QImageReader` per document. `open`, `readFrame`, `rewind`, `scan`, `close`. Runs only on the animation thread. | F-013, F-016, F-029, F-030 |
| `PlaybackClock` (interface), `QtPlaybackClock` | `playback_clock.{h,cpp}` (viewer-private) | Monotonic `nowMs()` plus a single-shot `arm/disarm`. The injectable seam. | C-011, F-001, F-002, NF-003 |
| `FrameStreamer` | `frame_streamer.{h,cpp}` | QObject living on the animation thread; executes commands against a `FrameSource`; posts results to the GUI thread. | F-013, F-027, F-028 |
| `AnimationController` | `animation_controller.{h,cpp}` (QML module SOURCES) | Playback state machine, timing, look-ahead pairing, loop policy, pause reasons, generation token, QML surface. Owned by `ImageDocument` (like `clipboard_`). | F-001..F-005, F-011, F-012, F-016..F-019, F-021, F-023..F-028, C-008, C-010, C-012 |
| `ImageDocument` (modified) | `image_document.{h,cpp}` | Starts/stops the controller on `Ready`/`select`/`open`/`shutdown`; keeps `image_` equal to the displayed frame; new `frameChanged()` signal and `animation` property. | C-008, F-020, F-021 |
| `DecodedImageCache` (modified) | `decoded_image_cache.{h,cpp}` | New mutable `limit_` (default `byteLimit`) with `setLimit()` that trims. | F-015 |
| `ImageCanvas` (modified) | `image_canvas.{h,cpp}` | New `Q_INVOKABLE replaceFrame()` (section 6.3). | F-018, F-019, NF-003 |
| `WindowKeyRouter` (modified) | `window_key_router.h` | Space handling (section 8). | F-004, F-032 |
| QML (modified) | `qml/Main.qml`, `qml/footer/FooterKeyHints.qml`, `qml/shortcuts/ShortcutHelpPopup.qml` | Button, strip, bindings, help/footer rows. No new QML file, so `qml_files` in CMake is unchanged (REQ-C-004 n/a). | F-005..F-011, F-024, F-033, F-034, NF-* |
| Icons | `icons/play.svg`, `icons/pause.svg`; CMake `foreach(icon ...)` gets `play pause` | Glyphs. | F-010, C-003 |

Ownership rule: the controller never touches the canvas or QML; `ImageDocument` is the only object that knows both.
Only the streamer thread ever touches the reader. Only the GUI thread touches `image_`, controller state and
`QImage`s handed over (immutable after publication).

---

## 3. Resolution of the SPEC open items

### 3.1 Single-frame GIF detection and initial state (SPEC OQ 8, REQ-F-021)

Decision: frame 0 is displayed through the unchanged static path; animation status is confirmed
afterwards, off the GUI thread, and until confirmed the document behaves as static.

- `decodeImage()` is not changed for animation: it still returns frame 0 (F1). GIF is recognised by
  `information_.format == "GIF"` (already `reader.format().toUpper()`). Static PNG/JPEG/WebP/APNG and animated WebP
  never enter the controller.
- Initial state: `animation.animated == false`, so the button is disabled and hidden, no "Animated" label,
  Space not consumed, footer hint absent. This is exactly the REQ-F-021 single-frame state, so a
  single-frame GIF needs no special-casing: it simply never leaves it.
- After `Ready`, the controller asks the streamer to `open` (fresh reader, decode frame 0 and discard; check
  its size equals `image_.size()`), then to read frame 1 as the first look-ahead. Result:
  - frame 1 decoded: `animated = true` (`animatedChanged`), controller begins/continues playback;
  - end of sequence at index 0: single-frame; reader closed, controller returns to `Inactive`;
  - failure at index 1: resolved by the scan arbiter (3.7): `count > 1` means damaged (REQ-F-023), otherwise single-frame.
- Cost of the alternative "decode frame 1 before publishing frame 0": it would delay first paint by one
  full frame decode (up to 32 MP). Rejected. The window during which a multi-frame GIF looks static is one frame
  decode after paint, imperceptible next to the >=100 ms minimum first delay.
- Frame-0 double decode: the static path decodes frame 0 and the streamer decodes it again to prime the
  reader. Accepted: one extra decode per GIF open, off-thread, after display; it keeps the static path,
  the cache, prefetch and single-frame behaviour byte-for-byte unchanged (REQ-C-009). Handing the reader
  over from `decodeImage` was considered and rejected (section 11).
- Timing origin: the frame-0 display clock starts when the controller's `start()` is called in `complete()`
  (frame 0 published), not when the streamer finishes priming. Frame 0's delay is learned during priming;
  the deadline is `startMs + delay0` and playback advances when `deadline reached AND next frame ready`
  (section 5). This satisfies REQ-F-001 (frame 0 for exactly its delay).

### 3.2 Injectable clock seam (SPEC OQ 3, REQ-C-011)

```cpp
// playback_clock.h
class PlaybackClock {
 public:
  virtual ~PlaybackClock() = default;
  [[nodiscard]] virtual qint64 nowMs() const = 0;       // monotonic, ms
  virtual void arm(qint64 delayMs) = 0;                 // single-shot, replaces any armed wake
  virtual void disarm() = 0;
  void setWakeHandler(std::function<void()> handler) { wake_ = std::move(handler); }
 protected:
  void wake() const { if (wake_) wake_(); }
 private:
  std::function<void()> wake_;
};
class QtPlaybackClock final : public PlaybackClock {  // QElapsedTimer + QTimer(Qt::PreciseTimer, singleShot)
};
```

`AnimationController(std::unique_ptr<PlaybackClock> clock, FrameSourceFactory factory = defaultFactory,
Execution mode = Execution::Threaded, QObject* parent = nullptr)`. The production constructor
(`AnimationController(QObject*)`, used by `ImageDocument`) passes `QtPlaybackClock`, the real factory and
`Threaded`; `ImageDocument` gets a second constructor overload taking a controller factory (mirroring the
existing `Decoder` injection) so integration tests can supply the fake. `ManualPlaybackClock` (test-only, in `tests/`)
has `advance(ms)` which moves `now` and fires `wake()` for every armed deadline crossed, in order. No test-only
branch exists in production code: the seam is the constructor argument.

`Execution::Inline` runs streamer commands synchronously (direct call, results delivered immediately), used with
a fake `FrameSource` for deterministic REQ-F-001/002/003/016/017/023 tests without threads or sleeping. The
threaded production path is covered separately (section 12, T-U4, T-I1).

Timing rule (all on the GUI thread, all in clock time): frames are scheduled by deadline, not by sleeping:
`deadline_ = max(deadline_ + delay, now)` when advancing on time; if the look-ahead was late, `deadline_ = now + delay`
(no burst catch-up). With ideal look-ahead and a fake source this gives exactly REQ-F-001/002/NF-003 timing.

### 3.3 Can `ImageCanvas` swap images per frame cheaply? (SPEC OQ 5)

No, not with `setImage()`. It resets fit, zoom and pan (F4), bumps `generation_` (re-fires `firstRendered`, which
calls `revealDetails()` every frame), and emits `imageChanged` (F5: `++inputEpoch` cancels drags, `concealDetails()`
hides the strip every frame). Routing frames through `ImageDocument::image` and `changed()` has the same effect, and
any unrelated `changed()` (e.g. `transform()`) would re-assign the binding.

Decision: a dedicated, small path.

```cpp
// image_canvas.h
Q_INVOKABLE void replaceFrame(const QImage& frame);
```
- If `frame.size() == image_.size()` and `image_` is non-null: `image_ = frame; update();` only. No `generation_`
  bump, no `view_` change, no `viewChanged`/`imageChanged`/`firstRendered`.
- Otherwise (defensive; e.g. a late frame after a document switch) fall back to `setImage(frame)`.
- Paint cost is unchanged from a static repaint (`paint()` draws the visible source rectangle of `image_`; texture
  upload cost is the canvas-sized backing surface, not the image size, per README "Design notes"). This is a
  per-frame `QQuickPaintedItem::update()`; a specialised texture path stays out of scope (SPEC Non-Goals). Measurement
  on the existing native performance harness (`release_performance_test.cpp`, README "Native performance measurement")
  is a required verification item (T-P1); if a 32 MP frame exceeds REQ-NF-003's 50 ms GUI stall, that is raised
  as a follow-up, not silently accepted.
- `ImageDocument::image()` must always equal the frame the canvas shows: the controller's frame is stored into
  `image_` (without emitting `changed()`), so a later `changed()` re-evaluation of `canvas.image` yields the same
  `cacheKey` and `setImage` returns early. This is what keeps transforms (which emit `changed()`) from snapping the
  canvas back to frame 0 (REQ-F-018/019).

QML wiring (Main.qml): `Connections { target: window.document; function onFrameChanged() { canvas.replaceFrame(window.document.image); } }`.

### 3.4 Glyph provider (SPEC OQ 2, REQ-F-010, REQ-C-003)

Decision: viewer-owned assets `apps/viewer/icons/play.svg` and `pause.svg`. Reason: F10 shows the shared
`holonight-icons` repo has no suitable play/pause glyph, and REQ-C-003/C-007 forbid touching it in this cycle.
Both are 24x24, `viewBox 0 0 24 24`, stroke `#ffffff` (tinted by HnIcon/`icon.color` like `information.svg`), round
caps and joins, stroke width 1.8, with the REUSE SPDX header. Play: outlined triangle `M8 5 L19 12 L8 19 Z`;
pause: two bars `M9 6V18` and `M15 6V18`. They are registered in the `foreach(icon ...)` list (`play pause`), which
also covers install and license-check. If `holonight-icons` later ships glyphs, migrating is a source-path swap
(record as a follow-up work package, not done here).

Rendering: the button uses `icon.source` exactly as `ViewerHeaderButton` does. Icon size is
`Math.min(HnMetrics.iconSize(HnControlSize.Hero), 32)`, which leaves at least 8 px padding inside 48 px (REQ-F-010).
Crispness at 1x/2x is a manual visual check (T-M2).

### 3.5 Allocation-limit interaction (SPEC OQ 4, REQ-F-014)

Verified: the limit is already installed once for the process, before any reader exists, at 128 (MiB)
by the `ImageDocument` constructor (F3). It is a global static, so the streamer's reader (on another thread) is
subject to the same value; no change to the value is needed and no other code opens larger readers (the only other
readers are `decodeImage` and tests). Design points:

1. Move the two calls to `configureDecodeLimits()` (idempotent), called from the `ImageDocument` constructor
   (unchanged position, before `thread_.start()`) and from `QtGifFrameSource`'s constructor, so a standalone streamer
   in a unit test has the same policy.
2. Qt applies the limit to one image allocation per `read()` (Qt behaviour, to pin in T-Q1): it does not
   accumulate across the sequential reads on one reader. Multi-frame reading therefore does not eat into the
   limit; the aggregate bound is our own accounting (section 7), not `setAllocationLimit`.
3. Codec-private memory (the GIF handler's composition canvas and dispose-to-previous copy) is not visible to our
   accounting or necessarily to the limit; README "Memory beyond the bounds" already excludes codec-private
   allocations. It is documented as an unbounded-by-design item (risk R3).
4. Every frame is re-checked after `read()` with the same predicates as the static path
   (`acceptableSize(image.size())`, `image.sizeInBytes() <= 128 MiB`), moved to `image_limits.h`. A frame that fails
   any check, or a null `read()`, is a damaged frame (REQ-F-023).

SPEC problem P4 (see section 13): for GIF, every composed frame has the logical-screen size that frame 0 already
passed, so "a valid frame followed by a 32769 px wide frame" cannot occur. The REQ-F-014 tests on later frames
cannot be built with a real GIF; they are covered with a fake `FrameSource` (unit) instead.

### 3.6 Space routing (SPEC OQ 6, REQ-F-004, REQ-F-032)

See section 8 in full. Summary: handled in `WindowKeyRouter`, not with a `Shortcut { sequence: "Space" }`,
because a QML `Shortcut` is resolved before the key event reaches the focused item and would steal Space from a
focused header button (violating REQ-F-032), whereas the event-filter sees the key press after shortcut matching
and can inspect `activeFocusItem`.

### 3.7 Frame count without a second reader (SPEC OQ 7, REQ-F-012)

SPEC problem P3: `QImageReader::imageCount()` for GIF is not cheap. Qt behaviour (to pin): `QGifHandler::imageCount()`
and `loopCount()` both trigger a whole-file structural scan (I/O over every data block; no LZW decode), and the
existing test confirms the count is available from the same reader (`reader.imageCount() == 2` before `read()`).
The scan is O(file size) (up to 256 MiB), so it must never run on the GUI thread.

Decision: the same single reader answers the count, on the animation thread, at a point where it delays nothing:

1. Order of work on the streamer per session: `open` (read frame 0), read frame 1 (look-ahead, becomes
   `animated`), then one `scan()` (`imageCount()` and `loopCount()`), then wait for `readNext` commands.
2. `scan()` result is posted once: `frameCount` (0/negative = unknown) and `loopCount`.
3. If the scan is slower than frame 0's delay, look-ahead of frame 2 starts late and playback holds frame 1 (never
   blocks the GUI thread, REQ-NF-003) until frame 2 arrives. Bounded by file size; accepted, measured in T-P2.
4. Fallback per REQ-F-012: if `scan()` reports unknown (<= 0), the count is set when the sequence first reaches
   its end (`EndOfSequence` after index n-1, so `frameCount = n`).
5. The scan is also the end-of-sequence/damage arbiter: a null `read()` after `k` good frames is a clean end iff
   `frameCount <= k` (known from the scan, or, if unknown, a scan is run at that moment). Otherwise the
   frame is damaged. This is how the single-frame case and mid-file truncation are told apart (a null read alone is
   ambiguous in `QImageReader::error()`).

"Playback never waits for the count": the count is display-only; timing uses no count. The loop policy needs
`loopCount` only at the first end of sequence, by which time the scan has normally finished.

### 3.8 Interaction with static path, cache, EXIF, orientation (SPEC OQ, item 8)

- Static path: untouched. Animated documents are static documents plus an attached controller; `state_`
  machine (Empty/Loading/Ready/Error) is unchanged. Frame 0 failures already produce the Error state (REQ-F-022).
- Decoded cache: keys are URLs and values whole images (F6); it cannot hold frames and is not used for frames.
  A GIF entry cached by prefetch is frame 0 only. On selecting it, the cache hit supplies the displayed frame 0
  instantly and the controller still primes a fresh reader (same start path). Memory coordination is section 7.
  Because entries are validated by size/mtime and refresh bumps `cache_epoch_` (F6), a refreshed GIF is
  re-decoded.
- EXIF: `ExifMetadata::read` runs once on the file before decode (unchanged); GIF has no EXIF, so
  `information_.exif` is empty and `informationSections()` shows no camera/location. `reader.setAutoTransform(true)`
  is a no-op for GIF (no transformation option), so decoded frames need no orientation normalisation.
- Orientation/transform: `orientation_` is a view transform applied at paint time (`ImageCanvas::paint` uses
  `ImageOrientation::mapping`) and at copy time (`ImageOrientation::apply`). Both read whichever `QImage` is
  current, so transforms apply to the current frame with no code change (REQ-F-018/019/020), provided frame size
  is constant (true for GIF) and `image_` tracks the displayed frame (3.3).
- Information: `decodedSize`, `formattedFileSize`, `transformedDimensions` are frame-invariant.

### 3.9 Refresh (Ctrl+R) (REQ-C-012)

`ImageDocument::refresh()` -> `++cache_epoch_` -> `select(selected_url_)`. `select()` already discards the current
image and cancels the decode; it additionally calls `animation_.stop()`, which increments the generation (section 9)
and posts `close` to the streamer. When the reloaded frame 0 completes, `complete()` calls `animation_.start()`, a
new session with `userPaused = false`, index 0, loops reset: it plays from frame 0 exactly like a fresh open, and any
in-flight look-ahead of the old session is discarded by generation. Same code path for `open()` and `navigate()`.

---

## 4. Data flow

```
open/navigate/refresh -> select(): stop() controller, ++request_id_, decode request (existing)
  decode worker: decodeImage() -> frame 0 (existing) -> complete(request, result)
  complete(): image_ = frame 0, state_=Ready, emit changed()
     if information_.format == "GIF":  animation_.start(path, image_.size())     [gen G]
GUI controller.start(G): state=Priming, animated=false, startMs=clock.now(), userPaused=false
  -> streamer.open(G, path, size)                                    (animation thread)
       open QFile+QImageReader; read frame 0 (discard; verify size); record delay0   -> Opened(G, delay0) | Failed
       read frame 1                                                  -> Frame(G, 1, image, delay1) | End | Failed
       scan()                                                        -> Info(G, frameCount, loopCount)
GUI on Opened: delay0 known: arm(startMs + clampDelay(delay0) - now)   (if playing)
GUI on Frame(G,1): stash as next_; animated=true (if first); tryAdvance()
tryAdvance (timer wake or Frame arrival), playing && now >= deadline_ && next_:
     display next_ (image_ = frame; emit frameChanged); index++, delay=clamp(next_.delay);
     deadline_ = max(deadline_+delay, now) or now+delay if late; arm; next_.reset();
     streamer.readNext(G)                                            -> Frame | End | Failed
End(G) after index n-1:  frameCount=n if unknown; consult loop policy:
     more plays: streamer.rewind(G) (jumpToImage(0)) then readNext -> frame 0 as next_ ; loops++
     no more plays: finishAtDeadline_=true -> at last frame's deadline: state=Finished (playing=false, stays on last frame)
Failed(G) (after frame 0): failureNotice=tr("Playback stopped: damaged frame"); state=Stopped; disarm; last good frame stays
```

Pause/resume (REQ-F-017): `pause()` records `remaining_ = max(0, deadline_ - now)`, `disarm()`. `resume()` sets
`deadline_ = now + remaining_`, `arm(remaining_)`. Neither touches the reader or the displayed image; the look-ahead
already fetched (or in flight) simply waits in `next_`. No re-decode occurs; `resume()` never calls a `FrameSource`
method (asserted in T-U6 by a fake that counts `readFrame` calls).

Auto-pause (REQ-F-025/026, C-010): two independent writable flags `suspendedByModal`, `suspendedByWindow`. Effective
`playing = animated && !failed && !finished && !userPaused && !suspendedByModal && !suspendedByWindow`. The clock is
armed iff `playing`. Because a user pause is a separate flag, modal close or window show can never override it, and a
paused-then-modal-then-close sequence stays paused. All flag changes happen on the GUI thread, so there is no race
between "autoplay just started" and "modal just opened" (SPEC OQ 6): whichever order, the flags are the truth and
`recompute()` (arm/disarm) is idempotent.

Navigate/close/cancel: `stop()` -> `++generation_`, disarm, drop `next_`, clear state, `streamer.close(G)`. The
streamer closes the reader when it processes the command (after any in-flight `read()` returns, since reads
are not interruptible) and results tagged with an older generation are dropped on arrival.

Finished state: toggling from `Finished` rewinds and plays again (`loops` reset, `rewind` then look-ahead from frame 0).
Toggling from `Stopped` (damaged) is a no-op (`canToggle == false`).

---

## 5. Interfaces

### 5.1 C++ (names and files are new unless marked existing)

```cpp
// frame_source.h (viewer-private)
struct FrameResult {
  enum class Status { Ok, EndOfSequence, Damaged } status = Status::Damaged;
  QImage image;   // ARGB32_Premultiplied, DPR 1, fresh per frame; checked with image_limits.h predicates
  int delayMs = 0;  // raw QImageReader::nextImageDelay() read immediately after read(); NOT clamped here
};
struct SequenceInfo { int frameCount = 0; int loopCount = 0; };  // raw Qt values; <=0 count = unknown
class FrameSource {
 public:
  virtual ~FrameSource() = default;
  virtual bool open(const QString& path) = 0;   // opens QFile+QImageReader, canRead() true
  virtual FrameResult readFrame() = 0;
  virtual bool rewind() = 0;                    // jumpToImage(0); if false, reopens the same QFile (still one reader)
  virtual SequenceInfo scan() = 0;              // imageCount()/loopCount()
  virtual void close() = 0;
};
using FrameSourceFactory = std::function<std::unique_ptr<FrameSource>()>;
std::unique_ptr<FrameSource> makeQtGifFrameSource();

int clampFrameDelay(int rawMs);   // <= 10 -> 100, else unchanged (REQ-F-002)

struct LoopPolicy {               // isolated so Qt's convention is pinned in one place
  static LoopPolicy fromQt(int rawLoopCount);
  bool infinite = false;
  int totalPlays = 1;
  [[nodiscard]] bool morePlays(int playsDone) const { return infinite || playsDone < totalPlays; }
};
```

`AnimationController` (`animation_controller.h`):

```cpp
class AnimationController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Owned by ImageDocument")
  Q_PROPERTY(bool animated READ animated NOTIFY stateChanged)             // multi-frame confirmed
  Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)               // effective
  Q_PROPERTY(bool userPaused READ userPaused NOTIFY stateChanged)
  Q_PROPERTY(bool canToggle READ canToggle NOTIFY stateChanged)           // animated && not damaged
  Q_PROPERTY(int frameCount READ frameCount NOTIFY stateChanged)          // 0 = unknown
  Q_PROPERTY(int frameIndex READ frameIndex NOTIFY stateChanged)
  Q_PROPERTY(QString failureNotice READ failureNotice NOTIFY stateChanged)  // empty unless damaged
  Q_PROPERTY(bool suspendedByModal READ suspendedByModal WRITE setSuspendedByModal NOTIFY stateChanged)
  Q_PROPERTY(bool suspendedByWindow READ suspendedByWindow WRITE setSuspendedByWindow NOTIFY stateChanged)
 public:
  enum class Execution { Threaded, Inline };
  explicit AnimationController(QObject* parent = nullptr);   // production
  AnimationController(std::unique_ptr<PlaybackClock>, FrameSourceFactory, Execution, QObject* parent = nullptr);
  ~AnimationController() override;
  Q_INVOKABLE void toggle();                                  // Space and button
  void start(const QString& path, QSize firstFrameSize);      // called by ImageDocument (not QML)
  void stop();
  void shutdown();                                            // like ClipboardController::shutdown
 signals:
  void stateChanged();
  void frameReady(QImage frame);                              // GUI thread, already generation-checked
  void shutdownFinished();
};
```

`stateChanged` is one coarse signal, following the repository pattern (`ImageDocument::changed`,
`ClipboardController::changed`); QML bindings re-evaluate cheaply. `frameIndex` changes each frame and therefore
emits every frame; the strip does not bind to it (only `frameCount`, `playing`, `animated`, `failureNotice`), so
per-frame re-evaluation stays negligible. If profiling shows churn, split `frameIndex` into its own signal.

`ImageDocument` additions:

```cpp
Q_PROPERTY(AnimationController* animation READ animation CONSTANT)
AnimationController* animation() { return &animation_; }
signals: void frameChanged();     // frame swapped; image_ already updated; changed() NOT emitted
private: AnimationController animation_;
```
`connect(&animation_, &AnimationController::frameReady, this, [this](const QImage& f){ image_ = f; emit frameChanged(); });`
`select()` and the invalid-URL branch of `open()` call `animation_.stop()`; `complete()` calls `animation_.start(localPath(), image_.size())`
when `state_ == Ready && information_.format == "GIF"`; `shutdown()` calls `animation_.shutdown()` and
`workerFinished()`'s threshold becomes 4 (F14) with `connect(&animation_, &AnimationController::shutdownFinished, ...)`.

`FrameStreamer` (animation thread): `open(G, path, size)`, `readNext(G)`, `rewind(G)`, `close(G)` as queued slots;
posts `opened`, `frame`, `end`, `failed`, `info` back with `G` via `QMetaObject::invokeMethod(controller, ..., Qt::QueuedConnection)`.
In `Inline` mode commands are invoked directly and results delivered synchronously.

`DecodedImageCache`: `void setLimit(qint64 bytes)` (clamped to `[0, byteLimit]`, evicts LRU until `bytes_ <= limit_`);
`put()` compares against `limit_` instead of the constant; `byteLimit` stays for compatibility (`folder_browsing_test.cpp:254`).

### 5.2 QML changes (exact objectNames)

`Main.qml`:

1. `ViewerButton` gains `property real cornerRadius: HnMetrics.internalSpacing(HnControlSize.Compact)` used by `background.radius`
   (default preserves every existing button).
2. New button inside `canvasArea`, after `nextButton`:
   ```qml
   ViewerButton {
       id: playPauseButton
       objectName: "playPauseButton"
       readonly property bool shown: window.arrowsShown
       floating: true
       display: Controls.AbstractButton.IconOnly
       implicitWidth: 48
       implicitHeight: 48
       cornerRadius: 24
       anchors.centerIn: parent
       icon.source: window.document.animation.playing ? "icons/pause.svg" : "icons/play.svg"
       icon.width: Math.min(HnMetrics.iconSize(HnControlSize.Hero), 32)
       icon.height: icon.width
       Accessible.name: window.document.animation.playing ? qsTr("Pause") : qsTr("Play")
       enabled: window.document.animation.canToggle && !window.modalActive
       opacity: shown ? 1 : 0
       visible: window.document.animation.animated && (shown || opacity > 0)
       Behavior on opacity { NumberAnimation { duration: 150 } }
       onHoveredChanged: hovered ? window.pauseArrows() : window.resumeArrows()
       onClicked: window.togglePlayback()
   }
   ```
   The glyph shows the action available (REQ-F-008); the accessible name mirrors it (Play when paused, Pause when
   playing, per REQ-F-009 text; the AC in SPEC is inverted, see P2). Same fade rule as `previousButton`: faded means
   `visible == false`, therefore not clickable and not in the accessibility tree.
3. `showArrows()` also holds the countdown while `playPauseButton.hovered` (adds `&& !playPauseButton.hovered`).
4. `function togglePlayback(): void { window.document.animation.toggle(); window.revealDetails(); window.clearImageFocus(); }`
   (`revealDetails` reuses `detailsTimer`, no new timer, so the state change is visible even though the keyboard hid the arrows).
5. Bindings:
   ```qml
   Binding { target: window.document.animation; property: "suspendedByModal"; value: window.modalActive }
   Binding { target: window.document.animation; property: "suspendedByWindow"
             value: !window.visible || window.visibility === Window.Minimized }
   Connections { target: window.document; function onFrameChanged() { canvas.replaceFrame(window.document.image); } }
   ```
   Focus loss is deliberately not bound (REQ-F-026).
6. `keyRouter` gets `playbackAvailable: window.document.animation.canToggle && window.canInspect` and
   `onPlaybackToggleRequested: window.togglePlayback()`.
7. Details strip: the metadata `Repeater` model gains an animation tail:
   `[...existing four..., ...(anim.animated ? [qsTr("Animated"), anim.playing ? qsTr("Playing") : qsTr("Paused")] : []), ...(anim.animated && anim.frameCount > 0 ? [qsTr("%1 frames").arg(anim.frameCount)] : [])]`
   (`.concat` form in the file). Delegate `HnLabel` gets `Accessible.role: Accessible.StaticText; Accessible.name: rawText`
   (REQ-NF-005). A third child of `detailsFlow`:
   ```qml
   HnLabel {
       objectName: "playbackNotice"
       width: detailsFlow.width
       visible: window.document.animation.failureNotice.length > 0
       color: HoloniightPalette.warning
       wrapMode: Text.Wrap
       textFormat: Text.PlainText
       rawText: window.document.animation.failureNotice
       Accessible.role: Accessible.StaticText
       Accessible.name: rawText
   }
   ```
   The strip's existing `height: detailsFlow.height + 16` absorbs it; its `width` uses only the metadata flow
   (unchanged formula), the notice wraps within it. "%1 frames" is always >= 2, so no plural form is needed
   (a `%n` form would render "10 frame(s)" without translation files). All strings are in `qsTr()` (REQ-NF-006);
   all colours are palette tokens (REQ-NF-002).
8. `onDocumentStateChanged`/`Connections` on `animation.failureNotice`: when it becomes non-empty, call
   `window.revealDetails()` and `canvas.Accessible.announce(...)` with the notice (the strip auto-conceals; see P7).

`FooterKeyHints.qml`: add `property bool animated: false`; `hints` becomes the existing seven plus, when
`animated`, `{ name: "PlayPause", keyGroups: [[Qt.Key_Space]], label: qsTr("Play/Pause") }` inserted before `Help`.
Objects: `footerHintPlayPause`, `footerHintPlayPauseKeycap`, `footerHintPlayPauseLabel` (existing naming scheme).
`Main.qml` passes `animated: window.document.animation.canToggle`. With `animated == false` the list is identical to
today, so the existing `footer_key_hints_test.cpp` (7 rows) is untouched (REQ-C-009).

`ShortcutHelpPopup.qml`: new section after "Transform":
`{ key: "Playback", label: qsTr("Playback"), rows: [{ keyGroups: [[Qt.Key_Space]], description: qsTr("Toggle play/pause"), keycap: true }] }`
producing `shortcutHelpSectionPlayback`, `shortcutHelpRowPlayback0`, `...Keycap`, `...Description`. The help is
static (lists it for all images, like Rotate). `shortcut_help_popup_test.cpp` expectations on section lists need the
new section added (a deliberate test change).

---

## 6. Threading and generation-token model

- Threads: GUI (controller, canvas, `image_`); existing decode worker (`thread_`, unchanged); existing clipboard and
  directory workers (unchanged); new animation thread `QThread animation_thread_` owned by the controller, started
  lazily on the first `start()` and quit in `shutdown()`. One long-lived streaming thread rather than a pool: reads are
  strictly sequential on one reader, and a dedicated thread avoids blocking the decode worker (whose queue serves
  navigation) during a long read.
- Affinity: `QFile`, `QImageReader` and the `FrameSource` are created, used and destroyed on the animation
  thread only. GUI-to-worker commands and worker-to-GUI results are `Qt::QueuedConnection` invocations carrying
  value types (`QImage` is implicitly shared and immutable after creation; frames are never modified after
  publication, so no detach races between the canvas, clipboard worker and controller).
- Generation token: `quint64 generation_` on the GUI thread, incremented by `start()` and `stop()` (and
  `shutdown()`); mirrored into a `std::shared_ptr<std::atomic<quint64>>` read by the streamer. Every command
  and every result carries its generation `G`. Streamer: before starting a command, and after each `read()`, compare `G`
  with the atomic; if stale, discard the result without posting and (for `open`) skip work. GUI: every result
  handler begins `if (G != generation_) return;`. Two checks, both cheap: a stale frame can never be published
  (REQ-F-027/028), including the case where the decode completes after the switch.
- Cancellation granularity: `QImageReader::read()` is not interruptible. Worst case a stale frame decode finishes
  and is dropped; a later `open(G')` closes the old reader first. Commands queued behind a long read execute in order, and
  stale commands are no-ops. This is the same trade-off as the existing decode worker ("waits for the active read").
- Shutdown: `ImageDocument::shutdown()` calls `animation_.shutdown()` (bumps generation, `close`, `quit()`), joins
  through `shutdownFinished`, feeding `workerFinished()` (threshold 3 to 4). The destructor bumps generation and
  `quit()/wait()` like the existing workers, so no thread outlives the document (REQ-C-008).
- `Execution::Inline` mode (tests only, chosen at construction) uses direct calls and the same generation checks.

---

## 7. Memory accounting (REQ-F-015, REQ-NF-004)

Let F = W x H x 4 for the GIF's logical screen (constant for all frames), so F <= 32,000,000 x 4 = 128,000,000 B
(about 122 MiB) < 128 MiB `image_limit` (the pixel cap binds first).

Buffers alive at once for an animated document:

| Buffer | Owner | Size | Notes |
|--------|-------|------|-------|
| Displayed frame `image_` (shared with canvas `image_`) | GUI | F | replaced, never grown |
| Look-ahead frame `next_` | controller | F | at most one; consumed at advance |
| Frame in flight in the streamer / conversion temp | animation thread | up to F, transient | becomes `next_` or is dropped; the same transient the static path has (README "Memory beyond the bounds") |
| Cached neighbour images | decode worker cache | trimmed (below) | frame 0 only per entry |
| Codec-private canvases (GIF plugin composition/disposal) | Qt | ~1-2 F (Qt behaviour, to pin) | outside the accounted budget, like other codec-private memory |

There is no vector or list of decoded frames anywhere (REQ-F-015 review criterion): `next_` is one
`std::optional<Frame>`, `image_` one `QImage`. Frame count therefore cannot grow memory (a 100-frame and 10-frame
GIF of equal dimensions hold the same buffers).

Budget rule (README "Displayed image plus decode cache" = 256 MiB): with displayed + look-ahead = 2F, the cache
must fit in `256 MiB - 2F`. The existing static invariant (displayed <= 128 MiB, cache <= 128 MiB) would allow
3F = 366 MiB. Implementation: when the decode worker finishes a GIF frame 0 (format `GIF`; it cannot yet know
if it is multi-frame, so single-frame GIFs get the same conservative treatment) it calls
`cache_.setLimit(min(byteLimit, 256 MiB - 2 * image.sizeInBytes()))`; for any other format it restores
`setLimit(byteLimit)`. This runs inside the existing worker lambda (`image_document.cpp:350-380`) so the cache stays
single-thread. At F = 122 MiB the cache limit is about 12 MiB, so effectively no neighbour is cached; small GIFs keep
the full 128 MiB. Trimming on `setLimit` evicts LRU entries immediately.

The controller re-checks each frame with `image_limits.h` after `read()` (defence in depth; see 3.5) and treats a
failure as damaged.

Not accounted (documented in README "Design notes" update): codec-private canvases, the transient
raw+converted pair during conversion, and the clipboard copy (unchanged, F7).

Leaks: the streamer owns the reader via `unique_ptr`, closed on `close`, session replacement and thread exit;
`stop()` drops `next_`. T-U7 opens/closes 100-frame fakes 10x and asserts outstanding-frame count (QImage cleanup callbacks) returns
to zero; sanitiser run in CI per REQ-NF-004.

---

## 8. Keyboard: Space routing detail

`WindowKeyRouter` additions (file `window_key_router.h`):

```cpp
Q_PROPERTY(bool playbackAvailable MEMBER m_playbackAvailable)
signals: void playbackToggleRequested();
```
In `eventFilter`, inside the existing `else if (m_imageReady) { switch (key) ... }` chain (which is reached only when
`!modalActive`, the menu is not open and the key is not Tab):

```cpp
case Qt::Key_Space:
  if (!m_playbackAvailable || shift || keyEvent->isAutoRepeat() || focusedButton()) return false;
  emit playbackToggleRequested();
  break;
```
`focusedButton()` returns true when `m_window->activeFocusItem()` is one of `informationButton`, `fullscreenButton`,
`actionsButton` or `playPauseButton` (found by objectName exactly as the Tab branch already does). In that case
the event is not consumed and Qt delivers Space to the focused `Controls.Button`, which activates it as today
(REQ-F-032; activating `playPauseButton` yields the same `toggle()`). With the actions menu open the earlier
`m_menuOpen` branch returns `false` for Space (unchanged). With a modal active the top of the filter returns
`false`. For static images `playbackAvailable` is false, so the key falls through unconsumed.

Why not `Shortcut { sequence: "Space" }`: shortcut matching runs from `QGuiApplicationPrivate::processKeyEvent` before the
`KeyPress` reaches the focus item, unless the focus item accepts `ShortcutOverride`; `Controls.Button` does not, so a
Shortcut would intercept Space while a header button has keyboard focus. The existing single-key shortcuts (`F`, `R`,
`[`) have no button conflict. `WindowKeyRouter` already owns focus-sensitive keys (Tab, arrows). The router also
gives REQ-F-032's "a window-level Space handler exists and drives the same toggle as the button": both call
`togglePlayback()`.

`ImageCanvas::eventFilter` still emits `keyboardInput()` for any key press and hides the transient buttons (existing
"any keyboard input hides them"). `togglePlayback()` therefore reveals the details strip (3.2/5.2 item 4) so the
Playing/Paused state remains observable.

Cache-friendly note: Space auto-repeat is ignored to avoid flicker toggling while held.

---

## 9. Loop and end semantics

`LoopPolicy::fromQt(raw)` is the only place that interprets Qt's loop value. Provisional mapping from Qt's
documented `QImageReader::loopCount()`/`QMovie::loopCount()` convention (to pin in T-Q1):
`raw < 0` = infinite; `raw == 0` = play once (no repeats); `raw = N > 0` = N repeats after the first play, so
`totalPlays = raw + 1`. Unknown (scan failed) is treated as play once (GIF default without a NETSCAPE extension).
SPEC problem P1: REQ-F-003/016 define "0 = infinite, N = N plays"; that contradicts Qt's convention. The design keeps the
SPEC's user-visible intent (infinite stays infinite, finite stops on the final frame paused) and maps to Qt's
values; the SPEC's test parameters ("loop count 0 infinite", "loop count 1/2") must be re-derived in terms of the
file's NETSCAPE2.0 field and the resulting play counts (T-Q1 fixes the table).

Rewind: at `EndOfSequence` with `morePlays`, controller sends `rewind(G)`; the streamer calls `jumpToImage(0)`
(REQ-F-016), falling back to reopening the same `QFile` if the handler refuses (the single-reader invariant still
holds: the old reader is destroyed first), then reads frame 0 as the next look-ahead.

---

### 9.1 T-Q1 pinned results (Qt 6.11.2 GIF handler, `tests/frame_source_test.cpp`)

The characterisation test contradicts three provisional assumptions above. None invalidates the architecture, but
the implementation follows these results, not the earlier text:

1. **`jumpToImage(0)` is refused** (returns false mid-sequence and at the end, for `QFile` and `QBuffer`). The rewind
   in section 9 therefore always takes the "reopen" path: destroy the reader, `seek(0)` the same `QFile`, construct a
   new `QImageReader` (verified: it reads frame 0 with its delay and reports the same `imageCount`). The single-reader
   invariant holds. `rewind()` must not depend on `jumpToImage`.
2. **`QImageReader::size()` can under-report the canvas** (a 1x1 first frame on a 4096x2048 screen reports 1x1 while
   `read()` returns 4096x2048). The `size()` pre-check in the static path is not a GIF bound; the per-read allocation
   limit and the post-`read()` `acceptableSize`/byte checks are. The streamer never trusts `size()`.
3. **Delays**: `nextImageDelay()` after `read()` is the delay of the frame just read (confirmed); before the first read
   it is a 100 ms default. Qt converts centiseconds to ms and defaults only a zero delay (to 100 ms); 5 cs arrives raw as 50 ms.
   `clampFrameDelay()` stays responsible for the <= 10 ms -> 100 ms rule.

Confirmed as designed: loop convention (`NETSCAPE 0` -> -1 infinite, absent -> 0, N -> N repeats), `imageCount()`
available before any read, end of sequence is a null read with `InvalidDataError`, the allocation limit is
per `read()` and does not accumulate across sequential reads, GIF87a and GIF89a are both read.

---

## 10. Key decisions (rationale)

| Decision | Rationale | Reqs |
|----------|-----------|------|
| Animation is an add-on to the static path; frame 0 double decode | Zero change to Empty/Loading/Ready/Error, cache, prefetch, EXIF; single-frame GIF is trivially static | F-021, F-022, C-008, C-009 |
| Dedicated streaming thread + streamer QObject | Sequential reader must live on one thread; not on the decode worker whose queue serves navigation | F-013, NF-003 |
| Deadline-based scheduling on an injectable clock; advance = deadline reached AND look-ahead ready | Deterministic tests; no sleeping; late decode never blocks GUI | C-011, NF-003, F-001, F-002 |
| Separate `userPaused`, `suspendedByModal`, `suspendedByWindow` flags | User pause is never overridden; order-independent | F-025, F-026, C-010 |
| `replaceFrame()` on the canvas, `image_` updated without `changed()` | `setImage` resets view and floods QML handlers | F-018, F-019, NF-003 |
| Space in `WindowKeyRouter`, not `Shortcut` | Must yield to focused buttons | F-032 |
| Button and strip built into `Main.qml`, no new QML files | `ViewerButton` and strip internals are private to `Main.qml`; avoids CMake/format-discovery churn | C-002, C-004, C-005 |
| Viewer-owned glyphs | Shared icon repo has none; other repos out of bounds | F-010, C-003, C-007 |
| Count and loop via one `scan()` on the animation thread after frame 1 | Same reader; off GUI; also disambiguates end vs damage | F-012, F-013, F-023 |
| Cache limit shrinks with frame size while a GIF is displayed | Keeps display + look-ahead + cache within 256 MiB | F-015, NF-004 |

---

## 11. Alternatives considered

1. `QMovie`: hides the reader and threading, cannot honour the exact clamp, look-ahead, pause-with-remaining-delay,
   generation cancellation or fake clock; rejected for REQ-F-013/C-011.
2. Hand the reader from `decodeImage()` to the controller (avoid double decode): needs `DecodeResult` to carry a live
   `QFile`+`QImageReader` across threads and through the cache/prefetch paths (which drop results), entangling static
   and animated paths. Deferred as a possible optimisation; measured cost is one frame decode after paint.
3. Decode frame 1 before showing frame 0 to detect multi-frame: delays first paint (3.1). Rejected.
4. Pre-scan `imageCount()` at open on the decode worker: O(file) before the first paint. Rejected.
5. `QTimer` factory injection instead of a clock interface: cannot express remaining-delay arithmetic for pause/resume
   without a time source anyway; the clock interface covers both.
6. Route frames through `ImageDocument::image` + `changed()`: resets view, cancels drags, hides strip each frame (F4/F5).
7. Cache decoded frames of the GIF: violates the fixed budget and REQ-F-015 ("no growable list of frames").
8. `Shortcut { sequence: "Space" }`: steals Space from focused buttons (section 8).
9. A menu item "Play/Pause": not required by the SPEC; left out to avoid scope growth (a later `Controls.Action`
   could reuse `togglePlayback()`).
10. Look-ahead depth > 1: more memory for no visible gain at >= 100 ms clamped delays; one frame matches the SPEC.

---

## 12. Test and verification plan (REQ-C-006, C-009)

All new tests are added to the `viewer-smoke` executable list in `tests/CMakeLists.txt` (the SPEC's `apps/viewer/tests/`
does not exist; tests live in `tests/`, P8) and run through `task check`/CTest.

- T-Q1 characterisation (`frame_source_test.cpp`, real Qt GIF plugin, skipped like `FirstAnimationFrameOnly` if the
  plugin is absent): pins `imageCount`, `loopCount` (no extension / NETSCAPE 0 / N), `nextImageDelay` semantics
  (delay of the frame just read), `jumpToImage(0)`, null-read-at-end vs truncated file, per-read allocation-limit behaviour, GIF87a.
- T-U1..U8 (`animation_controller_test.cpp`, `ManualPlaybackClock`, fake `FrameSource`, `Inline`): autoplay timing
  (F-001), clamp table (F-002), finite/infinite loops and rewind (F-003, F-016), pause/resume remaining delay and
  no extra `readFrame` (F-017), damaged frame N>=1 including over-limit fake frames (F-014, F-023), generation drop of stale
  frames (F-027/028), pause reasons matrix (F-025/026, C-010), outstanding-frame count <= 2 and leak loop (F-015, NF-004).
- T-U9 threaded (`Execution::Threaded`, real GIF fixture built like the existing raw-bytes fixture): opening, stop mid-decode,
  no crash; shutdown joins.
- T-I1 (`image_document_test.cpp`): rename/rewrite `Document.FirstAnimationFrameOnly` (F12, allowed by REQ-C-009) to assert
  autoplay of frame 1; a single-frame GIF stays static; Refresh restarts at frame 0 (C-012); frame 0 failures give Error (F-022);
  copy while playing copies the current frame and does not change state (F-020).
- T-UI1 (`animation_ui_test.cpp`, offscreen QML with the fake controller): button objectName/enabled/visibility/fade
  (F-005..F-009), Space vs focused header button vs open card vs static image (F-004, F-032), strip labels and notice
  (F-011, F-024), transforms do not pause (F-018/019), footer/help rows (F-033/034); `ImageCanvas::replaceFrame` keeps zoom/pan and emits no `imageChanged`.
- Existing files touched: `shortcut_help_popup_test.cpp` (new section), `installed_runtime.cpp` (`required` adds `"gif"`, extension list adds `gif`),
  `scripts/format-fixtures.py` (a small GIF fixture), README (supported formats table, keyboard table `Space`, overlays, design notes, limits text
  "animated files show first frame only" narrowed to non-GIF), qualification per README "Missing codecs fail qualification".
- T-P1/T-P2 native: 32 MP GIF frame swap timing on the existing performance harness; scan time on a 256 MiB GIF.
- T-M1..M3 manual (ask the user; never automate pointer or focus per AGENTS.md): minimise/restore (F-026), 1x/2x glyph crispness (F-010), 420x280 layout (NF-001), transparency and disposal (F-030).

---

## 13. SPEC problems flagged (not silently worked around)

| ID | Where | Problem | Design handling |
|----|-------|---------|-----------------|
| P1 | REQ-F-003, REQ-F-016 | "Loop count 0 = infinite, N = N plays" conflicts with Qt's convention (-1 infinite, 0 no repeat, N repeats after the first play) as documented for `QImageReader/QMovie::loopCount()`. | `LoopPolicy::fromQt`; T-Q1 pins; SPEC test parameters to be restated as plays. |
| P2 | REQ-F-009 AC | AC says after pausing the name becomes "Pause"; the requirement text and REQ-F-008 say Play when paused. AC is inverted. | Follow the requirement text (name = available action). |
| P3 | REQ-F-012 | "Reader reports count cheaply": GIF `imageCount()`/`loopCount()` do a whole-file scan (Qt behaviour, to pin). | Scan once on the animation thread after frame-1 look-ahead; fallback at first wrap (3.7). |
| P4 | REQ-F-014 | All GIF frames share the logical-screen size that frame 0 passes, so over-limit later frames cannot exist in a real GIF; a "100 M px frame" is frame 0 (Error state, F-022), not "notice after frame 0"; `canRead()` is not a size check. | Per-frame re-check retained; later-frame limit tests use a fake `FrameSource`. |
| P5 | REQ-F-015 | "Cache is recycled per frame / one-frame buffer": the real `DecodedImageCache` is a URL-keyed whole-image LRU and is not involved with frames. Real invariant is displayed + one look-ahead. | 2F accounting and cache-limit shrink (section 7). |
| P6 | REQ-F-013 | "Scratch frame buffer reused": `QImageReader::read()` returns a fresh image per frame, and `decodeImage`'s temporary reader precedes the streamer's (sequential, never simultaneous). | One reader open at any time; frames fresh; documented. |
| P7 | REQ-F-024 vs REQ-F-011 | Notice must stay until navigation, but the strip conceals after 3 s. | Notice is part of the strip and re-appears on every reveal; the strip is revealed on failure and the event is announced for accessibility. Add a pinned label only if the user wants persistence beyond reveals. |
| P8 | REQ-C-006 | Path `apps/viewer/tests/` does not exist; tests are in `tests/`. Also REQ-F-031's "File > Image Information ... GIF: not supported" has no counterpart in the code; qualification is `installed_runtime.cpp` + fixtures (F13). | Use `tests/`; extend qualification to GIF and update README. |
| P9 | REQ-F-026 | On Wayland a compositor-initiated minimise is not reported to the client as `Window.Minimized`; only client-initiated minimise/hide is. | Bind `!visible || Minimized`; verify natively (T-M1); record residual risk R2. |
| P10 | REQ-F-006 AC | "Opacity and arrowsShown transition together (same frame)" conflicts with a 150 ms opacity animation. | Same `shown` flag and same `Behavior`; interpreted as sharing the flag. |
| P11 | REQ-F-008 | "Centered on the image": the image can be zoomed/panned. | Centred on `canvasArea`. |
| P12 | Intro / REQ-F-031 | "GIF becomes a guaranteed format" needs README and qualification updates that no requirement names. | Included in section 12. |

---

## 14. Known risks

- R1: Qt behaviours marked "to pin" (loop convention, `imageCount` scan cost, per-read allocation limit, `nextImageDelay`
  timing, `jumpToImage(0)`) are recalled, not read. T-Q1 gates implementation; the isolated adapters (`FrameSource`,
  `LoopPolicy`) confine any correction.
- R2: Compositor-initiated minimise on Wayland is not detectable (P9); playback would keep running while hidden.
- R3: Codec-private composition memory is outside the 256 MiB accounting (7). Worst case near 2F to 4F at the 32 MP
  limit; consistent with README's existing disclaimer but larger. Mitigation if measured too high: lower cache to zero
  (already effective) and document.
- R4: Large-frame paint cost may exceed NF-003's 50 ms; then a texture path must be proposed (SPEC Non-Goal).
- R5: A scan of a very large GIF can delay the second look-ahead (3.7); playback holds a frame instead of stuttering
  the GUI. Consider a size threshold that skips the proactive scan and defers to first wrap if T-P2 shows a problem.
- R6: Double decode of frame 0 and a fresh `QFile` open mean a file replaced between decode and priming can mismatch;
  size mismatch or priming failure leaves the document static (no animation), never an error.
- R7: 420x280 minimum: centred button and a wrapped strip (notice adds a line) can overlap on the shortest canvases
  (NF-001 manual check; strip is `visible` only transiently).
- R8: Prefetch of a neighbouring GIF decodes frame 0 only and later re-primes on selection; behaviour is correct but
  keeps the extra decode.

---

## 15. Requirement-to-component traceability

Abbreviations: AC = `AnimationController`, FS = `FrameSource`/`QtGifFrameSource`, ST = `FrameStreamer`, PC = `PlaybackClock`,
ID = `ImageDocument`, CV = `ImageCanvas`, KR = `WindowKeyRouter`, DC = `DecodedImageCache`, IL = `image_limits.h`,
M = `Main.qml`, FH = `FooterKeyHints.qml`, SH = `ShortcutHelpPopup.qml`, IC = icons, T = tests/build files.

| REQ | Component(s) | Section |
|-----|--------------|---------|
| F-001 | AC, PC, ID (start in `complete`) | 3.1, 3.2, 4 |
| F-002 | AC (`clampFrameDelay`), PC | 3.2, 5.1 |
| F-003 | AC, `LoopPolicy`, FS (`scan`) | 9 |
| F-004 | KR, M (`togglePlayback`), AC | 8 |
| F-005 | M (`playPauseButton.onClicked`), AC | 5.2 |
| F-006 | M (`arrowsShown`, `arrowTimer`, opacity/visible) | 5.2 |
| F-007 | M (`enabled`/`visible`), AC (`animated`, `canToggle`) | 5.2 |
| F-008 | M (`cornerRadius`, glyph binding), IC | 3.4, 5.2 |
| F-009 | M (`Accessible.name`) | 5.2, P2 |
| F-010 | IC, CMake icon list | 3.4 |
| F-011 | M (strip model), AC (`frameCount`, `playing`) | 5.2 |
| F-012 | FS (`scan`), ST, AC | 3.7 |
| F-013 | FS, ST (one reader, animation thread) | 3.1, 6 |
| F-014 | IL, ST/AC (per-frame re-check), tests with fake FS | 3.5, P4 |
| F-015 | AC (`next_` only), ID, DC (`setLimit`) | 7 |
| F-016 | FS (`rewind`), AC, `LoopPolicy` | 9 |
| F-017 | AC (`pause`/`resume`, `remaining_`) | 4 |
| F-018 | CV (`replaceFrame`), ID (`image_` tracks frame), AC (no transform hooks) | 3.3, 3.8 |
| F-019 | AC (`userPaused` independent of transforms) | 4 |
| F-020 | ID (`copyImage` uses current `image_`), ClipboardController (unchanged) | 3.8 |
| F-021 | ID/AC (initial `animated=false`), M, KR, FH | 3.1 |
| F-022 | ID/`decodeImage` (unchanged Error path) | 3.8 |
| F-023 | AC, ST, FS (`Damaged`), M (`playbackNotice`) | 4, 5.2 |
| F-024 | M (`playbackNotice`, palette token), AC (`failureNotice` cleared by `stop`) | 5.2, P7 |
| F-025 | AC (`suspendedByModal`), M (`Binding`) | 4, 5.2 |
| F-026 | AC (`suspendedByWindow`), M (`Binding`) | 4, P9 |
| F-027 | AC/ST generation token | 6 |
| F-028 | AC/ST (double check before publish) | 6 |
| F-029 | FS (Qt GIF plugin), T (GIF87a/89a fixtures) | 12 |
| F-030 | FS (Qt composites; no custom disposal) | 3.8 |
| F-031 | `decodeImage` error path, `installed_runtime.cpp` + fixtures, README | 12, P8 |
| F-032 | KR, M | 8 |
| F-033 | SH | 5.2 |
| F-034 | FH, M | 5.2 |
| NF-001 | M (layout, 48 px centred), T-M3 | 5.2, R7 |
| NF-002 | M (palette tokens only) | 5.2 |
| NF-003 | ST, PC, CV (`replaceFrame`), AC (deadline scheduling) | 3.2, 3.3, 6 |
| NF-004 | AC, ST, DC (`setLimit`), sanitiser run | 7 |
| NF-005 | M (`Accessible.*` on button, strip labels, notice) | 5.2 |
| NF-006 | M, FH, SH (`qsTr`) | 5.2 |
| NF-007 | M (same `Behavior on opacity { NumberAnimation { duration: 150 } }`) | 5.2 |
| C-001 | all (Qt6 only, no new dependency) | 2 |
| C-002 | C++ in `apps/viewer`, QML in `apps/viewer/qml` | 2 |
| C-003 | IC (viewer-owned) | 3.4 |
| C-004 | CMake (no new QML file; new C++ in `SOURCES`/`viewer-private`, icons in list) | 2 |
| C-005 | T/CMake (existing checks cover new files; `check-qml-format-discovery.py` unaffected) | 2, 12 |
| C-006 | T (`tests/CMakeLists.txt` viewer-smoke list) | 12 |
| C-007 | whole design (viewer only; glyph decision) | 3.4 |
| C-008 | ID owns AC; AC owns FS lifetime; `shutdown` and destructor | 4, 6 |
| C-009 | static path unchanged; only `FirstAnimationFrameOnly` and help-section test change | 3.8, 12 |
| C-010 | AC (reasons limited to modal and window) | 4 |
| C-011 | PC, `ManualPlaybackClock`, `Execution::Inline` | 3.2 |
| C-012 | ID (`refresh` -> `select` -> `stop`/`start`), AC | 3.9 |
