# Review fixes requirements

Approved through the supplied implementation plan on 2026-09-11.

- R1: When a CLI argument names an existing local path, Viewer shall convert it to an absolute local-file URL before strict URL parsing. Otherwise strict parsing and local-file validation shall remain unchanged. A missing `frame:1.png` receives URL rejection; `./frame:1.png` receives the missing-file error.
- R2: When normal tidy runs, it shall cover application headers and explicit application/test translation units, exclude generated sources, and retain warnings-as-errors. Qt-facing names/types shall remain stable, with narrow explained suppressions.
- R3: When an opening request is accepted as failed (including invalid requests), ImageDocument shall emit one openingFailed(fileName, error) signal. Stale/canceled work, directory updates and transform resets shall not emit it. Each repeated opening/refresh failure shall report again. CLI validation and stderr message text shall remain unchanged.
- R4: When formatting runs, Task and CMake shall include application headers. Task formatting and QML checks shall share discovery: explicit QMLFORMAT, PATH tools, qtpaths6 directories, Arch fallback. Invalid overrides and missing tools shall fail clearly; formatter failures shall propagate, paths shall be quoted and temporary output shall stay under build/.

Non-goals: drag behavior, arrow visibility, uninstall scope, thread architecture, performance, localization, build parallelism, provider source changes.

Local acceptance passed; see [verification](VERIFICATION.md) for commands, evidence and limitations.
