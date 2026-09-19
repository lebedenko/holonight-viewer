# Contributing

Use the same specification-driven development sequence as the sibling projects:

1. Clarify intent, scope, non-goals, and unresolved questions.
2. Write EARS requirements in docs/sdd/<cycle>/SPEC.md and obtain approval.
3. Write DESIGN.md and traceable TASKS.md; obtain design and task approval.
4. Implement the approved work in small, reviewable changes.
5. Verify requirements with focused tests, build and quality checks, and visual
   inspection where applicable. Record commands, outcomes, and limitations.
6. Update the specification, design, task status, README, and backlog. Mark work
   complete only when its acceptance checks pass; retain pending checks explicitly.

An explicitly approved implementation plan authorizes its specified scope;
routine implementation choices do not need repeated approval. Scope changes that
alter requirements return to the relevant approval step.

Start with `task deps`, then `task build` and `task run` (optional `PRESET=release`
and arguments after `--`). Run `task check` (alias `task verify`) before submitting:
it sequentially builds debug and release, runs tests, checks formatting, runs
`task lint` (C++ tidy and QML lint), checks licenses, verifies staged installation,
and checks QML import policy and type metadata.
Individual `test`, `format-check`, `tidy`, `qml-lint`, `license-check`,
`install-check`, `qml-import-check`, and `qmltypes-check` commands remain available. Use `task format`
to format sources and `task --list` to discover commands. Visual inspection (`task visual-check`) and
Docker installed-runtime qualification remain separate from `check`.

`task run` does not register development desktop files. Use `task stage DESTDIR=...`
to inspect a payload without changing the host. Coordinated system installation and
removal belong to the umbrella; standalone CMake installation and legacy ownership
review are documented in README.md. The executable remains `hn-viewer`.
`task clean` removes only Viewer debug, release, test and system-install build
directories; it preserves `build/deps` and verification evidence. Record staged
verification separately from any actual host installation.

Use C++23, Qt 6.11+, shared HoloNight controls and tokens, translated UI strings,
and native decorations. Add GPL-3.0-or-later metadata for new material and preserve
third-party attribution. Do not include build output or local dependency installs.
