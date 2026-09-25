# HoloNight Viewer

Use Conventional Commits for every new commit: `type(scope): imperative summary`, or `type: imperative summary` when a scope adds no clarity.

Viewer owns static image viewing, CLI opening, navigation, menus and overlays. Shared design primitives belong to holonight-qt; configuration belongs to holonight-config. Keep dependencies directed toward providers and implementation private to this application.

Presentation lives in apps/viewer/qml; Main.qml is the entry point of executable-owned HolonightViewer modules. Import runtime controls as `QtQuick.Controls as Controls`, qualify types/enums/attached properties, and import Holonight.Core and Holonight.Controls explicitly where used. Do not import a concrete style or Basic. The embedded style default remains overridable.

Use task check for acceptance, or configure an external CMake build with BUILD_TESTING=ON and explicit CMAKE_PREFIX_PATH/QML_IMPORT_PATH. Run format-check, qml-import-check, qmltypes-check, qml-lint and CTest. Stage /usr with DESTDIR; coordinated system installation/removal is owned by the umbrella. Never automate desktop pointer movement or focus; ask for manual native interaction checks.
