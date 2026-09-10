# Publication tasks

- [x] T1 (R1, R6): Record approved scope/design; prepare source release notes and
  README/backlog delivery wording.
- [ ] T2 (R2): Commit documentation to PR #1; require both final-head workflows.
- [ ] T3 (R2): Mark ready, merge with a merge commit, compare source trees and
  require both main workflows.
- [ ] T4 (R3, R4): Annotate the local tag; generate archive/checksums; extract
  without Git metadata, build/test and validate staged install/version/content.
- [ ] T5 (R5): Push tag, create draft and attach assets; require tag CI and
  download/check uploaded archive; publish regular latest release.
- [ ] T6 (R1–R6): Report release URL, exact tag commit, checksum and CI links.

T2–T6 intentionally remain pending in the prepublication source snapshot. Their
completion evidence belongs to the release/PR and final completion report, avoiding
an additional unvalidated source commit after tagging.
