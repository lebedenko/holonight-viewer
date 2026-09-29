#include "thumbnail_decoder.h"

#include "image_limits.h"
#include "thumbnail_size.h"

#include <QFile>
#include <QFileInfo>

#include <holonight_images/svg.h>
#include <utility>

namespace {
ThumbnailResult failure(QString error) { return {.image = {}, .sourceSize = {}, .error = std::move(error)}; }

QString outcomeError(HolonightImages::Outcome outcome) {
  using HolonightImages::Outcome;
  switch (outcome) {
    case Outcome::Success:
      return {};
    case Outcome::Cancelled:
      return QStringLiteral("Cancelled");
    case Outcome::Unsupported:
      return QStringLiteral("The image format is not recognized.");
    case Outcome::Damaged:
      return QStringLiteral("The image is damaged or could not be decoded.");
    case Outcome::ResourceLimit:
      return QStringLiteral("The image exceeds the viewing limit.");
    case Outcome::IoFailure:
      return QStringLiteral("The image could not be read.");
  }
  Q_UNREACHABLE();
}

ThumbnailResult finish(QImage image, QSize sourceSize) {
  image.convertTo(QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return failure(QStringLiteral("There is not enough memory to display this image."));
  }
  image.setDevicePixelRatio(1);
  return {.image = std::move(image), .sourceSize = sourceSize, .error = {}};
}

ThumbnailResult decodeSvg(QFile& file, int boxPixels, const std::atomic_bool& cancelled) {
  const auto source = HolonightImages::loadSvg(file, kSvgFileLimitBytes, cancelled);
  if (source.outcome != HolonightImages::Outcome::Success) {
    return failure(outcomeError(source.outcome));
  }
  const auto rendered = HolonightImages::rasterizeSvg(
      source.bytes, {.bound = {boxPixels, boxPixels}, .outputBytes = kImageLimitBytes}, cancelled);
  if (rendered.inspection.outcome != HolonightImages::Outcome::Success || rendered.image.isNull()) {
    return failure(outcomeError(rendered.inspection.outcome));
  }
  const QSize box(kThumbnailBoxLogical, kThumbnailBoxLogical);
  return finish(rendered.image, HolonightImages::svgPixelSize(rendered.inspection.facts.documentSize, box));
}
}  // namespace

ThumbnailResult decodeThumbnail(const ThumbnailRequest& request, const std::atomic_bool& cancelled) {
  if (cancelled.load()) {
    return failure(outcomeError(HolonightImages::Outcome::Cancelled));
  }
  const QFileInfo info(request.path);
  if (!info.isFile()) {
    return failure(QStringLiteral("Not a regular image file."));
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return failure(QStringLiteral("The file could not be opened for reading."));
  }
  if (info.suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0) {
    return decodeSvg(file, request.boxPixels, cancelled);
  }
  auto decoded = HolonightImages::decode(
      file, {.limits = kRasterLimits, .bound = {request.boxPixels, request.boxPixels}}, cancelled);
  if (decoded.outcome != HolonightImages::Outcome::Success || decoded.image.isNull()) {
    return failure(outcomeError(decoded.outcome));
  }
  return finish(std::move(decoded.image), decoded.inspection.orientedSize);
}
