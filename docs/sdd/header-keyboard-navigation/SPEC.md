# Header keyboard navigation requirements

Approved through the user-supplied implementation plan on 2026-09-13. These focus rules supersede earlier canvas-focus requirements.

- R1: When Viewer opens, no control shall show keyboard focus; Tab and Shift+Tab shall cycle Information → Fullscreen → Actions and reverse, wrapping and skipping disabled buttons. In an empty window, only Fullscreen and Actions shall be stops.
- R2: While no modal is active, image commands including arrow-key pan shall work without canvas focus. After an image action from a shortcut, button, menu, or pointer gesture, Viewer shall clear the control focus ring; the next Tab or Shift+Tab shall enter the header cycle.
- R3: The canvas shall never enter the tab chain or show a focus outline.
- R4: While the Actions menu is open, J and K shall move selection like Down and Up, skipping disabled items and retaining menu activation. Modal dialogs shall keep usable focus and suppress image commands.
- R5: Shortcut Help and README shall describe the new behavior.

Non-goals: changing image geometry, rendering, or sibling packages.

Status: implemented; local automated and visual acceptance passed on 2026-09-14. See [verification](VERIFICATION.md).
