# Verification

Date: 2026-09-23. Native acceptance passed; publication is authorized by the
subsequent **“commit, publish, pin”** request. The umbrella ledger records exact revisions;
the clipboard-service deferral is closed for this scoped follow-up.

- Installed `wl-clip-persist 0.5.0-2` with `sudo -A pacman -S --needed --noconfirm wl-clip-persist`.
- Validated and installed one personal desktop entry as described in [acceptance](ACCEPTANCE.md). The package itself supplies no autostart entry. Fresh-login acceptance subsequently passed; see below.
- `cmake --build build/test --target clipboard-probe --parallel 4`: passed without compiler warnings.
- `python3 scripts/check-clipboard-persistence.py`: seven tests pass, including separate costs, both Wayland object notations, missing/ambiguous handoff, wrong pixels/dimensions, early receive, process failure/restart, missing memory, duplicate trials, five-trial aggregation and mismatched provenance.
- `ctest --test-dir build/test -R '^viewer-clipboard-persistence-runner$' --output-on-failure`: passed.
- `clang-tidy -p=build/test -removed-arg=-mno-direct-extern-access --config-file=.clang-tidy tests/clipboard_probe.cpp`: passed. The removed argument is a GCC option unsupported by the local clang; external-header diagnostics are suppressed by existing project configuration.
- `python3 scripts/format-sources.py --check`: passed.
- `reuse lint`: passed outside the sandbox; the first sandbox attempt could not bind the Python worker-pool socket.
- Local Markdown links and `git diff --check`: passed.

The receiver no longer requests window activation. It flushes a receive-start marker
before reading, so the sampler can reject reception before observed Viewer exit.
Original read_ms and pixel reporting remain intact. Product application and provider
sources are unchanged: reuse the application acceptance recorded in
[shared-image-outcomes](../shared-image-outcomes/VERIFICATION.md), not a new native
acceptance claim. CI successes in the umbrella ledger apply only to their old exact
commits, not these local tooling changes.

Prepared fixtures and independent stored-pixel hashes are retained under
`build/clipboard-persistence/`; build/tidy/format/license logs are initially retained
in `/tmp/viewer-clipboard-*.log` and copied into that evidence directory.
The initial pending gates below were subsequently completed in this session;
the final closure section records their acceptance.
No older input/focus automation was run.

## Fresh-login acceptance — 2026-09-23

User reported **“logged in”**. Session 6 is active Wayland, started 22:13:37 EEST.
Exactly one `wl-clip-persist` process exists for the user: PID 513767, started
22:13:37, with `/usr/bin/wl-clip-persist --clipboard regular`.
The generated `app-wl\x2dclip\x2dpersist@autostart.service` became active at
22:13:38; its SourcePath is the personal desktop entry and FragmentPath is under
`/run/user/1000/systemd/generator.late/`. The process cgroup belongs to that
unit under app-graphical.slice. This demonstrates XDG/systemd desktop autostart
from the existing session; it was not manually launched for qualification.
Raw observed fields: `build/clipboard-persistence/login.json`.
Commands: `pgrep`, `systemctl --user list-units/show`, `ps`, `loginctl show-session`
and `/proc/513767/cgroup`. Manual persistence and costs remain pending.

First native attempt: post-exit receiver succeeded with 6000×4000 and exact expected
RGBA hash, 101 ms receiver read. Raw evidence remains in `trial-1/`. Its report
failed because this libwayland emits HH:MM:SS.microseconds timestamps instead of
numeric milliseconds. The parser now accepts both forms; eight validator tests
pass including this actual timestamp form. That attempt is excluded from the
five identical measurements; the corrected runner is frozen for `measured-1` to
`measured-5`. No application, receiver or sampling logic changed.

## Five frozen measurements — 2026-09-23

All five copy/exit/receive trials passed exact 6000×4000 RGBA validation.
Evidence: `build/clipboard-persistence/measured-1` through `measured-5`,
with raw samples, protocol/receiver logs, received PNGs and `report.json`.

| Trial | Handoff ms | Receiver read ms | Service peak RSS KiB | Receiver HWM KiB | Viewer HWM KiB |
|---|---|---|---|---|---|
| 1 | 2.506 | 99 | 5192 | 475360 | 468968 |
| 2 | 1.387 | 105 | 5192 | 474956 | 470996 |
| 3 | 1.326 | 100 | 5196 | 474808 | 470052 |
| 4 | 1.863 | 102 | 5196 | 474604 | 473672 |
| 5 | 2.156 | 112 | 5196 | 474992 | 467968 |

