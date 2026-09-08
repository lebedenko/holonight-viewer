#include "directory_model.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>

#include <algorithm>
#include <filesystem>

QUrl normalizedLocalUrl(const QUrl& url) {
  return QUrl::fromLocalFile(QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath()));
}

namespace {
bool asciiDigit(QChar character) { return character >= u'0' && character <= u'9'; }
QStringView digitRun(QStringView text, qsizetype& position) {
  auto begin = position;
  while (position < text.size() && asciiDigit(text[position])) {
    ++position;
  }
  while (begin < position && text[begin] == u'0') {
    ++begin;
  }
  return text.mid(begin, position - begin);
}
}  // namespace

bool naturalFileNameLess(const QString& left, const QString& right) {
  const auto foldedLeft = left.toCaseFolded();
  const auto foldedRight = right.toCaseFolded();
  qsizetype leftPosition = 0;
  qsizetype rightPosition = 0;
  while (leftPosition < foldedLeft.size() && rightPosition < foldedRight.size()) {
    if (asciiDigit(foldedLeft[leftPosition]) && asciiDigit(foldedRight[rightPosition])) {
      const auto leftDigits = digitRun(foldedLeft, leftPosition);
      const auto rightDigits = digitRun(foldedRight, rightPosition);
      if (leftDigits.size() != rightDigits.size()) {
        return leftDigits.size() < rightDigits.size();
      }
      const auto comparison = leftDigits.compare(rightDigits);
      if (comparison != 0) {
        return comparison < 0;
      }
      continue;
    }
    if (foldedLeft[leftPosition] != foldedRight[rightPosition]) {
      return foldedLeft[leftPosition] < foldedRight[rightPosition];
    }
    ++leftPosition;
    ++rightPosition;
  }
  if (leftPosition != foldedLeft.size() || rightPosition != foldedRight.size()) {
    return leftPosition == foldedLeft.size();
  }
  return left < right;
}

DirectoryResult scanDirectory(const QUrl& selected, const std::atomic_bool& cancelled) {
  DirectoryResult result;
  const auto explicitUrl = normalizedLocalUrl(selected);
  const QFileInfo selectedInfo(explicitUrl.toLocalFile());
  QSet<QString> suffixes;
  for (const auto& format : QImageReader::supportedImageFormats()) {
    suffixes.insert(QString::fromLatin1(format).toCaseFolded());
  }
  std::error_code error;
  const auto path = std::filesystem::path(selectedInfo.absolutePath().toStdString());
  std::filesystem::directory_iterator iterator(path, error);
  const std::filesystem::directory_iterator end;
  while (!error && iterator != end) {
    if (cancelled.load()) {
      return {};
    }
    const QFileInfo info(QString::fromStdString(iterator->path().string()));
    if (!info.isHidden() && info.isFile() && suffixes.contains(info.suffix().toCaseFolded())) {
      result.urls.append(QUrl::fromLocalFile(info.absoluteFilePath()));
    }
    iterator.increment(error);
  }
  if (error) {
    result.urls.clear();
    result.error =
        DirectoryModel::tr("The containing folder could not be read: %1").arg(QString::fromStdString(error.message()));
  }
  if (!result.urls.contains(explicitUrl)) {
    result.urls.append(explicitUrl);
  }
  struct Cancelled {};
  try {
    std::ranges::sort(result.urls, [&cancelled](const QUrl& left, const QUrl& right) {
      if (cancelled.load()) {
        throw Cancelled{};
      }
      return naturalFileNameLess(left.fileName(), right.fileName());
    });
  } catch (const Cancelled&) {
    return {};
  }
  return result;
}

DirectoryModel::DirectoryModel(QObject* parent) : DirectoryModel(scanDirectory, parent) {}
DirectoryModel::DirectoryModel(Scanner scanner, QObject* parent)
    : QAbstractListModel(parent), worker_(new QObject), scanner_(std::move(scanner)) {
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(&thread_, &QThread::finished, this, &DirectoryModel::shutdownFinished);
  thread_.start();
}
DirectoryModel::~DirectoryModel() {
  if (cancellation_) {
    cancellation_->store(true);
  }
  thread_.quit();
  thread_.wait();
}
int DirectoryModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(urls_.size());
}
QVariant DirectoryModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() >= rowCount()) {
    return {};
  }
  if (role == FileNameRole) {
    return urls_[index.row()].fileName();
  }
  if (role == UrlRole) {
    return urls_[index.row()];
  }
  return {};
}
QHash<int, QByteArray> DirectoryModel::roleNames() const { return {{FileNameRole, "fileName"}, {UrlRole, "url"}}; }
QUrl DirectoryModel::urlAt(int index) const { return index >= 0 && index < rowCount() ? urls_[index] : QUrl{}; }
int DirectoryModel::indexOf(const QUrl& url) const { return static_cast<int>(urls_.indexOf(url)); }
void DirectoryModel::replace(QList<QUrl> urls) {
  beginResetModel();
  urls_ = std::move(urls);
  endResetModel();
}
void DirectoryModel::clear() {
  ++generation_;
  pending_.reset();
  if (cancellation_) {
    cancellation_->store(true);
  }
  scanning_ = false;
  error_.clear();
  replace({});
  emit changed();
}
void DirectoryModel::scan(const QUrl& selected) {
  if (stopping_) {
    return;
  }
  clear();
  scanning_ = true;
  pending_ = normalizedLocalUrl(selected);
  replace({*pending_});
  emit changed();
  startPending();
}
void DirectoryModel::startPending() {
  if (busy_ || !pending_ || stopping_) {
    return;
  }
  const auto selected = *pending_;
  const auto generation = generation_;
  pending_.reset();
  busy_ = true;
  cancellation_ = std::make_shared<std::atomic_bool>(false);
  const auto cancel = cancellation_;
  QMetaObject::invokeMethod(
      worker_,
      [this, selected, generation, cancel] {
        auto result = scanner_(selected, *cancel);
        QMetaObject::invokeMethod(
            this,
            [this, generation, result = std::move(result)]() mutable {
              busy_ = false;
              cancellation_.reset();
              if (stopping_) {
                thread_.quit();
                return;
              }
              if (generation == generation_) {
                scanning_ = false;
                error_ = std::move(result.error);
                replace(std::move(result.urls));
                emit changed();
              }
              startPending();
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}
void DirectoryModel::shutdown() {
  if (stopping_) {
    return;
  }
  stopping_ = true;
  clear();
  if (!busy_) {
    thread_.quit();
  }
}
