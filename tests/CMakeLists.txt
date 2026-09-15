set_source_files_properties(../apps/viewer/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_executable(viewer-smoke smoke.cpp release_formats_test.cpp accessibility_test.cpp release_performance_test.cpp static_workflow_test.cpp folder_browsing_test.cpp image_document_test.cpp view_geometry_test.cpp image_inspection_test.cpp exif_metadata_test.cpp footer_key_hints_test.cpp image_information_popup_test.cpp menu_layout_test.cpp shortcut_help_popup_test.cpp empty_state_test.cpp transient_overlay_test.cpp portal_request_builder_test.cpp mock_portal.cpp portal_file_chooser_test.cpp)
qt_add_resources(viewer-smoke test-config PREFIX "/" BASE ../apps/viewer FILES ../apps/viewer/qtquickcontrols2.conf)
target_link_libraries(viewer-smoke PRIVATE viewer-ui viewer-uiplugin Qt6::Test GTest::gtest)
# The pixel matrix below exercises hairlines with the production RHI renderer.
# Every viewer-smoke run gets a private bus with no service directories, so the Open flow can
# only reach the mock portal and never activates the desktop's real one.
find_program(VIEWER_DBUS_RUN_SESSION dbus-run-session REQUIRED)
set(viewer_bus_runner "${VIEWER_DBUS_RUN_SESSION}" "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/dbus-session.conf" --)
add_test(NAME viewer-smoke COMMAND ${viewer_bus_runner} $<TARGET_FILE:viewer-smoke> --gtest_filter=-MenuLayout.SeparatorsRenderAsPhysicalHairlines)
set_tests_properties(viewer-smoke PROPERTIES ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${HOLONIGHT_QML_IMPORT_PATH};LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
add_test(NAME viewer-help COMMAND holonight-viewer --help)
add_test(NAME viewer-version COMMAND holonight-viewer --version)
add_test(NAME viewer-reject-file COMMAND holonight-viewer one.png two.png)
set_tests_properties(viewer-help viewer-version viewer-reject-file PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
set_tests_properties(viewer-reject-file PROPERTIES WILL_FAIL TRUE)

# Shadow the installed module with an invalid provider to exercise startup failure.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/invalid-import/Holonight/Controls/qmldir"
  "module Holonight.Controls\nHnApplicationWindow 1.0 Missing.qml\n")
add_test(NAME viewer-qml-failure COMMAND ${CMAKE_COMMAND} -E env
  "QT_QPA_PLATFORM=offscreen"
  "QML_IMPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/invalid-import:${HOLONIGHT_QML_IMPORT_PATH}"
  "LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
  $<TARGET_FILE:holonight-viewer>)
set_tests_properties(viewer-qml-failure PROPERTIES WILL_FAIL TRUE TIMEOUT 5)

target_compile_definitions(viewer-smoke PRIVATE VIEWER_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures/images")

set_target_properties(viewer-smoke PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME viewer-qml-format-discovery COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/check-qml-format-discovery.py")
add_test(NAME viewer-opening COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/check-opening.py" $<TARGET_FILE:holonight-viewer>)
set_tests_properties(viewer-opening PROPERTIES TIMEOUT 30 ENVIRONMENT
  "QML_IMPORT_PATH=${HOLONIGHT_QML_IMPORT_PATH};LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set_tests_properties(viewer-smoke PROPERTIES TIMEOUT 60)

qt_add_executable(clipboard-probe clipboard_probe.cpp)
target_link_libraries(clipboard-probe PRIVATE Qt6::Quick)
target_compile_definitions(viewer-smoke PRIVATE VIEWER_CLIPBOARD_PROBE="$<TARGET_FILE:clipboard-probe>")
add_dependencies(viewer-smoke clipboard-probe)

add_custom_target(release-fixtures
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/format-fixtures.py" "${CMAKE_CURRENT_BINARY_DIR}/release-fixtures")
add_dependencies(viewer-smoke release-fixtures)
target_compile_definitions(viewer-smoke PRIVATE RELEASE_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/release-fixtures")

qt_add_executable(installed-runtime-probe installed_runtime.cpp)
qt_add_resources(installed-runtime-probe runtime-config PREFIX "/" BASE ../apps/viewer FILES ../apps/viewer/qtquickcontrols2.conf)
target_link_libraries(installed-runtime-probe PRIVATE viewer-ui viewer-uiplugin)
install(TARGETS installed-runtime-probe RUNTIME DESTINATION libexec/holonight-viewer
  COMPONENT qualification EXCLUDE_FROM_ALL)

set_target_properties(installed-runtime-probe PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
add_dependencies(installed-runtime-probe release-fixtures)
add_test(NAME viewer-runtime-probe COMMAND installed-runtime-probe "${CMAKE_CURRENT_BINARY_DIR}/release-fixtures")
set_tests_properties(viewer-runtime-probe PROPERTIES TIMEOUT 15 ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;QML_IMPORT_PATH=${HOLONIGHT_QML_IMPORT_PATH};LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# Separate processes are required because Qt fixes the display scale at startup.
# CI provides Xvfb for offscreen OpenGL; local runs can use their existing display.
find_program(VIEWER_XVFB_RUN xvfb-run)
set(separator_runner)
if(VIEWER_XVFB_RUN)
  list(APPEND separator_runner "${VIEWER_XVFB_RUN}" -a)
endif()
foreach(theme dark light)
  foreach(scale 1 1.25 1.5 1.75 2)
    set(separator_test "viewer-menu-separators-${theme}-${scale}")
    add_test(NAME "${separator_test}" COMMAND ${separator_runner} ${viewer_bus_runner} $<TARGET_FILE:viewer-smoke>
      --gtest_filter=MenuLayout.SeparatorsRenderAsPhysicalHairlines)
    set_tests_properties("${separator_test}" PROPERTIES TIMEOUT 30 ENVIRONMENT
      "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=rhi;QSG_RHI_BACKEND=opengl;QSG_RENDER_LOOP=basic;LIBGL_ALWAYS_SOFTWARE=1;QT_SCALE_FACTOR=${scale};HOLONIGHT_APPEARANCE_FILE=${CMAKE_SOURCE_DIR}/tests/fixtures/${theme}.toml;QML_IMPORT_PATH=${HOLONIGHT_QML_IMPORT_PATH};LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR};VIEWER_CAPTURE_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/${separator_test}")
  endforeach()
endforeach()
