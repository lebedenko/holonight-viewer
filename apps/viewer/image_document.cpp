#include "image_document.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>

#include <utility>

namespace {
constexpr qint64 image_limit = 128 * 1024 * 1024;
constexpr qint64 file_limit = 256 * 1024 * 1024;
bool acceptableSize(QSize size) {
  return size.width() > 0 && size.height() > 0 && size.width() <= 32768 && size.height() <= 32768 &&
         static_cast<qint64>(size.width()) * size.height() <= 32000000;
}
QString limitError() {
  return ImageDocument::tr("This image exceeds the viewing limit (32 million pixels or 128 MiB decoded).");
}
}  // namespace

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled) {
  if (cancelled.load()) {
    return {};
  }
  const QFileInfo info(url.toLocalFile());
  if (!info.exists()) {
    return {.image = {}, .error = ImageDocument::tr("The file no longer exists.")};
  }
  if (!info.isFile()) {
    return {.image = {}, .error = ImageDocument::tr("Choose a regular image file, not a folder or special file.")};
  }
  QFile file(info.absoluteFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return {.image = {}, .error = ImageDocument::tr("The file could not be opened for reading.")};
  }
  if (file.size() > file_limit) {
    return {.image = {}, .error = ImageDocument::tr("The file exceeds the 256 MiB input limit.")};
  }
  if (cancelled.load()) {
    return {};
  }
  QImageReader reader(&file);
  reader.setAutoTransform(true);
  if (!reader.canRead()) {
    return {.image = {}, .error = ImageDocument::tr("The image format is unsupported or its header is damaged.")};
  }
  if (!acceptableSize(reader.size())) {
    return {.image = {}, .error = limitError()};
  }
  QImage image = reader.read();
  if (cancelled.load()) {
    return {};
  }
  if (image.isNull()) {
    return {.image = {},
            .error = ImageDocument::tr("The image is damaged, unreadable, or exceeds the decode memory limit.")};
  }
  if (!acceptableSize(image.size()) || image.sizeInBytes() > image_limit) {
    return {.image = {}, .error = limitError()};
  }
  image.convertTo(QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {.image = {}, .error = ImageDocument::tr("There is not enough memory to display this image.")};
  }
  image.setDevicePixelRatio(1);
  return {.image = std::move(image), .error = {}};
}

QUrl commandLineUrl(const QString& argument) {
  QUrl parsed(argument, QUrl::StrictMode);
  if (!parsed.scheme().isEmpty()) {
    return parsed;
  }
  return QUrl::fromLocalFile(QDir::current().absoluteFilePath(argument));
}

ImageDocument::ImageDocument(QObject* parent) : ImageDocument(decodeImage, parent) {}

ImageDocument::ImageDocument(Decoder decoder, QObject* parent)
    : QObject(parent), worker_(new QObject), decoder_(std::move(decoder)) {
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
    selected_url_ = QUrl{};
    image_ = {};
    file_name_.clear();
    state_ = Error;
    error_ = tr("Open exactly one local image file. Remote URLs are not supported.");
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
  selected_url_ = url;
  selected_index_ = directory_.indexOf(url);
  image_ = {};
  error_.clear();
  file_name_ = url.fileName();
  state_ = Loading;
  pending_ = Request{.request_id = request_id_, .url = url, .cache_epoch = cache_epoch_};
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
        if (worker_cache_epoch_ != request.cache_epoch) {
          cache_.clear();
          displayed_.reset();
          worker_cache_epoch_ = request.cache_epoch;
        }
        auto entry = cache_.take(request.url);
        if (!request.prefetch && displayed_) {
          cache_.put(std::move(*displayed_));
          displayed_.reset();
        }
        DecodeResult result;
        if (entry) {
          result.image = entry->image;
        } else {
          entry = DecodedImageCache::metadata(request.url);
          result = decoder_(request.url, *cancel);
          entry->image = result.image;
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
  if (!request.prefetch && request.request_id == request_id_) {
    image_ = std::move(result.image);
    error_ = std::move(result.error);
    state_ = image_.isNull() ? Error : Ready;
    if (state_ == Error && error_.isEmpty()) {
      error_ = tr("The image could not be decoded.");
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
  pending_ = Request{.request_id = request_id_, .url = neighbor, .cache_epoch = cache_epoch_, .prefetch = true};
  startPending();
}

void ImageDocument::workerFinished() {
  if (++finished_workers_ == 2) {
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
  directory_.shutdown();
  if (!busy_) {
    thread_.quit();
  }
}
