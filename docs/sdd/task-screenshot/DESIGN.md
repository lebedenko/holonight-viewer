# Task screenshot design

`Taskfile.yml` exposes `screenshot` and forwards arguments to a Python helper.
The helper launches `task run` in the current development environment, identifies
the newly created Viewer window through Hyprland's JSON client listing, waits
for the optional interaction delay, reads its final geometry, expands it by the
optional pixel margin, and asks `grim` to write a PNG under `build/`. It tears down the launched process in a
`finally` path, including capture errors and interruption. The helper reports
missing compositor tools, invalid delay or margin values, and missing windows distinctly.

The output stays under `build/` so generated screenshots do not enter source
control. Window bounds are compositor coordinates; decorations are included to
the extent Hyprland reports them in client geometry.
