qt_add_library(viewer-ui STATIC)
qt_add_qml_module(viewer-ui
  URI HolonightViewer VERSION 1.0
  OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/HolonightViewer"
  IMPORT_PATH "${HOLONIGHT_QML_IMPORT_PATH}"
  QML_FILES Main.qml
  SOURCES directory_model.h directory_model.cpp decoded_image_cache.h decoded_image_cache.cpp image_document.h image_document.cpp image_canvas.h image_canvas.cpp view_geometry.h view_geometry.cpp
  NO_IMPORT_SCAN)
target_link_libraries(viewer-ui PUBLIC Qt6::Quick Qt6::QuickControls2 HolonightQt::Core HolonightQt::Controls)
target_include_directories(viewer-ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
qt_add_executable(holonight-viewer main.cpp)
qt_add_resources(holonight-viewer viewer-config PREFIX "/" FILES qtquickcontrols2.conf)
target_link_libraries(holonight-viewer PRIVATE viewer-ui viewer-uiplugin)
target_compile_definitions(holonight-viewer PRIVATE VIEWER_VERSION="${PROJECT_VERSION}")
install(TARGETS holonight-viewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_target_properties(holonight-viewer PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
