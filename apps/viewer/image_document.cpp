#include "image_document.h"

#include "image_information_formatter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QSvgRenderer>

#include <utility>

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
    : ImageDocument(std::move(decoder), std::move(scanner), PlaybackOptions{}, parent) {}

ImageDocument::ImageDocument(Decoder decoder, DirectoryModel::Scanner scanner, PlaybackOptions playback,
                             QObject* parent)
    : QObject(parent),
      animation_(std::move(playback.clock), std::move(playback.source), playback.execution),
      directory_(std::move(scanner)),
      retained_budget_(playback.retained_bytes),
      worker_(new QObject),
      decoder_(std::move(decoder)) {
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
      emit imageChanged();
      emit previewImageChanged();
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
  cancelPastedPath();
  if (stopping_) {
    return;
  }
  ++cache_epoch_;
  ++thumbnail_generation_;
  emit thumbnailGenerationChanged();
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
    svg_preview_ = {};
    svg_data_.clear();
    svg_size_ = {};
    file_name_.clear();
    state_ = Error;
    error_ = tr("Open exactly one local image file. Remote URLs are not supported.");
    emit openingFailed(file_name_, error_);
    directory_.clear();
    emit imageChanged();
    emit previewImageChanged();
    emit changed();
    return;
  }
  select(normalizedLocalUrl(urls.first()));
  directory_.scan(selected_url_);
}

void ImageDocument::cancelPastedPath() {
  if (tentative_active_ || (pending_ && pending_->tentative)) {
    ++request_id_;
    pending_.reset();
    if (cancellation_ && tentative_active_) {
      cancellation_->store(true);
    }
  }
}

void ImageDocument::tryOpenPastedPath(const QUrl& url) {
  cancelPastedPath();
  if (stopping_ || !isLocalUrl(url)) {
    return;
  }
  ++request_id_;
  if (cancellation_ && state_ != Loading) {
    cancellation_->store(true);
  }
  pending_ = Request{
      .request_id = request_id_, .url = normalizedLocalUrl(url), .cache_epoch = cache_epoch_, .tentative = true};
  startPending();
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
  svg_preview_ = {};
  svg_data_.clear();
  svg_size_ = {};
  error_.clear();
  file_name_ = url.fileName();
  state_ = Loading;
  pending_ = Request{.request_id = request_id_, .url = url, .cache_epoch = cache_epoch_};
  emit imageChanged();
  emit previewImageChanged();
  emit changed();
  startPending();
}
void ImageDocument::navigate(int direction) {
  cancelPastedPath();
  if ((direction < 0 && !canPrevious()) || (direction > 0 && !canNext())) {
    return;
  }
  direction_ = direction;
  select(directory_.urlAt(selected_index_ + direction));
}
void ImageDocument::previous() { navigate(-1); }
void ImageDocument::next() { navigate(1); }
QString ImageDocument::folderName() const {
  if (selected_url_.isEmpty()) {
    return {};
  }
  const auto name = QFileInfo(localPath()).absoluteDir().dirName();
  return name.isEmpty() ? QStringLiteral("/") : name;
}

void ImageDocument::openFromFolder(const QUrl& url) {
  cancelPastedPath();
  if (stopping_ || url == selected_url_ || directory_.indexOf(url) < 0) {
    return;
  }
  select(url);
}

void ImageDocument::refresh() {
  cancelPastedPath();
  if (stopping_ || selected_url_.isEmpty()) {
    return;
  }
  ++cache_epoch_;
  ++thumbnail_generation_;
  emit thumbnailGenerationChanged();
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
  tentative_active_ = request.tentative;
  cancellation_ = std::make_shared<std::atomic_bool>(false);
  const auto cancel = cancellation_;
  QMetaObject::invokeMethod(worker_, [this, request, cancel] { decodeRequest(request, cancel); }, Qt::QueuedConnection);
}

void ImageDocument::decodeRequest(const Request& request, const std::shared_ptr<std::atomic_bool>& cancel) {
  if (request.tentative) {
    DecodeResult result;
    const QFileInfo info(request.url.toLocalFile());
    if (!cancel->load() && info.isFile() && info.isReadable()) {
      result = decoder_(request.url, *cancel);
    }
    QMetaObject::invokeMethod(
        this, [this, request, result = std::move(result)] mutable { complete(request, std::move(result)); },
        Qt::QueuedConnection);
    return;
  }
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
  if (!request.prefetch) {
    // Playback holds the displayed frame and one look-ahead; the cache gets what remains of the shared budget.
    // A neighbor's format and size must never change the foreground reservation.
    cache_.setLimit(retained_budget_ - ((gif ? 2 : 1) * result.image.sizeInBytes()));
  }
  if (!cancel->load() && !result.image.isNull()) {
    if (request.prefetch || gif) {
      // Frame zero may be reused, but it must fit in the LRU rather than stay alive as an extra displayed frame.
      cache_.put(std::move(*entry));
    } else {
      displayed_ = std::move(entry);
    }
  }
  if (request.prefetch) {
    result = {};
  }
  QMetaObject::invokeMethod(
      this, [this, request, result = std::move(result)] mutable { complete(request, std::move(result)); },
      Qt::QueuedConnection);
}

