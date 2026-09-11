# Empty-window decoration design

Approved through the user-supplied implementation plan on 2026-09-11.

Embed apps/viewer/icons/empty-viewer.svg in the existing QML module. Derive its geometry from packaging/org.holonight.Viewer.svg; remove gradient layers, highlights and panel fills, crop transparent margins with stroke clearance, and retain licensing.

Replace the empty HnEmptyState with a centered HnIcon in canvasArea. Bind visibility to ImageDocument.Empty, normalColor to HoloniightPalette.surface, and square dimensions to the R3 formula. Reuse installed HnIcon source sizing, tinting, error handling and palette revision tracking. Keep it accessibility-ignored and use the existing canvas accessible name for empty-state semantics.

Update the smoke regression and use existing rendered test infrastructure for dimensions, states, themes and scaling. Store generated evidence in build/empty-window-decoration/ and build/visual/. Update README and backlog with evidence and explicit limitations.

The derivative uses viewBox `17 14 136 136`, preserving original path coordinates with stroke clearance. The installed HnIcon binds Image.sourceSize to its size property; Qt documents SVG rasterization at the requested [sourceSize](https://doc.qt.io/qt-6/qml-qtquick-image.html#sourceSize-prop).

Implementation details from focused verification: resolve the resource URL in Viewer before passing it to HnIcon. Use direct x/y center bindings to retain exact fractional padding (Qt center anchors rounded by up to half a logical pixel). Live-theme verification launches with an isolated complete appearance document before provider initialization.

Warning follow-up: keep displayed side independent from HnIcon.size, which is
bounded to the installed provider’s 1024-physical-pixel raster limit. Larger decorations
scale this raster; their padding and displayed dimensions remain unchanged. An
empty source prevents requests before the central canvas has a positive side.
