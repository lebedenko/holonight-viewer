#include "image_canvas.h"

#include <QPainter>

#include <algorithm>

ImageCanvas::ImageCanvas(QQuickItem* parent) : QQuickPaintedItem(parent) {}
void ImageCanvas::setImage(const QImage& image) {
  if (image_.cacheKey() == image.cacheKey()) {
    return;
  }
  image_ = image;
  update();
  emit imageChanged();
}
QRectF ImageCanvas::fitRect(QSize image, QSizeF canvas) {
  if (image.isEmpty() || canvas.isEmpty()) {
    return {};
  }
  const auto scale = std::min(canvas.width() / image.width(), canvas.height() / image.height());
  const QSizeF fitted(image.width() * scale, image.height() * scale);
  return {(canvas.width() - fitted.width()) / 2, (canvas.height() - fitted.height()) / 2, fitted.width(),
          fitted.height()};
}
void ImageCanvas::paint(QPainter* painter) {
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->drawImage(fitRect(image_.size(), size()), image_);
}
