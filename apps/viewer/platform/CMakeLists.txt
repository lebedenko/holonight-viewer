find_package(PkgConfig REQUIRED)
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

add_library(viewer-platform STATIC
  portal_request_builder.h
  portal_request_builder.cpp
  wayland_foreign_export.h
  wayland_foreign_export.cpp
)
target_sources(viewer-platform PRIVATE "${xdg_foreign_code}" "${xdg_foreign_header}")
set_target_properties(viewer-platform PROPERTIES AUTOMOC OFF)
target_link_libraries(viewer-platform PUBLIC Qt6::Core Qt6::DBus PRIVATE Qt6::Gui Qt6::GuiPrivate PkgConfig::WaylandClient)
target_include_directories(viewer-platform SYSTEM PRIVATE "${xdg_foreign_dir}")
target_include_directories(viewer-platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
viewer_warnings(viewer-platform)
