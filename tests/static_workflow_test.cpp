#include "clipboard_controller.h"
#include "image_canvas.h"
#include "image_document.h"
#include "image_orientation.h"

#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

namespace {
QImage asymmetric() {
  QImage image(3, 2, QImage::Format_ARGB32_Premultiplied);
  image.setPixelColor(0, 0, Qt::red);
  image.setPixelColor(1, 0, QColor(0, 255, 0, 128));
  image.setPixelColor(2, 0, Qt::blue);
  image.setPixelColor(0, 1, Qt::transparent);
  image.setPixelColor(1, 1, Qt::yellow);
  image.setPixelColor(2, 1, Qt::cyan);
  return image;
}
QImage reference(const QImage& source, int orientation) {
  QImage output(ImageOrientation::dimensions(orientation, source.size()), source.format());
  for (int row = 0; row < source.height(); ++row) {
    for (int column = 0; column < source.width(); ++column) {
      const int reflected = orientation >= 4 ? source.width() - 1 - column : column;
      QPoint target;
      switch (orientation & 3) {
        case 0:
          target = {reflected, row};
          break;
        case 1:
          target = {source.height() - 1 - row, reflected};
          break;
        case 2:
          target = {source.width() - 1 - reflected, source.height() - 1 - row};
          break;
        case 3:
          target = {row, source.width() - 1 - reflected};
          break;
      }
      output.setPixel(target, source.pixel(column, row));
    }
  }
  return output;
}
bool ready(ImageDocument& document) {
  return QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; });
}
}  // namespace

TEST(Workflow, OrientationsCompositionPaintingAndClipping) {
  const auto original = asymmetric();
  for (int orientation = 0; orientation < 8; ++orientation) {
    const auto expected = reference(original, orientation);
    EXPECT_EQ(ImageOrientation::apply(original, orientation), expected);
    for (int operation : {1, 3, 4, 6}) {
      EXPECT_EQ(ImageOrientation::apply(original, ImageOrientation::compose(orientation, operation)),
                reference(expected, operation));
    }
    EXPECT_EQ(ImageOrientation::compose(ImageOrientation::compose(orientation, 1), 3), orientation);
    EXPECT_EQ(ImageOrientation::compose(ImageOrientation::compose(orientation, 4), 4), orientation);
    for (qreal ratio : {1.0, 1.25, 1.5}) {
      ImageCanvas canvas;
      canvas.setImage(original);
      canvas.setOrientation(orientation);
      canvas.setDisplayPixelRatio(ratio);
      canvas.setSize(QSizeF(expected.size()) / ratio);
      canvas.actualSize();
      EXPECT_EQ(canvas.imageRect().size() * ratio, QSizeF(expected.size()));
      QImage painted(expected.size(), original.format());
      painted.fill(Qt::transparent);
      painted.setDevicePixelRatio(ratio);
      {
        QPainter painter(&painted);
        canvas.paint(&painter);
      }
      painted.setDevicePixelRatio(1);
      EXPECT_EQ(painted, expected) << orientation << ":" << ratio;
      canvas.setSize(QSizeF(1 / ratio, 1 / ratio));
      canvas.actualSize();
      QImage clipped(3, 3, original.format());
      clipped.fill(Qt::magenta);
      clipped.setDevicePixelRatio(ratio);
      {
        QPainter painter(&clipped);
        canvas.paint(&painter);
      }
      EXPECT_EQ(clipped.pixelColor(2, 2), QColor(Qt::magenta));
      EXPECT_EQ(canvas.image().cacheKey(), original.cacheKey());
    }
  }
  EXPECT_NE(ImageOrientation::compose(1, 4), ImageOrientation::compose(4, 1));
}

