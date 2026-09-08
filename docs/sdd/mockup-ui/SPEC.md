# Mockup UI spec

Approved scope: supplied Mockup-based Viewer UI implementation plan.

R1: The Viewer shall preserve native decorations and show a compact shared header with a centered elided filename, information, fullscreen and menu controls, and a permanent wrapping shortcut footer.
R2: When mouse movement occurs in the client area, the Viewer shall reveal boundary-aware navigation arrows for five seconds; when keyboard input occurs, it shall hide arrows immediately without consuming input.
R3: When a selected or refreshed image first renders, the Viewer shall reveal details for five seconds, including cached images. Mouse movement shall restart this independent timer; other keyboard input and ordinary repaints shall not restart it. While empty, loading or failed, details shall be hidden.
R4: Details shall overlay the canvas without changing geometry and show filename, transformed dimensions, locale-aware decimal file size, physical-pixel percentage and folder position in that order, wrapping at narrow widths and eliding filenames first.
R5: All existing actions, shortcuts, focus/activation, pan, zoom, drop and feedback shall remain available. Viewer-owned presentation shall not vary with focus; native dialogs retain platform styling.

Non-goals: deletion, command palette, shortcut aliases, custom chrome, provider changes.

R6: Viewer font sizing shall use points only. Controls shall inherit shared
point-based typography rather than applying pixel-sized font overrides.
Approved by the user’s typography correction.

R7: Header actions shall use consistent scalable SVG icons, not font glyphs. The Actions popup shall retain a visible inset from every client-area edge. Approved by the user’s icon and menu-spacing correction.
