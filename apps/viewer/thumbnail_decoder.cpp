#include "thumbnail_decoder.h"

#include "image_limits.h"
#include "svg_helper.h"
#include "thumbnail_size.h"

#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <holonight_images/image.h>
#include <holonight_images/svg.h>
#include <holonight_thumbnails/cache.h>
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

HolonightThumbnails::Request cacheRequest(const QFileInfo& info, QSize required, HolonightThumbnails::Kind kind) {
  return {.uri = QUrl::fromLocalFile(info.absoluteFilePath()),
          .modified = info.lastModified(),
          .size = info.size(),
          .required = required,
          .kind = kind,
          .revision = {}};
}

bool useDiskCache(int boxPixels) { return boxPixels > 0 && boxPixels <= 1024; }

ThumbnailResult decodeSvg(QFile& file, const QFileInfo& info, int boxPixels, const std::atomic_bool& cancelled) {
  const auto source = HolonightImages::loadSvg(file, kSvgFileLimitBytes, cancelled);
  if (source.outcome != HolonightImages::Outcome::Success) {
    return failure(outcomeError(source.outcome));
  }
  QSvgRenderer renderer;
  const auto inspection = inspectViewerSvg(source.bytes, file.fileName(), renderer, cancelled);
  if (renderer.isValid() && !cancelled.load()) {
    const auto size = inspection.facts.documentSize;
    auto result = finish(renderLocalSvg(renderer, size, {boxPixels, boxPixels}, cancelled),
                         HolonightImages::svgPixelSize(size, {kThumbnailBoxLogical, kThumbnailBoxLogical}));
    result.cacheEligible = false;
    return result;
  }
  if (inspection.outcome != HolonightImages::Outcome::Success) {
    return failure(outcomeError(inspection.outcome));
  }
  const QSize box(kThumbnailBoxLogical, kThumbnailBoxLogical);
  const auto sourceSize = HolonightImages::svgPixelSize(inspection.facts.documentSize, box);
  const auto cache =
      cacheRequest(info, HolonightImages::svgPixelSize(inspection.facts.documentSize, {boxPixels, boxPixels}),
                   HolonightThumbnails::Kind::Svg);
  if (useDiskCache(boxPixels)) {
    if (auto hit = HolonightThumbnails::lookup(cache, cancelled)) {
      return finish(std::move(*hit), sourceSize);
    }
  }
  auto rendered = HolonightImages::rasterizeSvg(
      source.bytes, {.bound = {boxPixels, boxPixels}, .outputBytes = kImageLimitBytes}, cancelled);
  if (rendered.inspection.outcome != HolonightImages::Outcome::Success || rendered.image.isNull()) {
    return failure(outcomeError(rendered.inspection.outcome));
  }
  auto result = finish(std::move(rendered.image), sourceSize);
  if (!result.image.isNull() && useDiskCache(boxPixels) && !cancelled.load() &&
      QFileInfo(info.absoluteFilePath()).size() == info.size() &&
      QFileInfo(info.absoluteFilePath()).lastModified() == info.lastModified()) {
    HolonightThumbnails::store(cache, result.image, cancelled);
  }
  return result;
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
    return decodeSvg(file, info, request.boxPixels, cancelled);
  }
  const auto inspection = HolonightImages::inspect(file, kRasterLimits, cancelled);
  if (inspection.outcome != HolonightImages::Outcome::Success) {
    return failure(outcomeError(inspection.outcome));
  }
  const QSize pixelBox(request.boxPixels, request.boxPixels);
  const auto required =
      inspection.orientedSize.width() <= request.boxPixels && inspection.orientedSize.height() <= request.boxPixels
          ? inspection.orientedSize
          : inspection.orientedSize.scaled(pixelBox, Qt::KeepAspectRatio);
  const auto cache = cacheRequest(info, required, HolonightThumbnails::Kind::Raster);
  if (useDiskCache(request.boxPixels)) {
    if (auto hit = HolonightThumbnails::lookup(cache, cancelled)) {
      return finish(std::move(*hit), inspection.orientedSize);
    }
  }
  auto decoded = HolonightImages::decode(
      file, {.limits = kRasterLimits, .bound = {request.boxPixels, request.boxPixels}}, cancelled);
  if (decoded.outcome != HolonightImages::Outcome::Success || decoded.image.isNull()) {
    return failure(outcomeError(decoded.outcome));
  }
  auto result = finish(std::move(decoded.image), decoded.inspection.orientedSize);
  if (!result.image.isNull() && useDiskCache(request.boxPixels) && !cancelled.load() &&
      QFileInfo(info.absoluteFilePath()).size() == info.size() &&
      QFileInfo(info.absoluteFilePath()).lastModified() == info.lastModified()) {
    HolonightThumbnails::store(cache, result.image, cancelled);
  }
  return result;
}
