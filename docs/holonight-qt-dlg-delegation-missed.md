<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com> -->

# HoloNight native-dialog delegation handoff

Status: **unresolved, deferred to the next release**, owned by holonight-qt.
The approved source-qualification plan makes this a next-release acceptance gate,
not a current-release blocker or a pass. Reproduction and acceptance remain intact. Viewer changes do
not resolve native Open acceptance with the default HoloNight platform theme.
See [Viewer verification](sdd/release-readiness/VERIFICATION.md).

## Tested environment and evidence

The 2026-09-08 desktop comparison used Arch Linux, Qt / Qt Image Formats 6.11.2,
Hyprland 0.56.2 on native Wayland (`wayland-1`, XWayland excluded), HoloNight Qt
revision `22ded7815727ce483fd91e82a9cc04bfe252ec3b`, and HoloNight Config revision
`fe69a59e6b73167fd5349223a4d265d75386c139`. Provider revision identifies the tested
implementation more precisely than a package version. The session default is
`QT_QPA_PLATFORMTHEME=holonight`; comparison processes used
`QT_QPA_PLATFORM=wayland`, DISPLAY unset, installed provider QML/library paths,
and `QT_QPA_PLATFORMTHEME=xdgdesktopportal` only for the comparison process.

Read-only inspection of `src/platformtheme/holonighttheme.h` at that Qt-provider
revision shows `HoloniightTheme : QPlatformTheme` overriding palette, themeHint,
colorScheme and font. It does not override `usePlatformNativeDialog` or
`createPlatformDialogHelper`; the platformtheme sources contain no dialog helper
implementation. This is evidence of missing native-dialog delegation. It is not
proof that all Open operations fail: CLI decoding works, Qt fallback dialogs may
work, and the portal-theme comparison opened a selected file successfully.

The earlier native smoke did not find a separate native Open dialog with the
HoloNight theme. With xdgdesktopportal, a GTK native Wayland “Open image” window
appeared. Escape canceled; Ctrl+L, path paste and Return selected an existing
corrupt PNG, followed by Viewer's expected decoding-error announcement. This
establishes selection/cancellation in that configuration, not successful decoding
of that deliberately corrupt file or complete default-theme acceptance. Logs and
AT-SPI snapshots are under `build/release-readiness/native-wayland/` as described
in Viewer verification. Portal backend package versions were not captured in the
original smoke and remain to be recorded during provider acceptance.

## Reproduction

1. Use the installed providers above and launch Viewer with
   `QT_QPA_PLATFORM=wayland QT_QPA_PLATFORMTHEME=holonight`, DISPLAY unset, and
   the documented QML_IMPORT_PATH / LD_LIBRARY_PATH for a custom provider prefix.
2. Open a known image via CLI. Press Ctrl+O and inspect native windows and dialog
   behavior; cancel and confirm the existing image/view remains unchanged.
3. Repeat in a new process with only QT_QPA_PLATFORMTHEME changed to
   `xdgdesktopportal`. Press Ctrl+O, cancel with Escape, then reopen and select a
   path with Ctrl+L/path paste/Return. Compare window identity, completion and
   cancellation with the HoloNight run. Do not alter global session settings.

## Proposed provider work

Implement portal delegation in holonight-qt's platform theme, preserving HoloNight
palette, fonts, icons, color scheme and controls in the application. Delegate file
requests through a supported Qt portal helper or a provider-owned implementation;
resolve private-Qt ABI and helper lifetime there, without Viewer-specific logic or
recursive platform-theme creation. Propagate accept/reject exactly once. Cancel
and parent destruction must close outstanding requests without opening a file.
Associate the request with its Wayland parent window and requested modality; retain
focus behavior across portal completion. If no portal/backend is available, use a
defined Qt fallback dialog instead of silently dropping the request. User
cancellation must not trigger a second fallback dialog. These are proposed
requirements, not implemented behavior.

## Provider acceptance

- Open a valid image using button and Ctrl+O with the HoloNight theme selected.
- Cancel by Escape and dialog close; retain image, orientation, zoom and focus.
- Select Unicode/space/percent paths and symlinks; preserve the chosen local URL.
- Verify parent association, modality, focus restoration, repeated opens and
  parent shutdown while a portal request is outstanding.
- Simulate missing/unavailable portal backend and verify usable fallback;
  cancellation must remain cancellation.
- Compare light/dark HoloNight application appearance before/after delegation,
  including controls, fonts and icons; record Qt/provider/compositor/backend
  versions and real native input. Complete Viewer keyboard and Orca dialog checks.

No provider sources, dependency revisions or global environment settings were
changed for this handoff. The next-release provider gate remains open until these checks pass.
