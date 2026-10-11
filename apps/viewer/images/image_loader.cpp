#include "image_loader.h"

#include <QFileInfo>
#include <QMetaObject>

#include <utility>

ImageLoader::ImageLoader(Decoder decoder, qint64 retainedBytes, QObject* parent)
    : QObject(parent), worker_(new QObject), decoder_(std::move(decoder)), retained_budget_(retainedBytes) {
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(&thread_, &QThread::finished, this, &ImageLoader::shutdownFinished);
  thread_.start();
}

ImageLoader::~ImageLoader() {
  cancel();
  thread_.quit();
  thread_.wait();
}

void ImageLoader::submit(Request request, bool cancelActive) {
  if (stopping_ || (request.kind == Kind::Prefetch && pending_ && pending_->kind != Kind::Prefetch)) {
    return;
  }
  if (cancelActive && request.kind != Kind::Prefetch && cancellation_) {
    cancellation_->store(true);
  }
  pending_ = std::move(request);
  startPending();
}

void ImageLoader::cancel() {
  pending_.reset();
  if (cancellation_) {
    cancellation_->store(true);
  }
}

bool ImageLoader::cancelTentative() {
  const bool pending = pending_ && pending_->kind == Kind::Tentative;
  if (pending) {
    pending_.reset();
  }
  if (tentative_active_ && cancellation_) {
    cancellation_->store(true);
  }
  return tentative_active_ || pending;
}

void ImageLoader::startPending() {
  if (busy_ || delivering_ || !pending_ || stopping_) {
    return;
  }
  const auto request = *pending_;
  pending_.reset();
  busy_ = true;
  tentative_active_ = request.kind == Kind::Tentative;
  cancellation_ = std::make_shared<std::atomic_bool>(false);
  const auto cancel = cancellation_;
  QMetaObject::invokeMethod(worker_, [this, request, cancel] { decodeRequest(request, cancel); }, Qt::QueuedConnection);
}

void ImageLoader::decodeRequest(const Request& request, const std::shared_ptr<std::atomic_bool>& cancel) {
  if (request.kind == Kind::Tentative) {
    DecodeResult result;
    const QFileInfo info(request.url.toLocalFile());
    if (!cancel->load() && info.isFile() && info.isReadable()) {
      result = decoder_(request.url, *cancel);
    }
    QMetaObject::invokeMethod(
        this,
        [this, request, result = std::move(result), cancelled = cancel->load()] mutable {
          complete(request, std::move(result), cancelled);
        },
        Qt::QueuedConnection);
    return;
  }
  if (worker_cache_epoch_ != request.cache_epoch) {
    cache_.clear();
    displayed_.reset();
    worker_cache_epoch_ = request.cache_epoch;
  }
  auto entry = cache_.take(request.url);
  if (request.kind != Kind::Prefetch && displayed_) {
    cache_.put(std::move(*displayed_));
    displayed_.reset();
  }
  DecodeResult result;
  if (entry) {
    result.outcome = HolonightImages::Outcome::Success;
    result.image = entry->image;
    result.information = entry->information;
  } else {
    entry = DecodedImageCache::metadata(request.url);
    result = decoder_(request.url, *cancel);
    entry->image = result.image;
    entry->information = result.information;
  }
  const bool gif = result.information.format == QLatin1String("GIF");
  if (request.kind != Kind::Prefetch) {
    // Playback holds the displayed frame and one look-ahead; the cache gets what remains of the shared budget.
    // A neighbor's format and size must never change the foreground reservation.
    cache_.setLimit(retained_budget_ - ((gif ? 2 : 1) * result.image.sizeInBytes()));
  }
  if (!cancel->load() && !result.image.isNull()) {
    if (request.kind == Kind::Prefetch || gif) {
      // Frame zero may be reused, but it must fit in the LRU rather than stay alive as an extra displayed frame.
      cache_.put(std::move(*entry));
    } else {
      displayed_ = std::move(entry);
    }
  }
  if (request.kind == Kind::Prefetch) {
    result = {};
  }
  QMetaObject::invokeMethod(
      this,
      [this, request, result = std::move(result), cancelled = cancel->load()] mutable {
        complete(request, std::move(result), cancelled);
      },
      Qt::QueuedConnection);
}

void ImageLoader::retainTentative(const Request& request, const DecodeResult& result, quint64 epoch) {
  if (stopping_) {
    return;
  }
  auto entry = DecodedImageCache::metadata(request.url);
  entry.image = result.image;
  entry.information = result.information;
  QMetaObject::invokeMethod(
      worker_,
      [this, epoch, entry = std::move(entry)] mutable {
        cache_.clear();
        displayed_.reset();
        worker_cache_epoch_ = epoch;
        const bool gif = entry.information.format == QLatin1String("GIF");
        cache_.setLimit(retained_budget_ - ((gif ? 2 : 1) * entry.image.sizeInBytes()));
        if (!entry.image.isNull()) {
          if (gif) {
            cache_.put(std::move(entry));
          } else {
            displayed_ = std::move(entry);
          }
        }
      },
      Qt::QueuedConnection);
}

void ImageLoader::complete(const Request& request, DecodeResult result, bool cancelled) {
  busy_ = false;
  tentative_active_ = false;
  cancelled = cancelled || (cancellation_ && cancellation_->load());
  cancellation_.reset();
  if (stopping_) {
    thread_.quit();
    return;
  }
  if (cancelled) {
    result = {};
    result.outcome = HolonightImages::Outcome::Cancelled;
  }
  delivering_ = true;
  emit completed(request, std::move(result));
  delivering_ = false;
  startPending();
}

void ImageLoader::shutdown() {
  if (stopping_) {
    return;
  }
  stopping_ = true;
  cancel();
  if (!busy_) {
    thread_.quit();
  }
}
