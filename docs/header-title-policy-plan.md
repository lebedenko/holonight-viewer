# Header title display policy — implementation plan

Status: Proposed

## Goal

Avoid duplicate native and application header titles on labwc while retaining
useful headings on desktops without title bars. Separate factual decoration
detection from application display policy and provide a persistent user override.

## Display behavior

Add **Windowed header title → Automatic / Always / Never** to the Viewer menu.
Default to **Automatic**. The preference applies only in windowed mode;
fullscreen permits the title whenever the existing toolbar is visible.

Automatic uses this precedence:

| Condition | Header title |
| --- | --- |
| External title bar confirmed present | Hide |
| External title bar confirmed absent | Show |
| Unknown, labwc, negotiated server-side decorations | Hide |
| Any other unknown state | Show |

Treat the labwc rule as a display default, without changing
`externalTitleBarState` to `Present`. Users with custom decoration rules can
select an explicit override. Always and Never override detection in windowed
mode, but neither forces the toolbar itself to become visible.

## Detection and policy implementation

- Add read-only `decorationMode` to `HnWindowPresentation` in holonight-qt,
  forwarding its existing internal `HnWindowDecoration` state with change
  notification. Avoid creating a second decoration probe.
- Add a Viewer-owned policy object combining the preference, fullscreen state,
  external-title evidence, decoration mode, and desktop identity.
- Recognize labwc through normalized `XDG_CURRENT_DESKTOP` tokens. Use
  `LABWC_PID` only when no recognized desktop is identified. Ambiguous desktop
  identities receive no heuristic.
- Recompute title visibility when inputs change. Preserve the existing surface
  invalidation and stale-callback protections.
- Keep SystemServices' factual title-bar reporting unchanged; no new compositor
  backend is required.

## Preference storage and UI

- Store `window.header_title = "automatic" | "always" | "never"` in
  `$XDG_CONFIG_HOME/holonight/viewer.toml`, falling back to
  `$HOME/.config/holonight/viewer.toml`.
- Use HoloNight Config's document APIs to read and preserve unrelated settings.
  Keep the Viewer schema and settings adapter application-owned, and declare
  the direct Config dependency in the build and dependency tooling.
- Missing or invalid preferences default to Automatic. Diagnose invalid values.
  Do not create or rewrite configuration merely by starting the application.
- Save only after user selection. Apply successful selections immediately;
  report save failures without silently claiming persistence.
- Use exclusive checkable menu choices, following the existing menu styling
  and keyboard-navigation conventions.
- In grid mode, include the folder and image count in the native window title
  as well as the application heading. During scanning, use the folder name
  without a provisional count. Retain existing image and empty-state native
  titles.
- Hide only the title label. Preserve header actions, geometry, keyboard
  navigation, menu anchoring, and fullscreen auto-hide.

## Verification

- Test every row of the Automatic policy table, explicit overrides, fullscreen
  precedence, ambiguous desktop identity, and decoration-state transitions.
- Test preference round-tripping, missing and invalid values, preservation of
  unrelated settings, and failed writes using temporary configuration roots.
- Extend header and menu tests to exercise the actual policy binding and
  exclusive selection, rather than only assigning label visibility directly.
- Verify grid native titles during scanning and after counts change.
- Run Viewer's required `task check` and the Qt provider's relevant presentation,
  QML, and install-tree package-consumer checks.
- Manually verify labwc windowed/fullscreen transitions, a decoration-disabled
  rule with the explicit override, and existing Qt-decoration/Sway behavior.
  Native interaction checks remain manual under repository instructions.

## Documentation and delivery

- Update the existing external-window-title-presentation initiative and local
  work-package documentation to replace the “Unknown always shows” and “no user
  override” rules with this policy.
- Document the preference, labwc heuristic, and configuration location.
- Scope implementation to Viewer and the small Qt API addition, with local
  builds and verification. Deployment and publication are separate steps.
