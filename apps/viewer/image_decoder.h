#pragma once

#include "decoded_image_cache.h"

#include <QUrl>

#include <atomic>
#include <optional>

struct DecodeResult {
  QImage image;
  QString error;
  ImageInformation information;
  QByteArray svgData;  // Non-empty only for a successfully validated SVG.
  std::optional<HolonightImages::Outcome> outcome = std::nullopt;
  QSizeF svgSize = {};
  bool svgLocalImages = false;
  QImage svgPreview = {};
};

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled);
QString rasterError(HolonightImages::Outcome outcome);
