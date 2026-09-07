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
  ++request_id_;
  pending_.reset();
  if (cancellation_) {
    cancellation_->store(true);
  }
  image_ = {};
  error_.clear();
  file_name_.clear();
  if (urls.size() != 1 || !isLocalUrl(urls.first())) {
    state_ = Error;
    error_ = tr("Open exactly one local image file. Remote URLs are not supported.");
  } else {
    file_name_ = urls.first().fileName();
    state_ = Loading;
    pending_ = Request{.request_id = request_id_, .url = urls.first()};
  }
  emit changed();
  startPending();
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
        auto result = decoder_(request.url, *cancel);
        QMetaObject::invokeMethod(
            this,
            [this, request_id = request.request_id, result = std::move(result)]() mutable {
              complete(request_id, std::move(result));
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

void ImageDocument::complete(quint64 request_id, DecodeResult result) {
  busy_ = false;
  cancellation_.reset();
  if (stopping_) {
    thread_.quit();
    return;
  }
  if (request_id == request_id_) {
    image_ = std::move(result.image);
    error_ = std::move(result.error);
    state_ = image_.isNull() ? Error : Ready;
    if (state_ == Error && error_.isEmpty()) {
      error_ = tr("The image could not be decoded.");
    }
    emit changed();
  }
  startPending();
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
  connect(&thread_, &QThread::finished, this, &ImageDocument::shutdownFinished);
  if (!busy_) {
    thread_.quit();
  }
}
