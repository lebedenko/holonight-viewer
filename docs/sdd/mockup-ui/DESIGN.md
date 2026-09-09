# Mockup UI design

Approved through the supplied implementation plan.

Use installed HoloNight header, controls and tokens with inline Viewer presentation overrides. Keep existing document and gesture ownership. ImageCanvas observes window events without consuming them. Separate QML timers own arrow and details visibility. Increment a canvas image generation on every image replacement (including clearing for a cached selection); queue first-paint notification to the GUI thread, validate generation there, and notify once. Metadata formatting reads the immutable document snapshot. Details and navigation are siblings over the canvas, outside geometry calculations.

Presentation follows the Viewer portion of `docs/mockups/moc1.png`. Shared
HnHeaderBar and HoloNight buttons/menu items use local background/content overrides;
Qt Basic information/help dialogs use the shared palette and an explicit Close
control. Native Open remains provider-owned. Footer Flow children keep each
translated key/action hint together. The strip sizes to its natural content until
constrained, then elides the filename before wrapping metadata. Scanning, folder
errors, decode errors and clipboard feedback retain their existing lifecycles.

The image generation is incremented for both clear and non-null replacements.
QQuickPaintedItem paint runs while the GUI thread is blocked during scene-graph
synchronization; the queued callback runs on the item's GUI thread and checks the
current generation. QObject context ownership cancels delivery after destruction.
Rotation, zoom and pan never increment this generation. The event filter observes
only the owning QQuickWindow and always returns false, including shortcut override
events so shortcuts dismiss arrows before activation.

Typography correction (R6): ViewerButton inherits HoloNight Button’s
`font.pointSize: HolonightTheme.bodySize`. Remove the local pixel-size override,
which combined with the inherited point size and caused the Qt warning.

R7: Embed three original 24-unit outline SVGs in the Viewer QML resource module. Use shared Button icon tinting with the Hero icon-size token (24) and Large hit target (40). Keep fonts point-based. Right-align the menu beneath its button with 12-unit popup margins and a shared spacing gap.

Open the Actions popup with `open()` so its explicit x/y alignment is preserved;
`popup()` applies context-menu positioning and overrides that placement.

Header icon width and height each bind directly to the shared size token. Do
not bind one icon subproperty to another: updates to the grouped icon value
caused a runtime binding loop in `icon.height`. This corrects R7 without changing
the approved icon dimensions or typography.

R8: Keep the filename and outer Flow. Replace the combined metadata label with
a Flow of repeated rows, each pairing a vertical HnSeparator with a plain-text
HnLabel. Bind the translated section array to document metadata and magnification.
Sum natural row widths for filename elision and constrain rows to the strip width.
This implements the user-requested presentation refactor.
