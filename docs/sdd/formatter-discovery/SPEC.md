# Formatter discovery

Approved implementation plan, 2026-09-23. Viewer baseline:
`6ed3a3aa471b5c7c585c40f495b2849d83679f2c` (initially clean).

- R1: Match Files: explicit `QMLFORMAT`, then `qtpaths6` queries in order
  `QT_INSTALL_BINS`, `QT_HOST_BINS`, `QT_INSTALL_LIBEXECS`, `QT_HOST_LIBEXECS`,
  then `/usr/lib/qt6/bin/qmlformat`, then PATH `qmlformat-qt6`, then `qmlformat`.
- R2: Preserve override validation (invalid overrides never fall back), paths
  containing spaces, argument forwarding and formatter exit codes. Missing tools
  and failed directory queries fall through. Add no version probes or dependencies.
- R3: Update README and isolated fake-executable regressions. Run discovery first,
  shell syntax, format-check with QMLFORMAT unset, and full task check using
  compatible builds. Review logs and diff. Automated-only; no fresh clean build,
  isolated-runtime rerun or native qualification required for this script change.
- R4: The initial handoff remains local and uncommitted until requested. The user
  subsequently authorized commit and pin on 2026-09-23, including publication of
  the Viewer commit before pinning. Application/public APIs, Files, providers and
  existing native deferrals remain unchanged.
