#include "image_decoder.h"

#include "image_limits.h"
#include "svg_helper.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include <utility>

namespace {
// Worker-owned validation; persistent GUI renderers stay in ImageDocument.
DecodeResult decodeSvg(QFile& file, const std::atomic_bool& cancelled, ImageInformation facts) {
  const auto source = HolonightImages::loadSvg(file, kSvgFileLimitBytes, cancelled);
  DecodeResult result;
  result.information = std::move(facts);
  if (source.outcome != HolonightImages::Outcome::Success) {
    if (source.outcome == HolonightImages::Outcome::ResourceLimit) {
      result.error = QCoreApplication::translate("ImageDocument", "The SVG file exceeds the 10 MiB size limit.");
    } else {
      result.outcome = source.outcome;
    }
    return result;
  }
  QSvgRenderer renderer;
  const auto inspection = inspectViewerSvg(source.bytes, file.fileName(), renderer, cancelled);
  if (inspection.outcome == HolonightImages::Outcome::Unsupported && renderer.isValid()) {
    result.svgSize = inspection.facts.documentSize;
    result.svgLocalImages = true;
  } else if (inspection.outcome == HolonightImages::Outcome::Success) {
    result.svgSize = inspection.facts.documentSize;
  } else if (inspection.outcome == HolonightImages::Outcome::Cancelled) {
    result.outcome = inspection.outcome;
    return result;
  }
  if (cancelled.load()) {
    result.outcome = HolonightImages::Outcome::Cancelled;
    return result;
  }
  if (result.svgSize.isEmpty()) {
    result.error =
        inspection.outcome == HolonightImages::Outcome::Unsupported
            ? QCoreApplication::translate("ImageDocument", "The SVG contains unsupported resource references.")
            : QCoreApplication::translate("ImageDocument", "The SVG file is damaged or could not be parsed.");
    return result;
  }
  result.information.format = QStringLiteral("SVG");
  result.information.decodedSize = result.svgSize.toSize().expandedTo(QSize(1, 1));
  result.svgData = source.bytes;
  const QSize previewBound(256, 256);
  result.svgPreview = result.svgLocalImages
                          ? renderLocalSvg(renderer, result.svgSize, previewBound, cancelled)
                          : HolonightImages::rasterizeSvg(
                                source.bytes, {.bound = previewBound, .outputBytes = kImageLimitBytes}, cancelled)
                                .image;
  return result;
}
QString limitError() {
  return QCoreApplication::translate("ImageDocument",
                                     "This image exceeds the viewing limit (32 million pixels or 128 MiB decoded).");
}
}  // namespace

QString rasterError(HolonightImages::Outcome outcome) {
  using HolonightImages::Outcome;
  switch (outcome) {
    case Outcome::Success:
    case Outcome::Cancelled:
      return {};
    case Outcome::Unsupported:
      return QCoreApplication::translate("ImageDocument", "The image format is not recognized.");
    case Outcome::Damaged:
      return QCoreApplication::translate("ImageDocument", "The image is damaged or could not be decoded.");
    case Outcome::ResourceLimit:
      return limitError();
    case Outcome::IoFailure:
      return QCoreApplication::translate("ImageDocument", "The image could not be read.");
  }
  Q_UNREACHABLE();
}
namespace {
DecodeResult readImage(QFile& file, const std::atomic_bool& cancelled, ImageInformation facts) {
  auto result = HolonightImages::decode(file, {.limits = kRasterLimits, .bound = {}}, cancelled);
  facts.format = QString::fromLatin1(result.inspection.format).toUpper();
  return {
      .image = std::move(result.image), .error = {}, .information = facts, .svgData = {}, .outcome = result.outcome};
}
}  // namespace

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled) {
  if (cancelled.load()) {
    return {};
  }
  const QFileInfo info(url.toLocalFile());
  ImageInformation facts;
  if (info.isFile()) {
    facts.encodedSize = info.size();
    facts.modified = info.lastModified();
  }
  if (!info.exists()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "The file no longer exists."),
            .information = facts,
            .svgData = {}};
  }
  if (!info.isFile()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument",
                                                 "Choose a regular image file, not a folder or special file."),
            .information = facts,
            .svgData = {}};
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "The file could not be opened for reading."),
            .information = facts,
            .svgData = {}};
  }
  // SVG is dispatched by extension, not content-sniffing: it has no reliable magic-byte signature, and this is the
  // same signal REQ-F-001..003 already use for format registration and directory scanning. It skips EXIF entirely
  // (XML has none) and the raster kFileLimitBytes/kImageLimitBytes checks, which do not apply to it.
  if (info.suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0) {
    return decodeSvg(file, cancelled, facts);
  }
  if (file.size() > kFileLimitBytes) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "The file exceeds the 256 MiB input limit."),
            .information = facts,
            .svgData = {}};
  }
  facts.exif = ExifMetadata::read(file, cancelled);
  if (cancelled.load() || !file.seek(0)) {
    return {};
  }
  auto decoded = readImage(file, cancelled, facts);
  if (decoded.image.isNull()) {
    return decoded;
  }
  auto image = std::move(decoded.image);
  facts = std::move(decoded.information);
  if (!acceptableImage(image)) {
    return {.image = {}, .error = limitError(), .information = facts, .svgData = {}};
  }
  image.convertTo(QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "There is not enough memory to display this image."),
            .information = facts,
            .svgData = {}};
  }
  image.setDevicePixelRatio(1);
  facts.decodedSize = image.size();
  return {.image = std::move(image),
          .error = {},
          .information = facts,
          .svgData = {},
          .outcome = HolonightImages::Outcome::Success};
}
