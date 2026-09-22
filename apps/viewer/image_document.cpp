#include "image_document.h"

#include "image_limits.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QPainter>
#include <QStringList>
#include <QSvgRenderer>
#include <QVariantMap>

#include <array>
#include <utility>

namespace {
constexpr qint64 kDisplayAndCacheBytes = 2 * kImageLimitBytes;
// Validation-only: constructed on the worker thread, discarded before this function returns. ImageDocument builds
// its own persistent QSvgRenderer once, on the GUI thread, from the source path while retaining these bytes.
DecodeResult decodeSvg(QFile& file, const std::atomic_bool& cancelled, ImageInformation facts) {
  const auto bytes = file.readAll();
  if (cancelled.load()) {
    return {};
  }
  // Loading by filename retains the document directory as the base for trusted local references.
  QSvgRenderer renderer;
  if (!renderer.load(file.fileName())) {
    return {.image = {},
            .error = ImageDocument::tr("The SVG file is damaged or could not be parsed."),
            .information = facts,
            .svgData = {}};
  }
  facts.format = QStringLiteral("SVG");
  facts.decodedSize = svgIntrinsicSize(renderer);
  return {.image = {}, .error = {}, .information = facts, .svgData = bytes};
}
QString limitError() {
  return ImageDocument::tr("This image exceeds the viewing limit (32 million pixels or 128 MiB decoded).");
}
QString decoderError(bool readable, QSize dimensions) {
  if (!readable) {
    return ImageDocument::tr("The image format is unsupported or its header is damaged.");
  }
  if (!dimensions.isValid()) {
    return ImageDocument::tr("The image dimensions could not be read by the installed decoder.");
  }
  return ImageDocument::tr("The image is damaged, unreadable, or exceeds the decode memory limit.");
}
DecodeResult readImage(QFile& file, const std::atomic_bool& cancelled, ImageInformation facts) {
  auto result = HolonightImages::decode(file, {.limits = kRasterLimits, .bound = {}}, cancelled);
  facts.format = QString::fromLatin1(result.inspection.format).toUpper();
  if (result.outcome == HolonightImages::Outcome::Cancelled) {
    return {};
  }
  if (result.outcome != HolonightImages::Outcome::Success) {
    const auto error =
        result.outcome == HolonightImages::Outcome::ResourceLimit
            ? limitError()
            : decoderError(result.outcome != HolonightImages::Outcome::Unsupported, result.inspection.sourceSize);
    return {.image = {}, .error = error, .information = facts, .svgData = {}};
  }
  return {.image = std::move(result.image), .error = {}, .information = facts, .svgData = {}};
}
}  // namespace

QSize svgIntrinsicSize(const QSvgRenderer& renderer) {
  const auto viewBox = renderer.viewBoxF();
  return viewBox.isValid() ? viewBox.size().toSize() : renderer.defaultSize();
}

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
    return {.image = {}, .error = ImageDocument::tr("The file no longer exists."), .information = facts, .svgData = {}};
  }
  if (!info.isFile()) {
    return {.image = {},
            .error = ImageDocument::tr("Choose a regular image file, not a folder or special file."),
            .information = facts,
            .svgData = {}};
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return {.image = {},
            .error = ImageDocument::tr("The file could not be opened for reading."),
            .information = facts,
            .svgData = {}};
  }
  // SVG is dispatched by extension, not content-sniffing: it has no reliable magic-byte signature, and this is the
  // same signal REQ-F-001..003 already use for format registration and directory scanning. It skips EXIF entirely
  // (XML has none) and the raster kFileLimitBytes/kImageLimitBytes checks, which do not apply to it.
  if (info.suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0) {
    if (file.size() > kSvgFileLimitBytes) {
      return {.image = {},
              .error = ImageDocument::tr("The SVG file exceeds the 10 MiB size limit."),
              .information = facts,
              .svgData = {}};
    }
    return decodeSvg(file, cancelled, facts);
  }
  if (file.size() > kFileLimitBytes) {
    return {.image = {},
            .error = ImageDocument::tr("The file exceeds the 256 MiB input limit."),
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
            .error = ImageDocument::tr("There is not enough memory to display this image."),
            .information = facts,
            .svgData = {}};
  }
  image.setDevicePixelRatio(1);
  facts.decodedSize = image.size();
  return {.image = std::move(image), .error = {}, .information = facts, .svgData = {}};
}

