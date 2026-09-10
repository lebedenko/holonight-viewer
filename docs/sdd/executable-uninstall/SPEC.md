# Executable naming and uninstall requirements

Approved through the supplied implementation plan on 2026-09-10.

- R1: When Viewer is built or installed, its executable shall be named `hn-viewer`,
  while its internal target, application identity, settings paths, desktop ID,
  license directory and qualification-tool paths shall remain unchanged.
- R2: When development or desktop launching and installation/runtime checks run,
  they shall use `hn-viewer`, including the runtime-path audit, without an alias.
- R3: When `task uninstall` runs, it shall invoke a helper through sudo targeting
  `/usr`, without configuration or an install manifest, removing only both Viewer
  executable names, its desktop entry, icon and two installed license files.
- R4: When payload files are absent, uninstall shall succeed; when the license
  directory is empty, it shall remove it, preserving nonempty directories,
  providers, user data and unrelated files.
- R5: When removal succeeds, uninstall shall refresh the desktop database;
  if sudo, removal or refresh fails, the failure shall propagate and later steps
  shall not run.
- R6: When DESTDIR is supplied to the helper, it shall operate on that staged
  `/usr` tree. Verification shall use disposable trees under `build/`, never host
  installation/removal. Documentation shall explain uninstall-before-reinstall.

Non-goals: custom-prefix uninstall, provider changes, compatibility aliases,
application behavior changes and rewriting historical release/verification records.
