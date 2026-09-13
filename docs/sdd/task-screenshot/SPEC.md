# Task screenshot requirements

Intent: make a reproducible screenshot of the running Viewer, including transient
UI that needs time for manual interaction. This is a developer tool, not an
application feature. The current supported capture environment is Hyprland on
Wayland with `hyprctl` and `grim` available.

- R1: When `task screenshot` is invoked, it shall start Viewer through `task run`
  and wait for its new window to appear.
- R2: When `--delay N` is supplied, it shall allow N nonnegative seconds after
  the window appears before capturing it; without the option, it shall capture
  as soon as the window is ready.
- R3: Immediately before capture, it shall read the target window's position and
  size and save that region as a PNG under `build/`. When `--margin N` is supplied,
  it shall extend the region by N nonnegative integer pixels on each side.
- R4: After capture or a recoverable failure, it shall close only the Viewer
  instance it started and report a useful error when capture cannot complete.

No change to Viewer production UI, sibling providers, or system installation is
in scope. Other compositor capture backends are deferred.
