#include "image_canvas.h"
#include "image_document.h"

#include <QDir>
#include <QElapsedTimer>
#include <QLocale>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>

#include <gtest/gtest.h>

TEST(Viewer, InspectionControlsAndLifecycle) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->resize(420, 280);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  // Wayland configure events arrive after visibility/activation notifications.
  QTest::qWait(150);
  window->showNormal();
  window->resize(420, 280);
  QTest::qWait(150);
  auto* canvas = window->findChild<ImageCanvas*>(QStringLiteral("imageCanvas"));
  ASSERT_NE(canvas, nullptr);
  auto* actual_button = window->findChild<QQuickItem*>(QStringLiteral("actualSizeButton"));
  ASSERT_NE(actual_button, nullptr);
  EXPECT_FALSE(actual_button->isEnabled());
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  const auto path = QStringLiteral(VIEWER_FIXTURE_DIR) + "/inspection.png";
  QImage fixture(6000, 4000, QImage::Format_ARGB32_Premultiplied);
  for (int row = 0; row < fixture.height(); ++row) {
    for (int column = 0; column < fixture.width(); ++column) {
      fixture.setPixelColor(column, row,
                            (((column / 40) + (row / 40)) % 2) != 0 ? QColor(235, 110, 45) : QColor(40, 110, 190));
    }
  }
  ASSERT_TRUE(fixture.save(path));
  document.open({QUrl::fromLocalFile(path)});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_TRUE(canvas->fitting());
  QTest::qWait(30);
  const auto fit_canvas_size = canvas->size();
  auto* open_button = window->findChild<QQuickItem*>(QStringLiteral("actionsButton"));
  ASSERT_NE(open_button, nullptr);
  // Zoom clears button focus, and arrows pan without canvas focus.
  for (const auto key : {Qt::Key_Plus, Qt::Key_Equal}) {
    canvas->fit();
    open_button->forceActiveFocus(Qt::TabFocusReason);
    EXPECT_FALSE(canvas->hasActiveFocus());
    QTest::keyClick(window, key, Qt::ControlModifier);
    EXPECT_FALSE(canvas->hasActiveFocus());
    const auto before_keyboard_pan = canvas->imageRect();
    QTest::keyClick(window, Qt::Key_Down);
    EXPECT_LT(canvas->imageRect().y(), before_keyboard_pan.y());
  }
  canvas->fit();
  auto* menu = window->findChild<QObject*>("actionsMenu");
  ASSERT_NE(menu, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(menu, "open"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return menu->property("opened").toBool(); }));
  // Actual Size can be outside the viewport in this small window.
  // Exercise the menu's keyboard activation independently of scroll position.
  ASSERT_TRUE(menu->setProperty("currentIndex", 7));
  QTest::keyClick(window, Qt::Key_Return);
  EXPECT_DOUBLE_EQ(canvas->magnification(), 1);
  EXPECT_FALSE(canvas->fitting());
  EXPECT_FALSE(canvas->hasActiveFocus());
  QTest::qWait(30);
  EXPECT_EQ(canvas->size(), fit_canvas_size);
  const QPointF anchor(canvas->width() / 3, canvas->height() / 3);
  const auto wheel = [&](QPoint angle, QPoint pixel = {}, bool inverted = false) {
    const auto position = canvas->mapToScene(anchor);
    QWheelEvent event(position, window->mapToGlobal(position), pixel, angle, Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, inverted);
    QCoreApplication::sendEvent(window, &event);
  };
  const auto source_anchor = (anchor - canvas->imageRect().topLeft()) * canvas->displayPixelRatio();
  actual_button->forceActiveFocus(Qt::TabFocusReason);
  wheel({0, 120});
  EXPECT_FALSE(canvas->hasActiveFocus());
  EXPECT_DOUBLE_EQ(canvas->magnification(), 1.25);
  EXPECT_NEAR(
      ((anchor - canvas->imageRect().topLeft()) * canvas->displayPixelRatio() / 1.25 - source_anchor).manhattanLength(),
      0, 1e-8);
  wheel({}, {0, -40});
  EXPECT_NEAR(canvas->magnification(), 1, 1e-12);
  wheel({120, 0});
  EXPECT_NEAR(canvas->magnification(), 1, 1e-12);
  wheel({0, 120}, {}, true);
  EXPECT_NEAR(canvas->magnification(), 1.25, 1e-12);
  QTest::keyClick(window, Qt::Key_1);
  const auto before_drag = canvas->imageRect();
  const auto drag_start = canvas->mapToScene(QPointF(canvas->width() / 2, canvas->height() / 2)).toPoint();
  QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, drag_start);
  QTest::mouseMove(window, drag_start + QPoint(20, 20));
  QTest::mouseMove(window, drag_start + QPoint(60, 50));
  QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, drag_start + QPoint(60, 50));
  EXPECT_GT(canvas->imageRect().x(), before_drag.x());
  EXPECT_GT(canvas->imageRect().y(), before_drag.y());
  EXPECT_FALSE(canvas->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Tab);
  EXPECT_FALSE(canvas->hasActiveFocus());
  const auto while_control_focused = canvas->imageRect();
  QTest::keyClick(window, Qt::Key_Right);
  EXPECT_NEAR(canvas->imageRect().x(), while_control_focused.x() - 40, 1e-8);
  QTest::keyClick(window, Qt::Key_Backtab);
  EXPECT_FALSE(canvas->hasActiveFocus());
  const auto before_pan = canvas->imageRect();
  QTest::keyClick(window, Qt::Key_Right);
  EXPECT_NEAR(canvas->imageRect().x(), before_pan.x() - 40, 1e-8);
  QTest::keyClick(window, Qt::Key_Equal, Qt::ControlModifier);
  EXPECT_NEAR(canvas->magnification(), 1.25, 1e-12);
  open_button->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(window, Qt::Key_Minus, Qt::ControlModifier);
  EXPECT_FALSE(canvas->hasActiveFocus());
  EXPECT_NEAR(canvas->magnification(), 1, 1e-12);
  const auto center_source = [&] {
    return (QPointF(canvas->width() / 2, canvas->height() / 2) - canvas->imageRect().topLeft()) *
           canvas->displayPixelRatio() / canvas->magnification();
  };
  const auto center = center_source();
  // A resize invalidates the remainder of an active drag until the next gesture.
  QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, drag_start);
  QTest::mouseMove(window, drag_start + QPoint(20, 0));
  QTest::mouseMove(window, drag_start + QPoint(40, 0));
  window->resize(720, 520);
  QTest::qWait(30);
  const auto canceled_drag = canvas->imageRect();
  QTest::mouseMove(window, drag_start + QPoint(80, 0));
  EXPECT_EQ(canvas->imageRect(), canceled_drag);
  QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, drag_start + QPoint(80, 0));
  // Restore the pre-gesture center before checking resize preservation.
  canvas->pan((center_source() - center) * canvas->magnification() / canvas->displayPixelRatio());
  window->resize(760, 540);
  QTest::qWait(30);
  EXPECT_NEAR((center_source() - center).manhattanLength(), 0, 1e-8);
  QTest::keyClick(window, Qt::Key_F);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->visibility() == QWindow::FullScreen; }));
  QTest::qWait(150);
  EXPECT_DOUBLE_EQ(canvas->magnification(), 1);
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->visibility() != QWindow::FullScreen; }));
  QTest::qWait(150);
  const auto before_dialog = canvas->imageRect();
  QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->findChild<QObject*>(QStringLiteral("openDialog")) != nullptr; }));
  auto* dialog = window->findChild<QObject*>(QStringLiteral("openDialog"));
  EXPECT_FALSE(actual_button->isEnabled());
  wheel({0, 120});
  QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
  EXPECT_DOUBLE_EQ(canvas->magnification(), 1);
  EXPECT_EQ(canvas->imageRect(), before_dialog);
  ASSERT_TRUE(QMetaObject::invokeMethod(dialog, "reject"));
  QTest::qWait(30);
  EXPECT_EQ(canvas->imageRect(), before_dialog);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
  EXPECT_TRUE(canvas->fitting());
  EXPECT_EQ(canvas->imageRect(), ImageCanvas::fitRect(fixture.size(), canvas->size()));
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    for (const auto size : {QSize(420, 280), QSize(1000, 700)}) {
      window->resize(size);
      QTest::qWait(30);
      const auto suffix = size.width() == 420 ? QStringLiteral("-small.png") : QStringLiteral("-large.png");
      canvas->fit();
      QTest::qWait(50);
      EXPECT_TRUE(window->grabWindow().save(capture + "-inspect-fit" + suffix));
      canvas->actualSize();
      QTest::qWait(50);
      EXPECT_TRUE(window->grabWindow().save(capture + "-inspect-actual" + suffix));
      canvas->pan({-120, -80});
      QTest::qWait(50);
      EXPECT_TRUE(window->grabWindow().save(capture + "-inspect-pan" + suffix));
      canvas->zoom(32, {canvas->width() / 2, canvas->height() / 2});
      QTest::qWait(50);
      EXPECT_TRUE(window->grabWindow().save(capture + "-inspect-max" + suffix));
    }
  }
  document.open({QUrl::fromLocalFile(path)});
  EXPECT_TRUE(canvas->image().isNull());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  EXPECT_TRUE(canvas->fitting());
  document.open({QUrl::fromLocalFile(path + ".missing")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Error; }));
  EXPECT_FALSE(actual_button->isEnabled());
  EXPECT_TRUE(canvas->image().isNull());
  EXPECT_TRUE(canvas->fitting());
  window->close();
}

