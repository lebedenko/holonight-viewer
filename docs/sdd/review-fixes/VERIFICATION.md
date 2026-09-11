# Review fixes verification

Approved plan implemented and local acceptance passed on 2026-09-11. Logs live under `build/review-fixes/`.
The pre-existing untracked `docs/app-review-2026-09-11.md` was preserved.

| Requirements | Command / coverage | Outcome |
| --- | --- | --- |
| R1, R3 | `task test` (`test.log`): relative colon filenames, explicit relative/absolute paths, Unicode/spaces, file URLs, missing colon paths, remote/host/query/fragment rejection; exactly one stderr report for each failed startup opening | Passed, 7 CTest entries before registration of the formatter fixture test |
| R2 | `cmake --build build/test --target tidy` (`tidy-final.log`) | Passed all 19 application/test translation units with application headers and warnings-as-errors enabled |
| R4 | `task format-check` (`format-task.log`) | Passed C++ and QML |
| R4 | `cmake --build build/test --target format-check` (`format-cmake.log`) | Passed C++ translation units and application headers |
| R4 | `python3 scripts/check-qml-format-discovery.py` (`formatter-discovery.log`) | Passed explicit override (including spaces and command names), invalid/empty/non-executable overrides, PATH precedence, qtpaths6 discovery, Arch fallback, missing tool and exit-code propagation through wrapper and check script |
| R4 | `QMLFORMAT=/usr/lib/qt6/bin/qmlformat task format-check` (`format-override.log`) | Passed using the explicit installed formatter |
| R4 | `bash -n scripts/qml-format.sh scripts/check-qml-format.sh` | Passed |
| R1–R4 | `task check` (`check.log`) | Builds, all 8 CTest entries, formatting and lint passed; sandbox denied REUSE multiprocessing socket at license-check |
| R1–R4 | Unsandboxed `task check` (`check-unsandboxed.log`) | Passed (exit 0): debug/release builds, all 8 CTest entries, Task formatting, all 19 tidy translation units, QML lint, REUSE licensing, staged install/installed CLI and four-format GIO launch, and disposable uninstall checks |

The scanner-injection test deterministically releases a directory result only
after a decode failure, confirms an additional state notification and no additional
failure event, then verifies repeated open, refresh and repeated invalid requests.
Existing stale-result and shutdown tests now inspect failure signals: a stale
error cannot add a report, and a canceled decoder returning an error stays silent.
Successful transforms and a failed speculative decode are also exercised.

The first header-enabled tidy run exposed the enum checks and override-accessor
annotations beyond the original single-translation-unit diagnostic sample; those
were fixed before the passing full target run. Declaration-local suppressions
preserve QObject identity semantics and the existing Qt-facing enum types and
QML-invokable method; no new checks are disabled globally.

Installed providers under `build/deps/prefix` were reused. `task deps` was not run
because it rebuilds sibling providers and this scope explicitly requires installed
providers. No sibling sources were modified. This host discovers the formatter via
the existing Arch fallback; PATH and qtpaths6 discovery are fixture-tested, not
claims of additional real Qt installations. Temporary formatter fixtures/output
stay under `build/` and are removed after use.

No host installation/removal, visual or Docker qualification is claimed. These
maintenance changes do not change presentation; existing release deferrals remain.
Spark delegation was attempted for formatter scripts/Taskfile but the service
reported its usage limit before edits. The main agent completed that isolated work
under the approved fallback.

Final staged evidence: `build/install-check.OIbZec`,
`build/opening-check.3ue5de_p`, `build/installed-desktop.ir8i0_8u`, and
`build/uninstall-check.11k444ax`. These are disposable staged checks, not a host
installation. The full check was rerun unchanged outside the sandbox after its
local socket restriction blocked REUSE. Recoverable local Ninja log warnings
appear in intermediate logs; all final acceptance commands completed successfully.
`git diff --check` also passed. Requirements R1–R4 and tasks T1–T5 are complete.
