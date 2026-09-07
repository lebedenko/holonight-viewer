#pragma once
#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class ImageCanvas : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
 public:
  explicit ImageCanvas(QQuickItem* parent = nullptr);
  QImage image() const { return image_; }
  void setImage(const QImage& image);
  void paint(QPainter* painter) override;
  static QRectF fitRect(QSize image, QSizeF canvas);
 signals:
  void imageChanged();

 private:
  QImage image_;
};
