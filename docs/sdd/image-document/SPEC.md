# Stage 1: open an image

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native desktop acceptance and
stage 0 acceptance remain open.

## Scope

Open one local static image in the existing themed shell. Keep document/loading
state in C++ and presentation and input feedback in QML. Use installed HoloNight
packages and preserve native decorations and existing fullscreen/quit behavior.

| ID | EARS requirement |
| --- | --- |
| IMG-01 | When launched without a file, the system shall retain the empty canvas and provide a discoverable Open action. |
| IMG-02 | When given exactly one local file through the command line, Open dialog, or drop, the system shall submit it through one document-loading path. |
| IMG-03 | If an opening request contains a remote URL, a directory, or multiple files, the system shall reject the request with a useful explanation. |
| IMG-04 | While a file is decoding, the system shall keep the UI responsive and display a loading state identifying the requested file. |
| IMG-05 | When decoding succeeds, the system shall honor embedded orientation, preserve aspect ratio and transparency, and fit the image within the available canvas. |
| IMG-06 | When the canvas changes size or fullscreen state, the system shall recompute the fit without clipping or distorting the image. |
| IMG-07 | If a file is missing, unreadable, corrupt, unsupported, or exceeds the decoding resource limit, the system shall show a useful error and permit another opening request. |
| IMG-08 | When a new request supersedes an earlier request, the system shall prevent the earlier result or error from replacing the current request's state. |
| IMG-09 | The system shall bound concurrently executing and queued decoding work and decoded-image memory, with explicit limits documented in the design. |
| IMG-10 | The system shall support static PNG and JPEG with installed Qt handlers and derive additional selectable formats from available handlers; for a multi-frame file it shall display only the first frame during this cycle. |
| IMG-11 | The system shall provide translated user-facing strings and keyboard access to Open, preserve help/version options, and leave original image files unchanged. |
| IMG-12 | The system shall open local paths containing spaces and Unicode through all three opening entry points. |

PNG/JPEG establish the approved common-format baseline; packaged MIME declarations
remain a stage 5 responsibility. For invalid CLI syntax, exit nonzero; a valid
single-file request that fails decoding opens the window with its recoverable
error state. A canceled dialog leaves the current document unchanged. On a new
accepted request, replace the old presentation with loading feedback so an older
image cannot be mistaken for the requested file.

## Verification gate

Cover all three entry points and loading/error recovery. Include fixtures for
EXIF orientation, alpha transparency, large dimensions/resource rejection, corrupt
and unsupported data, Unicode/spaces, unreadable and missing files. Exercise rapid
replacement with controlled asynchronous completion order and verify that stale
success and failure cannot alter the current state. Verify bounded work under a
burst of requests and responsive shutdown during decoding.

Run contributor build/test/quality/install gates. Inspect dark/light appearance,
keyboard opening, minimum-size/resize/fullscreen behavior and fractional scaling.
Record fixtures, commands, outcomes and remaining manual checks in VERIFICATION.md.
Generated fixtures and captures belong under build/.

Non-goals: zoom/pan controls, folder discovery/navigation, cache/prefetch,
transforms, metadata panels, clipboard, animation playback, command mode, overlays,
trash, editing, desktop MIME advertising and changes to shared providers.

Implementation follows the approved [design](DESIGN.md) and [tasks](TASKS.md).
See [verification](VERIFICATION.md) for evidence and pending manual acceptance.