TEST(Workflow, InformationCacheRefreshAndUnchangedSource) {
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/workflow-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  const auto path = dir.filePath(QString::fromUtf8("зображення space.png"));
  ASSERT_TRUE(asymmetric().save(path));
  ASSERT_TRUE(asymmetric().save(dir.filePath("z.png")));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  file.close();
  ImageDocument document;
  document.open({QUrl::fromLocalFile(path)});
  EXPECT_EQ(document.information().encodedSize, -1);
  ASSERT_TRUE(ready(document));
  EXPECT_EQ(document.information().format, "PNG");
  EXPECT_EQ(document.information().encodedSize, bytes.size());
  EXPECT_EQ(document.information().decodedSize, QSize(3, 2));
  EXPECT_TRUE(document.information().modified.isValid());
  document.transform(1);
  EXPECT_EQ(document.transformedDimensions(), QSize(2, 3));
  document.resetTransform();
  EXPECT_EQ(document.orientation(), 0);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  const bool hasNext = document.canNext();
  if (hasNext) {
    document.next();
  } else {
    document.previous();
  }
  EXPECT_EQ(document.orientation(), 0);
  ASSERT_TRUE(ready(document));
  if (hasNext) {
    document.previous();
  } else {
    document.next();
  }
  ASSERT_TRUE(ready(document));
  EXPECT_EQ(document.information().encodedSize, bytes.size());
  EXPECT_EQ(document.information().format, "PNG");
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  EXPECT_EQ(file.readAll(), bytes);
  file.close();
  QImage changed(7, 5, QImage::Format_RGB32);
  changed.fill(Qt::green);
  ASSERT_TRUE(changed.save(path));
  document.transform(1);
  document.refresh();
  EXPECT_EQ(document.orientation(), 0);
  ASSERT_TRUE(ready(document));
  EXPECT_EQ(document.information().decodedSize, QSize(7, 5));
  ASSERT_TRUE(QFile::remove(path));
  document.refresh();
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Error; }));
  EXPECT_EQ(document.localPath(), path);
  EXPECT_EQ(document.information().encodedSize, -1);
  EXPECT_TRUE(document.informationText().contains("Unavailable"));
}

TEST(Workflow, ClipboardCaptureBusyFailureAndShutdown) {
  std::atomic_bool release{false};
  std::atomic_int started{0};
  ClipboardController controller([&](const QImage& image, int orientation) {
    ++started;
    while (!release.load()) {
      QThread::msleep(1);
    }
    return ImageOrientation::apply(image, orientation);
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  auto* clipboard = QGuiApplication::clipboard();
  clipboard->setText("previous");
  if (clipboard->supportsSelection()) {
    clipboard->setText("primary", QClipboard::Selection);
  }
  QImage source = asymmetric();
  controller.copyImage(source, 1, "captured.png");
  ASSERT_TRUE(controller.busy());
  controller.copyImage(source, 2, "ignored.png");
  controller.copyPath("ignored");
  source.fill(Qt::black);
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load() == 1; }));
  EXPECT_EQ(clipboard->text(), "previous");
  release.store(true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !controller.busy(); }));
  EXPECT_EQ(clipboard->image(), reference(asymmetric(), 1));
  EXPECT_TRUE(controller.feedback().contains("captured.png"));
  if (clipboard->supportsSelection()) {
    EXPECT_EQ(clipboard->text(QClipboard::Selection), "primary");
  }
  ClipboardController failure([](const QImage&, int) { return QImage{}; });
  failure.copyImage(asymmetric(), 0, "failure.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !failure.busy(); }));
  EXPECT_EQ(clipboard->image(), reference(asymmetric(), 1));
  EXPECT_TRUE(failure.feedback().contains("Could not prepare failure.png"));
  release.store(false);
  controller.copyImage(asymmetric(), 2, "shutdown.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load() == 2; }));
  QSignalSpy finished(&controller, &ClipboardController::shutdownFinished);
  controller.shutdown();
  release.store(true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !finished.isEmpty(); }));
  EXPECT_EQ(clipboard->image(), reference(asymmetric(), 1));
}

