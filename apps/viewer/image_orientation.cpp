#include "image_orientation.h"

#include <array>

QTransform ImageOrientation::matrix(int orientation) {
  static const std::array rotations = {QTransform(), QTransform(0, 1, -1, 0, 0, 0), QTransform(-1, 0, 0, -1, 0, 0),
                                       QTransform(0, -1, 1, 0, 0, 0)};
  return QTransform::fromScale(orientation >= 4 ? -1 : 1, 1) * rotations.at(static_cast<std::size_t>(orientation & 3));
}
QTransform ImageOrientation::mapping(int orientation, QSize size) {
  return QImage::trueMatrix(matrix(orientation), size.width(), size.height());
}
QSize ImageOrientation::dimensions(int orientation, QSize size) {
  return (orientation & 1) != 0 ? size.transposed() : size;
}
int ImageOrientation::compose(int orientation, int operation) {
  const auto combined = matrix(orientation) * matrix(operation);
  for (int candidate = 0; candidate < 8; ++candidate) {
    if (matrix(candidate) == combined) {
      return candidate;
    }
  }
  Q_UNREACHABLE();
}
QImage ImageOrientation::apply(const QImage& image, int orientation) { return image.transformed(matrix(orientation)); }
