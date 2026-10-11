# Viewer review fixes design

AnimationController publishes snapshots of all exposed state, with a separate frame-index notification. Neighbor selection skips SVG by case-insensitive suffix.

ImageLoader owns the decode worker, cancellation, newest pending value request, busy state, epochs, LRU, displayed entry and retained budget. Requests carry URL, document token, epoch and foreground/tentative/prefetch kind. GUI completion clears active state before delivery and dispatches pending work after handlers return. Cache mutation and tentative adoption are serialized on the worker. ImageDocument keeps identity, validation, scanning, SVG renderer, transforms, clipboard and animation. Shutdown registers loader/directory/clipboard/animation before starting any participant.

Four private static targets export their own directories: images (Core/Gui/Svg, Images), browsing (Core, Images, images), thumbnails (Quick, images, Thumbnails), platform (Gui/DBus, private GuiPrivate/Wayland). URL normalization moves to images. Executable-owned controllers, formatting, canvas, geometry and QML registration stay in the application. Tests use a private loader adapter for cache observations.

Risks: synchronous completion reentrancy, worker adoption ordering, tentative cancellation while foreground loads, and accidental transitive includes. Barrier tests, integration regressions and external build address these. Existing asynchronous shutdown draining is retained.