QUrl commandLineUrl(const QString& argument) {
  const QFileInfo local(argument);
  if (local.exists()) {
    return QUrl::fromLocalFile(local.absoluteFilePath());
  }
  QUrl parsed(argument, QUrl::StrictMode);
  if (!parsed.scheme().isEmpty()) {
    return parsed;
  }
  return QUrl::fromLocalFile(QDir::current().absoluteFilePath(argument));
}

QString abbreviateHomePath(const QString& absolutePath, const QString& home) {
  if (home.isEmpty()) {
    return absolutePath;
  }
  const auto normalizedHome = QDir::cleanPath(home);
  const auto normalizedPath = QDir::cleanPath(absolutePath);
  if (normalizedPath == normalizedHome) {
    return QStringLiteral("~");
  }
  // The separator check keeps "/home/alice2" from matching home "/home/alice".
  if (normalizedHome != u"/" && normalizedPath.startsWith(normalizedHome + u'/')) {
    return u'~' + normalizedPath.sliced(normalizedHome.size());
  }
  return absolutePath;
}

QString formatSummaryLine(const QString& format, QSize decodedSize, qint64 encodedSize) {
  QStringList parts{format};
  if (decodedSize.isValid() && !decodedSize.isEmpty()) {
    const auto megapixels = static_cast<double>(decodedSize.width()) * decodedSize.height() / 1'000'000.0;
    parts << ImageDocument::tr("%1 × %2").arg(decodedSize.width()).arg(decodedSize.height())
          << ImageDocument::tr("%1 MP").arg(QLocale().toString(megapixels, 'f', 1));
  }
  if (encodedSize >= 0) {
    parts << QLocale().formattedDataSize(encodedSize, 1, QLocale::DataSizeSIFormat);
  }
  const auto summary = joinNonEmpty(parts);
  return summary.isEmpty() ? ImageDocument::tr("Details unavailable") : summary;
}

QString formatTransformedLine(QSize decodedSize, QSize transformedSize) {
  if (!decodedSize.isValid() || transformedSize != decodedSize.transposed() || transformedSize == decodedSize) {
    return {};
  }
  return ImageDocument::tr("Rotated view %1 × %2").arg(transformedSize.width()).arg(transformedSize.height());
}

QString formatModifiedText(const QDateTime& modified) {
  return modified.isValid() ? QLocale().toString(modified.toLocalTime(), QLocale::ShortFormat) : QString{};
}

QString joinNonEmpty(const QStringList& parts, QStringView separator) {
  QStringList present;
  for (const auto& part : parts) {
    if (!part.isEmpty()) {
      present << part;
    }
  }
  return present.join(separator);
}

ImageDocument::ImageDocument(QObject* parent) : ImageDocument(decodeImage, parent) {}

ImageDocument::ImageDocument(Decoder decoder, QObject* parent)
    : ImageDocument(std::move(decoder), scanDirectory, parent) {}

ImageDocument::ImageDocument(Decoder decoder, DirectoryModel::Scanner scanner, QObject* parent)
    : QObject(parent), directory_(std::move(scanner)), worker_(new QObject), decoder_(std::move(decoder)) {
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(&thread_, &QThread::finished, this, &ImageDocument::workerFinished);
  connect(&directory_, &DirectoryModel::shutdownFinished, this, &ImageDocument::workerFinished);
  connect(&directory_, &DirectoryModel::changed, this, [this] {
    selected_index_ = directory_.indexOf(selected_url_);
    // Selection emits its own Loading change; scans only update browsing state.
    emit changed();
    maybePrefetch();
  });
  connect(&clipboard_, &ClipboardController::shutdownFinished, this, &ImageDocument::workerFinished);
  connect(&animation_, &AnimationController::shutdownFinished, this, &ImageDocument::workerFinished);
  // The displayed frame replaces image_ without a changed(): only the canvas needs to repaint.
  connect(&animation_, &AnimationController::frameReady, this, [this](const QImage& frame) {
    if (state_ == Ready) {
      image_ = frame;
      emit frameChanged();
    }
  });
  thread_.start();
}

ImageDocument::~ImageDocument() {
  // Normal application shutdown drains asynchronously before destruction.
  if (cancellation_) {
    cancellation_->store(true);
  }
  thread_.quit();
  thread_.wait();
}

bool ImageDocument::isLocalUrl(const QUrl& url) {
  return url.isValid() && url.isLocalFile() && url.host().isEmpty() && !url.toLocalFile().isEmpty() &&
         !url.hasQuery() && !url.hasFragment();
}