TEST(Viewer, StaticWorkflowControls) {
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  auto* canvas = window->findChild<ImageCanvas*>("imageCanvas");
  ASSERT_NE(canvas, nullptr);
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  const auto path = QStringLiteral(VIEWER_FIXTURE_DIR) + "/workflow.png";
  ASSERT_TRUE(asymmetric().save(path));
  document.open({QUrl::fromLocalFile(path)});
  ASSERT_TRUE(ready(document));
  QTest::qWait(30);
  canvas->actualSize();
  QTest::keyClick(window, Qt::Key_R);
  EXPECT_EQ(document.orientation(), 1);
  EXPECT_TRUE(canvas->fitting());
  EXPECT_TRUE(canvas->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_H);
  EXPECT_EQ(document.orientation(), ImageOrientation::compose(1, 4));
  QTest::keyClick(window, Qt::Key_V);
  QTest::keyClick(window, Qt::Key_R, Qt::ShiftModifier);
  EXPECT_EQ(document.orientation(), 2);
  auto* reset = window->findChild<QObject*>("resetTransformAction");
  ASSERT_NE(reset, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(reset, "trigger"));
  EXPECT_EQ(document.orientation(), 0);
  const auto openMenu = [&] {
    auto* button = window->findChild<QQuickItem*>("actionsButton");
    if (!button) {
      return false;
    }
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      button->mapToScene({button->width() / 2, button->height() / 2}).toPoint());
    QTest::qWait(100);
    return true;
  };
  ASSERT_TRUE(openMenu());
  auto* firstMenu = window->findChild<QObject*>("actionsMenu");
  ASSERT_NE(firstMenu, nullptr);
  for (int step = 0; step < 9 && firstMenu->property("currentIndex").toInt() != 0; ++step) {
    QTest::keyClick(window, Qt::Key_Down);
  }
  ASSERT_EQ(firstMenu->property("currentIndex").toInt(), 0);
  QTest::keyClick(window, Qt::Key_Return);
  QTest::qWait(100);
  EXPECT_EQ(document.orientation(), 1);
  EXPECT_TRUE(canvas->hasActiveFocus());
  ASSERT_TRUE(openMenu());
  auto* resetItem = window->findChild<QQuickItem*>("resetTransformMenuItem");
  ASSERT_NE(resetItem, nullptr);
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    resetItem->mapToScene({resetItem->width() / 2, resetItem->height() / 2}).toPoint());
  QTest::qWait(100);
  EXPECT_EQ(document.orientation(), 0);
  EXPECT_TRUE(canvas->hasActiveFocus());
  ASSERT_TRUE(openMenu());
  auto* infoItem = window->findChild<QQuickItem*>("informationMenuItem");
  ASSERT_NE(infoItem, nullptr);
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                    infoItem->mapToScene({infoItem->width() / 2, infoItem->height() / 2}).toPoint());
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("modalActive").toBool(); }));
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
  EXPECT_EQ(QGuiApplication::clipboard()->text(), path);
  QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  EXPECT_EQ(QGuiApplication::clipboard()->image(), asymmetric());
  QTest::keyClick(window, Qt::Key_F);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->visibility() == QWindow::FullScreen; }));
  for (auto key : {Qt::Key_I, Qt::Key_F1}) {
    QTest::keyClick(window, key);
    ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("modalActive").toBool(); }));
    QTest::qWait(100);
    QTest::keyClick(window, Qt::Key_R);
    QTest::keyClick(window, Qt::Key_F);
    QTest::keyClick(window, Qt::Key_Q);
    EXPECT_EQ(document.orientation(), 0);
    EXPECT_TRUE(window->isVisible());
    EXPECT_EQ(window->visibility(), QWindow::FullScreen);
    auto* text = window->findChild<QQuickItem*>("detailsText");
    ASSERT_NE(text, nullptr);
    text->forceActiveFocus();
    ASSERT_TRUE(QMetaObject::invokeMethod(text, "selectAll"));
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    EXPECT_EQ(QGuiApplication::clipboard()->text(), text->property("text").toString());
    QTest::keyClick(window, Qt::Key_End, Qt::ControlModifier);
    QTest::qWait(30);
    auto* scroll = window->findChild<QObject*>("detailsScroll");
    ASSERT_NE(scroll, nullptr);
    auto* flickable = scroll->property("contentItem").value<QQuickItem*>();
    ASSERT_NE(flickable, nullptr);
    if (flickable->property("contentHeight").toReal() > flickable->height()) {
      EXPECT_GT(flickable->property("contentY").toReal(), 0);
    }
    QTest::keyClick(window, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
    EXPECT_EQ(window->visibility(), QWindow::FullScreen);
    EXPECT_TRUE(canvas->hasActiveFocus());
  }
  QTest::keyClick(window, Qt::Key_Escape);
  const auto capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    for (const auto size : {QSize(420, 280), QSize(1000, 700)}) {
      window->resize(size);
      QTest::qWait(100);
      for (auto key : {Qt::Key_I, Qt::Key_F1}) {
        QTest::keyClick(window, key);
        QTest::qWait(200);
        EXPECT_TRUE(window->grabWindow().save(capture + QString("-workflow-%1-%2.png").arg(size.width()).arg(key)));
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::qWait(100);
      }
      auto* button = window->findChild<QQuickItem*>("actionsButton");
      ASSERT_NE(button, nullptr);
      QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                        button->mapToScene({button->width() / 2, button->height() / 2}).toPoint());
      QTest::qWait(200);
      EXPECT_TRUE(window->grabWindow().save(capture + QString("-workflow-menu-%1.png").arg(size.width())));
      QTest::keyClick(window, Qt::Key_Escape);
    }
  }
  window->resize(420, 280);
  QTest::qWait(100);
  ASSERT_TRUE(openMenu());
  auto* menu = window->findChild<QObject*>("actionsMenu");
  ASSERT_NE(menu, nullptr);
  for (int step = 0; step < 9 && menu->property("currentIndex").toInt() != 8; ++step) {
    QTest::keyClick(window, Qt::Key_Down);
  }
  EXPECT_EQ(menu->property("currentIndex").toInt(), 8);
  QTest::keyClick(window, Qt::Key_Return);
  ASSERT_TRUE(QTest::qWaitFor([&] { return window->property("detailDialog").toInt() == 2; }));
  QTest::keyClick(window, Qt::Key_Escape);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !window->property("modalActive").toBool(); }));
  document.transform(1);
  QTest::keyClick(window, Qt::Key_F5);
  EXPECT_EQ(document.orientation(), 0);
  ASSERT_TRUE(ready(document));
  window->close();
}

