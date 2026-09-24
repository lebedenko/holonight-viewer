# Shared SVG support — holonight-viewer

Approved scope: user-supplied implementation plan, 2026-09-24. Work package SVG-002.
Baseline: `1200adb820c802259af37b583ce076a863036c56`.

## Requirements

- When loading SVG, Viewer shall use the shared loader with a 10 MiB input budget and retain source bytes.
- For self-contained SVG, Viewer shall use shared facts/rasterization and load its GUI renderer from retained bytes.
- For local linked-image documents, Viewer shall preserve its existing relative filename-based loading behavior.
- Canvas layout, information and previews shall consistently prefer default document size over viewBox size.
- Vector painting, zoom, navigation, clipboard and existing animation behavior shall remain available.

See [design](DESIGN.md) and [tasks](TASKS.md).
