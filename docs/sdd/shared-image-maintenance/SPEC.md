# Shared image maintenance

Approved scope: supplied implementation plan. Work package M-003.
Baseline: `af57d28416d68ca695c3a2d9bc30bb3b341e7dd0` (clean HEAD and umbrella pin verified 2026-09-23).

## Requirements

- When opted in, tooling shall add versioned measurement metadata and strict five-trial JSON/Markdown comparisons, then run fresh identical-production baseline/candidate datasets.
- Tooling shall preserve production APIs, behavior, codecs and cache policy.
- Verification shall retain raw evidence and failures outside tracked source.
- Publication and umbrella pins shall wait for explicit authorization.

See [design](DESIGN.md), [tasks](TASKS.md) and [verification](VERIFICATION.md).
