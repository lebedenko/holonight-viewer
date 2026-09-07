set_source_files_properties(../apps/viewer/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_executable(viewer-smoke smoke.cpp image_document_test.cpp)
qt_add_resources(viewer-smoke test-config PREFIX "/" BASE ../apps/viewer FILES ../apps/viewer/qtquickcontrols2.conf)
target_link_libraries(viewer-smoke PRIVATE viewer-ui viewer-uiplugin Qt6::Test GTest::gtest)
add_test(NAME viewer-smoke COMMAND viewer-smoke)
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
add_test(NAME viewer-opening COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/check-opening.py" $<TARGET_FILE:holonight-viewer>)
set_tests_properties(viewer-opening PROPERTIES TIMEOUT 30 ENVIRONMENT
  "QML_IMPORT_PATH=${HOLONIGHT_QML_IMPORT_PATH};LD_LIBRARY_PATH=${HOLONIGHT_DEPENDENCY_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
set_tests_properties(viewer-smoke PROPERTIES TIMEOUT 60)
