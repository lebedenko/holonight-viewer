# Verification results

Follow-up (2026-09-09): the user completed the guided stacking-window walkthrough
on labwc and reported Viewer working as expected. This supersedes the historical
stacking-environment limitations below for that human check; see
[release evidence](../release-readiness/VERIFICATION.md). Physical two-monitor
mixed-scale checks remain pending because the user has no second monitor.

Verified locally on 2026-09-08, Arch Linux, Qt 6.11.2, GCC 16.2.1,
clang 22.1.8, and REUSE 6.2.0. No sibling or umbrella source changes were made.

| Check | Result | Requirements |
| --- | --- | --- |
| `task deps` | Passed: both pinned-revision sibling providers built and installed under build/deps; sibling working trees remained clean | R1, R9 |
| `task build` | Passed: debug executable | R1 |
| `task build PRESET=release` | Passed: release executable | R1 |
| `task test` | Passed: 5/5 CTest cases, including 2 GTest smoke cases | R2–R6 |
| `task format-check` | Passed: C++ and QML formatting | R8 |
| `task tidy` | Passed: both C++ translation units, no project diagnostics; external-header diagnostics suppressed by existing policy | R8 |
| `task qml-lint` | Passed without diagnostics | R2, R4, R8 |
| `reuse lint` | Passed; run outside sandbox because Python multiprocessing requires a local socket | R7 |
| `task install-check` | Passed: files, desktop validation, version and sustained QML startup with installed provider imports only | R6, R7 |
| `task visual-check` | Passed: dark/light captures at 100% and 125%, minimum and initial sizes | R10 |
| Live Wayland smoke | Passed: creation, active window shortcuts, fullscreen restoration, quit, native window flag | R2, R3 |
| `git diff --check` | Passed | R8 |

The smoke test loads the production Main.qml, checks the empty state and native
window flags, checks minimum-size bounds, sends real f/Escape/q key events, and
verifies restoration of normal or maximized state. The style-selection test clears
QT_QUICK_CONTROLS_STYLE, QT_QUICK_CONTROLS_FALLBACK_STYLE and
QT_QUICK_CONTROLS_CONF before application creation. It creates a generic Controls
Button after the Viewer window and checks the Holonight style and its specific
foregroundColor property. CLI tests cover help, version, unsupported file
arguments, and QML startup failure using an intentionally invalid provider module.

Visual inspection found centered readable text, clear hints and no clipping in
[dark initial size](evidence/dark.png) and
[light minimum size at 125%](evidence/light-125-small.png). Additional captures
are reproducible with `task visual-check` under build/visual. A live Wayland
capture also rendered correctly. The compositor initially maximized the window;
the test now verifies restoration of that actual starting state.

Implementation findings resolved during verification: nested Task calls needed
explicit preset propagation; the test QML resource needed a Main.qml alias;
and QtQuick.Controls must initialize the embedded style before shared controls
import Basic. A narrow unused-import annotation preserves that intentional import.

Remaining acceptance checks:

- CI workflow execution is pending. No Docker/Podman runtime was available locally;
  the workflow builds its own Arch image rather than depending on a published one.
- Visual inspection of server-side titlebar buttons on a stacking compositor is
  pending. The live test and client-surface captures verify native window flags and
  behavior, but do not capture compositor-owned decoration pixels. No custom
  titlebar or frameless flag is set. R10 is therefore only partially verified.

The scaffold is implemented and local automated checks pass. Full visual/CI
acceptance is not marked complete until these remaining checks are performed.

## Development portal registration follow-up

A user-reported launch warning showed that temporary staged installation did not
register org.holonight.Viewer with the running host portal. `task run` now invokes
`task desktop-install`, which registers a user desktop entry and icon before
launch. Its Exec line points to the selected build and installed provider paths.
Validated the generated entry with desktop-file-validate and checked Task command
ordering. Installed the entry under ~/.local/share with authorization and launched
the real window for three seconds: no portal warning or other diagnostic was
emitted (timeout exit 124 was intentional). REUSE and whitespace checks passed.

