# Image Information Popup — Verification

## Scope and environment

Review and corrections on 2026-09-14 cover the Image Information portion of
`viewer-review-1.md`, with the requirements in [SPEC.md](SPEC.md), implementation
decisions in [DESIGN.md](DESIGN.md), and checks in [TASKS.md](TASKS.md).
Help redesign, HUD changes and other items in the original review remain out of scope.
The user's “fix findings” instruction authorizes the documentation alignment and
keyboard-scroll regression check.

Verification uses Qt 6.11.2 and installed HoloNight packages under
`build/deps/prefix`, with no sibling source changes or provider rebuilds.
Automated GUI checks use Qt's offscreen platform and software rendering.
Generated logs and captures remain under `build/` and are not committed.

## Automated evidence

| Command | Result | Evidence |
| --- | --- | --- |
| `task test` (initial review) | Passed all 8 CTest targets, including popup, workflow, EXIF and accessibility cases in viewer-smoke | `build/image-information-review-test.log` |
| `task format-check` (initial review) | Passed | `build/image-information-review-format.log` |
| `cmake --build build/test --target qml-lint` (initial review) | Passed | `build/image-information-review-qmllint.log` |
| Focused keyboard-scroll regression (command below) | Passed after reproducing and fixing clipped focus/cursor behavior | `build/image-information-fixes-regression.log` |
| `task check` (after corrections) | Debug/Release builds, all 8 CTest targets, formatting, clang-tidy and QML lint passed; stopped at REUSE's sandbox socket error | `build/image-information-fixes-check-final.log` |
| `task license-check` (outside sandbox) | Passed, 157/157 files licensed | `build/image-information-fixes-license.log` |
| `task install-check` | Passed staged payload/runtime, four-format CLI and installed GIO launch checks | `build/image-information-fixes-install.log` |
| `task uninstall-check` | Passed | `build/image-information-fixes-uninstall.log` |

REUSE's Python multiprocessing forkserver needs a local socket that the sandbox
denied (`PermissionError: [Errno 1] Operation not permitted`). The license check
was rerun successfully with elevated permission; the remaining aggregate-check
steps were run separately. This is not a claim that `task check` exited zero.
Ninja also reported recovery of a prematurely ended build log; compilation and
the subsequent C++/QML checks succeeded.

The initial review's passing tests did not establish that keyboard focus scrolls
an offscreen FILE path into view. T-013 adds that missing check using a short,
400×300 host, EXIF sections and a wrapped path. Focus and clipboard assertions
alone do not prove cursor visibility. The first `task check` attempt reproduced
the problem: the path cursor bottom remained at y=159 in a 142px viewport after
Tab, Ctrl+End and Ctrl+Home, with `contentY` still zero
(`build/image-information-fixes-check.log`). The test setup was also refined to
require a clipped cursor rather than the entire path top being below the viewport.

The fix explicitly enables keyboard selection on the read-only editor and reveals
its cursor in the outer ScrollView on focus/cursor changes. The final regression
asserts cursor positions at both text ends, full vertical cursor visibility and an
unchanged header position. It passes with this command, after rebuilding:

```sh
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
QML_IMPORT_PATH="$PWD/build/deps/prefix/lib/qt6/qml" \
LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib" \
build/test/tests/viewer-smoke \
  --gtest_filter=ImageInformationPopup.PathCursorRemainsVisibleWithKeyboardScroll
```

## Visual evidence

The saved running-viewer capture
`build/visual/information-popup/shots/information-1000x700.png` was inspected during
the review. It shows the 96px preview, uppercase subdued section labels, brighter
values, dot-joined camera/location facts, abbreviated wrapped path and top-right
close icon. Its companion captures are `plain-1000x700.png` and
`help-1000x700.png` in the same directory. These are existing implementation
captures, not a fresh native capture of the corrections. Automated tests compare
the Information and Help dimmer alpha values (0.22 and 0.5).

## Acceptance boundaries

- All checks required by `task check` passed, with the sandbox-blocked license
  step and subsequent installation/uninstall steps completed separately as recorded above.
- Offscreen geometry, input and accessibility checks do not establish native
  screen-reader behavior or physical mixed-monitor scaling.
- Staged installation/uninstall checks, when passed, qualify disposable trees;
  they do not establish an actual host `/usr` installation or Docker qualification.
- Refresh and transform shortcuts are disabled while the modal popup is open.
  Live-update tests change the document through its API; they do not claim that
  Ctrl+R or R can be used inside the popup.
