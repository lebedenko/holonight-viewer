# Design

Use the Files consumer regression technique: QQuickRenderControl with a paint-device
render target whose DPR differs from the offscreen screen. Send the normal DPR-change
event explicitly because no platform surface exists. Instantiate the real popup with
ImageDocument, attach it after creation and retain the same ImageCanvas throughout.

Load a deterministic 128×128 black/white checker PNG. The 94×94 logical preview fits
identically at every DPR, but physical magnification crosses 1 between DPR 1.25 and
1.5. Call production ImageCanvas::paint on a DPR-tagged QImage using QPainter; inspect
both its smoothing hint and interior blended pixels, without compositor screenshots.
Collect QML warnings. Close, change DPR and reopen the same popup.

If reproduced, replace Screen.devicePixelRatio with the canvas Window.window binding
and a null-window fallback of 1. Qt attached-window and DPR notifications keep it live.
