# Verification — 2026-09-10

Scope: approved executable rename and `/usr` uninstall, using existing installed
providers in `build/deps/prefix`. No sibling sources changed. No host installation
or uninstall performed. Generated evidence remains under `build/`.

- R1–R2: `task check` — passed with sandbox approval (exit 0); full log:
  `build/executable-uninstall-check-approved.log`. The initial sandboxed run
  passed all seven CTest tests, formatting and C++/QML lint, then REUSE failed
  on a denied local socket (`build/executable-uninstall-check.log`). Staged installation checks require
  `usr/bin/hn-viewer`, reject the legacy binary, validate desktop metadata and
  exercise desktop launching and file opening. Internal CMake target references
  resolve the renamed output; application identity code is unchanged. All seven
  tests, formatting, C++/QML lint, REUSE, four-format GIO launch and uninstall
  regression checks passed. Stage: `build/install-check.phXKET`.
- R2: `QT_QPA_PLATFORM=offscreen task run -- --version` passed and reports
  `holonight-viewer 0.1.0` (the retained application identity) (`build/executable-uninstall-run-offscreen.log`). An initial
  launch without offscreen exited 134 in this environment; native GUI launch is
  not established by this check (`build/executable-uninstall-run.log`).
- R3–R6: `python3 scripts/check-uninstall.py` passed
  (`build/executable-uninstall-focused.log`). Disposable trees cover both names
  together, current-only and legacy-only installations, missing files/directories,
  repeated calls, empty/nonempty license directories, spaces in DESTDIR, preserved
  providers/user settings/unrelated files and no configuration or manifest.
  Real nonrecursive removal rejects a directory at an executable path.
- R5: The same regression mocks sudo/removal/rmdir/database commands. It verifies
  sudo → removal → empty-directory handling → refresh ordering, stops after sudo,
  removal or directory-removal failures, and propagates refresh failure through
  both helper and real `task uninstall`. Fake sudo checks the exact command,
  including clearing inherited DESTDIR, before redirecting to the disposable tree.
- `bash -n scripts/uninstall.sh scripts/check-install.sh scripts/isolated-runtime.sh`
  and task discovery passed. The runtime audit explicitly selects `hn-viewer`
  alongside HoloNight libraries; qualification-tool paths retain their identity.
  The actual audit loop was also exercised with a disposable `hn-viewer` fixture
  and mocked readelf: a development RUNPATH failed (exit 1), `/usr/lib` passed
  (exit 0). See `build/executable-uninstall-audit.log`.

- R2–R6: The successful staged install was subsequently uninstalled twice with
  the real desktop-database command; both invocations passed. Its executable had
  only the relative RUNPATH `$ORIGIN:$ORIGIN/../lib`, with no development paths. Empty license directory removal and payload absence
  passed (`build/executable-uninstall-staged-removal.log`).

Limitations: privileged execution and native menu interaction are mocked/offscreen;
no host sudo removal or Docker installed-runtime qualification is claimed. An absent
applications directory skips database refresh so a wholly absent install is harmless.
Custom-prefix uninstall is outside scope. Historical evidence remains unchanged.
