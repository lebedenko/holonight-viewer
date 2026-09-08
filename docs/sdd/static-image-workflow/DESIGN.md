# Design

Approved through the supplied implementation plan, before implementation.

Represent the dihedral group as quarter-turn plus reflection (eight integer
values). Share its QTransform mapping between canvas painting and worker copy.
Compose operations in display coordinates. ImageDocument owns orientation and
metadata; ImageCanvas uses oriented dimensions in ViewGeometry and inverse maps
its clipped source rectangle to decoded coordinates before painting.

Attach format, size, modification time and decoded size to decode results and
cache entries. Obtain facts only on the decode worker; publish only current
requests. Clear facts and orientation at selection start.

ImageDocument owns a ClipboardController with its own QThread. Invocation takes
an implicitly shared QImage snapshot and orientation. Preparation returns an
image or error to the GUI thread, which publishes only if not shutting down.
No queue; busy gates both copy commands. Include this worker in asynchronous
application shutdown. Injection of preparation supports deterministic failure,
latency and shutdown tests. Snapshot can retain up to 128 MiB beyond the existing
256 MiB bound after navigation; transformed output adds up to 128 MiB, and Qt/
platform transport can retain further copies. Measure these separately in scope.

Shared QML Actions back menu entries and shortcuts. Lazy modal Dialogs use
scrollable selectable text, installed controls/style, and a common dialog gate.
Information text remains bound to current document facts. Canvas focus survives
loading/errors and is restored on dialog closure and transform invocation.

The Actions menu uses Popup.Item with a bounded scrolling ListView so minimum
window geometry and focus behavior are consistent across Qt platforms. Dialogs
use the installed style fallback with HoloNight palette values and visible
overflow scrollbars. Native clipboard transfer uses a separate rendered Qt receiver.
