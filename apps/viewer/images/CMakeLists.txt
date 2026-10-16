add_library(viewer-images STATIC
  image_loader.h
  image_loader.cpp
  image_decoder.h
  image_decoder.cpp
  decoded_image_cache.h
  decoded_image_cache.cpp
  image_information.h
  exif_metadata.h
  exif_metadata.cpp
  image_orientation.h
  image_orientation.cpp
  frame_source.h
  frame_source.cpp
  frame_streamer.h
  frame_streamer.cpp
  local_url.h
  local_url.cpp
)
target_link_libraries(viewer-images PUBLIC HolonightFileBrowser::Preview Qt6::Core Qt6::Gui HolonightImages::Images PRIVATE Qt6::Svg)
target_include_directories(viewer-images PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
viewer_warnings(viewer-images)
