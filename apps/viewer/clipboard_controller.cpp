#include "clipboard_controller.h"

#include "clipboard_png_p.h"
#include "image_limits.h"
#include "image_orientation.h"

#include <QBuffer>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>
#include <exception>
#include <holonight_images/svg.h>
#include <utility>

QSize svgClipboardSize(QSizeF intrinsic) {
  const auto width = intrinsic.width();
  const auto height = intrinsic.height();
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) {
    return {};
  }
  constexpr qint64 pixels = std::min<qint64>(32000000, kImageLimitBytes / 4);
  const auto scale = std::min({1.0, 32768.0 / width, 32768.0 / height, std::sqrt(pixels / width / height)});
  if (scale <= 0) {
    return {};
  }
  QSize size(std::max(1, static_cast<int>(scale < 1 ? std::floor(width * scale) : std::round(width))),
             std::max(1, static_cast<int>(scale < 1 ? std::floor(height * scale) : std::round(height))));
  while (static_cast<qint64>(size.width()) * size.height() > pixels) {
    if (size.width() >= size.height()) {
      size.rwidth()--;
    } else {
      size.rheight()--;
    }
  }
  return size;
}

namespace {
QImage rasterizeClipboardSvg(const SvgClipboardSource& source) {
  const auto size = svgClipboardSize(source.intrinsic_size);
  if (size.isEmpty()) {
    return {};
  }
  if (!source.local_images) {
    const std::atomic_bool cancelled{false};
    return HolonightImages::rasterizeSvg(source.bytes, {.bound = size, .outputBytes = kImageLimitBytes}, cancelled)
        .image;
  }
  QSvgRenderer renderer;
  renderer.setOptions(QtSvg::DisableAnimations);
  if (!renderer.load(source.local_path)) {
    return {};
  }
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {};
  }
  image.fill(Qt::transparent);
  QPainter painter(&image);
  renderer.render(&painter, image.rect());
  return image;
}
}  // namespace

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
  if (image.isNull()) {
    return;
  }
  prepareCopy([image] { return image; }, orientation, fileName);
}
void ClipboardController::copySvg(SvgClipboardSource source, int orientation, const QString& fileName) {
  prepareCopy([source = std::move(source)] { return rasterizeClipboardSvg(source); }, orientation, fileName);
}
void ClipboardController::prepareCopy(std::function<QImage()> source, int orientation, const QString& fileName) {
  if (busy_ || stopping_) {
    return;
  }
  busy_ = true;
  feedback_ = tr("Preparing %1 for copying…").arg(fileName);
  emit changed();
  QMetaObject::invokeMethod(
      worker_,
      [this, source = std::move(source), orientation, fileName] {
        QByteArray png;
        try {
          const auto output = prepare_(source(), orientation);
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
