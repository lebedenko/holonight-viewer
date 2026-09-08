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

class ImageDocument : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Created by the application")
  Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)
  Q_PROPERTY(QSize transformedDimensions READ transformedDimensions NOTIFY changed)
  Q_PROPERTY(QString localPath READ localPath NOTIFY changed)
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
  enum State { Empty, Loading, Ready, Error };
  Q_ENUM(State)
  using Decoder = std::function<DecodeResult(const QUrl&, const std::atomic_bool&)>;
  explicit ImageDocument(QObject* parent = nullptr);
  explicit ImageDocument(Decoder decoder, QObject* parent = nullptr);
  ~ImageDocument() override;
  int orientation() const { return orientation_; }
  QSize transformedDimensions() const { return ImageOrientation::dimensions(orientation_, image_.size()); }
  QString localPath() const { return selected_url_.toLocalFile(); }
  QString informationText() const;
  const ImageInformation& information() const { return information_; }
  ClipboardController* clipboard() { return &clipboard_; }
  Q_INVOKABLE void transform(int operation);
  Q_INVOKABLE void resetTransform();
  Q_INVOKABLE void copyImage();
  Q_INVOKABLE void copyPath();
  State state() const { return state_; }
  QString fileName() const { return file_name_; }
  QString error() const { return error_; }
  QImage image() const { return image_; }
  int position() const { return selected_index_ + 1; }
  int count() const { return directory_.rowCount(); }
  bool scanning() const { return directory_.scanning(); }
  bool canPrevious() const { return !stopping_ && !scanning() && selected_index_ > 0; }
  bool canNext() const { return !stopping_ && !scanning() && selected_index_ >= 0 && selected_index_ + 1 < count(); }
  QString folderError() const { return directory_.error(); }
  Q_INVOKABLE void previous();
  Q_INVOKABLE void next();
  Q_INVOKABLE void refresh();
  static QStringList nameFilters();
  static bool isLocalUrl(const QUrl& url);
  Q_INVOKABLE void open(const QList<QUrl>& urls);
  Q_INVOKABLE void shutdown();

 signals:
  void changed();
  void orientationChanged();
  void shutdownFinished();

 private:
  struct Request {
    quint64 request_id;
    QUrl url;
    quint64 cache_epoch;
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
