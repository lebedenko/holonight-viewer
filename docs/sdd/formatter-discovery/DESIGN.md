# Design

Move PATH discovery after Qt installation discovery and the fixed fallback in
`scripts/qml-format.sh`, following Files' existing order. Retain the explicit
validation branch and final `exec` unchanged. No shared tooling abstraction.

Extend `scripts/check-qml-format-discovery.py` using its existing temporary PATH,
fake executables and relocated fixed fallback. Exercise competing candidates,
all four directory priorities, failed/empty queries, absent and non-executable
candidates, PATH-only preference, and overrides with all discovery tiers present.
Keep argument and formatter/check-wrapper failure propagation assertions.

Document precedence in README. No application sources or build settings change.
