# DESIGN: Consistent key hints

Adopt HnKeyHint.keyGroups using explicit Qt key constants after the provider is published and pinned. Remove local keycap painting and symbol sizing. Use shared wrapping for constrained help. Keep existing action handlers and validation messages; update size measurements and accessibility composition to match the shared badge.

## Adoption sites

- `apps/viewer/qml/footer/FooterKeyHints.qml`: semantic arrays for navigation,
  zoom, fit, actual size, rotate, fullscreen and help; compose parent accessibility
  using the shared readable name and translated action label.
- `apps/viewer/qml/shortcuts/ShortcutHelpPopup.qml`: semantic data for keyboard
  rows, retaining textual gestures. Replace the keycap content override with `wrap`.
- `apps/viewer/qml/Main.qml`: menu shortcut badges use the same explicit key
  combinations as their existing actions. Preserve all shortcut activation logic.

Verification: footer and shortcut/help regressions first; `task check` or the
repository's equivalent external-build acceptance with explicit provider paths,
including formatting, QML import/type/lint checks and CTest. Native menu/help review
remains a final ecosystem task.

## Scrollable menu navigation

Shared badges increase the menu's measured row height. A menu can consequently
scroll at the default window size. Disable ListView's automatic key navigation so
Controls.Menu continues to skip separators and disabled actions; keep pointer
scrolling enabled when content exceeds the viewport. Shortcut bindings and action
handlers remain unchanged. Tests measure overflow instead of assuming it from
window height, and activate Actual Size by keyboard in the compact viewport.

## KH-007 design refinement

Use the published provider's frameless sequence label in ViewerMenuItem. Bind the
full font directly to the existing menu label, and color to the action's enabled
state. Keep RowLayout sizing, semantic key arrays, menu navigation and scrollbar
reservation unchanged.

Bind each footer HnKeyHint point size to its adjacent description's resolved
font point size. Qt resolves this value for pixel-sized descriptions too, avoiding
loss of fractional point-to-pixel conversion and stale unit overrides. The badge
retains its provider-default monospace family.
Help continues to use shared framed hints. No action or shortcut logic changes.
