# Clipboard persistence acceptance

Approved by the supplied implementation plan, 2026-09-23. Starting Viewer revision:
`6f9c048e2f936bc93e017a89cd18fb032159d603`.

- R1: A personal desktop service shall preserve the regular selection in memory after Viewer exits, using wl-clip-persist --clipboard regular with upstream defaults.
- R2: Human-driven native checks shall cover eight orientations, alpha, dimensions, exact special-character/symlink paths and GIMP interoperability after exit.
- R3: Five identical synthetic copy/exit/receive trials shall retain raw measurements, binary/fixture hashes and versions, separating service and receiver latency/memory from Viewer. Costs require explicit user acceptance.
- R4: A user-performed fresh login shall start exactly one service through existing desktop autostart.

No history, reboot persistence, shell/installer feature, input/focus automation or
repeated four-scale qualification. Physical mixed-monitor and unrelated release
gates stay deferred; previous Integrated and single-monitor acceptance stay intact.
