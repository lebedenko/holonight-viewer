# Requirements

Approved source: umbrella viewer-holonight-alignment / user implementation plan.

- R1: Viewer shall preserve application identity, CLI, formats, menu contents, shortcuts and layout.
- R2: Application QML shall use runtime Controls imports and retain explicit Core/composite imports and overridable embedded style defaults.
- R3: Presentation shall live below apps/viewer/qml; production and harness executables shall own HolonightViewer modules constructed by one function, with non-QML implementation shared privately.
- R4: Generic CMake shall honor caller provider/tooling paths; presets and Task shall supply development defaults.
- R5: Check and CI shall enforce imports, qmltypes, recursive formatting, scoped warnings and external-build fixtures.
- R6: Local workflows shall stage without privilege; coordinated removal shall use umbrella ownership records. Legacy files shall never be silently adopted or removed.
- R7: CI shall use the accepted published provider revisions and retain the existing image approach.
