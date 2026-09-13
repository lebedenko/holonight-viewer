# Task screenshot verification

2026-09-14, local Hyprland Wayland session:

- `task --list` shows `screenshot`.
- `python3 scripts/screenshot.py --help` succeeds; a negative delay is rejected
  with exit status 2.
- `task screenshot -- --delay 0.2` launched the debug Viewer through `task run`,
  captured `build/screenshot.png` at 1500 × 1050, and returned successfully.
  Visual inspection showed only the Viewer window. `pgrep -x hn-viewer` found no
  remaining process afterward.
- `git diff --check` passed.

The native capture required access to the session compositor socket, which the
command sandbox denied. The successful run used the approved live-session command.
The Task runner logged termination status 143 during cleanup, while the screenshot
command itself returned status 0 and the Viewer process exited. Full `task check`
was not rerun: this cycle changes developer tooling and documentation only.

Margin follow-up (2026-09-14): `task screenshot -- --delay 0.2 --margin 16`
completed and produced a larger 1548 × 1098 PNG; a zero-margin comparison produced
1500 × 1050. `python3 scripts/screenshot.py --margin -1` returned status 2 with a
validation error, and `git diff --check` passed. The captured region is expanded
in Hyprland coordinate pixels. On this scaled display, those units map to more
physical PNG pixels, which accounts for the 48-pixel width and height increase.
