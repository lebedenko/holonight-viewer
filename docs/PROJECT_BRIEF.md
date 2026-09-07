# Project brief

HoloNight Viewer is a focused native Qt image viewer: a spacious image canvas,
keyboard-first operation, restrained shared styling, and little persistent chrome.
Build a small standalone application rather than a fork or a desktop-shell feature.
The left half of mockups/moc1.png is visual direction, not a feature commitment.

Future concepts include asynchronous loading, directory navigation, fit/1:1 zoom,
pan, transforms, animation, basic metadata, clipboard and trash, command mode, and
mouse-revealed overlays. Proposed shortcuts in the original notes remain proposals
except f, Escape, and q implemented by this scaffold. Avoid an editing suite,
albums, tagging, or a photo database. Crop is only a possible later extension.

The original product and decoration discussions are preserved verbatim under
ideas/ so future cycles do not depend on temporary files. They are historical
proposals, not verified external research or current shared-theme contracts.

HoloNight Files is a separate proposal for a graphical Vim-like file manager:
modal navigation/selection/search/commands, optional places and preview panes,
and asynchronous file operations. Quick Look is a separate preview proposal for
images, text, PDF, media, directories, and archives. Neither belongs in Viewer.
Adaptive CSD/SSD belongs in shared holonight-qt work; Viewer currently inherits
its native-decoration HnApplicationWindow contract.
