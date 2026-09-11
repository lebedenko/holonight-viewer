# Traceable tasks

Approved through the user-supplied implementation plan on 2026-09-11.

- [x] T1 (R1–R5): Update `apps/viewer/Main.qml` shortcuts (`[`, `]`, `Ctrl+R`, `Ctrl++`/`Ctrl+=`, `Ctrl+-`, `Ctrl+0`, `?`).
- [x] T2 (R6): Update UI presentation strings in `apps/viewer/Main.qml` (Help text dialog, bottom caption strip) and `README.md`.
- [x] T3 (R1–R5): Update automated test suites (`tests/accessibility_test.cpp`, `tests/image_inspection_test.cpp`, `tests/folder_browsing_test.cpp`, `tests/static_workflow_test.cpp`) to verify the new key mappings.
- [x] T4 (R1–R6): Run full verification pipeline (`task check` / `task verify` including debug/release builds, tests, formatting, tidy, qml-lint, license, install/uninstall check).
- [x] T5 (R1–R6): Record verification evidence in `docs/sdd/shortcut-remapping/VERIFICATION.md` and update `docs/BACKLOG.md`.

Local automated acceptance passed on 2026-09-11. See [verification](VERIFICATION.md) for full records.
