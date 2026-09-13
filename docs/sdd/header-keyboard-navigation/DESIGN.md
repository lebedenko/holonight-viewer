# Header keyboard navigation design

Approved through the user-supplied implementation plan on 2026-09-13.

An invisible, non-tab-stop item owns neutral window focus. A small `WindowKeyRouter` event filter on the QML window intercepts Tab and Shift+Tab, finds the three header buttons by object name, and focuses the next enabled button in either direction. This also handles the first key from neutral focus. Image command handlers and pointer gestures return focus to the neutral item when no menu or dialog is active. The router emits arrow-pan requests to QML, which calls `ImageCanvas.pan()`; it ignores those keys while the Actions menu or modal is open. The canvas retains rendering and geometry only, with no tab stop or outline. While the Actions menu is open, the router forwards J/K as Down/Up key events to reuse Qt Menu's selection and disabled-item behavior. Dialog and menu focus are preserved until they close.

Acceptance checks cover exact header traversal, empty-state skipping, focus-ring clearing, window-wide pan, menu navigation, modal suppression, pointer gestures, and both themes.

Implemented and verified on 2026-09-14; see [verification](VERIFICATION.md).
