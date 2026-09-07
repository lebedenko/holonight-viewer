# Stage 1 design: open an image

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native desktop acceptance and
stage 0 acceptance remain open.

## Document and loading

Introduce an ImageDocument QObject owning Empty, Loading, Ready and Error states,
the requested local URL/display name, translated error text, oriented pixel
dimensions, current QImage and a monotonically increasing request ID. Expose it
to the root QML component as a typed required property using initial properties.
Keep the instance and worker lifetime explicit in main; avoid a global singleton.

One C++ entry point accepts a list of URLs. It requires exactly one local file URL
with no remote host, then schedules file inspection and decoding. Resolve CLI
relative paths against the invocation directory; preserve spaces and Unicode.
Do not interpret an ordinary path containing spaces as separate arguments. CLI
URL syntax is parsed strictly; remote schemes are rejected. Support `--` for
filenames beginning with a dash. Dialog and drop pass their QUrls unchanged.

Invalid CLI syntax, remote URLs and multiple arguments exit nonzero with translated
diagnostics. A single local path starts the window: missing, unreadable, directory
and decode failures become recoverable document errors. Invalid interactive
requests invalidate older work and enter Error with an explanation. Dialog cancel
submits nothing and preserves the current state.

Each submitted request immediately increments the ID, clears the displayed image
and previous error, and enters Loading or validation Error. A dedicated worker
thread executes at most one decode. While it is busy, the document holds only the
newest pending URL/ID, replacing earlier pending requests. Do not enqueue a worker
event for every opening request. On completion, accept the result only when its
ID equals the current ID; discard stale errors and successes before presentation.
Then start the newest pending request, if any. Use queued connections to transfer
immutable result values; worker code never touches QML, QPixmap or UI objects.

The worker opens a read-only QFile and rejects directories and non-regular files
(including FIFOs/devices); ordinary symlinks to regular files are supported.
File access and dimension probing also happen in the worker. Handle files changing
or disappearing between validation and read through the actual read result.

## Decoding and resource limits

