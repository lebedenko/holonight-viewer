#include "image_canvas.h"

#include "image_orientation.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

ImageCanvas::ImageCanvas(QQuickItem* parent) : QQuickPaintedItem(parent) {}
void ImageCanvas::refresh() {
  update();
  emit viewChanged();
}
void ImageCanvas::setImage(const QImage& image) {
  if (image_.cacheKey() == image.cacheKey()) {
    return;
  }
  image_ = image;
  view_.setImage(ImageOrientation::dimensions(orientation_, image.size()));
  refresh();
  emit imageChanged();
}
void ImageCanvas::setOrientation(int orientation) {
  if (orientation < 0 || orientation > 7) {
    return;
  }
  orientation_ = orientation;
  view_.setImage(ImageOrientation::dimensions(orientation_, image_.size()));
  view_.fit();
  refresh();
  emit orientationChanged();
}
void ImageCanvas::setDisplayPixelRatio(qreal ratio) {
  if (!std::isfinite(ratio) || ratio <= 0 || ratio == pixel_ratio_) {
    return;
  }
  pixel_ratio_ = ratio;
  view_.setViewport(size(), ratio);
  refresh();
  emit viewportChanged();
}
void ImageCanvas::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size()) {
    view_.setViewport(newGeometry.size(), pixel_ratio_);
    refresh();
    emit viewportChanged();
  }
}
void ImageCanvas::fit() {
  view_.fit();
  refresh();
}
void ImageCanvas::actualSize() {
  view_.actualSize();
  refresh();
}
void ImageCanvas::zoom(qreal factor, QPointF anchor) {
  view_.zoom(factor, anchor);
  refresh();
}
void ImageCanvas::zoomSteps(qreal steps, QPointF anchor) {
  if (std::isfinite(steps)) {
    zoom(std::pow(1.25, std::clamp(steps, -8.0, 8.0)), anchor);
  }
}
void ImageCanvas::pan(QPointF delta) {
  view_.pan(delta);
  refresh();
}
QRectF ImageCanvas::fitRect(QSize image, QSizeF canvas) {
  ViewGeometry view;
  view.setViewport(canvas, 1);
  view.setImage(image);
  return view.rect();
}
void ImageCanvas::paint(QPainter* painter) {
  if (!view_.valid()) {
    return;
  }
  const auto destination = view_.rect();
  const auto visible = destination.intersected(boundingRect());
  const QRectF source((visible.topLeft() - destination.topLeft()) / view_.scale(), visible.size() / view_.scale());
  painter->setClipRect(boundingRect());
  painter->setRenderHint(QPainter::SmoothPixmapTransform, view_.magnification() < 1);
  const auto mapping = ImageOrientation::mapping(orientation_, image_.size());
  const auto decodedSource = mapping.inverted().mapRect(source);
  painter->translate(destination.topLeft());
  painter->scale(view_.scale(), view_.scale());
  painter->setTransform(mapping, true);
  painter->drawImage(decodedSource, image_, decodedSource);
}
