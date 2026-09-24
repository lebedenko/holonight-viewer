# Shared SVG support — Design

In image_document.cpp replace ad hoc readAll/validation with shared bounded loading and inspectSvg. Only
LocalImageReference permits the existing consumer-owned filename renderer path. Pass source kind/facts through
DecodeResult. Load the persistent GUI renderer from bytes for self-contained documents and use shared rasterization
for their previews. Keep the linked-image path local, with renderer geometry passed through shared sizing helpers.
Use fractional document geometry for canvas layout; round only information pixel labels or raster output as needed.
Update image_canvas geometry and tests while preserving persistent renderer ownership and navigation cancellation.

Implementation uses published/pinned provider 3da5f4e51fe2eed9a1bd9f72c0ab523aa57a5ceb.

## Implementation files

`image_document.{h,cpp}`, `image_canvas.{h,cpp}`, `view_geometry.{h,cpp}` and `qml/Main.qml`;
regressions in `image_document_test.cpp` and `view_geometry_test.cpp`.
