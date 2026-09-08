#include "view_geometry.h"

#include "image_canvas.h"

#include <QPainter>

#include <gtest/gtest.h>
#include <limits>

TEST(ViewGeometry, FitActualSizeAndDisplayChanges) {
  ViewGeometry view;
  view.setViewport({400, 300}, 1.25);
  view.setImage({1000, 500});
  EXPECT_EQ(view.rect(), QRectF(0, 50, 400, 200));
  EXPECT_DOUBLE_EQ(view.magnification(), 0.5);
  view.actualSize();
  EXPECT_EQ(view.rect(), QRectF(-200, -50, 800, 400));
  view.pan({-40, 0});
  EXPECT_EQ(view.center(), QPointF(550, 250));
  view.setViewport({500, 400}, 1.5);
  EXPECT_DOUBLE_EQ(view.magnification(), 1);
  EXPECT_DOUBLE_EQ(view.center().x(), 550);
  EXPECT_NEAR(view.rect().width() * 1.5, 1000, 1e-9);
  view.fit();
  EXPECT_EQ(view.center(), QPointF(500, 250));
  EXPECT_EQ(view.rect(), QRectF(0, 75, 500, 250));
  view.setViewport({200, 400}, 1.5);
  EXPECT_EQ(view.rect(), QRectF(0, 150, 200, 100));
}
TEST(ViewGeometry, AnchoredZoomAndPanConstraints) {
  ViewGeometry view;
  view.setViewport({400, 300}, 1);
  view.setImage({1000, 800});
  view.actualSize();
  const QPointF anchor(100, 80);
  const auto source = (anchor - view.rect().topLeft()) / view.scale();
  for (int i = 0; i < 100; ++i) {
    view.zoom(1.25, anchor);
    EXPECT_NEAR(((anchor - view.rect().topLeft()) / view.scale() - source).manhattanLength(), 0, 1e-9);
    view.zoom(0.8, anchor);
  }
  EXPECT_NEAR(view.magnification(), 1, 1e-12);
  EXPECT_NEAR((view.center() - QPointF(500, 400)).manhattanLength(), 0, 1e-9);
  view.pan({1e6, 1e6});
  EXPECT_EQ(view.rect().topLeft(), QPointF(0, 0));
  view.pan({-1e6, -1e6});
  EXPECT_EQ(view.rect().bottomRight(), QPointF(400, 300));
  view.fit();
  const auto fitted = view.rect();
  view.pan({50, 50});
  EXPECT_EQ(view.rect(), fitted);
  view.zoom(0.5, {0, 0});
  EXPECT_EQ(view.center(), QPointF(500, 400));
  EXPECT_FALSE(view.canPan());
}
TEST(ViewGeometry, BoundsReplacementAndInvalidInputs) {
  ViewGeometry view;
  view.setViewport({400, 300}, 1);
  view.setImage({32768, 900});
  view.zoom(1e-9, {200, 150});
  EXPECT_DOUBLE_EQ(view.magnification(), 0.01);
  view.zoom(1e9, {200, 150});
  EXPECT_DOUBLE_EQ(view.magnification(), 32);
  const auto center = view.center();
  view.setViewport({0, 0}, 1.5);
  view.actualSize();
  EXPECT_EQ(view.center(), center);
  EXPECT_DOUBLE_EQ(view.magnification(), 32);
  view.setViewport({400, 300}, 1);
  const auto before = view.rect();
  view.zoom(-1, {0, 0});
  view.zoom(std::numeric_limits<double>::quiet_NaN(), {0, 0});
  view.pan({std::numeric_limits<double>::infinity(), 0});
  EXPECT_EQ(view.rect(), before);
  view.setImage({1, 1});
  EXPECT_TRUE(view.fitting());
  EXPECT_DOUBLE_EQ(view.magnification(), 300);
  view.zoom(2, {200, 150});
  EXPECT_DOUBLE_EQ(view.magnification(), 300);
  view.setViewport({100, 100}, 1);
  EXPECT_DOUBLE_EQ(view.magnification(), 300);
  view.zoom(2, {50, 50});
  EXPECT_DOUBLE_EQ(view.magnification(), 300);
  view.zoom(0.8, {50, 50});
  EXPECT_DOUBLE_EQ(view.magnification(), 240);
  view.actualSize();
  EXPECT_DOUBLE_EQ(view.magnification(), 1);
  view.setImage({});
  EXPECT_TRUE(view.fitting());
  EXPECT_TRUE(view.rect().isEmpty());
  EXPECT_FALSE(view.canPan());
}
TEST(ImageCanvas, PhysicalPixelsAlphaAndBoundedPainting) {
  QImage source(80, 40, QImage::Format_ARGB32_Premultiplied);
  source.fill(Qt::transparent);
  source.setPixelColor(40, 20, Qt::red);
  for (const auto ratio : {1.0, 1.25, 1.5}) {
    ImageCanvas canvas;
    canvas.setSize({160, 80});
    canvas.setDisplayPixelRatio(ratio);
    canvas.setImage(source);
    canvas.actualSize();
    QImage output(QSizeF(160 * ratio, 80 * ratio).toSize(), QImage::Format_ARGB32_Premultiplied);
    output.setDevicePixelRatio(ratio);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    canvas.paint(&painter);
    painter.end();
    int red_pixels = 0;
    for (int row = 0; row < output.height(); ++row) {
      for (int column = 0; column < output.width(); ++column) {
        if (output.pixelColor(column, row) == QColor(Qt::red)) {
          ++red_pixels;
        }
      }
    }
    EXPECT_EQ(red_pixels, 1);
    EXPECT_EQ(output.pixelColor(0, 0), QColor(Qt::transparent));
    canvas.zoom(32, {80, 40});
    EXPECT_EQ(canvas.size(), QSizeF(160, 80));
    EXPECT_EQ(canvas.image().cacheKey(), source.cacheKey());
  }
}