QStringList ImageDocument::nameFilters() {
  QStringList patterns;
  for (const auto& format : HolonightImages::supportedSuffixes()) {
    patterns.append("*." + format);
  }
  // SVG decodes via QSvgRenderer, not QImageReader, so it is listed unconditionally here.
  patterns.append(QStringLiteral("*.svg"));
  patterns.sort();
  patterns.removeDuplicates();
  return {tr("Images (%1)").arg(patterns.join(' ')), tr("All files (*)")};
}

void ImageDocument::open(const QList<QUrl>& urls) {
  if (stopping_) {
    return;
  }
  ++cache_epoch_;
  direction_ = 1;
  if (urls.size() != 1 || !isLocalUrl(urls.first())) {
    ++request_id_;
    pending_.reset();
    if (cancellation_) {
      cancellation_->store(true);
    }
    animation_.stop();
    orientation_ = 0;
    information_ = {};
    emit orientationChanged();
    selected_url_ = QUrl{};
    image_ = {};
    svg_data_.clear();
    file_name_.clear();
    state_ = Error;
    error_ = tr("Open exactly one local image file. Remote URLs are not supported.");
    emit openingFailed(file_name_, error_);
    directory_.clear();
    emit changed();
    return;
  }
  select(normalizedLocalUrl(urls.first()));
  directory_.scan(selected_url_);
}

void ImageDocument::select(const QUrl& url) {
  animation_.stop();
  ++request_id_;
  if (cancellation_) {
    cancellation_->store(true);
  }
  orientation_ = 0;
  information_ = {};
  emit orientationChanged();
  selected_url_ = url;
  selected_index_ = directory_.indexOf(url);
  image_ = {};
  svg_data_.clear();
  error_.clear();
  file_name_ = url.fileName();
  state_ = Loading;
  pending_ = Request{.requestId = request_id_, .url = url, .cacheEpoch = cache_epoch_};
  emit changed();
  startPending();
}
void ImageDocument::navigate(int direction) {
  if ((direction < 0 && !canPrevious()) || (direction > 0 && !canNext())) {
    return;
  }
  direction_ = direction;
  select(directory_.urlAt(selected_index_ + direction));
}
void ImageDocument::previous() { navigate(-1); }
void ImageDocument::next() { navigate(1); }
void ImageDocument::refresh() {
  if (stopping_ || selected_url_.isEmpty()) {
    return;
  }
  ++cache_epoch_;
  select(selected_url_);
  directory_.scan(selected_url_);
}

