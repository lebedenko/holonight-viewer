# Review fixes design

Approved scope: supplied four-change plan. Implement in order: path resolution, header lint, failure reporting, formatting portability.

R1 uses QFileInfo existence before QUrl StrictMode. R2 fixes the header regex and public camelCase naming, adds nodiscard accessors and explicit null comparisons, and documents declaration-local Qt exceptions. Tidy retains its explicit translation-unit list. R3 introduces an event signal at validation and accepted decode failures, replacing stderr's state-notification subscription. Existing generation/shutdown guards reject obsolete results. R4 separates formatting and tidy lists in CMake and shares a shell formatter wrapper between Task and QML check.

Tests extend real CLI processes and injected-decoder signal checks; formatter discovery uses disposable fixtures. Installed providers are reused and evidence stays under build/review-fixes. No visual behavior changes require new desktop acceptance.

Review boundaries follow the four ordered tasks:

1. `commandLineUrl` and the CLI path matrices implement R1.
2. `.clang-tidy`, accessor annotations, `WindowState` null comparison, and the
   internal request-field spelling implement R2. Public fields use camelCase;
   private data retains lower_case with a trailing underscore. Enum and QObject
   exceptions are declaration-local, with no globally disabled checks added.
3. `openingFailed`, its two emission sites and the main stderr connection
   implement R3. A decoder/scanner constructor overload allows the tests to hold
   directory completion until after a failure without changing production workers.
   Validation still clears the document; decoder completion still checks request
   generation, prefetch status and shutdown before reporting.
4. CMake's separate formatting list, `scripts/qml-format.sh`, its check/discovery
   scripts and Taskfile implement R4. QMLFORMAT is one executable, not a shell
   command string. The wrapper forwards all formatter arguments without evaluation.

The discovery regression relocates only the fixed Arch fallback in a disposable
copy, so an installed formatter cannot mask missing-tool failures. It tests the
production check script directly for formatter failure propagation. CTest includes
this fixture regression in the normal check sequence.
