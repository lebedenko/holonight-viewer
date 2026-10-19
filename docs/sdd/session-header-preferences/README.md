# Viewer session header preferences

Status: Implemented; acceptance results are reported with the change.

## Intent and boundaries

Provide explicit application preferences for sessions without native title bars,
without compositor detection. Native decorations and titles remain owned by Qt
and the compositor. The [decoration-independent Viewer record](../decoration-independent-viewer/README.md)
remains historical. Shared appearance and holonight-config interfaces are unchanged.

## Configuration contract

Viewer owns parsing, validation, provenance and watching in the application-private
`ViewerPreferences` QObject. Its read-only `showTitle` and `showCloseButton`
properties notify independently. Main constructs it before QML and passes it as an
initial property. QML harnesses may omit it to use deterministic built-in defaults.
System tomlplusplus is a private dependency; no shared library is added.

Resolve `$XDG_CONFIG_HOME/holonight-viewer`, falling back to
`$HOME/.config/holonight-viewer` when unset or empty. Merge built-in defaults,
`config.toml`, then at most one `config-<desktop>.toml`, per key:

```toml
[header]
show_title = true
show_close_button = true
```

Capture `XDG_CURRENT_DESKTOP` at launch, split on colons, skip empty entries,
lowercase ASCII letters and accept only ASCII letters, digits, hyphens and
underscores. Warn and skip invalid names. For
`HoloNight-Hyprland:HoloNight:Hyprland`, select the first existing file among
`config-holonight-hyprland.toml`, `config-holonight.toml`, `config-hyprland.toml`.
Later candidates never contribute, even when the selected file is invalid.

Missing keys inherit individually. Warn on unknown keys, invalid types and
unreadable or malformed files. Ignore invalid values; ignore unreadable or
malformed files entirely. Retain each effective value's source as Default, Base
or Override, and retain the selected override path. Never write configuration.

## Presentation and focus

The heading is left aligned and vertically centered, constrained before the action
row, plain text with middle elision. Preserve image/folder/scanning/count headings
and the “HoloNight Viewer” empty fallback. Native titles are independent.
Actions run Information, Fullscreen, Menu, Close, with Close at the far right.
All four header glyphs share the same visual extent. Close hover/press uses the
semantic error background and on-error foreground, matching labwc SSD behavior.
The theme-tinted close SVG uses the existing button component, accessible name
“Close”, and existing Quit action, including modal gating and normal shutdown.

Both preferences apply in windowed and fullscreen modes. Existing toolbar auto-hide
remains intact. Hidden Close takes no row spacing or focus target. Forward and
reverse traversal include enabled Close; keyboard focus still reveals a hidden
fullscreen header. If reload hides the focused Close, transfer focus to Menu before
hiding it. Do not steal focus from menus or modal dialogs.

## Reload

QFileSystemWatcher observes files and directories, including existing ancestors
when directories are absent. Watch parents to recover from directory replacement.
Debounce for 100 ms; refresh watches and recompute the complete snapshot from
defaults. Reselect the first existing override with the launch desktop identity.
Support ordinary edits, atomic replacement, creation, deletion and directory
recreation. Invalid reloads inherit valid lower layers instead of stale values.

## Acceptance

Focused configuration tests cover root fallback, combined priority, inheritance,
provenance, invalid names/types/files and no later-override fallback. Temporary
filesystem tests cover absent directories, edits, atomic saves, deletion,
recreation and higher-priority override selection. QML tests cover all four
visibility combinations in both modes, narrow long headings, accessibility,
activation, modal gating, focus removal and fullscreen keyboard traversal.
Existing title, scanning/count and shutdown coverage remains in place.

Run focused tests, then `task check` and `task check:integration` for formatting,
QML imports/types/lint, CTest and installed dependencies. Native interaction is
checked manually; never automate desktop pointer movement or focus.
