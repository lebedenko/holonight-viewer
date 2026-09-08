# Stage 2: inspect comfortably

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native mixed-scale display
acceptance and earlier stages’ pending acceptance remain open. See
[verification](VERIFICATION.md).

## Scope

Extend the existing single-image canvas with fit, actual size, pointer-centered
zoom, mouse drag and keyboard pan. Preserve the approved image-opening behavior,
native decorations and installed HoloNight integration. Stage 0 and stage 1's
remaining desktop acceptance checks stay open independently.

| ID | EARS requirement |
| --- | --- |
| VIEW-01 | When a newly opened image becomes ready, the system shall display it centered in Fit mode, preserving orientation, aspect ratio and transparency. |
| VIEW-02 | When the user selects Fit, the system shall center the entire image within the available canvas, including upscaling smaller images, and reset previous pan offsets. |
| VIEW-03 | While Fit mode is active, when the canvas size, fullscreen state or display scale changes, the system shall recompute the fit without clipping or distortion. |
| VIEW-04 | When the user selects Actual Size, the system shall display one source-image pixel per physical display pixel and center the image. |
| VIEW-05 | When the user zooms with the wheel over a ready image, the system shall change magnification while keeping the image point beneath the pointer stationary, except where centering or edge constraints require adjustment. |
| VIEW-06 | When the user invokes keyboard zoom, the system shall change magnification around the canvas center, subject to the same centering and edge constraints as pointer zoom. |
| VIEW-07 | While an image exceeds the canvas on an axis, when the user drags with the primary mouse button or invokes keyboard pan, the system shall move the image on that axis and allow every image edge to be reached. |
| VIEW-08 | While an image fits within the canvas on an axis, the system shall center it on that axis; while it exceeds the canvas, the system shall constrain panning so neither image edge can move inside the corresponding canvas edge. |
| VIEW-09 | While manual magnification is active, when the canvas size or fullscreen state changes, the system shall preserve magnification and the image point at the canvas center as far as edge constraints permit. |
| VIEW-10 | While Actual Size or manual magnification is active, when display scale changes, the system shall preserve the physical-pixel magnification and the image point at the canvas center as far as edge constraints permit. |
| VIEW-11 | When zoom reaches its documented finite bounds, the system shall stop at the bound without invalid geometry; Fit and Actual Size shall remain available for every accepted image. |
| VIEW-12 | While no image is ready or the Open dialog is active, the system shall suppress image manipulation; canceling the dialog shall preserve the current view. |
| VIEW-13 | The system shall provide discoverable keyboard access to Fit, Actual Size, zoom and pan, translated labels and accessible names for controls, and visible feedback identifying Fit mode or the current magnification. |
| VIEW-14 | While zooming or panning, the system shall keep input responsive, reuse the decoded image without rereading the source, and keep rendering storage bounded independently of the magnified image dimensions. |
| VIEW-15 | The system shall preserve existing Open, drop, CLI, fullscreen restoration and quit behavior and leave source files unchanged. |

Actual Size means physical pixels, including at 125% and 150% display scaling.
Displayed manual magnification uses the same convention: 100% means Actual Size.
Wheel and keyboard zoom leave Fit mode; returning to Fit explicitly restores
automatic fitting. Dragging moves the image with the pointer. Keyboard pan moves
the viewed region in the requested direction. Pan in Fit has no visible effect.

The design step will specify shortcut assignments, wheel/trackpad delta handling,
zoom increments and bounds, keyboard pan distance, control placement and rendering
strategy. Those choices must satisfy the contracts above without adding a new
workflow or changing the stage 1 decoding limits.

## Verification gate

Verify coordinate mapping, pointer and center anchoring, edge clamps, repeated
zoom round trips, Fit reset, Actual Size and mode transitions with focused tests.
Cover portrait/landscape, tiny and oversized accepted images, oriented images and
transparency. Test real window input for wheel zoom, dragging, keyboard controls,
dialog suppression/cancel, replacement and error recovery.

Inspect dark/light appearance and readable controls at minimum and normal window
sizes, 100%/125%/150% scaling, resize and fullscreen transitions. Verify physical
pixel mapping and movement between displays with different scales; retain any
unavailable desktop checks explicitly. Measure responsiveness and memory with a
large accepted image at maximum zoom, checking that zoom does not allocate a
full magnified-image buffer.

After implementation, run the CONTRIBUTING.md dependency, build, test, quality
and installation gates plus relevant desktop and visual checks. Record commands,
results, measurements and limitations in VERIFICATION.md. Generated fixtures,
captures and logs belong under build/. Stage completion requires acceptance
evidence; a requirements approval alone does not close any stage.

Non-goals: folder navigation, cache/prefetch, rotate/flip, metadata, clipboard,
animation, touch/pinch gestures, inertial pan, mouse-revealed overlays, command
mode, editing, new formats, desktop MIME advertising and provider changes.
