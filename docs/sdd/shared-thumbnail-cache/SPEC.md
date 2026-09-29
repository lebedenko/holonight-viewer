# Shared thumbnail disk cache — Viewer

Viewer uses `holonight-thumbnails` for its grid thumbnails so Files and Viewer can share validated freedesktop cache entries. On an in-memory miss, the existing thumbnail worker opens and inspects the source, looks up a disk entry, and decodes and stores on a miss. Cache failures do not change the decoding result.

The viewer retains its image decoder, thread pool, cancellation, in-memory LRU, grid request sizes, and logical texture dimensions. It derives logical dimensions from inspected source dimensions even when pixels come from disk. SVG resources must pass `holonight-images` inspection before lookup. Requests above 1024 device pixels bypass disk storage and lookup.

## Acceptance

- A valid shared raster entry is used by the thumbnail worker and preserves the source's logical display size.
- An SVG with unsupported resources cannot be made viewable through a cache entry.
- A cache miss or write failure still decodes the source through the existing path.
- Requests above the largest tier do not use disk entries.
- Existing thumbnail and grid tests, format checks, QML checks, and CTest pass with the provider installed at the accepted revision.