bool ImageDocument::commitPastedPath(const Request& request, const DecodeResult& result) {
  bool valid = request.request_id == request_id_ && result.error.isEmpty() &&
               (!result.outcome || *result.outcome == HolonightImages::Outcome::Success) &&
               (!result.image.isNull() || !result.svg_data.isEmpty());
  if (valid && !result.svg_data.isEmpty()) {
    QSvgRenderer validation;
    validation.setOptions(QtSvg::DisableAnimations);
    valid = result.svg_local_images ? validation.load(request.url.toLocalFile()) : validation.load(result.svg_data);
  }
  if (!valid) {
    return false;
  }
  animation_.stop();
  orientation_ = 0;
  emit orientationChanged();
  selected_url_ = request.url;
  selected_index_ = -1;
  file_name_ = request.url.fileName();
  direction_ = 1;
  // Reset canvas content only after validation succeeds, including same-sized images and SVGs.
  information_ = {};
  image_ = {};
  svg_preview_ = {};
  svg_data_.clear();
  svg_size_ = {};
  error_.clear();
  state_ = Loading;
  emit imageChanged();
  emit previewImageChanged();
  emit changed();
  ++cache_epoch_;
  ++thumbnail_generation_;
  emit thumbnailGenerationChanged();
  const auto epoch = cache_epoch_;
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
  return true;
}

void ImageDocument::applyDecoded(const Request& request, DecodeResult result) {
  information_ = std::move(result.information);
  image_ = std::move(result.image);
  svg_preview_ = std::move(result.svg_preview);
  svg_data_ = std::move(result.svg_data);
  svg_size_ = result.svg_size;
  svg_local_images_ = result.svg_local_images;
  svg_renderer_.setOptions(QtSvg::DisableAnimations);
  error_ = result.outcome ? rasterError(*result.outcome) : std::move(result.error);
  if (error_.isEmpty() && information_.format == QLatin1String("SVG") &&
      !(svg_local_images_ ? svg_renderer_.load(request.url.toLocalFile()) : svg_renderer_.load(svg_data_))) {
    svg_data_.clear();
    svg_size_ = {};
    error_ = tr("The SVG file is damaged or could not be parsed.");
  }
  state_ = (image_.isNull() && svg_data_.isEmpty()) ? Error : Ready;
  if (state_ == Error && error_.isEmpty()) {
    error_ = tr("The image could not be decoded.");
  }
  if (state_ == Error) {
    svg_preview_ = {};
    emit openingFailed(file_name_, error_);
  }
  emit imageChanged();
  emit previewImageChanged();
  emit changed();
  if (state_ == Ready && information_.format == QLatin1String("GIF")) {
    animation_.start(localPath(), image_.size());
  }
}

void ImageDocument::complete(const Request& request, DecodeResult result) {
  busy_ = false;
  tentative_active_ = false;
  cancellation_.reset();
  if (stopping_) {
    thread_.quit();
    return;
  }
  if (request.tentative && !commitPastedPath(request, result)) {
    startPending();
    maybePrefetch();
    return;
  }
  if (!request.prefetch &&
      (request.request_id == request_id_ || (!request.tentative && state_ == Loading && selected_url_ == request.url &&
                                             (!pending_ || pending_->tentative))) &&
      result.outcome != HolonightImages::Outcome::Cancelled) {
    applyDecoded(request, std::move(result));
  }
  if (request.tentative) {
    directory_.scan(selected_url_);
    emit pastedPathOpened();
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
    if (information_.format == QLatin1String("SVG")) {
      clipboard_.copySvg({.bytes = svg_data_,
                          .local_path = localPath(),
                          .intrinsic_size = svg_size_,
                          .local_images = svg_local_images_},
                         orientation_, file_name_);
    } else {
      clipboard_.copyImage(image_, orientation_, file_name_);
    }
  }
}
void ImageDocument::copyPath() {
  if (!stopping_) {
    clipboard_.copyPath(localPath());
  }
}
QVariantList ImageDocument::informationSections() const { return formatInformationSections(information_, localPath()); }

QString ImageDocument::formattedFileSize() const { return formatFileSize(information_.encoded_size); }

QString ImageDocument::summaryLine() const {
  return formatSummaryLine(information_.format, information_.decoded_size, information_.encoded_size);
}
QString ImageDocument::transformedLine() const {
  return formatTransformedLine(information_.decoded_size, transformedDimensions());
}
QString ImageDocument::modifiedText() const { return formatModifiedText(information_.modified); }
QString ImageDocument::displayPath() const { return abbreviateHomePath(localPath()); }
