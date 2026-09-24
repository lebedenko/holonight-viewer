# Shared SVG support — Tasks

Status: Done (local acceptance) against published/pinned Images 3da5f4e51fe2eed9a1bd9f72c0ab523aa57a5ceb.

- [x] Adopt bounded loader, shared facts and self-contained rasterization.
- [x] Use retained bytes for self-contained GUI renderer; preserve local-reference path.
- [x] Unify canvas/information/preview geometry and preserve vector painting.
- [x] Test default-size/viewBox disagreement, relative images, pathname replacement, clipboard and navigation.
- [x] Run focused tests, task check and required isolated runtime acceptance.
- [x] Record manual vector-zoom/navigation checks; commit locally and request authorized publication.

Automated evidence is recorded in [VERIFICATION.md](VERIFICATION.md). Native visual acceptance passed: the user
confirmed on 2026-09-24 that SVGs rendered correctly in both applications. Consumer publication and umbrella pin
updates require a separate authorized handoff.
