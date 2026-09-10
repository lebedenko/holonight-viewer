# Publication verification

Approved scope: [SPEC.md](SPEC.md). Historical native and performance acceptance:
[release-readiness verification](../release-readiness/VERIFICATION.md), unchanged.
This publication cycle changes documentation only.

Starting PR #1 head: 115f377321d5d2729c9ed6d6512c82170e931409, draft, both hosted
workflows successful. Existing provider checkouts match CI pins: Config
fe69a59e6b73167fd5349223a4d265d75386c139 and Qt
22ded7815727ce483fd91e82a9cc04bfe252ec3b.

Publication gates are pending in this prepublication snapshot. Exact final-head,
main and tag CI results, archive extraction/build/test/install checks, archive
SHA-256 and tag commit will be recorded in the completion report and
[PR #1](https://github.com/lebedenko/holonight-viewer/pull/1).
The [release page](https://github.com/lebedenko/holonight-viewer/releases/tag/v0.1.0)
is authoritative for publication status and downloadable assets. Local generated
logs, archive, extraction and downloaded-asset verification stay under
build/release/. No final publication success is claimed by this source snapshot.

## Prepublication local checks (2026-09-10)

Passed with Qt 6.11.2 and the pinned providers installed under
build/qualification/clean/build/deps/prefix: task deps (isolated qualification
checkout), task build, task test (7/7), task build PRESET=release,
task format-check, task tidy, task qml-lint, task license-check (108/108 files),
and task install-check (version 0.1.0, four formats, desktop launcher and payload).
Local Markdown links and git diff --check also passed. Logs are under
build/release/publication/.

The initial dependency bootstrap encountered an existing CMake cache referencing
the sibling source path; rebuilding in the existing isolated qualification checkout
passed without changing provider sources. REUSE initially hit the sandbox's local
multiprocessing socket restriction; the authorized rerun passed. Spark delegation
was attempted for README/backlog, but its usage limit prevented execution; main
completed those edits. No production changes or native remeasurement were needed.
