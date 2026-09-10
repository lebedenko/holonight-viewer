# Publication design

Use Git and GitHub CLI with the existing push/pull-request workflows. Add release
notes at docs/releases/v0.1.0.md and update README/backlog for source delivery.
Preserve the separate release-readiness acceptance history.

Commit documentation to PR #1, validate its final head, mark ready and merge with
a merge commit. Compare Git tree objects and validate main CI before creating an
annotated local tag. Archive that commit with a versioned directory and gzip -n;
extract beneath build/release and use providers independently installed from the
CI-pinned revisions. Run the contributor checks and staged installation against
the extracted source, with no Git metadata or sibling source dependency.

After successful archive validation, push the tag and create a draft release with
--verify-tag. Attach only the archive and SHA256SUMS. Require both push workflows
for v0.1.0, download the draft assets and compare checksums, then publish as latest.
Do not replace tags/assets on retry. Record final commit, digest, CI and release
URLs externally in the completion report; a commit cannot record its own future
hash/check results. The committed verification file records prepublication evidence
and points to those authoritative publication records.
