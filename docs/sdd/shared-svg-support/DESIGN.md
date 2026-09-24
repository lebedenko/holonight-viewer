# Shared SVG support — Design

In image_document.cpp replace ad hoc readAll/validation with shared bounded loading and inspectSvg. Only
LocalImageReference permits the existing consumer-owned filename renderer path. Pass source kind/facts through
DecodeResult. Load the persistent GUI renderer from bytes for self-contained documents and use shared rasterization
for their previews. Keep the linked-image path local, with renderer geometry passed through shared sizing helpers.
Use fractional document geometry for canvas layout; round only information pixel labels or raster output as needed.
Update image_canvas geometry and tests while preserving persistent renderer ownership and navigation cancellation.

Implementation may begin only after SVG-001 is published and pinned. No consumer implementation has started.
