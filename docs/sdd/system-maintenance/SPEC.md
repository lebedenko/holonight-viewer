# System installation and maintenance requirements

Approved by the supplied implementation plan on 2026-09-10.

- R1: When `task run` is invoked, Viewer shall build the selected preset and
  forward CLI arguments without registering desktop files in user data directories.
- R2: When `task install` is invoked, Viewer shall configure and build Release in
  `build/system-install` against installed `/usr` providers and `/usr/lib/qt6/qml`,
  overriding development provider settings, then install to `/usr` with sudo and
  refresh its desktop database. Build failure shall prevent installation.
- R3: The installed package shall retain its executable, desktop metadata, MIME
  declarations, icon, licenses, and installed desktop-launch verification.
- R4: The task interface shall expose described public tasks, combined `lint`, and
  sequential `check` (alias `verify`): debug, release, tests, format check, lint,
  license check, staged install check. Individual checks shall remain available.
- R5: When `task clean` is invoked, it shall remove only Viewer debug, release,
  test, and system-install build directories, preserving provider installations.
- R6: CI shall run `task deps`, `task check`, and separate isolated runtime
  qualification. Documentation shall explain the task interface and prerequisites.

Non-goals: application/API changes, sibling source changes, automatic provider
system installation, visual or Docker qualification inside `check`. Host install
is distinct from staging and must not be claimed without execution. Historical
verification records remain unchanged.
