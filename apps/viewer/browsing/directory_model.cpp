#include "directory_model.h"

#include "local_url.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <HolonightFileBrowser/directory_scan.h>
#include <HolonightFileBrowser/native_path.h>
#include <HolonightFileBrowser/natural_order.h>
#include <algorithm>
#include <holonight_images/image.h>
#include <sys/stat.h>

bool naturalFileNameLess(const QString& left, const QString& right) {
  return HolonightFileBrowser::naturalFileNameLess(left, right);
}

DirectoryResult scanDirectory(const QUrl& selected, const std::atomic_bool& cancelled) {
  DirectoryResult result;
  const auto explicitUrl = normalizedLocalUrl(selected);
  const QFileInfo selectedInfo(explicitUrl.toLocalFile());
  QSet<QString> suffixes;
  for (const auto& format : HolonightImages::supportedSuffixes()) {
    suffixes.insert(format.toCaseFolded());
  }
  // SVG decodes via QSvgRenderer, not QImageReader, so it is listed unconditionally here.
  suffixes.insert(QStringLiteral("svg"));
  const auto selectedPath = HolonightFileBrowser::nativePath(explicitUrl);
  const auto folder = selectedPath.left(std::max<qsizetype>(1, selectedPath.lastIndexOf('/')));
  // The containing-folder adapter retains Viewer's filtering and explicit-file policy.
  auto cancel = std::shared_ptr<const std::atomic_bool>(&cancelled, [](auto*) {});
  HolonightFileBrowser::walkDirectory(
      QFile::decodeName(folder), cancel, {}, -1, {},
      [&](const QList<DirectoryEntry>& entries, bool, const QString& error) {
        if (!error.isEmpty()) {
          result.urls.clear();
          result.error = DirectoryModel::tr("The containing folder could not be read: %1").arg(error);
          return;
        }
        for (const auto& entry : entries) {
          if (!entry.name.startsWith(u'.') && S_ISREG(entry.mode) &&
              suffixes.contains(QFileInfo(entry.name).suffix().toCaseFolded())) {
            result.urls.append(HolonightFileBrowser::localUrl(entry.native_path));
          }
        }
      },
      folder);
  if (cancelled.load()) {
    return {};
  }
  if (!result.urls.contains(explicitUrl)) {
    result.urls.append(explicitUrl);
    if (!selectedInfo.isFile()) {
      result.missing = explicitUrl;
    }
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
void DirectoryModel::replace(QList<QUrl> urls, QUrl missing) {
  beginResetModel();
  urls_ = std::move(urls);
  missing_ = std::move(missing);
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
  ++generation_;
  pending_.reset();
  if (cancellation_) {
    cancellation_->store(true);
  }
  error_.clear();
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
            [this, generation, result = std::move(result)] mutable {
              busy_ = false;
              cancellation_.reset();
              if (stopping_) {
                thread_.quit();
                return;
              }
              if (generation == generation_) {
                scanning_ = false;
                error_ = std::move(result.error);
                replace(std::move(result.urls), std::move(result.missing));
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
