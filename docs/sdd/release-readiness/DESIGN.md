# Design

Approved through the supplied implementation plan, following SPEC.md. Keep the
existing document/worker architecture and dependency revisions. Use freedesktop
Exec field substitution and standard Qt accessibility announcements. Qualification
lives in test tooling, not the public application CLI. An independent fixture
producer supplies deterministic encoded samples; document tests assert decoded
results and original bytes. Desktop tests use GIO's desktop-entry implementation.
An isolated runtime container receives installed payloads only, with no workspace
mounts. CI runs contributor checks before building and running that container.

The interactive clipboard probe retains automatic mode for existing transport
regressions and adds a user-triggered mode for native acceptance. Native acceptance
and performance evidence must distinguish automated coverage from human desktop
checks and clipboard manager costs. No provider sources are changed.

Implementation findings: the independently encoded compact WebP exposes an
installed Qt 6.11.2 decoder failure. A valid RIFF-padded version is the normal
fixture, while the compact stream remains a mandatory failing qualification test.
Viewer distinguishes unreadable dimensions from genuine size-limit rejection.
Accessible announcements use Qt Quick's attached Accessible API; state transitions
avoid repeated zoom/transform/folder announcements. Tests capture Qt accessibility
events and query interfaces without treating them as an Orca substitute.

The runtime diagnostic is installed only by the excluded `qualification` component.
It uses the actual UI/document and consumes each fixture exactly once despite
shared directory/document notifications. The normal Viewer installation remains
executable, entry, icon and licenses plus separately installed providers.

User scope clarification: qualify native Wayland only. X11/XWayland results from
earlier stages are historical and neither satisfy nor block release acceptance.

Native follow-up adds an explicit PNG receiver diagnostic alongside the unchanged
generic Qt image mode. This separates verified PNG transport from alpha loss in
Qt's private BMP clipboard representation without changing Viewer publication.
The CI image aliases Arch's go-task executable to task, verified by a fresh
container build and installed-only runtime check.

## Approved fixes design (2026-09-08)

Supersedes the earlier diagnostic-only clipboard and failing compact-codec status.
Encode PNG through QBuffer in the existing clipboard worker; pass bytes to the GUI
for QMimeData::setData("image/png"). Do not offer Qt's private image representation.
Use Qt first, then inspect a bounded read of the original file for an exact simple
RIFF container with a single VP8 or VP8L chunk (including required padding). Reject
other containers from fallback. WebPGetInfo validates dimensions before a RGBA8888
QImage allocation; WebPDecodeRGBAInto receives its byte capacity and stride. Feed
success through existing normalization. Link required PkgConfig::WebP privately.
Provider portal delegation belongs in a separate handoff document; no public
runtime API, provider revisions, viewing features or global settings change.

## Visible-focus repair

Use visualFocus for Viewer button borders and activeFocus for text/canvas, with
HoloniightPalette.borderFocus and HnMetrics.focusBorderWidth. The canvas outline
is an accessible-ignored, non-interactive overlay within its existing bounds. Menu
highlighting uses the shared focus outline while retaining disabled/mouse states.
Keyboard-driven regression checks sample rendered indicators and focus departure;
capture both themes at normal/minimum sizes and fractional scale. Native human
visible-focus acceptance remains separate from automated input evidence.
