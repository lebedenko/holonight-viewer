# Shared file UI controls

Viewer wraps its existing provider-backed renderer in HnThumbnailDelegate.
The renderer retains asynchronous loading, provider-only caching, logical sizing and
no-upscaling. Selection is explicit; keyboard focus has separate shared presentation.
Information uses a shared preview frame and ordered metadata sections; the file path
remains a selectable TextArea. Footer hints use HnLabeledKeyHint with the existing
semantic shortcut groups and measured priority algorithm that always retains Help.
Viewer continues to depend directly on Qt and gains no File Browser dependency.

Acceptance covers thumbnail cache/generation/viewport requests, keyboard activation,
information path selection, playback and footer fitting. Native visual acceptance
requires the host-built executable and manual keyboard/theme/scaling checks.

Verification results and compatible provider revisions are recorded in the umbrella
[initiative ledger](../../../../docs/initiatives/shared-file-ui-controls/TASKS.md).
