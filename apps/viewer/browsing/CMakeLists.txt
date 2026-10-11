add_library(viewer-browsing STATIC
  directory_model.h
  directory_model.cpp
)
target_link_libraries(viewer-browsing PUBLIC Qt6::Core PRIVATE viewer-images HolonightImages::Images)
target_include_directories(viewer-browsing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
viewer_warnings(viewer-browsing)
