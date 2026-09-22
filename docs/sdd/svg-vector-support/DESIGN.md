# SVG Vector Support — Design

**Spec**: `docs/sdd/svg-vector-support/SPEC.md` (20 requirements, REQ-F-001..014, REQ-C-001..003, REQ-NF-001..003)
**Status**: Draft for review

This document designs the implementation against the actual current code in `apps/viewer/`. All file paths, method
names, and line references below were read from the repository at the current `main` tip (see the commit history in
this session's `git status`), not inferred.

---

## 1. Components

For each changed file: what changes, and which REQ-ID(s) it satisfies.

### `apps/viewer/image_limits.h`

Add a new, SVG-specific constant, kept separate from the raster limits already there (`kImageLimitBytes = 128 MiB`,
`kFileLimitBytes = 256 MiB`, `acceptableSize()`, `acceptableImage()`):

```cpp
// SVG is XML text, not a raster codec; kFileLimitBytes/kImageLimitBytes do not apply to it.
inline constexpr qint64 kSvgFileLimitBytes = 10 * 1024 * 1024;
```

Satisfies **REQ-C-001**. Nothing else in this file changes — `acceptableSize()`/`acceptableImage()` are raster-pixel
budget checks and are not called on the SVG path at all (see §2).

### `apps/viewer/image_document.h` / `image_document.cpp`

This is where most of the work lands, because `ImageDocument` already owns the entire decode pipeline
(`decodeImage()`/`readImage()`), the format-string dispatch used for GIF (`information_.format ==
QLatin1String("GIF")` in `complete()`, line 419), `nameFilters()`, and the `DecodeResult`/`ImageInformation`
structs that both the decode worker and `informationSections()` consume.

- **`DecodeResult`** (currently `{QImage image; QString error; ImageInformation information;}`) gains one field:
  `QByteArray svgData;`. It stays empty for every non-SVG result and for failed SVG decodes; it holds the raw file
  bytes only for a successfully validated SVG. This is what lets `ImageDocument` retain "the loaded SVG renderer
  and raw SVG bytes... directly" (REQ-F-009) without adding a second return channel.

- **New free function** `QSize svgIntrinsicSize(const QSvgRenderer& renderer)`, declared next to `decodeImage()`/
  `commandLineUrl()` at file scope (both are already free functions in this header for the same reason: they're
  used from both the worker thread and, for this new function, from `ImageCanvas`). Implementation:

  ```cpp
  QSize svgIntrinsicSize(const QSvgRenderer& renderer) {
    const auto viewBox = renderer.viewBoxF();
    return viewBox.isValid() ? viewBox.size().toSize() : renderer.defaultSize();
  }
  ```

  Satisfies **REQ-F-005** (viewBox authoritative) and **REQ-F-006** (defaultSize fallback, which is itself already
  Qt's resolution of width/height attributes or the spec's 300×150 default — `QSvgRenderer::defaultSize()` does
  this internally, so no extra fallback logic is needed). This one function is the single source of truth for "SVG
  intrinsic size," called from two places (see §2), which is why it is factored out rather than inlined twice.

- **New free function** `DecodeResult decodeSvg(QFile& file, const std::atomic_bool& cancelled, ImageInformation
  facts)`, mirroring the shape of the existing `readImage()` helper in the anonymous namespace. It:
  1. Rejects the file if `file.size() > kSvgFileLimitBytes` **before** reading or parsing anything (REQ-F-008,
     REQ-C-001) — this is a size check on the already-open `QFile`, symmetric with the existing
     `file.size() > kFileLimitBytes` check in `decodeImage()` (line 133), just against the tighter constant.
  2. Reads the whole file into a `QByteArray`.
  3. Constructs a **transient, stack-local** `QSvgRenderer` and calls `load(file.fileName())`, preserving the
     document directory as the base for trusted local references. If loading fails,
     returns `{.image = {}, .error = <message>, .information = facts}` with `svgData` left empty — this is the
     exact same failure shape (`image` null, `error` non-empty) that every other decode failure in this file uses,
     which is what carries it through the existing error path (see §4).
  4. On success, sets `facts.format = QStringLiteral("SVG")`, `facts.decodedSize = svgIntrinsicSize(renderer)`, and
     returns `{.image = {}, .error = {}, .information = facts, .svgData = bytes}`.

  The transient renderer is discarded at the end of this function — it exists only to validate the file and read
  its intrinsic size on the worker thread. It is never handed to `ImageCanvas` or stored; see §3 for why a second,
  persistent `QSvgRenderer` is constructed later on the GUI thread instead of reusing this one.

- **`decodeImage()`** (the top-level entry point, called from the worker thread) gets one new early branch, placed
  right after the existing "file exists / is a regular file / opens for reading" checks (lines 121–135) and
  **before** both `ExifMetadata::read()` (line 136) and `file.size() > kFileLimitBytes` (line 133 applies only to
  the raster path once this branch exists):

  ```cpp
  if (info.suffix().compare("svg", Qt::CaseInsensitive) == 0) {
    if (file.size() > kSvgFileLimitBytes) {
      return {.image = {}, .error = tr("The SVG file exceeds the 10 MiB size limit."), .information = facts};
    }
    return decodeSvg(file, cancelled, facts);
  }
  ```

  SVG never reaches `ExifMetadata::read()`, `readImage()`/`QImageReader`, `acceptableImage()`, or
  `image.convertTo(Format_ARGB32_Premultiplied)` — those are all raster-only concerns and REQ-F-014 explicitly
  wants no EXIF attempt for SVG. Satisfies **REQ-F-008**, **REQ-F-009** (in conjunction with `complete()` below),
  **REQ-F-012** (nothing in this path touches the network — `QFile` + `QSvgRenderer::load(localPath)` are both
  local/synchronous), **REQ-F-013** (the local-path load retains the base directory needed to resolve local
  `<image xlink:href="...">`
  references at ordinary trust level; no extra sandboxing is added, matching the requirement's rationale that the
  user's own choice to open the file establishes trust).

- **Format dispatch by extension, not content-sniffing.** `decodeImage()` picks the SVG branch by file suffix, not
  by peeking at bytes the way `simpleWebPFallback()` or the raster `readImage()`/`QImageReader` path do. This is
  deliberate — see §4 for the rationale (SVG has no reliable magic-byte signature).

- **`ImageDocument` members** (header): add

  ```cpp
  QByteArray svg_data_;
  QSvgRenderer svg_renderer_;
  ```

  as plain value members, **not** `std::unique_ptr<QSvgRenderer>`. This matches the file's existing style: `Q_OBJECT`
  members `ClipboardController clipboard_;` and `AnimationController animation_;` are already owned by value inside
  `ImageDocument` despite being `QObject` subclasses. `svg_renderer_` is default-constructed in `ImageDocument`'s
  constructor (so it is born on the GUI thread, same as `ImageDocument` itself) and only ever mutated via `load()`
  from `complete()`, which always runs on the GUI thread (it's invoked via `Qt::QueuedConnection` back onto `this`,
  line 393–395). It is never touched from `worker_`'s thread. This sidesteps any `QObject` thread-affinity concern
  without needing a pointer, a mutex, or a move across threads.

- **`complete()`** (line 400): the success/failure decision currently reads `state_ = image_.isNull() ? Error :
  Ready;`. Extend the success condition to also accept a non-empty SVG payload:

  ```cpp
  svg_data_ = std::move(result.svgData);
  ...
  state_ = (image_.isNull() && svg_data_.isEmpty()) ? Error : Ready;
  ```

  Before publishing this state through `changed()`, load the persistent renderer from the selected local filename:

  ```cpp
  if (error_.isEmpty() && information_.format == QLatin1String("SVG") &&
      !svg_renderer_.load(request.url.toLocalFile())) {
    svg_data_.clear();
    error_ = tr("The SVG file is damaged or could not be parsed.");
  }
  state_ = (image_.isNull() && svg_data_.isEmpty()) ? Error : Ready;
  ```

  This preserves the SVG's base directory for relative local references, ensures observers never receive an
  invalid SVG renderer, and routes a second-load failure through the ordinary error path. It is the **one and only
  place** the persistent `svg_renderer_.load()` is called for a freshly opened document, so the SVG is parsed once
  per document open on the GUI thread, not once per paint. Satisfies **REQ-F-009**, and is the mechanism behind
  "re-parse-per-frame is avoided" (see §2/§3).

- **`select()`** (line 308) and the malformed-`open()` early-return branch (line 278–303) already reset `image_ =
  {}`, `information_ = {}`, `error_.clear()` etc. Add `svg_data_.clear();` alongside those resets. `svg_renderer_`
  does not need an explicit `.load({})` reset there: the `svgRenderer()` getter (below) already gates on
  `information_.format == "SVG"`, so a stale loaded renderer from a previous document is simply never exposed once
  `information_` is reset — but `svg_data_.clear()` is still added for the same reason `image_` and `information_`
  are cleared: so `state()`/`informationSections()` don't observe stale data between `select()` and the next
  `complete()`.

- **Two new `Q_PROPERTY` declarations / accessors**, both `NOTIFY changed` (same signal every other document-state
  property already uses):

  ```cpp
  Q_PROPERTY(QSvgRenderer* svgRenderer READ svgRenderer NOTIFY changed)
  Q_PROPERTY(QImage previewImage READ previewImage NOTIFY changed)
  ```

  ```cpp
  [[nodiscard]] QSvgRenderer* svgRenderer() {
    return state_ == Ready && information_.format == QLatin1String("SVG") ? &svg_renderer_ : nullptr;
  }
  [[nodiscard]] QImage previewImage() const;
  ```

  `svgRenderer()` is what the **main** `ImageCanvas` binds to (§2/§3); it is the discriminator the canvas uses to
  pick its paint path, so it must be non-null if and only if vector painting is what should happen. `previewImage()`
  is what the **popup thumbnail** `ImageCanvas` binds to instead of `image` (§2, REQ-NF-002); like `svgRenderer()`
  above, it is declared non-const (not `const`, and no `mutable` member needed) because it calls
  `QSvgRenderer::render()`, which is non-const — same rationale as `svgRenderer()`'s own non-const declaration two
  lines above. Its implementation:

  ```cpp
  QImage ImageDocument::previewImage() {
    if (!image_.isNull()) {
      return image_;  // Raster formats: already-decoded, already the right shape for drawImage().
    }
    if (state_ != Ready || information_.format != QLatin1String("SVG") || !svg_renderer_.isValid()) {
      return {};
    }
    constexpr int kPreviewMaxDimension = 256;  // Comfortably above the 96x96 popup box at any DPR this app targets.
    auto raster = QSize(svgIntrinsicSize(svg_renderer_)).scaled(kPreviewMaxDimension, kPreviewMaxDimension,
                                                                 Qt::KeepAspectRatio);
    if (raster.isEmpty()) {
      return {};
    }
    QImage preview(raster, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    QPainter painter(&preview);
    svg_renderer_.render(&painter, preview.rect());
    return preview;
  }
  ```

  Satisfies **REQ-NF-002**. See §4 for why this lives in `ImageDocument` rather than in `ImageCanvas`.

- **`nameFilters()`** (line 268): add one unconditional entry, independent of `QImageReader::supportedImageFormats()`:

  ```cpp
  QStringList ImageDocument::nameFilters() {
    QStringList patterns;
    for (const auto& format : QImageReader::supportedImageFormats()) {
      patterns.append("*." + QString::fromLatin1(format));
    }
    patterns.append(QStringLiteral("*.svg"));
    patterns.sort();
    patterns.removeDuplicates();
    return {tr("Images (%1)").arg(patterns.join(' ')), tr("All files (*)")};
  }
  ```

  Satisfies **REQ-F-002**, and transitively **REQ-F-001**(1) — `nameFilters()` is the sole source for both the
  portal file chooser (`apps/viewer/portal_file_chooser.cpp`, `setNameFilters()`) and the QML `FileDialog` fallback
  (`qml/Main.qml` lines 429 and 451, both bind `nameFilters: window.document.nameFilters`), so this single change
  reaches both file-open surfaces.

- **`#include <QSvgRenderer>`** and **`#include <QPainter>`** added to `image_document.cpp` (the latter is needed
  only for `previewImage()`'s on-demand rasterization; `image_document.h` needs `#include <QSvgRenderer>` for the
  `svg_renderer_` member and the `QSvgRenderer*` property).

### `apps/viewer/image_canvas.h` / `image_canvas.cpp`

- **New `Q_PROPERTY`**:

  ```cpp
  Q_PROPERTY(QSvgRenderer* svgRenderer READ svgRenderer WRITE setSvgRenderer NOTIFY imageChanged)
  ```

  reusing the existing `imageChanged` signal rather than adding a new one — QML consumers of this canvas (Main.qml's
  `onImageChanged:` handler at line 692) already treat "the displayed content changed" as one event; a second,
  parallel `svgRendererChanged` signal would just be two notifications for what is conceptually the same transition
  and would need to be wired up identically everywhere `imageChanged` already is.

- **New private members**:

  ```cpp
  QSvgRenderer* svg_renderer_ = nullptr;  // Non-owning; ImageDocument owns the lifetime.
  QSize content_size_;                    // Intrinsic size in content units, raster or vector.
  ```

  `content_size_` is new and replaces the raw `image_.size()` reads currently inline in `setImage()`,
  `setOrientation()`, and `paint()` (lines 37, 54, 112). It exists because, in vector mode, `image_` stays a null
  `QImage` (§3) — there is no `QImage` to call `.size()` on — but `ViewGeometry::setImage()` and
  `ImageOrientation::mapping()`/`dimensions()` both need *some* `QSize` regardless of mode, and they are pure
  `QSize`-in functions (`ImageOrientation::mapping()`/`dimensions()` operate only on `QSize` and `int`, verified in
  `apps/viewer/image_orientation.cpp` — only `ImageOrientation::apply()`, unused by `ImageCanvas`, touches actual
  pixels). `content_size_` unifies the two modes at exactly the one seam that needs it.

- **`setImage(const QImage&)`**: unchanged in spirit, but now also clears vector mode and updates `content_size_`:

  ```cpp
  void ImageCanvas::setImage(const QImage& image) {
    if (svg_renderer_ == nullptr && image_.cacheKey() == image.cacheKey()) {
      return;
    }
    ++generation_;
    image_ = image;
    svg_renderer_ = nullptr;
    content_size_ = image.size();
    view_.setImage(ImageOrientation::dimensions(orientation_, content_size_));
    refresh();
    emit imageChanged();
  }
  ```

- **New `setSvgRenderer(QSvgRenderer* renderer)`**, the vector-mode counterpart:

  ```cpp
  void ImageCanvas::setSvgRenderer(QSvgRenderer* renderer) {
    if (svg_renderer_ == renderer) {
      return;
    }
    ++generation_;
    svg_renderer_ = renderer;
    image_ = {};
    content_size_ = renderer != nullptr && renderer->isValid() ? svgIntrinsicSize(*renderer) : QSize{};
    view_.setImage(ImageOrientation::dimensions(orientation_, content_size_));
    refresh();
    emit imageChanged();
  }
  ```

  Reuses `svgIntrinsicSize()` from `image_document.h` (§1 above) rather than re-deriving viewBox-vs-defaultSize
  logic a second time — this is the second of the two call sites that function exists for.

- **`paint()`** (line 103): the destination/visible/clip/scale/orientation-mapping setup (lines 107–116) is
  unchanged and shared by both modes — it already operates purely on `view_` and `orientation_`, neither of which
  is raster-specific. Only the final draw call becomes a two-way branch:

  ```cpp
  void ImageCanvas::paint(QPainter* painter) {
    if (!view_.valid()) {
      return;
    }
    const auto destination = view_.rect();
    const auto visible = destination.intersected(boundingRect());
    const QRectF source((visible.topLeft() - destination.topLeft()) / view_.scale(), visible.size() / view_.scale());
    painter->setClipRect(boundingRect());
    painter->setRenderHint(QPainter::SmoothPixmapTransform, view_.magnification() < 1);
    const auto mapping = ImageOrientation::mapping(orientation_, content_size_);
    painter->translate(destination.topLeft());
    painter->scale(view_.scale(), view_.scale());
    painter->setTransform(mapping, true);
    // Dual-mode paint (REQ-NF-003): raster formats rasterize once at decode time and are blitted every frame;
    // SVG has no raster form to blit and is painted as vector geometry directly under the current transform, so
    // it stays crisp at any zoom instead of resampling a fixed-resolution QImage.
    if (svg_renderer_ != nullptr && svg_renderer_->isValid()) {
      svg_renderer_->render(painter, QRectF(QPointF(0, 0), content_size_));
    } else {
      const auto decodedSource = mapping.inverted().mapRect(source);
      painter->drawImage(decodedSource, image_, decodedSource);
    }
    if (painted_generation_ != generation_) {
      ...  // unchanged
    }
  }
  ```

  Satisfies **REQ-F-004**, **REQ-NF-001**, **REQ-NF-003**. Note the vector branch renders the *full* `content_size_`
  rect, not the visibility-cropped `decodedSource` the raster branch computes — see §4 for why that cropping is a
  raster-only optimization that doesn't apply here.

- **`replaceFrame()`** (GIF frame-swap path, line 41): unchanged. It is wired only to
  `AnimationController::frameReady`, which only fires after `animation_.start()`, which only runs for
  `format == "GIF"` (`image_document.cpp` line 419-421) — mutually exclusive with the SVG path by construction, so
  no guard against vector mode is needed here.

- **`#include <QSvgRenderer>`** added to `image_canvas.h`.

### `apps/viewer/directory_model.cpp`

`scanDirectory()` (line 59) builds its sibling suffix set from `QImageReader::supportedImageFormats()` (lines
64–66). Add one unconditional entry:

```cpp
QSet<QString> suffixes;
for (const auto& format : QImageReader::supportedImageFormats()) {
  suffixes.insert(QString::fromLatin1(format).toCaseFolded());
}
suffixes.insert(QStringLiteral("svg"));
```

Satisfies **REQ-F-003**, and transitively **REQ-F-001**(2)/(3): this suffix set is the only thing standing between
an `.svg` file and being invisible to sibling navigation, and it is independent of whatever raster plugins happen
to be installed.

### `CMakeLists.txt` (repo root) and `apps/viewer/CMakeLists.txt`

Two edits, in two different files:

1. Root `CMakeLists.txt` line 17 — add `Svg` to the `find_package` component list:

   ```cmake
   find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui GuiPrivate DBus Quick Qml QuickControls2 Svg)
   ```

2. `apps/viewer/CMakeLists.txt` line 71 — the *executable* target (`holonight-viewer`, built by
   `add_viewer_application()`) is what actually compiles `image_document.cpp` and `image_canvas.cpp` (they are
   listed directly in that function's `qt_add_qml_module(... SOURCES ...)` block, lines 65–66 — they are **not**
   part of the `viewer-private` static library, whose sources are `image_orientation.cpp directory_model.cpp
   decoded_image_cache.cpp exif_metadata.cpp view_geometry.cpp portal_request_builder.cpp wayland_foreign_export.cpp
   frame_source.cpp frame_streamer.cpp`). So `Qt6::Svg` is linked there, not into `viewer-private`:

   ```cmake
   target_link_libraries(${target} PRIVATE viewer-private Qt6::QuickControls2 Qt6::Svg
     HolonightQt::Core HolonightQt::Controls PkgConfig::WebP)
   ```

   `directory_model.cpp` lives in `viewer-private` and only needs `QSet`/`QImageReader` (already available via
   `Qt6::Quick`'s transitive `Qt6::Gui`), so `viewer-private` itself needs no new link.

3. `tests/CMakeLists.txt` (root-level `tests/`, where `image_document_test.cpp` and `view_geometry_test.cpp` live):
   whichever test binary links `image_document.cpp`/`decodeSvg()`/`svgIntrinsicSize()` needs `Qt6::Svg` too. Not
   read in detail for this design pass; flagged here so the implementer checks it — the acceptance criterion in
   REQ-C-002 ("verify the build succeeds with this dependency") covers `BUILD_TESTING=ON` as well as the plain app
   build.

Satisfies **REQ-C-002**, and — because no other `find_package`/`pkg_check_modules`/`target_link_libraries` entry is
touched — **REQ-C-003** (Qt6::Svg is the only new dependency; no third-party or codec-plugin addition).

### `apps/viewer/qml/Main.qml`

One property binding added to the main `ImageCanvas` instance (currently lines 683–709):

```qml
ImageCanvas {
    id: canvas
    objectName: "imageCanvas"
    anchors.fill: parent
    image: window.document.image
    svgRenderer: window.document.svgRenderer
    orientation: window.document.orientation
    ...
```

Nothing else in `Main.qml` changes. In particular, the error-state UI at lines 964–965 (`visible: ... state ===
ImageDocument.Error`, `rawText: ... window.document.error`) and the `Accessible.announce` at line 192 already read
generically from `document.state`/`document.error` — they need no SVG-specific branch because `ImageDocument`
surfaces SVG failures through the exact same `state_`/`error_` fields as every other format (§4).

### `apps/viewer/qml/information/ImageInformationPopup.qml`

One property renamed on the popup's (second, smaller) `ImageCanvas` instance (currently lines 75–83):

```qml
ImageCanvas {
    anchors.fill: parent
    anchors.margins: HnMetrics.borderWidth
    image: root.document.previewImage
    orientation: root.document.orientation
    displayPixelRatio: Screen.devicePixelRatio
    enabled: false
    Accessible.ignored: true
}
```

`image` now binds to the new `previewImage` property instead of `image`, and **`svgRenderer` is deliberately never
bound here** — that omission is the entire mechanism behind REQ-NF-002 (§4).

### Files that need **no** change

- `apps/viewer/exif_metadata.h` / `.cpp`: `ExifMetadata::payload()` signature-sniffs for JPEG (`\xff\xd8`), PNG
  (`\x89PNG\r\n\x1a\n`), WebP (`RIFF`...`WEBP`), and TIFF (`II*\0`/`MM\0*`) magic bytes (lines 277–291) and falls
  through to `return {};` for anything else. An SVG file's bytes (`<?xml...` or `<svg...`) match none of these, so
  `payload()` already returns empty and `parse()` already short-circuits on `payload.isEmpty()` (line 296). This
  file needs no change — and as designed above, `decodeImage()`'s new SVG branch never calls `ExifMetadata::read()`
  in the first place, so the question is moot twice over. Confirms **REQ-F-014**.
- `apps/viewer/decoded_image_cache.h` / `.cpp`: no code change, but see §2/§4 for why SVG never populates it.
- `apps/viewer/frame_source.h` / `.cpp`, `apps/viewer/animation_controller.*`: untouched. SVG animation is an
  explicit non-goal; nothing in this design constructs a `FrameSource` or calls `AnimationController::start()` for
  an SVG document (that dispatch is gated on `information_.format == "GIF"` and stays that way).
- `apps/viewer/view_geometry.h` / `.cpp`: untouched. `ViewGeometry::setImage(QSize)`,
  `fitMagnification()`, `zoom()` etc. already operate purely on a `QSize image_` with no assumption about how that
  size was produced. `ImageCanvas` is the only caller and already funnels both raster and vector sizes through the
  same `view_.setImage(ImageOrientation::dimensions(orientation_, content_size_))` call. Confirms **REQ-F-007**.

---

## 2. Data flow

### Opening an SVG file, end to end

1. **QML → `ImageDocument::open()`** — user picks a `.svg` via the portal chooser or drags a file in; `open()`
   (`image_document.cpp` line 278) validates it's a single local URL and calls `select()`.
2. **`select()` (GUI thread)** — resets `orientation_`, `information_`, `image_`, `svg_data_`, sets
   `state_ = Loading`, emits `changed()`, and queues a `Request` via `startPending()`.
3. **`startPending()` posts to `worker_` (worker thread, `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`)**
   — checks the `DecodedImageCache` (`cache_.take(request.url)`) exactly as for any format; an SVG URL was never
   put into the cache on a prior visit (see below), so this is always a miss for SVG and falls through to
   `result = decoder_(request.url, *cancel)`, i.e. `decodeImage()`.
4. **`decodeImage()` (worker thread)** — opens the file, sees `info.suffix()` is `svg`, checks the 10 MiB limit
   (**REQ-F-008**), and calls `decodeSvg()`, which reads the bytes, validates them with a **transient** stack
   `QSvgRenderer::load()`, computes `svgIntrinsicSize()` (**REQ-F-005/006**) into `facts.decodedSize`, and returns
   a `DecodeResult{image: null, error: "", information: facts (format="SVG"), svgData: <bytes>}`. This is still all
   on the worker thread — the transient renderer used here is destroyed before returning and never crosses a
   thread boundary.
5. Back in `startPending()`'s lambda: `result.image.isNull()` is `true` (it's always null for SVG), so the
   `DecodedImageCache` `put()`/`displayed_` bookkeeping (lines 377–389) is skipped entirely — SVG never enters
   the LRU cache, by the same guard that already exists, with no new code (**REQ-F-009**).
6. **Result posted back to `this` (GUI thread) via `Qt::QueuedConnection`**, `ImageDocument::complete()` runs:
   `svg_data_ = std::move(result.svgData)`, `information_ = std::move(result.information)`,
   `state_ = Ready` (since `svg_data_` is non-empty even though `image_` is null), and — because
   `information_.format == "SVG"` — `svg_renderer_.load(request.url.toLocalFile())` parses the SVG **for the
   second and last time**, now permanently on the GUI thread, with the SVG directory available for local relative
   references. A failure clears `svg_data_` and follows the ordinary error path. `emit changed()` fires only after
   this load succeeds.
7. **QML property bindings re-evaluate**: `window.document.svgRenderer` now returns `&svg_renderer_` (valid,
   non-null); the main `ImageCanvas`'s `svgRenderer` property binding pushes that pointer into
   `ImageCanvas::setSvgRenderer()`, which sets `svg_renderer_` (canvas-local, non-owning), clears `image_`, computes
   `content_size_` via `svgIntrinsicSize()` (same helper, same answer as step 4's `facts.decodedSize` — both derive
   from the same `viewBoxF()`/`defaultSize()` logic, just at different times on different threads from equivalent
   renderer state), and calls `view_.setImage(...)`, which in turn computes `fitMagnification()` — identically to
   how a raster image's `.size()` would (**REQ-F-007**).
8. **Every subsequent `ImageCanvas::paint()` call** (invalidated by `update()` inside `refresh()`, e.g. on every
   pan/zoom/resize) re-enters `paint()`, takes the `svg_renderer_ != nullptr` branch, and calls
   `svg_renderer_->render(painter, ...)` directly against the already-transformed `QPainter`. **No re-parse
   happens here** — `svg_renderer_.load()` was called exactly once, in step 6, and `render()` just walks the
   already-parsed DOM/paint-tree against whatever transform is currently set on the painter. This is what keeps
   interactive zoom crisp (**REQ-NF-001**): each frame paints vector geometry at the *current* transform, there is
   no intermediate fixed-resolution `QImage` to resample.

### Malformed or oversized SVG

Same steps 1–4, except:
- Oversized: the `file.size() > kSvgFileLimitBytes` check in `decodeImage()` returns an error `DecodeResult` before
  ever touching `QSvgRenderer` (**REQ-F-008**).
- Malformed: `decodeSvg()`'s transient `QSvgRenderer::load(file.fileName())` returns `false`, and `decodeSvg()` returns an
  error `DecodeResult` (`image` null, `svgData` empty, non-empty `error`).

Either way, step 5 is unaffected (nothing to cache either way), and step 6 becomes: `svg_data_` stays/becomes
empty, so `state_ = (image_.isNull() && svg_data_.isEmpty()) ? Error : Ready` evaluates to `Error`; `error_` is set
from `result.error`; `emit openingFailed(file_name_, error_)` fires — **identically to a raster decode failure**.
QML's existing generic error surface (`Main.qml` lines 964–965, 192) displays it with no SVG-specific branch
(**REQ-F-010, REQ-F-011**). See §4 for why this is "the same path" and not just "a similarly-shaped path."

### The `ImageInformationPopup` thumbnail (stays raster — REQ-NF-002)

1. User opens the info popup (`I` shortcut) while an SVG is displayed. `ImageInformationPopup.qml`'s small
   `ImageCanvas` has `image: root.document.previewImage` and **no `svgRenderer` binding at all** — so this
   `ImageCanvas` instance's `svg_renderer_` member is permanently `nullptr`, and its `paint()` can only ever take
   the raster `drawImage()` branch, regardless of what document is open.
2. `root.document.previewImage` (a `Q_PROPERTY` on `ImageDocument`, `NOTIFY changed`) is read by the QML engine.
   Since `image_` is null (SVG document), `previewImage()` falls into the SVG branch: it rasterizes
   `svg_renderer_` — the same **persistent**, already-parsed GUI-thread renderer from step 6 above, not a new
   parse — into a bounded (≤256px on the long edge) `QImage` via a throwaway local `QPainter`, and returns it.
3. Because `previewImage`'s Qt property NOTIFY signal is `changed()`, the QML engine only re-evaluates and re-calls
   this getter when the document changes (open/navigate/refresh) — not on every repaint of either `ImageCanvas`
   instance — so the on-demand rasterization happens once per document open, not once per frame.
4. The popup's `ImageCanvas` receives this `QImage` through its ordinary `setImage()` path and paints it exactly
   like any other raster image, fit into its 96×96 box by the same `ViewGeometry`/`fitRect` machinery every other
   thumbnail uses.

This path never touches `decoded_image_cache` (the rasterized preview is not a `CachedImage` and is never passed
to `DecodedImageCache::put()`) and never calls `QSvgRenderer::render()` from inside `ImageCanvas::paint()` — the
one call to `render()` for the preview happens inside `ImageDocument::previewImage()`, a different call site,
satisfying REQ-NF-002's acceptance criterion that "the popup preview code does not call the new vector-mode paint
path."

---

## 3. Interfaces / APIs

### New constant

```cpp
// image_limits.h
inline constexpr qint64 kSvgFileLimitBytes = 10 * 1024 * 1024;  // REQ-C-001
```

### `DecodeResult` (image_document.h) — one field added

```cpp
struct DecodeResult {
  QImage image;
  QString error;
  ImageInformation information;
  QByteArray svgData;  // Non-empty only for a successfully validated SVG.
};
```

### New free functions (image_document.h)

```cpp
QSize svgIntrinsicSize(const QSvgRenderer& renderer);
```
(`decodeSvg()` stays file-local to `image_document.cpp`, in the existing anonymous namespace alongside
`readImage()`/`simpleWebPFallback()` — it has no caller outside that file, unlike `svgIntrinsicSize()`.)

### `ImageDocument` — two new properties, no new enum value

```cpp
Q_PROPERTY(QSvgRenderer* svgRenderer READ svgRenderer NOTIFY changed)
Q_PROPERTY(QImage previewImage READ previewImage NOTIFY changed)
...
[[nodiscard]] QSvgRenderer* svgRenderer();       // non-const: QSvgRenderer::isValid() etc. are non-const
[[nodiscard]] QImage previewImage();             // non-const: calls the non-const QSvgRenderer::render()
```

**"Raster vs. SVG" mode is represented by `information_.format == "SVG"` plus which of `image_` /
`svg_data_`+`svg_renderer_` is non-empty/valid — not a new enum.** This is a direct extension of the pattern
already in the codebase: GIF-vs-static is *already* represented exactly this way — `complete()` checks
`information_.format == QLatin1String("GIF")` (line 419) to decide whether to call `animation_.start()`, with no
`enum class ContentKind { Static, Gif }` anywhere. Adding `enum State` values or a `std::variant<QImage,
SvgPayload>` would be a second, parallel way of asking the same question this codebase already asks via
`information_.format`, for no benefit — `format` is already populated, already a `QString` compared with
`QLatin1String` elsewhere, and already the thing `informationSections()`/`summaryLine()` key off of for display.
The **existing `State` enum** (`Empty, Loading, Ready, Error`) is deliberately left untouched — SVG is not a new
top-level document state, it's a new *kind* of `Ready` content, exactly as GIF already is.

`ImageDocument::open()`/`select()` reset `svg_data_.clear()` alongside the existing `image_ = {}` /
`information_ = {}` resets (image_document.cpp lines 295, 319).

### `ImageCanvas` — one new property, one new method, no removed API

```cpp
Q_PROPERTY(QSvgRenderer* svgRenderer READ svgRenderer WRITE setSvgRenderer NOTIFY imageChanged)
...
[[nodiscard]] QSvgRenderer* svgRenderer() const { return svg_renderer_; }
void setSvgRenderer(QSvgRenderer* renderer);
```

`image`/`setImage()`/`replaceFrame()` keep their current signatures unchanged; existing QML/tests that only ever
set `image` continue to work unmodified (raster mode is still the default — `svg_renderer_` starts `nullptr`).

### QML

- `Main.qml`: `ImageCanvas { ... svgRenderer: window.document.svgRenderer ... }` — new line, existing bindings
  unchanged.
- `ImageInformationPopup.qml`: `image: root.document.previewImage` — one binding's right-hand side changed from
  `root.document.image` to `root.document.previewImage`; `svgRenderer` is not bound (that omission *is* the API
  contract for REQ-NF-002).

### CMake

`Svg` added to the root `find_package(Qt6 ...)` component list; `Qt6::Svg` added to the executable target's
`target_link_libraries()` in `apps/viewer/CMakeLists.txt`. No new `pkg_check_modules`/third-party dependency.

---

## 4. Key decisions with rationale

**Dual-mode `paint()` is a single `if`, not a strategy/virtual dispatch.** `ImageCanvas` is a
`QQuickPaintedItem` with exactly one polymorphic seam Qt already gives it (`paint(QPainter*)` itself); introducing
an internal `PaintStrategy` interface with `RasterPaintStrategy`/`VectorPaintStrategy` subclasses for a
two-case, unlikely-to-grow-past-two-cases switch would add a vtable, an allocation or ownership question for the
strategy object, and an indirection at the one place (`paint()`) that most needs to stay easy to read against
`QPainter`'s already-imperative API. REQ-NF-003 explicitly asks for "conditional branches... clearly documented
with comments," not a polymorphic redesign — the two branches share every line of setup (clip rect, render hint,
orientation transform, translate/scale) and differ only in the final draw call, which is exactly the shape a plain
`if` communicates best.

**Format dispatch to the SVG decode branch is by file extension, not content-sniffing.** Every other format in
`decodeImage()` is identified by content: `QImageReader::canRead()`/`format()` sniff magic bytes, and
`simpleWebPFallback()` double-checks a `RIFF`/`WEBP`/`VP8` signature before trusting a WebP guess. SVG doesn't have
an equivalent reliable signature to sniff cheaply — a valid SVG can start with a UTF-8 BOM, an XML declaration, a
DOCTYPE, comments, or processing instructions before the first `<svg` token appears, and distinguishing "SVG" from
"arbitrary XML" by content alone is itself an unbounded-lookahead problem for a hostile or malformed file. The
`.svg` extension is also the one piece of information REQ-F-001/002/003 already anchor format registration to
(name filters, directory suffix scan), so using the same signal for decode dispatch keeps "is this file SVG?"
answered consistently by one rule across the whole feature, instead of "extension for browsing, content for
decoding" disagreeing on some edge-case file.

**Two `QSvgRenderer` instances exist for one open SVG document — a transient one (worker thread, validation-only,
`decodeSvg()`) and a persistent one (GUI thread, `ImageDocument::svg_renderer_`, parsed once in `complete()`).**
This is not redundant parsing-for-no-reason: `QSvgRenderer` is a `QObject`, and Qt's threading rules require a
`QObject`'s lifetime and mutation to stay on the thread it was created on (or be explicitly `moveToThread()`'d,
which buys nothing here since nothing about `QSvgRenderer` benefits from living on the worker thread). The decode
worker thread needs *some* way to validate the file and learn its intrinsic size before deciding success/failure —
constructing a short-lived, stack-local `QSvgRenderer` entirely within `decodeSvg()`'s call frame, on the worker
thread, and destroying it before returning, is safe because it never leaves that thread. The bytes it validated are
then retained as plain `QByteArray` data (thread-safe, value-typed) alongside the GUI-thread renderer. The
GUI-thread `ImageDocument` builds its *own*, permanent `QSvgRenderer` from the same local path in `complete()`, so
relative local resources retain their base directory. The alternative — skip worker-thread
validation and only construct the renderer in `complete()` — was considered and rejected: it would mean the
worker's `DecodeResult` can't distinguish "valid SVG under 10 MiB" from "malformed SVG" until the GUI thread gets
around to trying `load()`, which either pushes all failure handling onto the GUI thread (harder to keep symmetric
with every other format's worker-thread failure handling) or requires speculatively marking every SVG "successful"
and un-doing that in `complete()` — messier than validating once, cheaply, where every other format's validation
already happens.

**Malformed/oversized SVG reuses the existing `DecodeResult{image: null, error: <text>}` failure shape verbatim —
no new error path, no new signal.** `decoderError()`/`limitError()` (image_document.cpp lines 64–75) already show
this codebase's convention: every decode failure, for every existing format including the "tiled TIFF rejection"
the spec calls out, is just a `DecodeResult` with a null `image` and a human-readable `error` string; there is no
per-format error enum or per-format UI branch anywhere in `complete()` or `Main.qml`. (Searching the repository for
special-cased "tiled"/"Tiled" handling turns up nothing — a tiled TIFF fails today purely because
`QImageReader::canRead()`/`.size()` can't handle it, falling through to the generic `decoderError()` message. This
confirms there is no separate mechanism to imitate; the generic mechanism *is* the mechanism.) SVG's failure path
was designed to produce the same shape for the same reason: a new `SvgError` signal or a dedicated "invalid vector
document" state would be an unjustified special case the requirement doesn't ask for (REQ-F-010/011 explicitly ask
for "the same error path").

