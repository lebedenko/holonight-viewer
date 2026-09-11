#pragma once

#include <QImage>
#include <QObject>
#include <QThread>
#include <QtQml/qqmlregistration.h>

#include <functional>

// QObject owns identity and disables copying/moving.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class ClipboardController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Owned by ImageDocument")
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString feedback READ feedback NOTIFY changed)
 public:
  using Prepare = std::function<QImage(const QImage&, int)>;
  explicit ClipboardController(QObject* parent = nullptr);
  explicit ClipboardController(Prepare prepare, QObject* parent = nullptr);
  ~ClipboardController() override;
  [[nodiscard]] bool busy() const { return busy_; }
  [[nodiscard]] QString feedback() const { return feedback_; }
  void copyImage(const QImage& image, int orientation, const QString& fileName);
  void copyPath(const QString& path);
  void shutdown();
 signals:
  void changed();
  void shutdownFinished();

 private:
  QThread thread_;
  QObject* worker_;
  Prepare prepare_;
  bool busy_ = false;
  bool stopping_ = false;
  QString feedback_;
};
