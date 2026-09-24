#include "image_canvas.h"

#include "image_document.h"
#include "image_orientation.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPainter>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

ImageCanvas::ImageCanvas(QQuickItem* parent) : QQuickPaintedItem(parent) {
  QCoreApplication::instance()->installEventFilter(this);
}
bool ImageCanvas::eventFilter(QObject* watched, QEvent* event) {
  if (watched == window()) {
    if (event->type() == QEvent::MouseMove) {
      emit mouseMoved();
    }
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
      emit keyboardInput();
    }
  }
  return false;
}
void ImageCanvas::refresh() {
  update();
  emit viewChanged();
}
void ImageCanvas::setImage(const QImage& image) {
  if (svg_renderer_ == nullptr && image_.cacheKey() == image.cacheKey()) {
    return;
  }
  ++generation_;
  image_ = image;
  svg_renderer_ = nullptr;
  content_size_ = image.size();
  view_.setImage((orientation_ & 1) != 0 ? content_size_.transposed() : content_size_);
  refresh();
  emit imageChanged();
}
void ImageCanvas::setSvgRenderer(QSvgRenderer* renderer) {
  if (svg_renderer_ == renderer) {
    return;
  }
  ++generation_;
  svg_renderer_ = renderer;
  image_ = {};
  content_size_ = {};
  if (renderer != nullptr && renderer->isValid()) {
    content_size_ = svg_size_.isEmpty() ? svgIntrinsicSize(*renderer) : svg_size_;
  }
  view_.setImage((orientation_ & 1) != 0 ? content_size_.transposed() : content_size_);
  refresh();
  emit imageChanged();
}
void ImageCanvas::setSvgSize(QSizeF size) {
  if (svg_size_ == size) {
    return;
  }
  svg_size_ = size;
  if (svg_renderer_ != nullptr && svg_renderer_->isValid()) {
    content_size_ = size.isEmpty() ? svgIntrinsicSize(*svg_renderer_) : size;
    view_.setImage((orientation_ & 1) != 0 ? content_size_.transposed() : content_size_);
    refresh();
  }
  emit imageChanged();
}
void ImageCanvas::replaceFrame(const QImage& frame) {
  if (image_.isNull() || frame.size() != image_.size()) {
    setImage(frame);
    return;
  }
  image_ = frame;
  update();
}
void ImageCanvas::setOrientation(int orientation) {
  if (orientation < 0 || orientation > 7) {
    return;
  }
  orientation_ = orientation;
  view_.setImage((orientation_ & 1) != 0 ? content_size_.transposed() : content_size_);
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
  const auto transform = ImageOrientation::matrix(orientation_);
  const auto bounds = transform.mapRect(QRectF(QPointF{}, content_size_));
  const auto mapping = transform * QTransform::fromTranslate(-bounds.x(), -bounds.y());
  painter->translate(destination.topLeft());
  painter->scale(view_.scale(), view_.scale());
  painter->setTransform(mapping, true);
  // Dual-mode paint (REQ-NF-003): raster formats rasterize once at decode time and are blitted every frame; SVG
  // has no raster form to blit and is painted as vector geometry directly under the current transform, so it
  // stays crisp at any zoom instead of resampling a fixed-resolution QImage.
  if (svg_renderer_ != nullptr && svg_renderer_->isValid()) {
    svg_renderer_->render(painter, QRectF(QPointF(0, 0), content_size_));
  } else {
    const auto decodedSource = mapping.inverted().mapRect(source);
    painter->drawImage(decodedSource, image_, decodedSource);
  }
  if (painted_generation_ != generation_) {
    painted_generation_ = generation_;
    QMetaObject::invokeMethod(
        this,
        [this, generation = generation_] {
          if (generation == generation_ &&
              (!image_.isNull() || (svg_renderer_ != nullptr && svg_renderer_->isValid()))) {
            emit firstRendered();
          }
        },
        Qt::QueuedConnection);
  }
}
