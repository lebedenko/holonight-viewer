# Verification — 2026-09-10

Scope: the approved installation/maintenance plan. Evidence is local under
`build/system-maintenance/`; generated harnesses and disposable fixtures stay there.
Existing installed development and `/usr` providers were reused; no sibling sources
were modified. No host installation was performed.

| Requirement | Evidence | Outcome |
| --- | --- | --- |
| R1 | `run-check.log`, `run-version.log`, `run-window.log` | Release preset forwards `--version`; debug opens a Unicode/spaced filename and remains alive until deliberate termination. Isolated `XDG_DATA_HOME` contains no desktop entry or icon. |
| R2 | `system-build.log`, `system-stage.log` | Exact unprivileged install configure/build commands pass against `/usr`, in `build/system-install`. Cache and Ninja inspection confirm system provider discovery and no local provider build paths. Repeating configure/build leaves hashes of all three development caches unchanged. |
| R2 | `install-failure-fixed.jsonl`, `install-failure-fixed.log`, `install-order.jsonl` | Mocked command execution verifies explicit system environment despite conflicting inherited environment/Task variables; build failure stops before sudo. Successful mock orders configure, build, sudo install, then sudo desktop database refresh. |
| R3 | `system-stage.log` | DESTDIR installation contains executable, desktop entry, SVG and both license files; desktop-file-validate and staged update-desktop-database pass. Installed GIO opening passes PNG/JPEG/BMP/WebP with `/usr` provider paths. |
| R4 | `task-list.log`, `verify-dry.log`, `interface.log` | Public commands have descriptions; verify aliases check; no desktop-* commands. Full check result recorded below. |
| R5 | `interface.log`, `clean-fixture/` | Actual `task clean` using a copied Taskfile removes precisely four disposable Viewer build directories while retaining provider/evidence sentinels. Real local provider installation remains present. |
| R6 | CI and documentation diff review | CI uses task deps, task check, then existing separate installed-only Docker qualification. README and contributor instructions describe system prerequisites and maintenance commands. |

The initial `task check` passed builds, tests, format checking and lint, then failed
at REUSE because sandbox restrictions prohibit its Python multiprocessing socket
(`check.log`). An approved unsandboxed rerun is recorded in `check-unsandboxed.log`;
it passed end to end (exit 0): all seven CTest tests, format checks, C++ tidy,
QML lint, REUSE, staged payload/runtime checks and four-format installed GIO launch.

Verification found that Task's task-level environment did not override inherited
development variables. The final install recipe explicitly exports system paths
before its sequential commands. The failed pre-fix mock remains in
`install-failure.jsonl` for traceability. A staging harness initially assumed direct
provider library linkage; Viewer consumes provider QML modules, so the final
assertion verifies the resolved system CMake package and absence of local provider
paths. A launch harness initially counted Task's expected termination message as
an application failure; final checking excludes Task diagnostic lines.

Host `sudo cmake --install` and host desktop database refresh were **not executed**.
Staging and mock command-order evidence do not establish host installation.
Visual inspection and Docker qualification were not repeated for this task-only
change; they remain separate workflows. Historical acceptance records are unchanged.
Spark was assigned only Taskfile.yml, but its turn failed at the model usage limit
before editing; the main agent completed implementation and integration locally.
