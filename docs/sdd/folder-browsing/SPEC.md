# Stage 3: folder browsing requirements

Status: approved scope from the user-supplied implementation plan (2026-09-08).
CONTRIBUTING.md permits that plan to authorize requirements, design and tasks.

- **BROWSE-01:** When a local image is opened via CLI, dialog or drop, the Viewer
  shall load it immediately and asynchronously scan its containing folder without recursion.
- **BROWSE-02:** The snapshot shall include regular files and symlinks to regular
  files with installed Qt image-handler suffixes, case-insensitively; it shall
  exclude hidden siblings and always retain the explicit path, including a
  hidden, extensionless or missing file. Symlink paths shall not be canonicalized.
- **BROWSE-03:** The snapshot shall sort ASCII digit runs numerically and remaining
  text case-insensitively, breaking equal comparisons by original filename.
- **BROWSE-04:** While scanning, the Viewer shall show feedback and disable
  navigation. Afterwards it shall show current / total and enable Previous/Next
  buttons and Page Up/Down within boundaries, including during loading or errors.
- **BROWSE-05:** Navigation shall advance from the latest requested position,
  reset each image to Fit, focus the canvas and never skip an error automatically.
- **BROWSE-06:** F5 shall clear cached images, rescan and reload the selected path,
  retaining that path as an error entry if deleted. Open shall suppress browsing
  and Refresh. Enumeration failure shall preserve viewing with a separate error.
- **BROWSE-07:** Scanning shall use a dedicated worker, one active and only the
  newest pending request, generation rejection and cooperative cancellation.
- **BROWSE-08:** Decoding shall retain one active request and newest pending
  foreground request; foreground shall precede queued prefetch. Only the next
  neighbor in the latest direction shall be prefetched, once per selection,
  initially forward. Prefetch failures shall be silent.
- **BROWSE-09:** The decoded LRU shall hold at most two images and 128 MiB,
  excluding the displayed image. Worker-side absolute-path, size and mtime
  validation shall invalidate stale entries. Existing 128 MiB image limits apply.
- **BROWSE-10:** Folder changes shall clear obsolete queued work. Shutdown shall
  cancel and drain both workers asynchronously.
- **BROWSE-11:** Verification shall cover model, scheduling, production input,
  20,000 entries, large accepted images, GUI progress, timings and peak RSS,
  contributor checks and dark/light minimum-size visuals at 100/125/150%.

Non-goals: thumbnails, recursion, directory CLI arguments, watchers, animation,
new format declarations, provider changes or closure of earlier desktop gates.
Retained display plus cache is bounded by 256 MiB; codec/render storage is additional.
