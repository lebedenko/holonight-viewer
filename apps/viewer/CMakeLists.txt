add_subdirectory(images)
add_subdirectory(browsing)
add_subdirectory(platform)

function(add_viewer_application target)
  set(app_dir "${PROJECT_SOURCE_DIR}/apps/viewer")
  set(qml_files qml/Main.qml qml/ViewerActions.qml qml/ViewerImageView.qml
    qml/ViewerDialogHost.qml qml/ViewerStage.qml
    qml/header/ViewerHeader.qml qml/header/ViewerActionsMenu.qml
    qml/overlays/ImageDetailsStrip.qml qml/overlays/ViewerOverlayController.qml qml/overlays/ViewerEmptyState.qml qml/overlays/ViewerLoadingIndicator.qml
    qml/footer/FooterKeyHints.qml
    qml/information/ImageInformationPopup.qml qml/shortcuts/ShortcutHelpPopup.qml
    qml/grid/ThumbnailCell.qml qml/grid/ThumbnailGrid.qml)
  set(module_qml)
  foreach(file IN LISTS qml_files)
    string(REGEX REPLACE "^qml/" "" alias "${file}")
    set_source_files_properties("${app_dir}/${file}" PROPERTIES QT_RESOURCE_ALIAS "${alias}")
    list(APPEND module_qml "${app_dir}/${file}")
  endforeach()
  set(module_icons)
  foreach(icon broken-image empty-viewer information fullscreen menu play pause)
    set(file "${app_dir}/icons/${icon}.svg")
    set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "icons/${icon}.svg")
    list(APPEND module_icons "${file}")
  endforeach()
  qt_add_executable(${target} ${ARGN}
    "${app_dir}/viewer_preferences.h" "${app_dir}/viewer_preferences.cpp"
    "${app_dir}/image_information_formatter.cpp" "${app_dir}/view_geometry.cpp")
  qt_add_qml_module(${target}
    URI HolonightViewer VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/${target}/HolonightViewer"
    RESOURCE_PREFIX "/qt/qml"
    IMPORT_PATH "${CMAKE_CURRENT_BINARY_DIR}/qml/${target}" ${QML_IMPORT_PATH}
    DEPENDENCIES QtQuick
    QML_FILES ${module_qml}
    RESOURCES ${module_icons}
    SOURCES
      "${app_dir}/viewer_shortcuts.h" "${app_dir}/viewer_shortcuts.cpp"
      "${app_dir}/viewer_controller.h" "${app_dir}/viewer_controller.cpp"
      "${app_dir}/grid_selection_controller.h" "${app_dir}/grid_selection_controller.cpp"
      "${app_dir}/window_state.h" "${app_dir}/grid_navigation.h" "${app_dir}/thumbnail_metrics.h" "${app_dir}/thumbnail_metrics.cpp"
      "${app_dir}/information_section.h"
      "${app_dir}/folder_grid_model.h" "${app_dir}/folder_grid_model.cpp" "${app_dir}/window_key_router.h" "${app_dir}/window_key_router.cpp"
      "${app_dir}/clipboard_controller.h" "${app_dir}/clipboard_controller.cpp"
      "${app_dir}/animation_controller.h" "${app_dir}/animation_controller.cpp"
      "${app_dir}/image_document.h" "${app_dir}/image_document.cpp"
      "${app_dir}/image_canvas.h" "${app_dir}/image_canvas.cpp"
      "${app_dir}/portal_file_chooser.h" "${app_dir}/portal_file_chooser.cpp"
    NO_IMPORT_SCAN)
  qt_add_resources(${target} viewer-config PREFIX "/" BASE "${app_dir}"
    FILES "${app_dir}/qtquickcontrols2.conf")
  target_include_directories(${target} PRIVATE "${app_dir}")
  target_link_libraries(${target} PRIVATE viewer-images viewer-browsing HolonightFileBrowser::PreviewQuick viewer-platform
    Qt6::QuickControls2 Qt6::Svg tomlplusplus::tomlplusplus
    HolonightQt::Core HolonightQt::Controls)
  target_compile_definitions(${target} PRIVATE VIEWER_VERSION="${PROJECT_VERSION}")
  viewer_warnings(${target})
endfunction()

add_viewer_application(holonight-viewer main.cpp)
install(TARGETS holonight-viewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_target_properties(holonight-viewer PROPERTIES OUTPUT_NAME hn-viewer)
