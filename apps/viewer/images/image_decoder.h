#pragma once

#include "image_information.h"

#include <QImage>
#include <QUrl>

#include <HolonightFileBrowser/preview/runtime_options.h>
#include <atomic>
#include <functional>
#include <optional>

struct DecodeResult {
  QImage image;
  QString error;
  ImageInformation information;
  QByteArray svg_data;  // Non-empty only for a successfully validated SVG.
  std::optional<HolonightImages::Outcome> outcome = std::nullopt;
  QSizeF svg_size = {};
  bool svg_local_images = false;
  QImage svg_preview = {};
  HolonightImages::RuntimeStatus runtime = HolonightImages::RuntimeStatus::Success;
};

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled);
QString rasterError(HolonightImages::Outcome outcome);

DecodeResult decodeImageWithRuntime(const QUrl& url, const std::atomic_bool& cancelled,
                                    const HolonightFileBrowser::ImageRuntimeOptions& runtime);

// Private adapter seam: synchronous operations retain caller ownership of the device.
struct RasterOperations {
  std::function<HolonightImages::IsolatedResult<HolonightImages::MetadataResult>(QIODevice&, const std::atomic_bool&)>
      metadata;
  std::function<HolonightImages::IsolatedResult<HolonightImages::DecodeResult>(QIODevice&, const std::atomic_bool&)>
      decode;
};
DecodeResult decodeImageWithOperations(const QUrl& url, const std::atomic_bool& cancelled,
                                       const RasterOperations& operations);
HolonightFileBrowser::ImageRuntimeOptions viewerImageRuntimeOptions();
