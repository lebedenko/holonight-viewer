# Verification record

Verification on 2026-09-14:

| Requirements | Check | Outcome |
| --- | --- | --- |
| R1–R4 | Focused `viewer-smoke` keyboard, accessibility, browsing, and workflow tests | Passed. Exact Tab/Shift+Tab wrapping and disabled Information skipping; menu J/K/Up/Down skipping and activation; window-wide pan; modal suppression; pointer zoom/drag; no canvas tab stop or outline. |
| R1–R3 | `task visual-check` | Passed 8 test runs in dark/light themes at 1, 1.25, 1.5, and 2 scale. Focus ring pixels were checked against palette colors. Manual inspection of dark and light 1× captures confirmed focused Fullscreen rings and ring removal after image action. Captures are in `build/visual/`. |
| R1–R5 | `task tidy` | Passed after correcting a braces diagnostic in the visual test. |
| R1–R5 | `task check` | Passed the full gate after the visual-test braces correction: debug/release builds, all 8 CTest suites, formatting, C++ tidy, QML lint, REUSE license check, staged install and four-format desktop launch, and staged uninstall. The license check required an escalated run because Python's fork server could not bind inside the sandbox. |

Spark owned the isolated J/K menu change and corresponding workflow test additions. The main agent integrated the window router, focus rules, documentation, accessibility tests, and verification. No sibling sources were edited.

Staged verification evidence: `build/install-check.qITwza`, `build/opening-check.vfkl54sg`, `build/installed-desktop.wikfqq6m`, and `build/uninstall-check.x2hr4h8a`. Visual captures are under `build/visual/`. R1–R5 and T1–T5 are complete.
