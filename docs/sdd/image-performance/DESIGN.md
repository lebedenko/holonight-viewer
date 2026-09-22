# Design

Extend the existing Python standard-library measurement runner and opt-in GTest exercises.
Keep the legacy binary/output positional arguments and default large-workflow scenario.
Re-execute the runner under the repository private D-Bus configuration; measure the direct Viewer child with wait4 so peak RSS belongs to Viewer, not the bus wrapper.
Each scenario runs in five fresh sequential processes. Preserve per-trial XML/log/RSS samples and JSON, then aggregate median/min/max metrics. Select only document-level offscreen scenarios.

Navigation uses twelve distinguishable 2048-square PNGs (16 MiB decoded each). The cache has both a two-entry cap and a 128 MiB byte cap; this traversal exceeds its working-set capacity. Generate fixtures before timing, release producer pixels, and check file identity, dimensions and a unique pixel before accepting each selected result. Two-image repeated visits exercise reuse; repeated full forward/backward traversals exceed the cache; bursts exercise newest-selection behavior. Existing injected-decoder tests remain the deterministic source of cache/cancellation correctness.

A 1 ms GUI timer measures event-loop sampling gaps, not native frame latency. Process RSS includes fixture construction, codec/allocator costs and clipboard preparation where applicable. Sample Linux RSS every 20 ms and retain phase checkpoints for navigation. Empty application cache does not imply cold filesystem cache; never flush host caches. Existing performance limits remain unchanged.

Record baseline source revision plus tracked-diff and instrumentation hashes, binary hash, CMake configuration/provider revisions and host/compiler/Qt details. Do not collect the full environment. Production fixes require reproducible evidence; otherwise deliver tooling and measurements only.
