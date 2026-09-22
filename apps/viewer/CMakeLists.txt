find_package(PkgConfig REQUIRED)
pkg_check_modules(WebP REQUIRED IMPORTED_TARGET GLOBAL libwebp)
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

add_library(viewer-protocol STATIC "${xdg_foreign_code}" "${xdg_foreign_header}")
target_link_libraries(viewer-protocol PRIVATE PkgConfig::WaylandClient)
set_target_properties(viewer-protocol PROPERTIES AUTOMOC OFF)

add_library(viewer-private STATIC
  image_orientation.cpp directory_model.cpp decoded_image_cache.cpp exif_metadata.cpp
  view_geometry.cpp portal_request_builder.cpp wayland_foreign_export.cpp frame_source.cpp frame_streamer.cpp)
target_link_libraries(viewer-private PUBLIC Qt6::Quick Qt6::DBus PRIVATE
  PkgConfig::WebP PkgConfig::EXIF PkgConfig::WaylandClient Qt6::GuiPrivate viewer-protocol)
target_include_directories(viewer-private PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(viewer-private SYSTEM PRIVATE "${xdg_foreign_dir}")
viewer_warnings(viewer-private)

function(add_viewer_application target)
  set(app_dir "${PROJECT_SOURCE_DIR}/apps/viewer")
  set(qml_files qml/Main.qml qml/footer/FooterKeyHints.qml
    qml/information/ImageInformationPopup.qml qml/shortcuts/ShortcutHelpPopup.qml)
  set(module_qml)
  foreach(file IN LISTS qml_files)
    string(REGEX REPLACE "^qml/" "" alias "${file}")
    set_source_files_properties("${app_dir}/${file}" PROPERTIES QT_RESOURCE_ALIAS "${alias}")
    list(APPEND module_qml "${app_dir}/${file}")
  endforeach()
  set(module_icons)
  foreach(icon close empty-viewer information fullscreen menu play pause)
    set(file "${app_dir}/icons/${icon}.svg")
    set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "icons/${icon}.svg")
    list(APPEND module_icons "${file}")
  endforeach()
  qt_add_executable(${target} ${ARGN})
  qt_add_qml_module(${target}
    URI HolonightViewer VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/${target}/HolonightViewer"
    RESOURCE_PREFIX "/qt/qml"
    IMPORT_PATH "${CMAKE_CURRENT_BINARY_DIR}/qml/${target}" ${QML_IMPORT_PATH}
    DEPENDENCIES QtQuick
    QML_FILES ${module_qml}
    RESOURCES ${module_icons}
    SOURCES
      "${app_dir}/window_state.h" "${app_dir}/window_key_router.h"
      "${app_dir}/clipboard_controller.h" "${app_dir}/clipboard_controller.cpp"
      "${app_dir}/animation_controller.h" "${app_dir}/animation_controller.cpp"
      "${app_dir}/image_document.h" "${app_dir}/image_document.cpp"
      "${app_dir}/image_canvas.h" "${app_dir}/image_canvas.cpp"
      "${app_dir}/portal_file_chooser.h" "${app_dir}/portal_file_chooser.cpp"
    NO_IMPORT_SCAN)
  qt_add_resources(${target} viewer-config PREFIX "/" BASE "${app_dir}"
    FILES "${app_dir}/qtquickcontrols2.conf")
  target_link_libraries(${target} PRIVATE viewer-private Qt6::QuickControls2 Qt6::Svg
    HolonightQt::Core HolonightQt::Controls PkgConfig::WebP)
  target_compile_definitions(${target} PRIVATE VIEWER_VERSION="${PROJECT_VERSION}")
  viewer_warnings(${target})
endfunction()

add_viewer_application(holonight-viewer main.cpp)
install(TARGETS holonight-viewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_target_properties(holonight-viewer PROPERTIES OUTPUT_NAME hn-viewer)
