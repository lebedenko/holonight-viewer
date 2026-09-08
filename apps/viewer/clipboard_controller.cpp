#include "clipboard_controller.h"

#include "clipboard_png_p.h"
#include "image_orientation.h"

#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>

#include <exception>
#include <utility>

ClipboardController::ClipboardController(QObject* parent) : ClipboardController(ImageOrientation::apply, parent) {}
ClipboardController::ClipboardController(Prepare prepare, QObject* parent)
    : QObject(parent), worker_(new QObject), prepare_(std::move(prepare)) {
  worker_->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
  connect(&thread_, &QThread::finished, this, &ClipboardController::shutdownFinished);
  thread_.start();
}
ClipboardController::~ClipboardController() {
  thread_.quit();
  thread_.wait();
}
void ClipboardController::copyImage(const QImage& image, int orientation, const QString& fileName) {
  if (busy_ || stopping_ || image.isNull()) {
    return;
  }
  busy_ = true;
  feedback_ = tr("Preparing %1 for copying…").arg(fileName);
  emit changed();
  QMetaObject::invokeMethod(
      worker_,
      [this, image, orientation, fileName] {
        QByteArray png;
        try {
          const auto output = prepare_(image, orientation);
          QBuffer buffer(&png);
          if (output.isNull() || !buffer.open(QIODevice::WriteOnly) || !encodeClipboardPng(output, buffer)) {
            png.clear();
          }
        } catch (const std::exception&) {
          png.clear();  // Keep the previous clipboard intact if preparation or encoding fails.
        }
        QMetaObject::invokeMethod(
            this,
            [this, png = std::move(png), fileName] {
              busy_ = false;
              if (stopping_) {
                return;
              }
              if (png.isEmpty()) {
                feedback_ = tr("Could not prepare %1 for copying.").arg(fileName);
              } else {
                auto* mime = new QMimeData;
                mime->setData("image/png", png);
                QGuiApplication::clipboard()->setMimeData(mime, QClipboard::Clipboard);
                feedback_ = tr("Copied %1.").arg(fileName);
              }
              emit changed();
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}
void ClipboardController::copyPath(const QString& path) {
  if (busy_ || stopping_ || path.isEmpty()) {
    return;
  }
  QGuiApplication::clipboard()->setText(path, QClipboard::Clipboard);
  feedback_ = tr("Copied path for %1.").arg(QFileInfo(path).fileName());
  emit changed();
}
void ClipboardController::shutdown() {
  if (stopping_) {
    return;
  }
  stopping_ = true;
  thread_.quit();
}
