#pragma once

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
};

DecodeResult decodeImage(const QUrl& url, const std::atomic_bool& cancelled);
QUrl commandLineUrl(const QString& argument);

class ImageDocument : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Created by the application")
  Q_PROPERTY(State state READ state NOTIFY changed)
  Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QImage image READ image NOTIFY changed)
  Q_PROPERTY(QStringList nameFilters READ nameFilters CONSTANT)

 public:
  enum State { Empty, Loading, Ready, Error };
  Q_ENUM(State)
  using Decoder = std::function<DecodeResult(const QUrl&, const std::atomic_bool&)>;
  explicit ImageDocument(QObject* parent = nullptr);
  explicit ImageDocument(Decoder decoder, QObject* parent = nullptr);
  ~ImageDocument() override;
  State state() const { return state_; }
  QString fileName() const { return file_name_; }
  QString error() const { return error_; }
  QImage image() const { return image_; }
  static QStringList nameFilters();
  static bool isLocalUrl(const QUrl& url);
  Q_INVOKABLE void open(const QList<QUrl>& urls);
  Q_INVOKABLE void shutdown();

 signals:
  void changed();
  void shutdownFinished();

 private:
  struct Request {
    quint64 request_id;
    QUrl url;
  };
  void startPending();
  void complete(quint64 request_id, DecodeResult result);
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
