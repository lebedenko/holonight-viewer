# Design

The distribution package and one personal XDG desktop entry remain outside product
code. Existing Hyprland startup runs dex through uwsm. The receiver is the existing
interactive clipboard-probe; interactive startup leaves activation to the user.

Passive tooling records /proc RSS/HWM separately per process and retains raw Wayland
protocol diagnostics from Viewer. The interval from Viewer set_selection to that
source's cancelled event measures observed ownership handoff (including compositor
dispatch), not internal service CPU time or Viewer PNG preparation. In a controlled
trial with one persistence service and no other copy, cancellation is evidence of
service takeover. Receiver read_ms measures request, transfer and decode before PNG
saving/hashing. Manual delays between copy, exit, activation and paste are excluded.
Missing/ambiguous events and incorrect output fail reporting; no fabricated timings.
