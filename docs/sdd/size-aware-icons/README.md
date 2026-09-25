# Explicit shared icon rendering in Viewer

Baseline: `525da550db0628827191d2970f39a9c7d06d2b34`.

The empty-state glyph is a bundled SVG. Select semantic rendering explicitly and verify Viewer QML and runtime acceptance against the accepted `holonight-qt` revision.

Implementation: the empty-state QML caller. Local verification (2026-09-25): `task lint`, `task qmltypes-check`, `task format-check`, `task qml-import-check`, `task license-check`, and `task install-check` passed. The full CTest set passed (24) outside the sandbox for D-Bus access against the local provider build.
