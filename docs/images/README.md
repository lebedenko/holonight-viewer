# README capture

`landscape.svg` is an original repository-owned illustration, licensed under the
project's GPL-3.0-or-later terms. `viewer-dark.png` is a real Viewer window capture.
Reproduce on Hyprland with the installed screenshot tools:

```sh
HOLONIGHT_APPEARANCE_FILE="$PWD/tests/fixtures/dark.toml" QT_SCALE_FACTOR=1 \
  python3 scripts/screenshot.py --timeout 600 --size 1200x800 --delay 6 \
  --output docs/images/viewer-dark.png -- "$PWD/docs/images/landscape.svg"
```

Grim captures at scale 1 for the 1200×800 pixel image. The screenshot tool resizes
only its own started window and never moves the pointer or requests focus.
The public AppStream screenshot URL becomes available when these files reach main.
