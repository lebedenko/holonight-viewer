# Human-driven clipboard acceptance

Status: passed, including explicit user cost acceptance. Date: 2026-09-23.
Completed results and retained failed attempts: [verification](VERIFICATION.md).
Run commands from the Viewer repository. Use only synthetic fixtures while recording
Wayland diagnostics. Do not run the older compositor-input/focus automation scripts.
The operator alone activates windows, copies, pastes, navigates and quits.

## Personal setup and fresh login

Installed Arch package: `wl-clip-persist 0.5.0-2` using
`sudo -A pacman -S --needed --noconfirm wl-clip-persist`.
Personal file: `~/.config/autostart/wl-clip-persist.desktop` (validated with
`desktop-file-validate`), containing:

```ini
[Desktop Entry]
Type=Application
Name=Clipboard persistence
Comment=Keep the current regular Wayland clipboard in memory
Exec=/usr/bin/wl-clip-persist --clipboard regular
TryExec=/usr/bin/wl-clip-persist
Terminal=false
```

Existing `~/.config/hypr/autostart.lua` runs
`uwsm app dex -a -s ~/.config/autostart` on `hyprland.start`.
No compositor configuration was changed. After the user performs a fresh login,
record `pgrep -a -u "$USER" -x wl-clip-persist` and the process's user-systemd
unit/cgroup, start time and session start time. Exactly one instance must run with
the command above. Merely installing the entry is not startup acceptance.

[Upstream](https://github.com/Linus789/wl-clip-persist) describes keeping the current
selection in memory. This adds neither history nor reboot persistence. All CLI
options other than regular clipboard selection retain upstream defaults.

## Fixture and binary provenance

Prepared local fixtures: `build/clipboard-persistence/fixtures/`, copied unchanged
from `build/test/tests/fixtures/images/`; hashes, stored dimensions and independently
read RGBA hashes are in `build/clipboard-persistence/fixtures.json`.
These are synthetic images, never personal photos. The added relative symlink
`linked café 100%.png` targets the existing special-character alpha fixture.
Preserve the lexical symlink path for path-copy checks. JPEG stored-pixel hashes
are not orientation-normalized expected output hashes.

Five cost trials use the same unchanged 6000×4000 green `large.png`, no transforms:
source SHA-256 `24d6718ba9b8c2a14ce11fc1b629b684a55c28b62a81a9021bbb53fe8b5a5f68`;
expected RGBA SHA-256
`f0b4d4f26b22f0a630783c8eeccc8271b4090cfec270c4325e07e74a7e890636`.
Alpha and handedness are checked separately below; this uniform cost fixture cannot
prove those properties. These costs apply to this fixture, not all photographs.

Starting Viewer commit: `6f9c048e2f936bc93e017a89cd18fb032159d603`.
The receiver has local passive instrumentation changes; capture records exact Viewer,
receiver, service and runner SHA-256 values. Also retain `git diff`, package, Qt,
Hyprland and GIMP versions. Provider source and product implementation remain
unchanged. Use `build/release/apps/viewer/hn-viewer` and the rebuilt
`build/test/tests/clipboard-probe`; do not substitute installed older binaries.

## Five identical measurements

For N=1 through 5, run the following with a fresh output directory (replace `measured-1`):

```sh
python3 scripts/clipboard-persistence.py capture \
  --viewer build/release/apps/viewer/hn-viewer \
  --receiver build/test/tests/clipboard-probe \
  --fixture build/clipboard-persistence/fixtures/large.png \
  --output build/clipboard-persistence/measured-1 \
  --width 6000 --height 4000 \
  --rgba-sha256 f0b4d4f26b22f0a630783c8eeccc8271b4090cfec270c4325e07e74a7e890636
```

Manually activate Viewer, copy the image exactly once, wait for its Copied feedback,
then quit Viewer. After it has exited, activate the receiver and press Enter or
Ctrl+V once. The receiver saves pixels and exits. No other clipboard owner or copy
operation may intervene. Do not paste before quitting or reuse an old trial directory.

The runner samples each process's RSS/HWM every approximately 20 ms. Report service,
receiver and Viewer values separately, including service baseline, peak and retained
RSS. Sampling can miss short-lived peaks; HWM belongs to the process lifetime,
especially the persistent service, and is not a per-trial allocation delta.
The Wayland set_selection→source.cancelled interval is **observed service ownership
handoff latency**, including compositor dispatch. It excludes Viewer PNG preparation
and is not internal service-only CPU time. Receiver read_ms includes transfer and
Qt decoding, excluding later PNG saving and hashing. Logging/sampling overhead is
present in all five trials. Do not count human delays as transport latency.

```sh
python3 scripts/clipboard-persistence.py report \
  build/clipboard-persistence/measured-{1,2,3,4,5} \
  > build/clipboard-persistence/report.json
```

Retain every raw.json, viewer.log, receiver.log, received.png and summary.json,
including failed attempts. Missing/ambiguous handoff, multiple copies, wrong pixels,
failed process, early receive or differing fixture/binary/version records rejects
acceptance. A report always leaves explicit user cost acceptance pending. Present
all five values and median/range; obtain and record the user's acceptance verbatim.

## Correctness and interoperability matrix

Use the same interactive receiver manually, launching from a terminal:

```sh
QT_QPA_PLATFORM=wayland build/test/tests/clipboard-probe --interactive image build/clipboard-persistence/matrix.png
```

Use the existing asymmetric `workflow.png` for all eight orientations: reset,
then 0/1/2/3 clockwise quarter-turns; reset and mirror horizontally, then 0/1/2/3
clockwise quarter-turns. In every row, copy, quit Viewer, activate the receiver,
and paste. Retain distinct output PNGs and receiver logs. The 3×2 source rows are
red / half-transparent green / blue, then transparent / yellow / cyan. Quarter
turns give 2×3; even turns give 3×2. This single fixture verifies handedness,
dimensions and alpha. Independent expected RGBA hashes are retained in
`build/clipboard-persistence/matrix-reference.json`. The existing EXIF JPEGs are
also retained, but their uniform source is not handedness evidence.

Repeat Viewer copy→Viewer exit→paste in GIMP for the alpha fixture and a rotated,
mirrored workflow image; inspect orientation, alpha and size. Retain the user's
result and GIMP version. Earlier no-service post-exit loss remains historical
evidence, not a failure of this new configuration.

For exact path acceptance launch the receiver in `text` mode with a distinct output
file. Open each special-character source and the symlink in Viewer, Copy Path,
quit Viewer, then paste in the receiver. Compare UTF-8 bytes to the absolute lexical
path (including spaces, colon, percent, Unicode and the symlink name), with no newline
added or resolution to its target. Save expected/received bytes and hashes.

Do not close N3 or I-005 until the matrix, fresh login and five measured costs are
accepted. Physical mixed-monitor and unrelated release gates remain deferred.
