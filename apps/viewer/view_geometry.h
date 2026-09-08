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
  bool valid() const;
  bool fitting() const { return fitting_; }
  qreal magnification() const { return magnification_; }
  qreal scale() const { return magnification_ / pixel_ratio_; }
  QRectF rect() const;
  QPointF center() const { return center_; }
  bool canPan() const;

 private:
  qreal fitMagnification() const;
  void constrain();
  QSize image_;
  QSizeF canvas_;
  qreal pixel_ratio_ = 1;
  qreal magnification_ = 1;
  QPointF center_;
  bool fitting_ = true;
};
