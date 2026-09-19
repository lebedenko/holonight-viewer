# HoloNight alignment

Work package V-002; baseline c76a3c683ac500a75aab56c68998698cf428aad9.

Preserve hn-viewer, HolonightViewer, supported formats, shortcuts, Viewer menu contents and layout. Move QML under qml/ with feature directories and resource aliases established before module creation. Build executable-owned modules for production and test harnesses through one function, sharing private non-QML code. Use runtime Controls imports and retain Core/composite providers and overridable embedded style configuration.

Generic CMake must not inject local providers or force tooling paths. Presets/Task own development defaults. Share recursive formatting discovery, enforce import policy and qmltypes, and apply warnings only to owned sources. Place fixtures in the configured build directory and use mandatory PNG images for navigation checks. Replace privileged local install/removal with staging and document legacy cleanup.

Acceptance: external configure/build/CTest; formatting, lint, metadata, import policy, licenses, tidy; staged /usr runtime without source imports; explicit staged providers despite stale local artifacts; default/Fusion popup, menu and keyboard checks; CI at accepted provider pins. Native appearance requires manual user review. Record exact results in VERIFICATION.md before handoff.