## Roadmap stage 0 follow-up (2026-09-08)

Re-ran `task deps`, `task build`, `task test` (5/5),
`task build PRESET=release`, `task format-check`, `task tidy`, `task qml-lint`,
`task license-check`, `task install-check`, and `git diff --check`: passed.
REUSE again required execution outside the sandbox for its local worker socket.
Staged installation and sustained offscreen startup passed under
`build/install-check.bMDvFZ`.

A fresh source snapshot containing tracked and untracked non-ignored files was
copied with `git ls-files --cached --others --exclude-standard -z` to
`build/scaffold-clean-ozkw64xz/source`. Configured a separate Release build with
BUILD_TESTING=ON, Ninja, /usr prefix and the existing installed provider prefix;
`cmake --build ... --parallel 2` and `ctest --test-dir ... --output-on-failure`
passed (5/5). The log is `build/scaffold-clean-ozkw64xz/verification.log`.
This verifies independence from existing Viewer build output. It is **not** a
clean-checkout acceptance result: most scaffold files remain untracked, so the
committed tree cannot yet reproduce the working tree. Publishing/committing the
scaffold and executing its CI remain pending.

Added and ran `task desktop-check`: passed for debug and release registration in
one isolated XDG data directory under `build/desktop-check.7qbo60jy`. It validates
the desktop entry, checks exact executable/provider arguments, executes each
registered command with --version, verifies the icon, and confirms the packaged
entry is unchanged, uses PATH lookup, and omits MIME declarations. The check is
also included in build CI. It does not change host registration or claim desktop
menu/portal launch acceptance; the earlier authorized host launch evidence remains
above. The isolated check supports ordinary paths without backslashes or percent
signs and fails explicitly for those unusual paths.

CI execution remains pending: neither Docker/Podman nor GitHub CLI is installed,
and current workflow/application files are untracked. Stacking-compositor titlebar
inspection remains pending: no stacking compositor is installed (Cage is a kiosk
compositor and does not meet that gate). No new compositor visual result is claimed.
Stage 0 is therefore still open; stages 1–5 are not implemented by this follow-up.

## Fullscreen tiled-state regression (2026-09-08)

The user reported tiled → fullscreen → maximized-under-the-panel transitions on
Hyprland. This corrects existing R3 behavior. The fullscreen routing in Main.qml
now uses WindowState to change only Qt::WindowFullScreen in QWindow::windowStates;
it does not replace the complete state with showFullScreen/showMaximized/showNormal.
Keyboard, header and menu toggles share the same function.

The original WindowAndKeyboard test checked synchronous Qt visibility and could
finish before native fullscreen configure events arrived. It now processes events
for 250 ms between transitions and checks restored geometry. The strengthened
test failed before the fix on the live Hyprland session: the original tiled width
was about 762 logical pixels and the restored width about 2560. The protocol trace
showed unset_maximized on fullscreen entry. With the fix the same test passed and
all three fullscreen cycles emitted only set_fullscreen/unset_fullscreen, with no
maximize/unmaximize requests. Logs: build/fullscreen-before-settled.log and
build/fullscreen-after.log. Fixed delays permit the observed compositor round trips;
they are not a general synchronization guarantee for arbitrarily slow desktops.

Native
stacking-desktop restoration remains unverified by this Hyprland run; the offscreen
suite separately exercises normal and explicitly maximized window states.

Checks passed: task deps, task build, task test (all 7 CTest entries; 42 GTests
passed, 3 unrelated opt-in performance/clipboard tests skipped), task build
PRESET=release, task format-check, task qml-lint, task license-check, and task
install-check. The license task required execution outside the sandbox because
Python multiprocessing could not bind its worker socket. Logs are under
build/fullscreen-{deps,build,tests,release,format,qml-lint,license,install}.log.
Static analysis: task tidy passed (build/fullscreen-tidy.log); only suppressed
non-user-code warnings were reported. git diff --check also passed.
