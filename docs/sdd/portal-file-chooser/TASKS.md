<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com> -->

# SDD Tasks — portal-file-chooser

- [x] T-001: Build plumbing — CMake, languages, wayland-scanner
  - REQs: REQ-NF-001, REQ-C-001, REQ-C-003
  - Check: The project preset configures and builds with `LANGUAGES C CXX`, Qt6 DBus/GuiPrivate and REQUIRED wayland-client/wayland-protocols/wayland-scanner, and `xdg-foreign-unstable-v2-client-protocol.h` plus its `.c` are generated under build/ and compiled into viewer-ui

- [x] T-002: portal_request_builder pure functions and unit tests
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-009
  - Check: `portal_request_builder_test.cpp` compiles and passes; filter parsing, current_folder NUL-termination, handle tokens, and parent_window formatting and sender sanitizing/path prediction verified, including empty-input edge cases

- [x] T-003: Test infrastructure — dbus-run-session wrapper, mock_portal, fail-fast guard
  - REQs: REQ-NF-006, REQ-NF-008
  - Check: `viewer-smoke` runs under `dbus-run-session` via ctest with all existing tests passing; a sanity test shows the mock (on its own named connection) receives an OpenFile call from the default connection; running the binary with the portal name already owned fails with an explicit message

- [x] T-004: PortalFileChooser core — async OpenFile, pre-subscription, response handling, state machine
  - REQs: REQ-F-001, REQ-F-007, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-020, REQ-F-022, REQ-F-023, REQ-F-024, REQ-NF-003, REQ-NF-005
  - Check: Object-level tests in `portal_file_chooser_test.cpp` (PortalFileChooser without Main.qml) pass: exactly one OpenFile call to `org.freedesktop.portal.Desktop` `/org/freedesktop/portal/desktop` `org.freedesktop.portal.FileChooser` with modal/multiple/filters/current_filter/current_folder options, a Response emitted before the method reply is still received, differing-handle re-subscribe, stale Response ignored, code 0 emits `finished(urls)`, code 1 emits `cancelled()`, and a second `requestOpen()` while outstanding issues no call

- [x] T-005: Fallback on code 2 response and call failure
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018
  - Revised 2026-09-15 (T-013 finding): code 2 is now a cancellation per revised REQ-F-015; the fallback remains for call failures.
  - Check: code-2 Response is handled as cancellation without openDialog; missing portal service triggers fallback; injected timeout test with shortened duration triggers fallback; FileDialog objectName="openDialog" preserved

- [x] T-006: Main.qml integration and primary Open trigger wiring
  - REQs: REQ-F-002, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-020, REQ-F-024, REQ-C-004, REQ-C-005
  - Check: With the mock portal, Main.qml tests show Ctrl+O issues OpenFile with no `openDialog` object, `modalActive` stays true until Response, code 0 with a fixture URI reaches `Ready`, a non-file URI reaches `Error` without `openDialog`, code 1 leaves image/zoom unchanged and re-enables shortcuts; the unchanged smoke FileDialog test passes with no portal on the bus

- [x] T-007: Window destruction and Request.Close handling
  - REQs: REQ-F-021
  - Check: `portal_file_chooser_test.cpp` window-close test asserts `Request.Close` call recorded to mock portal; late Response after window destroyed is ignored

- [x] T-008: wayland_foreign_export — WaylandForeignExporter and per-request export
  - REQs: REQ-F-008, REQ-F-009, REQ-NF-002, REQ-NF-007
  - Check: fresh export created per OpenFile call; export destroyed on request end; offscreen platform test passes with empty parent_window; no warnings emitted; WAYLAND_DEBUG=1 shows export/destroy (manual REQ-NF-011)

- [x] T-009: Portal availability probing and comprehensive integration tests
  - REQs: REQ-NF-006, REQ-NF-008, REQ-F-019
  - Check: `portal_file_chooser_test.cpp` passes all scenarios: Unicode/space/percent-encoded fixture opens correctly; current_folder sent when document open, omitted when no document; code 2 → cancellation without fallback; missing service → fallback exactly once; non-file URI → error message, no dialog; repeated Open ignored while outstanding; window close during AwaitingResponse → Close recorded, late Response ignored; offscreen platform → empty parent_window; second Open after service stopped uses fallback; smoke test passes

- [x] T-010: CI/packaging dependencies and README
  - REQs: REQ-NF-001
  - Check: `packaging/Dockerfile.ci` lists wayland-protocols and wayland-scanner; README lists wayland-client, wayland-protocols, wayland-scanner, Qt DBus; CI build succeeds

- [x] T-011: Lint pass — format, tidy, qml-lint, no X11/XCB references
  - REQs: REQ-NF-004, REQ-C-001, REQ-C-002
  - Check: `task format-check` passes; `task tidy` passes; `git grep -iE 'x11|xcb'` finds no new matches in apps/viewer/ and tests/; diff touches only apps/, tests/, docs/, packaging/, CMake, README

- [x] T-012: Documentation — handoff doc update and VERIFICATION template
  - REQs: REQ-NF-010, REQ-NF-011 (records)
  - Check: `docs/holonight-qt-dlg-delegation-missed.md` updated with portal-as-primary note, xdg-foreign parent association, fallback-only-on-error behavior; `docs/sdd/release-readiness/VERIFICATION.md` gains an N1 portal section with an empty checklist for Hyprland/backend versions

- [x] T-013: Manual Hyprland acceptance and xdg-foreign verification
  - REQs: REQ-NF-010, REQ-NF-011
  - Check: Verified on real Hyprland with native FileChooser backend: picker window parented to Viewer; Unicode/space filenames open correctly; Escape cancels without changing image/zoom/focus; repeated Opens launch new pickers each time; Viewer quit while picker open closes picker; fallback appears when portal stopped; `WAYLAND_DEBUG=1` logs show export_toplevel and destroy per request; results recorded in VERIFICATION.md with Qt/Hyprland/portal/backend versions

- [x] T-014: Review fixes — cancellation focus and window destruction
  - REQs: REQ-F-014, REQ-F-015, REQ-F-021, REQ-F-022
  - Check: cancelling with codes 1 and 2 restores a previously focused visible control;
    destroying the assigned window while the chooser survives closes the request
    in Calling and AwaitingResponse and ignores late responses; destruction of a
    replaced window does not cancel the current request. Run `task check`.
  - Evidence: 19/19 portal tests and full `task check` passed on 2026-09-16
    (18/18 CTest entries); see release-readiness/VERIFICATION.md.