**The thumbnail popup gets its raster preview via a new `ImageDocument::previewImage()` getter, computed on demand
from the persistent `svg_renderer_`, not via `decoded_image_cache` and not via `ImageCanvas`'s vector path.**
Three alternatives were considered and rejected:
- *Route it through `decoded_image_cache`*: rejected because that cache is keyed and budgeted for `QImage`s that
  come from `decodeImage()`'s raster path (REQ-F-009 explicitly wants SVG to bypass it) — teaching the cache about
  a second, synthetic "rasterized-for-thumbnail" `QImage` per SVG would blur the one invariant the cache currently
  has ("every entry is a decode result") for a value that's cheap enough to not need caching at all (QML's own
  property-binding cache, driven by `changed()`, already ensures it's computed once per document open — see §2).
- *Give the popup's `ImageCanvas` a `svgRenderer` binding too and let it exercise the same vector `paint()`
  branch*: rejected explicitly by REQ-NF-002 and its rationale ("vector fidelity is not necessary at thumbnail
  scale") — it would also mean every dual-mode `paint()` complexity in `ImageCanvas` (§4 above) has to be
  correct at a second, differently-clipped, differently-scaled call site for no visible benefit at 96×96.
- *Add the rasterization logic to `ImageCanvas` itself (e.g., an internal "if in vector mode but a flag says
  thumbnail, rasterize internally")*: rejected because it would reintroduce exactly the branching REQ-NF-002 is
  trying to avoid inside the one class REQ-NF-003 already accepts is getting more complex; keeping rasterization
  in `ImageDocument` means `ImageCanvas` never needs to know a thumbnail context exists at all — it only ever sees
  a `QImage` (the same type it has always accepted).

**`previewImage()` and `svgRenderer()` are both declared non-`const`, not `const` with a `mutable` member.**
`QSvgRenderer::render()` and `QSvgRenderer::isValid()` are non-`const` methods, and `svg_renderer_` is a plain
(non-`mutable`) `QSvgRenderer` value member, so calling either through `this` requires the calling member function
itself to be non-`const`. Rather than marking the member `mutable` to preserve `const`-qualified getters — which
would silently widen what "const" means for `ImageDocument` and invite the same question again for any future
`QSvgRenderer` method a getter needs — both getters are simply declared non-`const`, matching how Qt's own
`Q_PROPERTY READ` accessors are permitted to be non-`const` and consistent with each other.

---

## 5. Alternatives considered

**Re-rasterize on zoom-settle** (render once to a `QImage` sized for the final zoom level after the user stops
interacting, instead of vector-painting every frame): rejected. This was discussed and dropped in the requirements
grill session — it would mean every intermediate frame *during* an active drag is either blurry (upscaled from a
lower-res raster) or itself re-rasterized at interactive-drag frequency (defeating the point, and adding jank the
current raster path doesn't have since raster images are decoded once regardless of zoom). It also reintroduces a
"when do we invalidate/regenerate the cached raster" state machine for no benefit over just painting vector
geometry directly, which Qt's `QSvgRenderer` already does efficiently against an arbitrary `QPainter` transform.

**Route SVG through `QImageReader`'s own SVG plugin** (Qt ships an `imageformats` SVG plugin backed by the same
`Qt6::Svg` module, which would let `QImageReader::read()` "just work" for `.svg` the way it already does for every
other format in `readImage()`): rejected for two reasons. First, plugin *presence* is not guaranteed — REQ-F-001's
whole point is that SVG availability must not depend on "adding or removing an unrelated image-codec plugin," and
the SVG `QImageReader` plugin is exactly such a plugin (distributed separately from `QSvgRenderer` itself in some
Qt packagings). Second, and more fundamentally, `QImageReader::read()` returns a `QImage` rasterized at whatever
size was requested once — it cannot give back a live vector document to re-render at a different zoom, so this
route cannot deliver REQ-F-004/REQ-NF-001's crisp-zoom requirement at all, independent of the plugin-availability
concern.

**Render SVG only via QML's own `Image`/`Shape` items, alongside or instead of the custom `ImageCanvas`**:
rejected. `ImageCanvas` is the single owner of the app's pan/zoom/orientation math (`ViewGeometry`,
`ImageOrientation::mapping()`) and the single paint surface the rest of the app (drag handlers, wheel/pinch zoom,
keyboard shortcuts, `firstRendered`/`viewChanged` signals) is wired to. Introducing a second, QML-item-based render
path for SVG specifically would mean either (a) duplicating `ViewGeometry`'s fit/zoom/pan/constrain logic in QML
bindings for the SVG case only, so two independent implementations of the same math have to stay behaviorally
identical, or (b) somehow feeding `ImageCanvas`'s existing zoom/pan state into a sibling QML item's transform each
frame, which is strictly more moving parts than the single `if` in `paint()` this design uses instead. It would
also mean `ImageInformationPopup.qml`'s already-shared `ImageCanvas` component could no longer trivially stay
raster-only by simply not binding `svgRenderer` (§4) — REQ-NF-002's mechanism relies on both canvas instances
being the *same* dual-mode class.

---

## 6. Known risks

- **`ImageCanvas` is measurably more branchy.** `paint()` gains a real `if`/`else` in its hottest, most
  correctness-sensitive method, and `setImage()`/`setSvgRenderer()` are now two entry points that must stay
  mutually exclusive (`content_size_`, `image_`, `svg_renderer_` all need to agree on which mode is active).
  REQ-NF-003 accepts this explicitly as a deliberate tradeoff; the mitigation is that both setters are short,
  symmetric, and funnel through the same `view_.setImage(ImageOrientation::dimensions(orientation_,
  content_size_))` call, so there is exactly one place mode-specific state could get out of sync with `view_`, not
  several.

- **`QSvgRenderer::load()`'s parse cost happens twice per SVG open** — once (validation-only) on the worker thread
  in `decodeSvg()`, once (permanent) on the GUI thread in `complete()`. For a file at the 10 MiB ceiling with
  pathologically nested markup, this is up to 2x the worst-case parse cost of a single parse. This was an accepted
  tradeoff in §4 (thread-affinity correctness for the `QObject`-based renderer) rather than a bug; if profiling
  later shows this matters, an alternative would be having `decodeSvg()` return only *raw bytes plus a
  cheaper/parse-free validity signal* (e.g., a lightweight well-formedness check) and deferring all real
  `QSvgRenderer::load()` cost to the GUI thread alone — out of scope for this design pass since REQ-F-008 already
  bounds the worst case to 10 MiB.

- **Prefetch parses and then discards SVG bytes.** `ImageDocument::maybePrefetch()` prefetches the next/previous
  sibling regardless of format, and for an SVG neighbor this still runs the *full* `decodeSvg()` validation (file
  read + transient `QSvgRenderer::load()`) on the worker thread — but because `startPending()`'s prefetch branch
  discards the whole `result` (including `svgData`) once the raster-only cache-population guards see a null
  `image` (`image_document.cpp` lines 377–392), that parse work is thrown away and redone in full when the user
  actually navigates there. Raster formats don't pay this cost twice because their decoded `QImage` *is* what gets
  cached and reused. This is a direct, accepted consequence of REQ-F-009 ("bypassing the decoded_image_cache") and
  is not addressed by this design — fixing it would mean giving SVG its own single-entry "last prefetched
  bytes/renderer" slot parallel to `decoded_image_cache`, which is more machinery than the spec asks for.

- **EXIF orientation does not apply to SVG, and this is already true with zero special-casing.** SVG files carry
  no EXIF block (`decodeImage()`'s SVG branch never calls `ExifMetadata::read()`), so `information_.exif` stays
  default-constructed and `orientation_` starts at `0` for every SVG open (same as any format `ExifMetadata::read()`
  can't parse). `ImageOrientation::dimensions()`/`mapping()` (`image_orientation.cpp`) are pure `QSize`/`int`
  functions with no notion of *how* that size was derived, so they apply identically and correctly to SVG's
  `content_size_` — no divergence to design around. One residual edge case, out of this spec's scope: the app's
  manual rotate command (`ImageDocument::transform()`, wired to a keyboard shortcut) is gated only on `state_ ==
  Ready`, not on format, so a user *can* manually rotate an open SVG — this already works correctly (the same
  `QSize`-only math applies), it's just not something REQ-F-005..007 asked for or excludes; noted here rather than
  designed against, per this task's instruction not to redesign parts of the app the spec doesn't touch.

- **`ImageDocument::copyImage()` silently copies nothing for an open SVG.** It calls
  `clipboard_.copyImage(image_, orientation_, file_name_)`, and `image_` is always a null `QImage` for SVG
  documents by this design (§3). The spec has no REQ covering clipboard behavior for SVG, so this design does not
  change `copyImage()`/`ClipboardController`; flagged here as a gap an implementer or a follow-up spec should
  decide on (e.g., clipboard could reuse `previewImage()`'s rasterization, or copy the raw SVG bytes/text), not
  something this design resolves.

- **`tests/image_document_test.cpp` and `tests/view_geometry_test.cpp` need `Qt6::Svg` linked** once they exercise
  `decodeSvg()`/`svgIntrinsicSize()`/`ImageDocument::svgRenderer()`/`previewImage()`; this design flags the need
  (§1, CMake) but does not audit `tests/CMakeLists.txt` in full — left for the implementation/task-breakdown phase.
