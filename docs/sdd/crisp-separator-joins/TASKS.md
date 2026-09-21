# Tasks

- [x] Audit current consumers and preserve unrelated edits.
- [x] Apply the accepted separator contract where needed.
- [x] Build and run relevant regressions against the explicitly staged modified provider.
- [x] Record local verification and remaining integration boundary.

Verified 2026-09-21 against the uncommitted provider working tree, staged from
`holonight-files/build/deps/holonight-qt` into this repository's dependency prefix.

Existing separators already use the accepted defaults and ordinary opacity. No product edits are
necessary. The unrelated GIF/TIFF and other worktree changes are preserved.

Full build and all 20 CTest entries passed across the initial run and D-Bus-enabled retry, including
ten dark/light separator DPR cases and Fusion controls. QML lint passes. The final separator/Fusion subset
passes 11/11 against the final staged provider.

Native connected-window acceptance belongs to Files and has passed. The user subsequently authorized
publication and pin updates; the umbrella ledger records published revisions and the CI snapshot.
