# First source release publication

Approved 2026-09-10: the user-supplied “Publish the first source-only release:
v0.1.0” implementation plan authorizes this scope, design and tasks under
CONTRIBUTING.md. It supersedes the qualification cycle's prohibition on merging,
tagging and publishing; earlier qualification history remains unchanged.

- R1: When delivering 0.1.0, the project shall publish a regular GitHub Release
  titled “HoloNight Viewer 0.1.0 — First source release”, marked latest, with
  source and checksums only; the existing version shall remain unchanged.
- R2: Before merging PR #1 with a merge commit, both hosted workflows shall pass
  on its final documentation head. Before tagging, both shall pass on the merge
  commit on main, whose source tree shall equal the reviewed head's tree.
- R3: When generating assets, the project shall use git archive at that exact
  validated commit, a holonight-viewer-0.1.0/ prefix and gzip -n, and shall record
  its SHA-256 in SHA256SUMS. Generated artifacts shall remain under build/.
- R4: Before pushing the annotated v0.1.0 tag, the extracted source without Git
  metadata shall build, pass tests and staged-install checks with separately
  installed CI-pinned providers; required licenses, fixtures, desktop entry and
  icon shall be present, and the executable shall report 0.1.0.
- R5: Before publishing, the draft release shall reference the existing tag,
  contain the prepared notes and assets, pass tag-triggered CI, and have its
  downloaded archive verified against the local checksum. Existing tags or
  published assets shall never be overwritten; failed checks or unexpected
  source differences shall stop publication.
- R6: Release notes shall document static-image features, formats, native Wayland,
  dependencies, exact tested provider revisions, Qt compatibility, build/install
  commands, accepted performance and the three unresolved next-release deferrals.

No production code, public API, CLI, provider source, distribution package,
portable binary, bundled provider or automatic publishing workflow changes.
Unchanged native measurements remain accepted; production changes would require
affected qualification to be repeated.
