# Header keyboard navigation requirements

Approved through the user-supplied implementation plan on 2026-09-13. These focus rules supersede earlier canvas-focus requirements.

- R1: When Viewer opens, no control shall show keyboard focus; Tab and Shift+Tab shall cycle Information → Fullscreen → Actions and reverse, wrapping and skipping disabled buttons. In an empty window, only Fullscreen and Actions shall be stops.
- R2: While no modal is active, image commands including arrow-key pan shall work without canvas focus. After an image action from a shortcut, button, menu, or pointer gesture, Viewer shall clear the control focus ring; the next Tab or Shift+Tab shall enter the header cycle.
- R3: The canvas shall never enter the tab chain or show a focus outline.
- R4: While the Actions menu is open, J and K shall move selection like Down and Up, skipping disabled items and retaining menu activation. Modal dialogs shall keep usable focus and suppress image commands.
- R5: Shortcut Help and README shall describe the new behavior.
- R6: In fullscreen, focusing any header control shall reveal the header and keep it visible while any header control has keyboard focus. Tab and Shift+Tab shall retain their existing order even after the fullscreen header has hidden. When focus leaves the header, ordinary fullscreen visibility rules shall apply.

Non-goals: changing image geometry, rendering, or sibling packages.

Status: R1–R5 implemented; local automated and visual acceptance passed on 2026-09-14. See [verification](VERIFICATION.md). R6 is implemented with automated regression coverage and user-confirmed native fullscreen focus acceptance on 2026-10-02.