TEST(Viewer, LargeImageInspection) {
  QQuickWindow window;
  window.resize(1000, 700);
  ImageCanvas canvas(window.contentItem());
  canvas.setSize({1000, 700});
  canvas.setDisplayPixelRatio(window.effectiveDevicePixelRatio());
  QImage image(6000, 4000, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(30, 120, 190));
  canvas.setImage(image);
  window.show();
  ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
  const bool max_zoom = qEnvironmentVariable("VIEWER_BENCHMARK_MODE") != "fit";
  if (max_zoom) {
    canvas.actualSize();
    canvas.zoom(32, {500, 350});
  }
  int ticks = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, &window, [&] { ++ticks; });
  timer.start(1);
  QElapsedTimer elapsed;
  elapsed.start();
  for (int i = 0; i < 60; ++i) {
    canvas.pan({(i % 2) != 0 ? 40.0 : -40.0, 0});
    QTest::qWait(1);
    ASSERT_FALSE(window.grabWindow().isNull());
  }
  EXPECT_GT(ticks, 0);
  EXPECT_EQ(canvas.image().cacheKey(), image.cacheKey());
  EXPECT_EQ(canvas.size(), QSizeF(1000, 700));
  RecordProperty("manipulation_ms", elapsed.elapsed());
  RecordProperty("timer_ticks", ticks);
  window.close();
}

