#pragma once

#include <QImage>
#include <QTransform>

// Values 0..3 are clockwise quarter turns; 4..7 reflect X before rotating.
namespace ImageOrientation {
QTransform matrix(int orientation);
QTransform mapping(int orientation, QSize size);
QSize dimensions(int orientation, QSize size);
int compose(int orientation, int operation);
QImage apply(const QImage& image, int orientation);
}  // namespace ImageOrientation
