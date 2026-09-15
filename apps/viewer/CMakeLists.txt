find_package(PkgConfig REQUIRED)
pkg_check_modules(WebP REQUIRED IMPORTED_TARGET libwebp)
pkg_check_modules(EXIF REQUIRED IMPORTED_TARGET libexif)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(WaylandProtocols REQUIRED wayland-protocols)
pkg_check_modules(WaylandScanner REQUIRED wayland-scanner)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER_EXECUTABLE wayland-scanner wayland_scanner)
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
# Generated protocol bindings stay under build/ with the rest of the tree's outputs.
set(xdg_foreign_xml "${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml")
set(xdg_foreign_dir "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols")
set(xdg_foreign_header "${xdg_foreign_dir}/xdg-foreign-unstable-v2-client-protocol.h")
set(xdg_foreign_code "${xdg_foreign_dir}/xdg-foreign-unstable-v2-protocol.c")
file(MAKE_DIRECTORY "${xdg_foreign_dir}")
add_custom_command(OUTPUT "${xdg_foreign_header}"
  COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${xdg_foreign_xml}" "${xdg_foreign_header}"
  DEPENDS "${xdg_foreign_xml}" VERBATIM)
add_custom_command(OUTPUT "${xdg_foreign_code}"
  COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${xdg_foreign_xml}" "${xdg_foreign_code}"
  DEPENDS "${xdg_foreign_xml}" VERBATIM)
qt_add_library(viewer-ui STATIC)
qt_add_qml_module(viewer-ui
  URI HolonightViewer VERSION 1.0
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/HolonightViewer"
  IMPORT_PATH "${HOLONIGHT_QML_IMPORT_PATH}"
  QML_FILES Main.qml FooterKeyHints.qml ImageInformationPopup.qml ShortcutHelpPopup.qml
  RESOURCES icons/close.svg icons/empty-viewer.svg icons/information.svg icons/fullscreen.svg icons/menu.svg
  SOURCES window_state.h window_key_router.h image_orientation.h image_orientation.cpp clipboard_controller.h clipboard_controller.cpp directory_model.h directory_model.cpp decoded_image_cache.h decoded_image_cache.cpp exif_metadata.h exif_metadata.cpp image_document.h image_document.cpp image_canvas.h image_canvas.cpp view_geometry.h view_geometry.cpp portal_request_builder.h portal_request_builder.cpp portal_file_chooser.h portal_file_chooser.cpp wayland_foreign_export.h wayland_foreign_export.cpp
  NO_IMPORT_SCAN)
target_link_libraries(viewer-ui PUBLIC Qt6::Quick Qt6::QuickControls2 HolonightQt::Core HolonightQt::Controls)
target_sources(viewer-ui PRIVATE "${xdg_foreign_header}" "${xdg_foreign_code}")
target_link_libraries(viewer-ui PUBLIC Qt6::DBus)
target_link_libraries(viewer-ui PRIVATE PkgConfig::WebP PkgConfig::EXIF PkgConfig::WaylandClient Qt6::GuiPrivate)
target_include_directories(viewer-ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Generated code is third-party style; SYSTEM keeps it out of warnings and clang-tidy.
target_include_directories(viewer-ui SYSTEM PRIVATE "${xdg_foreign_dir}")
qt_add_executable(holonight-viewer main.cpp)
qt_add_resources(holonight-viewer viewer-config PREFIX "/" FILES qtquickcontrols2.conf)
target_link_libraries(holonight-viewer PRIVATE viewer-ui viewer-uiplugin)
target_compile_definitions(holonight-viewer PRIVATE VIEWER_VERSION="${PROJECT_VERSION}")
install(TARGETS holonight-viewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_target_properties(holonight-viewer PROPERTIES OUTPUT_NAME hn-viewer QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
