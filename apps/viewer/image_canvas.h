#pragma once
#include "view_geometry.h"

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class ImageCanvas : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
  Q_PROPERTY(qreal displayPixelRatio READ displayPixelRatio WRITE setDisplayPixelRatio NOTIFY viewportChanged)
  Q_PROPERTY(bool fitting READ fitting NOTIFY viewChanged)
  Q_PROPERTY(qreal magnification READ magnification NOTIFY viewChanged)
  Q_PROPERTY(bool canPan READ canPan NOTIFY viewChanged)
  Q_PROPERTY(QRectF imageRect READ imageRect NOTIFY viewChanged)
 public:
  explicit ImageCanvas(QQuickItem* parent = nullptr);
  QImage image() const { return image_; }
  void setImage(const QImage& image);
  qreal displayPixelRatio() const { return pixel_ratio_; }
  void setDisplayPixelRatio(qreal ratio);
  bool fitting() const { return view_.fitting(); }
  qreal magnification() const { return view_.magnification(); }
  bool canPan() const { return view_.canPan(); }
  QRectF imageRect() const { return view_.rect(); }
  Q_INVOKABLE void fit();
  Q_INVOKABLE void actualSize();
  Q_INVOKABLE void zoom(qreal factor, QPointF anchor);
  Q_INVOKABLE void zoomSteps(qreal steps, QPointF anchor);
  Q_INVOKABLE void pan(QPointF delta);
  void paint(QPainter* painter) override;
  static QRectF fitRect(QSize image, QSizeF canvas);
 signals:
  void imageChanged();
  void viewChanged();
  void viewportChanged();

 protected:
  void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

 private:
  void refresh();
  QImage image_;
  ViewGeometry view_;
  qreal pixel_ratio_ = 1;
};
