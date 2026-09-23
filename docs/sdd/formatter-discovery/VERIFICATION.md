# Verification — 2026-09-23

Viewer baseline: `6ed3a3aa471b5c7c585c40f495b2849d83679f2c`, plus local changes.

## Focused checks

- `python3 scripts/check-qml-format-discovery.py`: the extended regression failed
  before the fix (`path-default` selected instead of `path-qt6`); passed afterward.
  Covers all Qt directory priorities against competing PATH tools, fixed fallback,
  failed/empty queries, missing/non-executable candidates, PATH-only preference,
  explicit override validation and precedence, paths with spaces, argument count
  and boundaries, and exit 23 propagation through both formatter wrappers.
- `bash -n scripts/qml-format.sh scripts/check-qml-format.sh`: passed.
- `env -u QMLFORMAT task format-check`: passed. On this host `qtpaths6` is absent;
  the fixed Qt fallback is available ahead of the unrelated PATH formatter.

## Build reuse and acceptance

Reused project-local builds and provider prefix. Provider source trees are clean
and revisions match the prior shared-image-outcomes acceptance record:

- Config `fe69a59e6b73167fd5349223a4d265d75386c139`.
- Qt `863af4183bdf09ce05199b37e8f5dfb46a311ba1`.
- Images `3633865d2f39e4f163f0159a0f252f88245379f0`.

CMake caches and installed tools confirm GCC 16.2.1, Qt 6.11.2, Release providers,
provider tests disabled and Qt provider Wayland disabled, matching that record.
Viewer uses the existing Debug/Release/test presets and local provider/QML paths.

The first `env -u QMLFORMAT task check` built successfully but CTest was blocked
by the sandbox denying private D-Bus socket binding (12 affected entries). The
same command was rerun with approval outside the sandbox. All 22 CTest entries
passed; main smoke has 188 passed and eight existing opt-in skips.

The complete rerun exited 0 with `QMLFORMAT` unset: incremental Debug/Release/test
builds, formatting, clang-tidy (39 files), QML lint, REUSE (250/250 files), staged
installation including CLI and GIO launch, import policy and generated type metadata
all passed. Complete command logs reviewed: no actionable build/lint warnings;
clang-tidy reports only suppressed non-user-code and existing NOLINT diagnostics.
Final diff/SDD review, `git diff --check` and local SDD link validation passed.
Logs: `build/verification/formatter-discovery/task-check.log` and
`build/verification/formatter-discovery/task-check-unsandboxed.log` (ignored).

No fresh clean application build, isolated-runtime rerun or native qualification
was required by this script-only plan. Existing native deferrals remain unchanged.
Initial verified handoff was local and uncommitted. The user subsequently
authorized commit and pin on 2026-09-23; the umbrella checkpoint records the
published Viewer revision and the single CI status check.
