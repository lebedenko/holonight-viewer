// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "image_decoder.h"

#include <QFileInfo>

#include <HolonightFileBrowser/preview/runtime_options.h>
#include <cstring>
#include <span>
namespace viewer_test {
inline HolonightFileBrowser::ImageRuntimeOptions imageRuntime = viewerImageRuntimeOptions();
inline DecodeResult decode(const QUrl& url, const std::atomic_bool& cancelled) {
  return decodeImageWithRuntime(url, cancelled, imageRuntime);
}
// Only for chooser/UI tests on the private bus, using PNG fixtures authored by those tests.
// This injected decoder never participates in runtime acceptance or application construction.
inline DecodeResult pngFixture(const QUrl& url, [[maybe_unused]] const std::atomic_bool& cancelled) {
  DecodeResult result;
  result.image = QImage(url.toLocalFile());
  result.information.format = "PNG";
  result.information.decoded_size = result.image.size();
  if (result.image.isNull()) {
    result.error = "Fixture could not be read.";
  }
  return result;
}
inline bool explicitRuntime = false;
inline void readImageRuntime(int& argc, char** argv) {
  auto arguments = std::span(argv, static_cast<std::size_t>(argc) + 1);
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(arguments[i], "--image-runtime") == 0) {
      explicitRuntime = true;
      imageRuntime.directory = QString::fromLocal8Bit(arguments[i + 1]);
      for (int j = i; j + 2 <= argc; ++j) {
        arguments[j] = arguments[j + 2];
      }
      argc -= 2;
      break;
    }
  }
}
}  // namespace viewer_test
