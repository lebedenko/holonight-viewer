# Shortcut remapping requirements

Approved through the user-supplied implementation plan on 2026-09-11.

- R1: When the user presses `[` or `]`, Viewer shall navigate to the previous or next sibling image in the folder respectively, stopping at folder boundaries, provided no modal dialog is active.
- R2: When the user presses `Ctrl+R`, Viewer shall rescan the current directory, clear cached images and snapshots, and reload the current image, provided no modal dialog is active and an image with a local path is loaded.
- R3: When the user presses `Ctrl++` (or `Ctrl+=`) or `Ctrl+-`, Viewer shall zoom in or out centered on the canvas, provided an image is inspectable and no modal dialog is active.
- R4: When the user presses `Ctrl+0`, Viewer shall reset the view to fit the entire image within the canvas area, provided an image is inspectable and no modal dialog is active.
- R5: When the user presses `?`, Viewer shall display the Shortcut Help modal dialog, provided no modal dialog is active.
- R6: Viewer shall update the Shortcut Help text dialog, the bottom caption strip (`[/]  navigate`, `Ctrl++/−  zoom`, `Ctrl+0  fit`, `?  help`), and `README.md` to consistently present the remapped shortcuts.

Non-goals:
- User-configurable custom keybinding profiles or shortcut configuration UI.
- Mouse wheel / touchpad zoom and pan behavior modifications.
- Modifying standard shortcuts for file open (`Ctrl+O`), fullscreen (`F`), quit (`Q`), or transforms (`R`, `Shift+R`, `H`, `V`).
- Changes to sibling HoloNight libraries or packages.

Status: implemented; local automated acceptance passed on 2026-09-11. See [verification](VERIFICATION.md).
