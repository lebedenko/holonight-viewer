add_library(viewer-thumbnails STATIC
  thumbnail_cache.h
  thumbnail_cache.cpp
  thumbnail_decoder.h
  thumbnail_decoder.cpp
  thumbnail_provider.h
  thumbnail_provider.cpp
  thumbnail_size.h
)
target_link_libraries(viewer-thumbnails PUBLIC Qt6::Quick PRIVATE viewer-images Qt6::Svg HolonightThumbnails::Thumbnails)
target_include_directories(viewer-thumbnails PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
viewer_warnings(viewer-thumbnails)