Service handoff median 1.863 ms, range 1.326–2.506 ms. Receiver read median
102 ms, range 99–112 ms. Service retained RSS 5192–5196 KiB. Receiver process
HWM 474604–475360 KiB; Viewer HWM 467968–473672 KiB. Handoff includes compositor
dispatch, not Viewer preparation. Receiver HWM includes receive/decode, conversion,
saving and hashing, not just read_ms. The persistent service HWM is lifetime,
not a fresh allocation total per trial. Sampling/logging overhead is included.
These results apply to this synthetic, highly compressible green fixture only.
User explicitly accepted: **“Accept measured costs”**. The exact scope and response
are retained in `build/clipboard-persistence/cost-acceptance.json`.

Orientation matrix in progress: original and 90° clockwise rows pass exact
RGBA/alpha hashes after Viewer exit. `matrix-1` received the original orientation;
`matrix-1-retry` passes 90°. `matrix-2` received 90° instead of requested 180°;
it is retained as an unpassed attempt while clarifying the manual key sequence.
No orientation failure is attributed to product code without that clarification.

The user clarified **“I pressed R only once”** for `matrix-2`. The 90° result
therefore matches the performed action; the 180° row is rerun with two R presses.
Remaining rows are guided one at a time to avoid ambiguous operator instructions.

All eight orientation rows now pass after user-driven transforms and copy/exit/receive:
`matrix-0`, `matrix-1-retry`, `matrix-2-retry`, and `matrix-3` through `matrix-7`.
The same six-color synthetic source covers full transparency, alpha 128,
handedness and exact 3×2/2×3 dimensions. Independent decoding of all eight saved
PNGs with Pillow also matches the expected RGBA hashes; results are retained in
`matrix-result.json`. Earlier wrong-orientation attempts remain untouched.

Both exact path rows passed after Viewer exited, including UTF-8 spaces/colon
and a Unicode/percent symlink name. `path-1` and `path-2` retain expected and
received bytes, SHA-256, logs and owner-exit ordering. The symlink's lexical path
was preserved instead of replacing it with its target. Both processes exited 0.

## Frozen native environment and hashes

Hyprland 0.56.2 (`efb50993780079460b0cbed1363e2166a2de1d9f`), native Wayland,
Qt 6.11.2, GIMP 3.2.6, wl-clip-persist 0.5.0-2. Monitor eDP-1, 2560×1600,
scale 1.25, 240.00101 Hz. All five records agree on versions, environment,
monitor configuration and fixture/binary/tool hashes. This is clipboard-specific
acceptance; the earlier single-monitor preview matrix remains its own evidence.
Viewer source baseline is `6f9c048e2f936bc93e017a89cd18fb032159d603`;
receiver and measurement tools are the local follow-up builds described above.

| Artifact | SHA-256 |
|---|---|
| Viewer binary | 164f50335c9aee7a3e0b7560adba310547e4948511f1cac972bb18bdbf290dde |
| Receiver binary | 36007035f88412a3a2114fbdb85f167a69bb3afff845cf6d16de006377c31956 |
| Service binary | bbf255c1f958483101c496f559e36a4f951f036f18c47ebd866acd3240d11508 |
| 6000×4000 fixture | 24d6718ba9b8c2a14ce11fc1b629b684a55c28b62a81a9021bbb53fe8b5a5f68 |
| Frozen capture/report tool | 71ba6670f75e2c2235b20d2bccd5e1cc439dbfb1872456c4f965ddb6da0a3097 |

## Final closure — 2026-09-23

GIMP 3.2.6 was launched with `GDK_BACKEND=wayland` and DISPLAY unset.
Viewer exited successfully before each user-performed Paste as New Image:

- 40×20 semi-transparent red: user reported **“Passed: size and transparency correct”**.
- Horizontal reflection then 90° clockwise, 2×3 with expected six-color/alpha layout:
  user reported **“Passed: orientation, size, and alpha correct”**.

GIMP acceptance is explicitly user-reported visual evidence, separate from the
receiver's automated pixel hashes. Raw owner-exit records and user reports are
retained under `gimp-alpha/` and `gimp-orientation/`. No pointer movement, focus,
copy, paste or quit action was automated. GIMP remains under the user's control.

Fresh-login singleton startup, all eight orientations/alpha/dimensions, exact
special-character/symlink paths, GIMP interoperability, five identical trials and
explicit cost acceptance all pass. N3/I-005 are closed; mixed-monitor and unrelated
release gates remain deferred. Existing Integrated and single-monitor acceptance
remain intact. Publication was outside the original acceptance scope; the subsequent
user request authorizes separate repository commits, publication and umbrella pins.

Final parser/CTest checks pass (eight validator cases); whitespace and local links
pass. The timestamp parser correction is tooling-only; the five trials all used
its same recorded hash, and no application rebuild or repeated four-scale
qualification is required. Full prior focused receiver/Files checks remain valid.

Publication preparation: final source review confirms product and install payload
sources are unchanged from the accepted baseline. Existing compatible clean-build
and isolated-runtime evidence is reused; test/tooling changes have the focused
verification recorded above. Exact publication and CI snapshots belong to the
[umbrella ledger](../../../../docs/initiatives/shared-image-outcomes/TASKS.md).
