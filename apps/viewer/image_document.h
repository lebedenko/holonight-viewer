#pragma once

#include "clipboard_controller.h"
#include "decoded_image_cache.h"
#include "directory_model.h"
#include "image_orientation.h"

#include <QImage>
#include <QObject>
#include <QThread>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

struct DecodeResult {
  QImage image;
  QString error;
  ImageInformation information;
};

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled);
QUrl commandLineUrl(const QString& argument);

// QObject owns identity and disables copying/moving.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class ImageDocument : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Created by the application")
  Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)
  Q_PROPERTY(QSize transformedDimensions READ transformedDimensions NOTIFY changed)
  Q_PROPERTY(QString localPath READ localPath NOTIFY changed)
  Q_PROPERTY(QString formattedFileSize READ formattedFileSize NOTIFY changed)
  Q_PROPERTY(QString informationText READ informationText NOTIFY changed)
  Q_PROPERTY(ClipboardController* clipboard READ clipboard CONSTANT)
  Q_PROPERTY(State state READ state NOTIFY changed)
  Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QImage image READ image NOTIFY changed)
  Q_PROPERTY(int position READ position NOTIFY changed)
  Q_PROPERTY(int count READ count NOTIFY changed)
  Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
  Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
  Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
  Q_PROPERTY(QString folderError READ folderError NOTIFY changed)
  Q_PROPERTY(QStringList nameFilters READ nameFilters CONSTANT)

 public:
  // Preserve the existing Qt-facing enum type and values.
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
  enum State { Empty, Loading, Ready, Error };
  Q_ENUM(State)
  using Decoder = std::function<DecodeResult(const QUrl&, const std::atomic_bool&)>;
  explicit ImageDocument(QObject* parent = nullptr);
  explicit ImageDocument(Decoder decoder, QObject* parent = nullptr);
  ImageDocument(Decoder decoder, DirectoryModel::Scanner scanner, QObject* parent = nullptr);
  ~ImageDocument() override;
  [[nodiscard]] int orientation() const { return orientation_; }
  [[nodiscard]] QSize transformedDimensions() const {
    return ImageOrientation::dimensions(orientation_, image_.size());
  }
  [[nodiscard]] QString localPath() const { return selected_url_.toLocalFile(); }
  [[nodiscard]] QString informationText() const;
  [[nodiscard]] QString formattedFileSize() const;
  [[nodiscard]] const ImageInformation& information() const { return information_; }
  ClipboardController* clipboard() { return &clipboard_; }
  Q_INVOKABLE void transform(int operation);
  Q_INVOKABLE void resetTransform();
  Q_INVOKABLE void copyImage();
  Q_INVOKABLE void copyPath();
  [[nodiscard]] State state() const { return state_; }
  [[nodiscard]] QString fileName() const { return file_name_; }
  [[nodiscard]] QString error() const { return error_; }
  [[nodiscard]] QImage image() const { return image_; }
  [[nodiscard]] int position() const { return selected_index_ + 1; }
  [[nodiscard]] int count() const { return directory_.rowCount(); }
  [[nodiscard]] bool scanning() const { return directory_.scanning(); }
  [[nodiscard]] bool canPrevious() const { return !stopping_ && !scanning() && selected_index_ > 0; }
  [[nodiscard]] bool canNext() const {
    return !stopping_ && !scanning() && selected_index_ >= 0 && selected_index_ + 1 < count();
  }
  [[nodiscard]] QString folderError() const { return directory_.error(); }
  Q_INVOKABLE void previous();
  Q_INVOKABLE void next();
  Q_INVOKABLE void refresh();
  static QStringList nameFilters();
  static bool isLocalUrl(const QUrl& url);
  Q_INVOKABLE void open(const QList<QUrl>& urls);
  Q_INVOKABLE void shutdown();

 signals:
  void openingFailed(QString fileName, QString error);
  void changed();
  void orientationChanged();
  void shutdownFinished();

 private:
  struct Request {
    quint64 requestId;
    QUrl url;
    quint64 cacheEpoch;
    bool prefetch = false;
  };
  void startPending();
  void complete(const Request& request, DecodeResult result);
  void select(const QUrl& url);
  void navigate(int direction);
  void maybePrefetch();
  void workerFinished();
  ClipboardController clipboard_;
  int orientation_ = 0;
  ImageInformation information_;
  DirectoryModel directory_;
  QUrl selected_url_;
  int selected_index_ = -1;
  int direction_ = 1;
  quint64 prefetched_selection_ = 0;
  quint64 cache_epoch_ = 0;
  int finished_workers_ = 0;
  // Accessed exclusively by the decode worker.
  quint64 worker_cache_epoch_ = 0;
  DecodedImageCache cache_;
  std::optional<CachedImage> displayed_;
  QThread thread_;
  QObject* worker_;
  Decoder decoder_;
  std::shared_ptr<std::atomic_bool> cancellation_;
  std::optional<Request> pending_;
  quint64 request_id_ = 0;
  bool busy_ = false;
  bool stopping_ = false;
  State state_ = Empty;
  QString file_name_;
  QString error_;
  QImage image_;
};
