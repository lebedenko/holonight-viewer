# SPEC: Consistent key hints

Approved scope: user-supplied implementation plan, 2026-09-20.
Work package KH-003; upstream baseline `01cdd23f7a9ddca7d74fab71ec067d9eead1a394`.

## Requirements

- When semantic key groups are supplied, hints shall use the shared renderer and preserve shortcut activation.
- When displaying symbols, the provider shall scale and align vector ink with resolved-font capital metrics.
- When space is constrained, shared wrapping shall prefer alternatives, then keys.
- When assistive technology reads hints, it shall receive translated semantic names without decorative duplicates.
- When disabled, hints shall retain the shared disabled palette.
- Literal text shall remain supported without parsing.

## Owned scope

Footer, shortcut help and menu hints.

## Acceptance

Focused regressions, repository acceptance and QML checks; provider gallery startup and install tests; visual review at 8/12/18 pt, multiple fonts and fractional scales. Manual ecosystem checks remain required. Publication and umbrella pins need explicit authorization.

## KH-007 frameless menu and footer typography refinement

Baseline: `746acb2f00e427eab97743feda7492fbc40c751e`. Provider
`9723cef7371a4ca5f2967d869b26ee7ff1e3c789` was published and pinned before
implementation (umbrella checkpoint `a79866f`). KH-003 evidence remains historical.

- Menu shortcuts use HnKeySequenceLabel with semantic arrays, no frame/background/
  badge padding, the adjacent label's full font and textMuted/textDisabled colors.
- Footer badges follow their description's resolved point or pixel size at runtime
  while retaining the theme monospace family.
- Preserve menu actions, shortcut bindings, navigation and scrollbar clearance.
- Help badges retain HnKeyHint and receive the provider's compact geometry.
- Test full typography and state colors, runtime footer size changes, geometry,
  navigation and wrapping. Review menu/footer/help captures at multiple sizes and
  fractional scales. Run focused checks before full Viewer acceptance.

Native interaction and final ecosystem integration remain manual user checks.