TEST(Workflow, LargeCopyResponsiveness) {
  const bool transfer = qEnvironmentVariableIsSet("VIEWER_COPY_TRANSFER");
  QQuickWindow owner;
  if (transfer) {
    owner.resize(200, 100);
    owner.show();
    owner.requestActivate();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&owner));
  }
  QImage image(8000, 4000, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(100, 200, 50, 128));
  ClipboardController controller;
  int ticks = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
  timer.start(1);
  QElapsedTimer elapsed;
  elapsed.start();
  controller.copyImage(image, 1, "32-million-pixels.png");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !controller.busy(); }));
  EXPECT_GT(ticks, 0);
  EXPECT_EQ(QGuiApplication::clipboard()->image().size(), QSize(4000, 8000));
  RecordProperty("copy_ms", elapsed.elapsed());
  RecordProperty("timer_ticks", ticks);
  RecordProperty("snapshot_bytes", image.sizeInBytes());
  RecordProperty("output_bytes", QGuiApplication::clipboard()->image().sizeInBytes());
  if (transfer) {
    QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/large-copy-XXXXXX");
    ASSERT_TRUE(dir.isValid());
    QProcess receiver;
    receiver.start(QStringLiteral(VIEWER_CLIPBOARD_PROBE), {"image", dir.filePath("received.png")});
    ASSERT_TRUE(QTest::qWaitFor([&] { return receiver.state() == QProcess::NotRunning; }, 30000));
    EXPECT_EQ(receiver.exitCode(), 0) << receiver.readAllStandardError().toStdString();
    EXPECT_EQ(QImageReader(dir.filePath("received.png")).size(), QSize(4000, 8000));
    RecordProperty("copy_and_transfer_ms", elapsed.elapsed());
    RecordProperty("transfer_timer_ticks", ticks);
  }
}

