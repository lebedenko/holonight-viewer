#include "view_geometry.h"

#include <algorithm>
#include <cmath>

namespace {
bool finite(QPointF point) { return std::isfinite(point.x()) && std::isfinite(point.y()); }
}  // namespace

bool ViewGeometry::valid() const { return !image_.isEmpty() && !canvas_.isEmpty(); }
qreal ViewGeometry::fitMagnification() const {
  return std::min(canvas_.width() / image_.width(), canvas_.height() / image_.height()) * pixel_ratio_;
}
void ViewGeometry::setImage(QSize image) {
  image_ = image;
  fit();
}
void ViewGeometry::setViewport(QSizeF canvas, qreal pixelRatio) {
  if (!std::isfinite(canvas.width()) || !std::isfinite(canvas.height()) || !std::isfinite(pixelRatio) ||
      pixelRatio <= 0) {
    return;
  }
  canvas_ = canvas;
  pixel_ratio_ = pixelRatio;
  if (valid()) {
    if (fitting_) {
      fit();
    } else {
      constrain();
    }
  }
}
void ViewGeometry::fit() {
  fitting_ = true;
  center_ = QPointF(image_.width() / 2.0, image_.height() / 2.0);
  magnification_ = valid() ? fitMagnification() : 1;
}
void ViewGeometry::actualSize() {
  if (!valid()) {
    return;
  }
  fitting_ = false;
  magnification_ = 1;
  center_ = QPointF(image_.width() / 2.0, image_.height() / 2.0);
  constrain();
}
QRectF ViewGeometry::rect() const {
  if (!valid()) {
    return {};
  }
  return {QPointF(canvas_.width() / 2, canvas_.height() / 2) - center_ * scale(), QSizeF(image_) * scale()};
}
void ViewGeometry::constrain() {
  const auto half_width = canvas_.width() / (2 * scale());
  const auto half_height = canvas_.height() / (2 * scale());
  center_.setX(image_.width() <= 2 * half_width ? image_.width() / 2.0
                                                : std::clamp(center_.x(), half_width, image_.width() - half_width));
  center_.setY(image_.height() <= 2 * half_height
                   ? image_.height() / 2.0
                   : std::clamp(center_.y(), half_height, image_.height() - half_height));
}
void ViewGeometry::zoom(qreal factor, QPointF anchor) {
  if (!valid() || !std::isfinite(factor) || factor <= 0 || !finite(anchor)) {
    return;
  }
  const auto minimum = std::min(0.01, fitMagnification());
  const auto maximum = std::max(32.0, fitMagnification());
  const auto lower = std::min(minimum, magnification_);
  const auto upper = std::max(maximum, magnification_);
  const auto next = std::clamp(magnification_ * factor, lower, upper);
  const auto source_anchor = (anchor - rect().topLeft()) / scale();
  fitting_ = false;
  magnification_ = next;
  center_ = source_anchor + (QPointF(canvas_.width() / 2, canvas_.height() / 2) - anchor) / scale();
  constrain();
}
void ViewGeometry::pan(QPointF delta) {
  if (!valid() || fitting_ || !finite(delta)) {
    return;
  }
  center_ -= delta / scale();
  constrain();
}
bool ViewGeometry::canPan() const {
  const auto destination = rect();
  return valid() && !fitting_ && (destination.width() > canvas_.width() || destination.height() > canvas_.height());
}
