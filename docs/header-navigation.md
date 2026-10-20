# Design: shared header close control adoption

Viewer replaces its inline close-button styling with
Holonight.Controls.HnHeaderCloseButton. The provider preserves compact dimensions,
24 px icon, padding, corner radius, semantic error hover/pressed surface,
foreground colors, focus outline, and Enter/Space activation.

Viewer continues to own quitAction, accessible translation, visibility preferences,
tab/backtab order and hidden-control focus transfer. Other header actions keep
their existing local control. The unused bundled close SVG and resource entry are
removed; the theme now provides window-close.

Build Icons and Qt providers first in an isolated prefix. Existing ViewerHeader,
ViewerPreferences and focus regressions cover close activation, hover/pressed
colors, visibility and keyboard navigation. Run repository acceptance and staged
runtime checks against those providers. Light/dark and fractional-scale visual
parity require manual native review. See the umbrella header-navigation contract.
