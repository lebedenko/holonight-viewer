#include "image_document.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QtEndian>

#include <utility>
#include <webp/decode.h>

namespace {
constexpr qint64 image_limit = 128 * 1024 * 1024;
constexpr qint64 file_limit = 256 * 1024 * 1024;
bool acceptableSize(QSize size) {
  return size.width() > 0 && size.height() > 0 && size.width() <= 32768 && size.height() <= 32768 &&
         static_cast<qint64>(size.width()) * size.height() <= 32000000;
}
// Only the simple single-bitstream RIFF form is eligible. Extended containers,
// ancillary chunks and animation retain the installed Qt decoder's interpretation.
QImage simpleWebPFallback(QFile& file, const std::atomic_bool& cancelled, bool& limited) {
  if (cancelled.load() || !file.seek(0)) {
    return {};
  }
  const auto header = file.peek(20);
  if (header.size() != 20 || header.first(4) != "RIFF" || header.sliced(8, 4) != "WEBP" ||
      (header.sliced(12, 4) != "VP8 " && header.sliced(12, 4) != "VP8L")) {
    return {};
  }
  const auto bytes = file.read(file_limit + 1);
  if (cancelled.load() || bytes.size() < 20 || bytes.size() > file_limit || bytes.first(4) != "RIFF" ||
      bytes.sliced(8, 4) != "WEBP" || (bytes.sliced(12, 4) != "VP8 " && bytes.sliced(12, 4) != "VP8L")) {
    return {};
  }
  const auto* data = static_cast<const uint8_t*>(static_cast<const void*>(bytes.constData()));
  const quint64 riff_size = qFromLittleEndian<quint32>(bytes.sliced(4, 4).constData());
  const quint64 chunk_size = qFromLittleEndian<quint32>(bytes.sliced(16, 4).constData());
  if (riff_size + 8 != static_cast<quint64>(bytes.size()) ||
      20 + chunk_size + (chunk_size & 1) != static_cast<quint64>(bytes.size())) {
    return {};
  }
  int width = 0;
  int height = 0;
  if (WebPGetInfo(data, bytes.size(), &width, &height) == 0) {
    return {};
  }
  if (!acceptableSize({width, height}) || static_cast<qint64>(width) * height * 4 > image_limit) {
    limited = true;
    return {};
  }
  if (cancelled.load()) {
    return {};
  }
  QImage image(width, height, QImage::Format_RGBA8888);
  if (image.isNull() ||
      WebPDecodeRGBAInto(data, bytes.size(), image.bits(), image.sizeInBytes(),
                         static_cast<int>(image.bytesPerLine())) == nullptr ||
      cancelled.load()) {
    return {};
  }
  return image;
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
  QImageReader reader(&file);
  reader.setAutoTransform(true);
  const bool readable = reader.canRead();
  facts.format = QString::fromLatin1(reader.format()).toUpper();
  const auto dimensions = readable ? reader.size() : QSize{};
  if (dimensions.isValid() && !acceptableSize(dimensions)) {
    return {.image = {}, .error = limitError(), .information = facts};
  }
  QImage image;
  if (readable && dimensions.isValid()) {
    image = reader.read();
  }
  if (cancelled.load()) {
    return {};
  }
  if (image.isNull()) {
    bool limited = false;
    image = simpleWebPFallback(file, cancelled, limited);
    if (cancelled.load()) {
      return {};
    }
    if (limited) {
      return {.image = {}, .error = limitError(), .information = facts};
    }
    if (!image.isNull()) {
      facts.format = QStringLiteral("WEBP");
    } else {
      return {.image = {}, .error = decoderError(readable, dimensions), .information = facts};
    }
  }
  return {.image = std::move(image), .error = {}, .information = facts};
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
    return {.image = {}, .error = ImageDocument::tr("The file no longer exists."), .information = facts};
  }
  if (!info.isFile()) {
    return {.image = {},
            .error = ImageDocument::tr("Choose a regular image file, not a folder or special file."),
            .information = facts};
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return {.image = {}, .error = ImageDocument::tr("The file could not be opened for reading."), .information = facts};
  }
  if (file.size() > file_limit) {
    return {.image = {}, .error = ImageDocument::tr("The file exceeds the 256 MiB input limit."), .information = facts};
  }
  if (cancelled.load()) {
    return {};
  }
  auto decoded = readImage(file, cancelled, facts);
  if (decoded.image.isNull()) {
    return decoded;
  }
  auto image = std::move(decoded.image);
  facts = std::move(decoded.information);
  if (!acceptableSize(image.size()) || image.sizeInBytes() > image_limit) {
    return {.image = {}, .error = limitError(), .information = facts};
  }
  image.convertTo(QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {.image = {},
            .error = ImageDocument::tr("There is not enough memory to display this image."),
            .information = facts};
  }
  image.setDevicePixelRatio(1);
  facts.decodedSize = image.size();
  return {.image = std::move(image), .error = {}, .information = facts};
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

ImageDocument::ImageDocument(QObject* parent) : ImageDocument(decodeImage, parent) {}

ImageDocument::ImageDocument(Decoder decoder, QObject* parent)
    : ImageDocument(std::move(decoder), scanDirectory, parent) {}

ImageDocument::ImageDocument(Decoder decoder, DirectoryModel::Scanner scanner, QObject* parent)
    : QObject(parent), directory_(std::move(scanner)), worker_(new QObject), decoder_(std::move(decoder)) {
  // Set policy before any worker starts, including Qt's environment override.
  qputenv("QT_IMAGEIO_MAXALLOC", "128");
  QImageReader::setAllocationLimit(128);
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
  for (const auto& format : QImageReader::supportedImageFormats()) {
    patterns.append("*." + QString::fromLatin1(format));
  }
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
    orientation_ = 0;
    information_ = {};
    emit orientationChanged();
    selected_url_ = QUrl{};
    image_ = {};
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
    error_ = std::move(result.error);
    state_ = image_.isNull() ? Error : Ready;
    if (state_ == Error && error_.isEmpty()) {
      error_ = tr("The image could not be decoded.");
    }
    if (state_ == Error) {
      emit openingFailed(file_name_, error_);
    }
    emit changed();
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
  if (++finished_workers_ == 3) {
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
QString ImageDocument::informationText() const {
  const auto unavailable = tr("Unavailable");
  const auto dimensions = [&unavailable](QSize size) {
    return size.isValid() ? tr("%1 × %2 pixels").arg(size.width()).arg(size.height()) : unavailable;
  };
  return tr("Path: %1\n\nFormat: %2\nFile size: %3\nModified: %4\nDecoded dimensions: %5\nTransformed dimensions: %6")
      .arg(
          localPath(), information_.format.isEmpty() ? unavailable : information_.format,
          information_.encodedSize < 0 ? unavailable : tr("%1 bytes").arg(QLocale().toString(information_.encodedSize)),
          information_.modified.isValid() ? QLocale().toString(information_.modified.toLocalTime(), QLocale::LongFormat)
                                          : unavailable,
          dimensions(information_.decodedSize), dimensions(transformedDimensions()));
}

QString ImageDocument::formattedFileSize() const {
  return information_.encodedSize < 0
             ? tr("Unavailable")
             : QLocale().formattedDataSize(information_.encodedSize, 1, QLocale::DataSizeSIFormat);
}