TEST(Canvas, FirstRenderGeneration) {
  ImageCanvas canvas;
  canvas.setSize(QSizeF(200, 150));
  QImage first(20, 10, QImage::Format_ARGB32);
  first.fill(Qt::red);
  QImage second(10, 20, QImage::Format_ARGB32);
  second.fill(Qt::blue);
  QImage target(200, 150, QImage::Format_ARGB32);
  QPainter painter(&target);
  QSignalSpy rendered(&canvas, &ImageCanvas::firstRendered);
  canvas.setImage(first);
  canvas.paint(&painter);
  canvas.setImage(second);
  QCoreApplication::processEvents();
  EXPECT_EQ(rendered.count(), 0);
  canvas.paint(&painter);
  QCoreApplication::processEvents();
  EXPECT_EQ(rendered.count(), 1);
  canvas.zoomSteps(1, QPointF(100, 75));
  canvas.pan(QPointF(10, 10));
  canvas.setOrientation(1);
  canvas.paint(&painter);
  QCoreApplication::processEvents();
  EXPECT_EQ(rendered.count(), 1);
  // A cached image is selected through the same clear/set lifecycle.
  canvas.setImage({});
  canvas.setImage(second);
  canvas.paint(&painter);
  QCoreApplication::processEvents();
  EXPECT_EQ(rendered.count(), 2);
}

