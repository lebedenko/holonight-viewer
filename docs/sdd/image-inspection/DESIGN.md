# Stage 2 design: inspect comfortably

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native mixed-scale display
acceptance and earlier stages’ pending acceptance remain open. See
[verification](VERIFICATION.md).

## View state and geometry

Keep ImageDocument and its decoding/resource contract unchanged. Extend
ImageCanvas with a small, independently testable C++ view-geometry value type.
It owns oriented image dimensions, canvas size in logical pixels, display pixel
ratio, Fit/manual mode, physical-pixel magnification and the image point at the
canvas center. ImageCanvas exposes read-only mode/magnification feedback and
invokable Fit, Actual Size, zoom-at-point and pan operations to QML. Mutations
validate inputs, constrain geometry, notify changed properties and schedule paint.
No document state, geometry formulas or duplicate mutable view state live in QML.

Let image dimensions be W,H, canvas dimensions Cw,Ch, display pixel ratio d,
physical magnification m, and logical scale s=m/d. For a center image point c,
the destination origin is canvasCenter - s*c; destination size is (W*s,H*s).
Fit uses s=min(Cw/W,Ch/H), m=s*d and c=(W/2,H/2). Actual Size selects manual
mode with m=1 and centers the image. Ordinary zoom selects manual mode, including
when the resulting scale equals Fit or 100%; only Fit restores automatic fitting.

To zoom at canvas point a, first compute its image coordinate p=(a-origin)/s.
After changing scale to s', use origin'=a-s'*p, then constrain it. On each axis,
center an image smaller than the canvas; otherwise clamp origin to
[canvasExtent-imageExtent, 0]. Derive the stored center from this constrained
origin. Zoom in letterbox space uses the same mapping and final constraints.

Manual zoom bounds are min(0.01, fitMagnification) through
max(32, fitMagnification): normally 1%–3200%, extended to include Fit for every
accepted image. Actual Size is always included. Compute bounds for the current
positive canvas geometry. If a resize/display change leaves existing manual
magnification outside new bounds, preserve it; subsequent zoom may move toward
the range without snapping or moving in the opposite direction, but may not move
farther outside it. Fit itself always uses its computed scale. Reject non-finite
or nonpositive scaling inputs and ignore operations while image/canvas geometry
is empty. Zero-size layout intervals retain valid center/magnification until
positive geometry returns. Clamp wheel exponents before exponentiation.

On resize/fullscreen change, Fit recomputes; manual mode retains m and the stored
center before applying constraints. Bind the canvas display-ratio input to the
owning Qt Quick window's effective device pixel ratio, updating geometry on ratio
changes. The installed Qt 6.11 headers expose this window property with a change
notification. Tests also exercise explicit ratio changes in the geometry type.
Manual mode keeps m across display moves, so Actual Size remains one physical
pixel per source pixel. Fractional placement may require sampling; the 100%
contract specifies pixel spacing, not forced snapping that would break anchoring.

Any image replacement/clear resets the view to Fit and cancels active input.
Stage 1 clears the image on loading/error, including reopening the same file,
so no previous image's view survives an accepted replacement. Dialog cancellation
does not replace the image or reset its view.

## Input and controls

Retain the existing Open/filename row. Add a compact row of styled Fit, Actual
Size, minus and plus buttons with a magnification label below it. Use existing
Holonight styling, shared spacing/typography tokens and translated labels.
Keep the controls usable at 420×280; allow translated labels to wrap into an
additional row if necessary, rather than raising the minimum window size.
Display “Fit” in Fit mode and a locale-formatted percentage in manual mode, with
enough precision to avoid showing 0% for valid small magnifications. Empty,
Loading and Error show no stale percentage; manipulation controls are disabled.

| Input | Behavior |
| --- | --- |
| 0 / Fit button | Center and enter Fit mode |
| 1 / Actual Size button | Center at 100% physical magnification |
| + or = / plus button | Multiply magnification by 1.25 about canvas center |
| - / minus button | Divide magnification by 1.25 about canvas center |
| Arrow keys | Move the viewed region 40 logical pixels in that direction per activation, including key repeat |
| Primary-button drag | Move the image with the pointer, constrained on each axis |
| Vertical wheel/trackpad scroll over canvas | Zoom about pointer position |

Use delivered vertical pixelDelta when nonzero (40 units per zoom step), otherwise
vertical angleDelta (120 units per step). Apply factor 1.25^steps, supporting
fractional deltas. Use the delivered sign so system scrolling direction is
respected; do not invert it a second time. Ignore horizontal-only and zero deltas.
Cap each event to ±8 steps before applying overall bounds. Wheel events outside
the canvas do not manipulate the image. Touch/pinch and kinetic scrolling are
outside scope.