void ImageDocument::startPending() {
  if (busy_ || !pending_ || stopping_) {
    return;
  }
  const auto request = *pending_;
  pending_.reset();
  busy_ = true;
  cancellation_ = std::make_shared<std::atomic_bool>(false);
  const auto cancel = cancellation_;
  QMetaObject::invokeMethod(
      worker_,
      [this, request, cancel] {
        if (worker_cache_epoch_ != request.cacheEpoch) {
          cache_.clear();
          displayed_.reset();
          worker_cache_epoch_ = request.cacheEpoch;
        }
        auto entry = cache_.take(request.url);
        if (!request.prefetch && displayed_) {
          cache_.put(std::move(*displayed_));
          displayed_.reset();
        }
        DecodeResult result;
        if (entry) {
          result.image = entry->image;
          result.information = entry->information;
        } else {
          entry = DecodedImageCache::metadata(request.url);
          result = decoder_(request.url, *cancel);
          entry->image = result.image;
          entry->information = result.information;
        }
        if (!result.image.isNull()) {
          // Playback holds the displayed frame and one look-ahead; the cache gets what remains of the shared budget.
          const bool gif = result.information.format == QLatin1String("GIF");
          cache_.setLimit(gif ? kDisplayAndCacheBytes - (2 * result.image.sizeInBytes())
                              : DecodedImageCache::byteLimit);
        }
        if (!cancel->load() && !result.image.isNull()) {
          if (request.prefetch) {
            cache_.put(std::move(*entry));
          } else {
            displayed_ = std::move(entry);
          }
        }
        if (request.prefetch) {
          result = {};
        }
        QMetaObject::invokeMethod(
            this, [this, request, result = std::move(result)]() mutable { complete(request, std::move(result)); },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

void ImageDocument::complete(const Request& request, DecodeResult result) {
  busy_ = false;
  cancellation_.reset();
  if (stopping_) {
    thread_.quit();
    return;
  }
  if (!request.prefetch && request.requestId == request_id_) {
    information_ = std::move(result.information);
    image_ = std::move(result.image);
    svg_data_ = std::move(result.svgData);
    error_ = std::move(result.error);
    if (error_.isEmpty() && information_.format == QLatin1String("SVG") &&
        !svg_renderer_.load(request.url.toLocalFile())) {
      svg_data_.clear();
      error_ = tr("The SVG file is damaged or could not be parsed.");
    }
    state_ = (image_.isNull() && svg_data_.isEmpty()) ? Error : Ready;
    if (state_ == Error && error_.isEmpty()) {
      error_ = tr("The image could not be decoded.");
    }
    if (state_ == Error) {
      emit openingFailed(file_name_, error_);
    }
    emit changed();
    if (state_ == Ready && information_.format == QLatin1String("GIF")) {
      animation_.start(localPath(), image_.size());
    }
  }
  startPending();
  maybePrefetch();
}

void ImageDocument::maybePrefetch() {
  if (stopping_ || busy_ || pending_ || scanning() || state_ == Loading || selected_index_ < 0 ||
      prefetched_selection_ == request_id_) {
    return;
  }
  prefetched_selection_ = request_id_;
  const auto neighbor = directory_.urlAt(selected_index_ + direction_);
  if (neighbor.isEmpty()) {
    return;
  }
  pending_ = Request{.requestId = request_id_, .url = neighbor, .cacheEpoch = cache_epoch_, .prefetch = true};
  startPending();
}

void ImageDocument::workerFinished() {
  if (++finished_workers_ == 4) {
    emit shutdownFinished();
  }
}

void ImageDocument::shutdown() {
  if (stopping_) {
    return;
  }
  stopping_ = true;
  ++request_id_;
  pending_.reset();
  if (cancellation_) {
    cancellation_->store(true);
  }
  clipboard_.shutdown();
  animation_.shutdown();
  directory_.shutdown();
  if (!busy_) {
    thread_.quit();
  }
}

void ImageDocument::transform(int operation) {
  if (state_ != Ready || stopping_ || operation < 0 || operation > 7) {
    return;
  }
  orientation_ = ImageOrientation::compose(orientation_, operation);
  emit orientationChanged();
  emit changed();
}
void ImageDocument::resetTransform() {
  if (state_ != Ready || stopping_) {
    return;
  }
  orientation_ = 0;
  emit orientationChanged();
  emit changed();
}
void ImageDocument::copyImage() {
  if (state_ == Ready && !stopping_) {
    clipboard_.copyImage(image_, orientation_, file_name_);
  }
}
void ImageDocument::copyPath() {
  if (!stopping_) {
    clipboard_.copyPath(localPath());
  }
}
QVariantList ImageDocument::informationSections() const {
  struct Section {
    const char* key;
    QString label;
    QStringList lines;
  };
  const auto& exif = information_.exif;
  const auto path = localPath();
  // Keys are stable identifiers for QML; labels are translated for display.
  const std::array<Section, 3> sections{{
      {.key = "Camera",
       .label = tr("Camera"),
       .lines = {exif.camera, joinNonEmpty({exif.lens, exif.focalLength, exif.aperture}),
                 joinNonEmpty({exif.shutter, exif.iso})}},
      {.key = "Location", .label = tr("Location"), .lines = {joinNonEmpty({exif.location, exif.altitude})}},
      {.key = "File", .label = tr("File"), .lines = {path.isEmpty() ? QString{} : displayPath()}},
  }};
  QVariantList result;
  for (const auto& section : sections) {
    auto present = section.lines;
    present.removeAll(QString{});
    if (!present.isEmpty()) {
      result.append(QVariantMap{{QStringLiteral("key"), QString::fromLatin1(section.key)},
                                {QStringLiteral("label"), section.label},
                                {QStringLiteral("lines"), present}});
    }
  }
  return result;
}

QString ImageDocument::formattedFileSize() const {
  return information_.encodedSize < 0
             ? tr("Unavailable")
             : QLocale().formattedDataSize(information_.encodedSize, 1, QLocale::DataSizeSIFormat);
}

QImage ImageDocument::previewImage() {
  if (!image_.isNull()) {
    return image_;
  }
  if (state_ != Ready || information_.format != QLatin1String("SVG") || !svg_renderer_.isValid()) {
    return {};
  }
  constexpr int kPreviewMaxDimension = 256;  // Comfortably above the 96x96 popup box at any DPR this app targets.
  const auto raster =
      svgIntrinsicSize(svg_renderer_).scaled(kPreviewMaxDimension, kPreviewMaxDimension, Qt::KeepAspectRatio);
  if (raster.isEmpty()) {
    return {};
  }
  QImage preview(raster, QImage::Format_ARGB32_Premultiplied);
  preview.fill(Qt::transparent);
  QPainter painter(&preview);
  svg_renderer_.render(&painter, preview.rect());
  return preview;
}
