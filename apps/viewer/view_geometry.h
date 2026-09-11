#pragma once
#include <QRectF>
#include <QSize>

// All points are source pixels or canvas-local logical pixels; magnification is physical.
class ViewGeometry {
 public:
  void setImage(QSize image);
  void setViewport(QSizeF canvas, qreal pixelRatio);
  void fit();
  void actualSize();
  void zoom(qreal factor, QPointF anchor);
  void pan(QPointF delta);
  [[nodiscard]] bool valid() const;
  [[nodiscard]] bool fitting() const { return fitting_; }
  [[nodiscard]] qreal magnification() const { return magnification_; }
  [[nodiscard]] qreal scale() const { return magnification_ / pixel_ratio_; }
  [[nodiscard]] QRectF rect() const;
  [[nodiscard]] QPointF center() const { return center_; }
  [[nodiscard]] bool canPan() const;

 private:
  [[nodiscard]] qreal fitMagnification() const;
  void constrain();
  QSize image_;
  QSizeF canvas_;
  qreal pixel_ratio_ = 1;
  qreal magnification_ = 1;
  QPointF center_;
  bool fitting_ = true;
};