QML pointer handlers use canvas-local coordinates and target no automatic item
transform; pass deltas/anchors to C++. Drag uses incremental translation, with a
fresh baseline for each gesture. Cancel/reset the gesture on dialog opening,
image replacement, loss of the grab, or viewport geometry change, preventing a
stale delta on the next event. Show an open-hand cursor when panning is possible
and a closed hand during drag. Do not move or scale the canvas item itself.

Use one shared readiness/dialog guard for buttons, shortcuts and pointer handlers.
Keep Ctrl+O, f, Escape and q unchanged. Fit/Actual Size/zoom are window shortcuts;
arrow keys act when the canvas has focus, preserving standard keyboard behavior
for focused controls. The canvas is reachable by Tab, receives focus on a pointer
press, and has an accessible image name and visible themed focus indication.
All buttons remain Tab/Space accessible. Extend the wrapped footer hint to name
0, 1, +/−, wheel, drag and arrows, including that arrows operate on the canvas.
Opening the dialog immediately suppresses manipulation without changing the view.

## Rendering and resources

Retain QQuickPaintedItem with an item-sized backing surface. Paint only the image
portion intersecting the canvas: map the visible destination rectangle back to
source coordinates and draw from the existing immutable QImage. Preserve alpha
and aspect ratio, use smooth sampling below 100% and nearest-neighbor sampling
at/above 100% for pixel inspection. Clip painting to the canvas. Do not create a
scaled QImage, magnified texture, layer or cache on zoom. The decoded image stays
shared; no file access or worker request occurs during view manipulation.

Rendering storage scales with physical canvas dimensions, independent of zoom.
Existing decoder/conversion and backend memory limitations still apply. Coalesce
paint requests through update(); do not add synchronous repaints or animations.
Keep all state mutation on the GUI thread and consume stable geometry/image
state during Qt Quick painting. Measure the existing renderer before considering
any replacement; a different rendering architecture requires an updated design.

## Integration and acceptance

Add the geometry source to the internal viewer-ui target and its tests to the
existing GTest executable. Retain the public fitRect helper if existing stage 1
tests need it, deriving its result from the same fit calculation. Existing format
and tidy source discovery covers new application/test C++; verify this alongside
QML lint and the installed runtime.

Test geometry with explicit numeric expectations for anchoring, clamps, modes,
pixel ratios, zoom bounds and invalid/zero inputs. Integration tests must send
actual wheel, drag and keyboard events to production Main.qml, exercise focus
and modal suppression, and verify observable geometry/rendering outcomes.
Reuse stage 1 orientation, transparency, replacement and unchanged-file fixtures.
Include a source-pixel grid capture to verify Actual Size at 1, 1.25 and 1.5 scale.

Extend visual-check with fit, actual-size, panned and maximum-zoom captures, dark
and light themes, minimum/normal window sizes and all three scales. Verify
resize/fullscreen center preservation and installed opening regressions. Measure
large-image manipulation timing, GUI timer progress and peak RSS at Fit and
maximum zoom with identical canvas size; record backend, image and hardware
context. Native cross-display movement, portal and file-manager checks remain
manual where automation cannot establish the behavior. Record those limitations
without treating synthetic ratio changes as native desktop acceptance.

Update README with controls, physical-pixel semantics and zoom bounds only after
implementation. Record all acceptance results in VERIFICATION.md and keep earlier
stage acceptance open. No installed provider or sibling source changes are needed.

## Implementation notes

The view model is ViewGeometry, shared by ImageCanvas and numeric tests. The
window's devicePixelRatio property supplies the canvas display ratio. QML uses
DragHandler translation deltas and an input epoch to ignore the rest of a gesture
after image, dialog or viewport changes; the next gesture starts fresh.

The Open shortcut uses `sequences: [StandardKey.Open]` so platforms with multiple
standard Open bindings register all of them without a QML Shortcut warning.

The percentage has a dedicated row below the wrapping button row. This keeps
Fit-to-percentage text changes from resizing the canvas during anchored zoom.
Empty/loading/error show a dash, retaining row height without a stale percentage.
Arrow keys act only with canvas focus; Tab/Backtab and a themed border make that
focus visible and reachable.

Keyboard and button Fit, Actual Size and zoom actions explicitly focus the canvas
after changing the view. Wheel zoom does likewise. This makes arrow-key panning
available immediately after zooming from another focused control; Tab can still
move focus away and arrows continue to respect focused controls. Shared QML
action functions keep button and shortcut behavior aligned.

The live desktop can enlarge the window beyond requested geometry. Integration
tests therefore use a 6000×4000 fixture to exercise panning on this desktop and
allow Wayland configure events to settle after visibility changes. This does not
change the application's native window sizing/decorations policy.
