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
      loader_(std::move(decoder), playback.retained_bytes) {
  connect(&loader_, &ImageLoader::completed, this, &ImageDocument::complete);
  connect(&loader_, &ImageLoader::shutdownFinished, this, [this] { participantFinished(ShutdownParticipant::Loader); });
  connect(&directory_, &DirectoryModel::shutdownFinished, this,
          [this] { participantFinished(ShutdownParticipant::Directory); });
  connect(&directory_, &DirectoryModel::changed, this, [this] {
    selected_index_ = directory_.indexOf(selected_url_);
    // Selection emits its own Loading change; scans only update browsing state.
    emit changed();
    maybePrefetch();
  });
  connect(&clipboard_, &ClipboardController::shutdownFinished, this,
          [this] { participantFinished(ShutdownParticipant::Clipboard); });
  connect(&animation_, &AnimationController::shutdownFinished, this,
          [this] { participantFinished(ShutdownParticipant::Animation); });
  // The displayed frame replaces image_ without a changed(): only the canvas needs to repaint.
  connect(&animation_, &AnimationController::frameReady, this, [this](const QImage& frame) {
    if (state_ == Ready) {
      image_ = frame;
      emit imageChanged();
      emit previewImageChanged();
      emit frameChanged();
    }
  });
}

ImageDocument::~ImageDocument() = default;

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
    loader_.cancel();
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
  if (loader_.cancelTentative()) {
    ++request_id_;
  }
}

void ImageDocument::tryOpenPastedPath(const QUrl& url) {
  cancelPastedPath();
  if (stopping_ || !isLocalUrl(url)) {
    return;
  }
  ++request_id_;
  loader_.submit({.request_id = request_id_,
                  .url = normalizedLocalUrl(url),
                  .cache_epoch = cache_epoch_,
                  .kind = ImageLoader::Kind::Tentative},
                 state_ != Loading);
}

void ImageDocument::select(const QUrl& url) {
  animation_.stop();
  ++request_id_;
  loader_.cancel();
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
  emit imageChanged();
  emit previewImageChanged();
  emit changed();
  loader_.submit({.request_id = request_id_, .url = url, .cache_epoch = cache_epoch_});
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
  loader_.retainTentative(request, result, cache_epoch_);
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
  if (stopping_) {
    return;
  }
  if (request.kind == ImageLoader::Kind::Tentative && !commitPastedPath(request, result)) {
    maybePrefetch();
    return;
  }
  if (request.kind != ImageLoader::Kind::Prefetch &&
      (request.request_id == request_id_ || (request.kind != ImageLoader::Kind::Tentative && state_ == Loading &&
                                             selected_url_ == request.url && !loader_.hasPendingForeground())) &&
      result.outcome != HolonightImages::Outcome::Cancelled) {
    applyDecoded(request, std::move(result));
  }
  if (request.kind == ImageLoader::Kind::Tentative) {
    directory_.scan(selected_url_);
    emit pastedPathOpened();
  }
  maybePrefetch();
}

void ImageDocument::maybePrefetch() {
  if (stopping_ || loader_.busy() || loader_.hasPending() || scanning() || state_ == Loading || selected_index_ < 0 ||
      prefetched_selection_ == request_id_) {
    return;
  }
  prefetched_selection_ = request_id_;
  const auto neighbor = directory_.urlAt(selected_index_ + direction_);
  if (neighbor.isEmpty() ||
      QFileInfo(neighbor.toLocalFile()).suffix().compare(QLatin1String("svg"), Qt::CaseInsensitive) == 0) {
    return;
  }
  loader_.submit(
      {.request_id = request_id_, .url = neighbor, .cache_epoch = cache_epoch_, .kind = ImageLoader::Kind::Prefetch});
}

void ImageDocument::participantFinished(ShutdownParticipant participant) {
  if (stopping_ && pending_shutdown_.remove(participant) && pending_shutdown_.isEmpty()) {
    emit shutdownFinished();
  }
}

void ImageDocument::shutdown() {
  if (stopping_) {
    return;
  }
  stopping_ = true;
  ++request_id_;
  pending_shutdown_ = {ShutdownParticipant::Loader, ShutdownParticipant::Directory, ShutdownParticipant::Clipboard,
                       ShutdownParticipant::Animation};
  loader_.shutdown();
  clipboard_.shutdown();
  animation_.shutdown();
  directory_.shutdown();
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
QList<InformationSection> ImageDocument::informationSections() const {
  return formatInformationSections(information_, localPath());
}

QString ImageDocument::formattedFileSize() const { return formatFileSize(information_.encoded_size); }

QString ImageDocument::summaryLine() const {
  return formatSummaryLine(information_.format, information_.decoded_size, information_.encoded_size);
}
QString ImageDocument::transformedLine() const {
  return formatTransformedLine(information_.decoded_size, transformedDimensions());
}
QString ImageDocument::modifiedText() const { return formatModifiedText(information_.modified); }
QString ImageDocument::displayPath() const { return abbreviateHomePath(localPath()); }
