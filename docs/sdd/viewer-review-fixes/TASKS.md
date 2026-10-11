# Viewer review fixes tasks

- [x] T-001: Separate animation publication and skip SVG prefetch (REQ-NF-001, REQ-NF-002); focused unit/UI regressions.
- [x] T-002: Extract ImageLoader and named shutdown participants (REQ-F-001, REQ-C-001); barrier and integration regressions.
- [x] T-003: Split private targets and move URL normalization (REQ-C-002); fresh external build.
- [x] T-004: Run task check and sanitizer-check; record verification and manual native checks.

Automated verification is complete. Full task check remains red at unchanged, deferred C++ lint; all 27 CTest entries, sanitizer, formatting, QML, licensing and staged install checks passed. The user confirmed all requested native checks passed; see VERIFICATION.md.
