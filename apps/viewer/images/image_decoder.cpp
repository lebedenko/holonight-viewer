#include "image_decoder.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSvgRenderer>

#include <HolonightFileBrowser/preview/image_limits.h>
#include <HolonightFileBrowser/preview/svg_helper.h>
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
    result.svg_size = inspection.facts.documentSize;
    result.svg_local_images = true;
  } else if (inspection.outcome == HolonightImages::Outcome::Success) {
    result.svg_size = inspection.facts.documentSize;
  } else if (inspection.outcome == HolonightImages::Outcome::Cancelled) {
    result.outcome = inspection.outcome;
    return result;
  }
  if (cancelled.load()) {
    result.outcome = HolonightImages::Outcome::Cancelled;
    return result;
  }
  if (result.svg_size.isEmpty()) {
    result.error =
        inspection.outcome == HolonightImages::Outcome::Unsupported
            ? QCoreApplication::translate("ImageDocument", "The SVG contains unsupported resource references.")
            : QCoreApplication::translate("ImageDocument", "The SVG file is damaged or could not be parsed.");
    return result;
  }
  result.information.format = QStringLiteral("SVG");
  result.information.decoded_size = result.svg_size.toSize().expandedTo(QSize(1, 1));
  result.svg_data = source.bytes;
  const QSize previewBound(256, 256);
  result.svg_preview = result.svg_local_images
                           ? renderLocalSvg(renderer, result.svg_size, previewBound, cancelled)
                           : HolonightImages::rasterizeSvg(
                                 source.bytes, {.bound = previewBound, .outputBytes = kImageLimitBytes}, cancelled)
                                 .image;
  return result;
}
QString limitError() {
  return QCoreApplication::translate("ImageDocument",
                                     "This image exceeds the viewing limit (64 million pixels or 256 MiB decoded).");
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
HolonightFileBrowser::ImageRuntimeOptions viewerImageRuntimeOptions() {
  return {QString::fromUtf8(VIEWER_IMAGE_RUNTIME)};
}

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled) {
  return decodeImageWithRuntime(url, cancelled, viewerImageRuntimeOptions());
}

DecodeResult decodeImageWithRuntime(const QUrl& url, const std::atomic_bool& cancelled,
                                    const HolonightFileBrowser::ImageRuntimeOptions& runtime) {
  const HolonightImages::IsolatedClient client(runtime.directory);
  return decodeImageWithOperations(url, cancelled,
                                   {.metadata =
                                        [&client](QIODevice& file, const std::atomic_bool& cancel) {
                                          return client.readMetadata(file, kRasterLimits, cancel);
                                        },
                                    .decode =
                                        [&client](QIODevice& file, const std::atomic_bool& cancel) {
                                          return client.decode(file, {.limits = kRasterLimits, .bound = {}}, cancel);
                                        }});
}

DecodeResult decodeImageWithOperations(const QUrl& url, const std::atomic_bool& cancelled,
                                       const RasterOperations& operations) {
  if (cancelled.load()) {
    return {};
  }
  const QFileInfo info(url.toLocalFile());
  ImageInformation facts;
  if (info.isFile()) {
    facts.encoded_size = info.size();
    facts.modified = info.lastModified();
  }
  if (!info.exists()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "The file no longer exists."),
            .information = facts,
            .svg_data = {}};
  }
  if (!info.isFile()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument",
                                                 "Choose a regular image file, not a folder or special file."),
            .information = facts,
            .svg_data = {}};
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "The file could not be opened for reading."),
            .information = facts,
            .svg_data = {}};
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
            .svg_data = {}};
  }
  const auto metadata = operations.metadata(file, cancelled);
  if (metadata.runtime != HolonightImages::RuntimeStatus::Success) {
    DecodeResult failed;
    failed.information = std::move(facts);
    failed.runtime = metadata.runtime;
    failed.error = HolonightFileBrowser::imageRuntimeError(metadata.runtime);
    return failed;
  }
  facts.exif = ExifMetadata::format(metadata.result->facts, metadata.result->outcome);
  if (cancelled.load() || metadata.result->outcome == HolonightImages::Outcome::Cancelled) {
    DecodeResult stopped;
    stopped.outcome = HolonightImages::Outcome::Cancelled;
    return stopped;
  }
  // Each operation captures from zero and validates the runtime independently.
  // The same open device is retained, but these captures are not an atomic snapshot.
  auto reply = operations.decode(file, cancelled);
  DecodeResult decoded;
  decoded.information = facts;
  decoded.runtime = reply.runtime;
  if (reply.runtime != HolonightImages::RuntimeStatus::Success) {
    decoded.error = HolonightFileBrowser::imageRuntimeError(reply.runtime);
    return decoded;
  }
  decoded.outcome = reply.result->outcome;
  decoded.image = std::move(reply.result->image);
  decoded.information.format = QString::fromLatin1(reply.result->inspection.format).toUpper();
  if (decoded.outcome != HolonightImages::Outcome::Success || cancelled.load()) {
    decoded.image = {};
    if (cancelled.load()) {
      decoded.outcome = HolonightImages::Outcome::Cancelled;
    }
    return decoded;
  }
  if (decoded.image.isNull()) {
    return decoded;
  }
  auto image = std::move(decoded.image);
  facts = std::move(decoded.information);
  if (!acceptableImage(image)) {
    return {.image = {}, .error = limitError(), .information = facts, .svg_data = {}};
  }
  image.convertTo(QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {.image = {},
            .error = QCoreApplication::translate("ImageDocument", "There is not enough memory to display this image."),
            .information = facts,
            .svg_data = {}};
  }
  image.setDevicePixelRatio(1);
  facts.decoded_size = image.size();
  return {.image = std::move(image),
          .error = {},
          .information = facts,
          .svg_data = {},
          .outcome = HolonightImages::Outcome::Success};
}
