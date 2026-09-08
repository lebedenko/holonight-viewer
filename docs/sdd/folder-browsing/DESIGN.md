# Folder browsing design

Approved by the supplied implementation plan; routine details below implement its scope.

DirectoryModel is an internal QAbstractListModel with filename/URL roles. It owns
an enumeration thread and publishes a complete snapshot on the GUI thread. Each
scan captures its explicit absolute (noncanonical) path. Error-code filesystem
iteration detects enumeration failure; cancellation interrupts iteration and sort.
A generation rejects superseded results. A single optional pending scan bounds work.

ImageDocument owns the model and selected index. External opening starts a fresh
snapshot; internal selection only changes index and requests decoding. The
selected index updates synchronously before dispatch. Model completion resolves
the explicit path's sorted position. Refresh establishes a fresh cache generation
and snapshot while retaining the current path. Folder errors are distinct from
image errors. Both worker-finished signals gate asynchronous shutdown completion.

The existing decode thread also owns the LRU and performs file metadata checks.
A foreground result is removed from the LRU; its storage is shared with the
presentation until the next request returns it to the bounded cache. Cache epochs
clear old folder/refresh storage before new decoding. One pending foreground
request overrides prefetch; prefetch is chosen lazily only when foreground work
is settled and scanning is complete. A selection generation prevents retry loops
and rejects stale results. Codec calls retain cooperative cancellation limitations.

QML exposes a compact navigation row, stable position/scanning feedback, separate
folder-error text, Page Up/Down and F5 shortcuts, and focuses the canvas after
navigation/refresh. The canvas remains a stable Tab focus target across loading
and errors; readiness and dialog guards control inspection input separately.
Existing image replacement resets canvas geometry to Fit.

Traceability: model BROWSE-01–03/07; document BROWSE-04–06/08–10;
cache BROWSE-08–09; QML BROWSE-04–06; verification BROWSE-11.
