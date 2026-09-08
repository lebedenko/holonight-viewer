# Folder browsing tasks

Approved through the supplied implementation plan, in the CONTRIBUTING.md sequence.

- [x] Record approved requirements, design and tasks before implementation.
- [x] Implement asynchronous DirectoryModel and focused tests (01–03, 07).
- [x] Implement selection, refresh, errors, LRU and prefetch scheduling (04–10).
- [x] Add QML controls and production-window input checks (04–06).
- [x] Measure large folders/images, responsiveness and memory (11).
- [x] Run contributor quality, installation, desktop and visual checks (11).
- [x] Update README/backlog and record evidence and remaining acceptance gates.

## Focus-warning correction

User-authorized correction within BROWSE-05/06: keep the canvas's Tab focus
policy stable while navigation/Refresh changes readiness. Existing input guards
remain responsible for inspection and dialog suppression. Directory opening
remains out of scope.

- [x] Fix the canvas focus policy and run existing production input checks.
- [x] Record contributor-check results and warning verification.
