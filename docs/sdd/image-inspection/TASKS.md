# Stage 2 traceable tasks

Status: requirements, design and tasks approved by the user on 2026-09-08.
Implementation and local verification complete; native mixed-scale display
acceptance and earlier stages’ pending acceptance remain open. See
[verification](VERIFICATION.md).

| Task | Requirements | Work and acceptance evidence | Status |
| --- | --- | --- | --- |
| T1 Establish baseline | VIEW-15 | Run task deps, task build and task test using installed providers; record baseline results under build/ | Complete: local checks passed |
| T2 Implement view geometry | VIEW-01–11 | Add Fit/manual state, physical scale, source center, anchored zoom, pan clamps, finite bounds and zero-size handling; numeric tests for wide/tall/tiny/large images, round trips, edges and ratio changes | Complete: local checks passed |
| T3 Integrate canvas rendering | VIEW-01–04, VIEW-08–11, VIEW-14 | Connect geometry, image resets, canvas resize and window pixel ratio; draw visible source region with bounded backing storage; verify pixel grid/alpha rendering and absence of source reloads | Complete: local checks passed |
| T4 Add controls and input | VIEW-02, VIEW-04–07, VIEW-12–13, VIEW-15 | Styled controls/status, shortcuts, wheel/trackpad deltas, drag cancellation, keyboard focus and modal guards; real window input tests including letterbox anchors, edge dragging, repeat and focus traversal | Complete: local checks passed |
| T5 Cover lifecycle regressions | VIEW-01, VIEW-03, VIEW-09–12, VIEW-15 | Test resize/fullscreen and ratio transitions in Fit/manual modes, replacement/error reset, dialog suppression/cancel preservation, Open/drop/CLI and unchanged original files | Complete: local checks passed |
| T6 Verify visual and resource behavior | VIEW-03–04, VIEW-07–10, VIEW-13–14 | Extend and inspect dark/light captures at 100%/125%/150%, minimum/normal sizes, Fit/Actual Size/panned/max zoom; measure large-image timing, event-loop progress and peak RSS; record native cross-display result or explicit pending check | Automated and live checks passed; native mixed-scale display check pending |
| T7 Run quality and installation gates | All | Debug/test/release, formatting, tidy, QML lint, licensing, staged installation/opening and desktop registration checks; retain exact commands/results | Complete: local checks passed |
| T8 Close cycle documentation | All | Update SPEC/DESIGN/TASKS, write VERIFICATION, update README/backlog; distinguish local implementation from full acceptance and retain previous pending checks | Complete: local checks passed |

Implement T2 after baseline T1, then T3 and T4. Add focused tests with each
behavior rather than deferring them to the end. T5–T7 provide acceptance evidence
before T8 records the final status. Design/task approval authorizes this scope;
routine implementation details need no repeated approval. Requirement changes
return to the relevant SDD approval step.

Required commands: `task deps`, `task build`, `task test`,
`task build PRESET=release`, `task format`, `task format-check`, `task tidy`,
`task qml-lint`, `task license-check`, `task install-check`, `task desktop-check`,
`task visual-check`, and `git diff --check`. Save generated fixtures, captures,
logs and measurements under build/. Record unavailable native desktop checks as
pending; automated input/scale tests do not close those gates.