TEST(Viewer, IndependentOverlayTimers) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QImage image(64, 48, QImage::Format_RGB32);
  image.fill(Qt::darkCyan);
  ASSERT_TRUE(image.save(dir.filePath("a.png")));
  ASSERT_TRUE(image.save(dir.filePath("b.png")));
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  auto* canvas = window->findChild<ImageCanvas*>("imageCanvas");
  auto* arrows = window->findChild<QObject*>("arrowTimer");
  auto* details = window->findChild<QObject*>("detailsTimer");
  auto* strip = window->findChild<QQuickItem*>("detailsStrip");
  auto* next = window->findChild<QQuickItem*>("nextButton");
  ASSERT_NE(arrows, nullptr);
  ASSERT_NE(details, nullptr);
  ASSERT_NE(strip, nullptr);
  ASSERT_NE(next, nullptr);
  const auto arrowsShown = [&] { return next->property("shown").toBool(); };
  const auto stripShown = [&] { return strip->property("shown").toBool(); };
  EXPECT_EQ(arrows->property("interval").toInt(), 3000);
  EXPECT_EQ(details->property("interval").toInt(), 4000);
  EXPECT_FALSE(strip->isVisible());
  // Shorter intervals exercise the production timer wiring without a long suite delay.
  arrows->setProperty("interval", 300);
  details->setProperty("interval", 500);
  document.open({QUrl::fromLocalFile(dir.filePath("a.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return stripShown(); }));
  EXPECT_FALSE(arrowsShown());
  const auto geometry = canvas->imageRect();
  const auto canvasPoint = canvas->mapToScene({10, 10}).toPoint();
  QTest::mouseMove(window, canvasPoint);
  EXPECT_TRUE(arrowsShown());
  QTest::qWait(200);
  QTest::mouseMove(window, canvasPoint + QPoint(10, 0));
  QTest::qWait(200);
  EXPECT_TRUE(arrowsShown());
  QTest::keyClick(window, Qt::Key_Tab);
  EXPECT_FALSE(arrowsShown());
  EXPECT_TRUE(stripShown());
  // Fit counts as a view command, so it restarts the HUD countdown; repainting alone does not.
  QTest::qWait(180);
  QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
  EXPECT_TRUE(stripShown());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !strip->isVisible(); }, 1500));
  EXPECT_EQ(canvas->imageRect(), geometry);
  canvas->update();
  QTest::qWait(100);
  EXPECT_FALSE(stripShown());
  QTest::keyClick(window, Qt::Key_R, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return stripShown(); }));
  EXPECT_FALSE(arrowsShown());
  EXPECT_EQ(document.formattedFileSize(),
            QLocale().formattedDataSize(document.information().encodedSize, 1, QLocale::DataSizeSIFormat));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  const auto originalPosition = document.position();
  QSignalSpy renders(canvas, &ImageCanvas::firstRendered);
  ASSERT_TRUE(document.canNext());
  {
    QTest::keyClick(window, Qt::Key_BracketRight);
    ASSERT_TRUE(QTest::qWaitFor([&] { return renders.count() == 1; }));
    EXPECT_TRUE(stripShown());
    EXPECT_FALSE(arrowsShown());
    QTest::keyClick(window, Qt::Key_BracketLeft);
    ASSERT_TRUE(QTest::qWaitFor([&] { return renders.count() == 2; }));
    EXPECT_EQ(document.position(), originalPosition);
    EXPECT_TRUE(stripShown());
  }
  QTest::qWait(700);
  EXPECT_FALSE(stripShown());
  QTest::mouseMove(window, QPoint(30, window->height() - 5));
  EXPECT_TRUE(stripShown());
  EXPECT_TRUE(arrowsShown());
  const auto capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    for (const auto size : {QSize(1000, 700), QSize(420, 280)}) {
      window->resize(size);
      QTest::mouseMove(window, QPoint(40, 20));
      QTest::qWait(100);
      EXPECT_TRUE(window->grabWindow().save(capture + QString("-overlays-%1.png").arg(size.width())));
      QTest::keyClick(window, Qt::Key_Tab);
      EXPECT_TRUE(window->grabWindow().save(capture + QString("-focus-%1.png").arg(size.width())));
    }
    QTest::keyClick(window, Qt::Key_F);
    QTest::qWait(100);
    EXPECT_TRUE(window->grabWindow().save(capture + "-fullscreen.png"));
  }

  window->close();
}
