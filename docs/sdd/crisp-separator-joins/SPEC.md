# Crisp separator joins — holonight-viewer

Status: Accepted for implementation, 2026-09-21. Work package CS-006.
Baseline: `2ba1e5217002bf369fbbc4223866e37e176d4879`. The approved user plan authorizes this scope.

- REQ-001: Adopt HnSeparator integer physical thickness, inherited opacity, borderPassive default,
  and Leading/Center/Trailing boundary alignment without caller DPR calculations.
- REQ-002: Bottom/right boundaries shall be trailing aligned; top/left boundaries leading aligned.
  Preserve unrelated worktree edits, application architecture and existing style overrides.
- REQ-003: Verify imports and relevant application regressions against explicitly staged modified Qt.
