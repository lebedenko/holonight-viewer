# Empty-window decoration requirements

Approved through the user-supplied implementation plan on 2026-09-11.

- R1: While the document is Empty, Viewer shall display a centered monochrome derivative of its application icon instead of a visible empty-state message.
- R2: While the document is Loading, Ready, or Error, Viewer shall hide the decoration and preserve existing document feedback, including for transparent images.
- R3: When the canvas resizes, Viewer shall use its shorter dimension d, padding clamp(d × 0.08, 32, 64), and square icon side max(0, floor(d − 2 × padding)). The decoration shall remain centered and contained in the central canvas excluding surrounding rows.
- R4: When the theme changes, the decoration shall use HoloniightPalette.surface against the existing background, through the installed HnIcon tint and palette tracking. Rasterization shall follow displayed size up to the installed provider’s 1024-physical-pixel limit; larger decorations shall scale that bounded raster.
- R5: While Empty, the canvas shall expose the translated accessible name “No image open”; the decoration shall be accessibility-ignored and noninteractive. Keyboard focus and drops shall remain usable.
- R6: The resource shall retain original rear-sheet, frame, mountains and sun geometry and license metadata in a compact square viewBox, with one opaque neutral color and transparent negative space. The packaged application icon shall remain unchanged.

No C++ public API, document state, dependency, or provider changes. Subjective visual acceptance is recorded separately from automated verification.

Status: implemented; local automated and subjective offscreen acceptance passed
on 2026-09-11. See [verification](VERIFICATION.md) for evidence and native-check
limitations.
