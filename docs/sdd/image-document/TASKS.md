# Stage 1 traceable tasks

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native desktop acceptance and
stage 0 acceptance remain open.

| Task | Requirements | Work and evidence | Status |
| --- | --- | --- | --- |
| T1 Document and input contract | IMG-01–04, IMG-07–08, IMG-11–12 | Typed state, URL validation, CLI adapter, translated errors; empty/syntax/recovery/path tests | Complete: local checks passed |
| T2 Bounded asynchronous decoder | IMG-04–05, IMG-07–10 | One worker, newest pending request, request IDs, reader limits, orientation/first-frame decoding; controlled scheduling and real fixture tests | Complete: local checks passed |
| T3 Lifetime and shutdown | IMG-04, IMG-08–09 | Cooperative cancellation, responsive closing, safe thread ownership; stale completion and in-flight shutdown tests with documented exit limitation | Complete: local checks passed |
| T4 Canvas and opening UI | IMG-01–07, IMG-11–12 | Painted image fit, themed state feedback, accessible Open/Ctrl+O, FileDialog and DropArea adapters, modal shortcut guards | Complete: local checks passed |
| T5 Regression and resource coverage | IMG-02–12 | Actual adapters, EXIF rotations/reflections, alpha, first frame, large/resource rejection, errors, Unicode/spaces, unchanged source hashes; burst responsiveness and peak-memory measurements | Complete: local checks passed |
| T6 Build and installed runtime | IMG-02, IMG-07, IMG-10–12 | Shared internal target, format/tidy lists, QML imports, CLI tests; installed PNG/JPEG opening with installed providers only | Complete: local checks passed |
| T7 Visual and keyboard acceptance | IMG-01, IMG-04–07, IMG-11 | Dark/light, 100%/125%/150%, minimum size, resize/fullscreen, Open/cancel/error recovery, live dialog/drop and keyboard checks | Automated and live fallback-dialog checks passed; native portal and external file-manager drop pending |
| T8 Close cycle documentation | All | SPEC/DESIGN/TASKS/VERIFICATION, README usage and limits, backlog status; retain every pending acceptance check | Complete: local checks passed |

Implement T1–T3 before integrating T4; build focused regression coverage alongside
the behavior it verifies. T5–T7 form the acceptance gate, followed by T8. No extra
approval is needed for routine implementation choices within the approved design.
Requirement changes return to the relevant approval step.

Required commands: `task deps`, `task build`, `task test`,
`task build PRESET=release`, `task format`, `task format-check`, `task tidy`,
`task qml-lint`, `task license-check`, `task install-check`, `task desktop-check`,
`task visual-check`, and `git diff --check`. Extend meaningful checks to image
opening before claiming stage 1 acceptance. Keep generated fixtures, captures and
measurement logs under build/ and record reproducible commands/results.

This task set and DESIGN.md were approved, including the 128 MiB per-image policy
and cooperative shutdown limitation. See VERIFICATION.md for commands, fixture
coverage, memory measurements and remaining manual acceptance. Stage 0 remains
open independently; this work does not close its CI/compositor/checkout gates.