Use QImageReader with autoTransform enabled and one read of the first frame.
Require PNG and JPEG handlers in build/installation verification. Derive other
dialog suffixes from supportedImageFormats; include an All files filter so content
detection can handle extensionless or misleadingly named files. Qt documents
orientation transforms, handler discovery and allocation limits in
[QImageReader](https://doc.qt.io/qt-6/qimagereader.html).

Set the process-wide reader allocation limit to 128 MiB before starting work and
ensure an environment override cannot silently raise that policy. Reject encoded
files over 256 MiB, unknown/nonpositive header dimensions, dimensions over 32,768
on either axis, and images over 32 million pixels using overflow-safe arithmetic.
Check decoded sizeInBytes against 128 MiB before accepting a result. Normalize
accepted images to 32-bit premultiplied pixels and devicePixelRatio 1, preserving
alpha and oriented pixel dimensions. Reject high-depth source images when their
decoded storage would exceed the reader limit. Large unsupported images receive
a specific resource-limit explanation; this cycle does not downsample to bypass
limits or silently present a partial-resolution document.

Retain one document image, one active decode and at most one pending path; no
decoded-image cache. Clear the old document and canvas reference when loading
begins. Decode/orientation/format conversion may temporarily hold several bounded
buffers, so 128 MiB is a per-image bound, not a whole-process RSS promise. Account
for these copies and canvas textures in measured peak memory. Codec-private
allocations and graphics-driver memory are outside QImageReader's allocation
guarantee; record this limitation and measured large-image results explicitly.

Worker shutdown clears pending work and rejects completions. Request cooperative
cancellation before/after file inspection and decode. Keep the event loop running
while the active read finishes, then join the stopped thread and destroy the
document. Never terminate a QThread or block the GUI with wait() during decoding.
QImageReader exposes no mid-read cancellation API: closing stays responsive but
process exit may wait for the active codec/file operation. This design does not
promise a hard shutdown deadline for a hung codec or filesystem. Verify normal
large-image shutdown and disclose this limit in acceptance evidence.

## Presentation and input

Keep the existing native HnApplicationWindow and shared theme/style setup. QML
owns the Open button, Ctrl+O shortcut, QtQuick.Dialogs FileDialog, DropArea,
loading/error/empty feedback, accessible names and layout. FileDialog uses
OpenFile mode. Keep Open available in all document states; guard global f/q/Escape
shortcuts while a dialog is active so typing and dialog cancellation are safe.
Display a wrapped/elided filename and useful translated error category; do not
rely on untranslated codec text as the sole explanation.

Introduce a small C++ QQuickPaintedItem canvas receiving the document's immutable
QImage through a GUI-thread setter. Drawing uses a centered aspect-preserving
rectangle and smooth sampling. This keeps decoding out of QML and avoids an image
provider retaining obsolete images. QML reserves space for the Open action and
bottom hints and supplies canvas geometry; resizing only recomputes the draw
rectangle. Fit uses min(canvasWidth/imageWidth, canvasHeight/imageHeight), including
upscaling, and never distorts the image. Alpha composites over the themed canvas.
Qt describes the backing texture and painting behavior in
[QQuickPaintedItem](https://doc.qt.io/qt-6/qquickpainteditem.html).

The backing surface scales with canvas size rather than source image size; verify
fractional device scaling visually. Painting must consume a stable image snapshot
under Qt Quick's synchronization rules. This initial renderer is deliberately
small; stage 2 can evolve it for zoom/pan after profiling. Do not introduce those
controls in this cycle. Preserve fullscreen restoration and existing quit behavior.

## Integration and verification

Share document/decoder/canvas sources between production and tests through a small
internal CMake target. Register typed document/canvas QML types in HolonightViewer
and make them available to the smoke test's real Main.qml. Update format/tidy
source lists, QML lint imports, CLI tests and staged runtime checks together.
Keep installed HoloNight packages and existing runtime lookup; no provider edits,
extra public library, network opening or desktop MIME declarations.

Use a controllable decoder seam for deterministic scheduling tests: hold request A,
submit B and C, release A, and assert only C runs next and only C can be presented.
Exercise stale failure, new validation error, and shutdown as well as stale success.
Use real readers for fixture checks. Generate PNG/JPEG/EXIF fixtures under build/;
test EXIF rotations and reflections with an asymmetric image and tolerant JPEG
pixel comparisons. Cover alpha, first frame, resource rejection, Unicode, spaces,
extensionless files and meaningful decode failures. Confirm fixture hashes are
unchanged after opening. Do not skip required PNG/JPEG coverage when plugins are
missing; optional-format coverage must report handler availability.

Test CLI, dialog acceptance/cancel and single/multiple/remote drops through their
actual adapters, including recoverable errors followed by a successful image.
Use production window captures and keyboard tests for fit, resize, fullscreen,
minimum size and dark/light 100%/125%/150% rendering. Native dialog/portal and live
compositor checks remain explicit manual acceptance where automation cannot reach
them. Record exact checks and limitations in VERIFICATION.md, and update README
and backlog without claiming pending stage 0 checks have passed.

## Implementation notes

The internal static `viewer-ui` QML module contains the document, decoder, canvas
and Main.qml; both the executable and smoke tests link that same module/plugin.
Dynamic installed provider imports remain unchanged. Dialog lifetime is managed by
a Loader, and the fallback dialog test selects a real file delegate via Qt mouse
events. Drop tests send Qt drag-enter/drop events to the production window.

The application logs translated document errors to stderr as well as showing them
in the window; a syntactically valid failed file request keeps the process/window
open for recovery. Staged CLI checks cover both required image handlers with
Unicode/space filenames, missing/corrupt files and a dash-prefixed path.

`shutdownFinished` is emitted after the worker thread finishes; normal window
closing waits asynchronously before quitting the application. The destructor joins
as a lifetime backstop. A QML startup failure returns before scheduling image work.