TEST(Workflow, ClipboardSeparateProcess) {
  if (QGuiApplication::platformName() == "offscreen" || QGuiApplication::platformName() == "minimal") {
    GTEST_SKIP() << "Requires a native clipboard platform";
  }
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/clipboard-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QQuickWindow owner;
  owner.resize(200, 100);
  owner.show();
  owner.requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(&owner));
  const auto source = dir.filePath("source.png");
  const auto link = dir.filePath(QString::fromUtf8("посилання space.png"));
  ASSERT_TRUE(asymmetric().save(source));
  ASSERT_TRUE(QFile::link(source, link));
  ImageDocument document;
  document.open({QUrl::fromLocalFile(dir.path() + "/./" + QFileInfo(link).fileName())});
  ASSERT_TRUE(ready(document));
  EXPECT_EQ(document.localPath(), link);
  const auto receive = [&](const QString& type, const QString& path) {
    QProcess process;
    process.start(QStringLiteral(VIEWER_CLIPBOARD_PROBE), {type, path});
    if (!QTest::qWaitFor([&] { return process.state() == QProcess::NotRunning; }, 10000)) {
      return false;
    }
    if (process.exitCode() != 0) {
      ADD_FAILURE() << process.readAllStandardError().toStdString();
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
  };
  for (int orientation = 0; orientation < 8; ++orientation) {
    owner.requestActivate();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&owner));
    document.resetTransform();
    document.transform(orientation);
    document.copyImage();
    // The captured pixels and filename must survive a new selection.
    document.open({QUrl::fromLocalFile(dir.filePath("missing.png"))});
    ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
    const auto target = dir.filePath("received.png");
    ASSERT_TRUE(receive("image", target));
    EXPECT_EQ(QImage(target).convertToFormat(QImage::Format_ARGB32_Premultiplied),
              reference(asymmetric(), orientation));
    EXPECT_TRUE(document.clipboard()->feedback().contains(QFileInfo(link).fileName()));
    document.open({QUrl::fromLocalFile(link)});
    ASSERT_TRUE(ready(document));
  }
  owner.requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(&owner));
  document.copyPath();
  const auto textPath = dir.filePath("received.txt");
  ASSERT_TRUE(receive("text", textPath));
  QFile text(textPath);
  ASSERT_TRUE(text.open(QIODevice::ReadOnly));
  EXPECT_EQ(QString::fromUtf8(text.readAll()), link);
}

TEST(Workflow, InformationRejectsStaleRequestsAndKeepsErrorFacts) {
  std::atomic_bool release{false};
  std::atomic_int started{0};
  ImageDocument document([&](const QUrl& url, const std::atomic_bool&) {
    if (++started == 1) {
      while (!release.load()) {
        QThread::msleep(1);
      }
    }
    return DecodeResult{
        .image = asymmetric(),
        .error = {},
        .information = {.format = url.fileName(), .encodedSize = 123, .modified = {}, .decodedSize = {3, 2}}};
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  document.open({QUrl::fromLocalFile(QStringLiteral(VIEWER_FIXTURE_DIR) + "/old.missing")});
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load() == 1; }));
  document.open({QUrl::fromLocalFile(QStringLiteral(VIEWER_FIXTURE_DIR) + "/new.missing")});
  EXPECT_TRUE(document.information().format.isEmpty());
  release.store(true);
  ASSERT_TRUE(ready(document));
  EXPECT_EQ(document.information().format, "new.missing");
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/error-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QFile broken(dir.filePath("broken.png"));
  ASSERT_TRUE(broken.open(QIODevice::WriteOnly));
  ASSERT_EQ(broken.write("broken"), 6);
  broken.close();
  const auto decoded = decodeImage(QUrl::fromLocalFile(broken.fileName()), std::atomic_bool{false});
  EXPECT_TRUE(decoded.image.isNull());
  EXPECT_EQ(decoded.information.encodedSize, 6);
  EXPECT_TRUE(decoded.information.modified.isValid());
  EXPECT_FALSE(decoded.information.decodedSize.isValid());
}
