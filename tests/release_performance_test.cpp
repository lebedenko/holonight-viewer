#include "image_canvas.h"
#include "image_document.h"

#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <future>
#include <gtest/gtest.h>

namespace {
void writeMarker(const QString& markerPath, const QString& content = QString{}) {
  QSaveFile marker(markerPath);
  ASSERT_TRUE(marker.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
  if (!content.isEmpty()) {
    const auto bytes = content.toUtf8();
    ASSERT_EQ(marker.write(bytes), bytes.size());
  }
  ASSERT_TRUE(marker.commit());
}
}  // namespace

TEST(ReleasePerformance, LargeWorkflow) {
  if (!qEnvironmentVariableIsSet("VIEWER_PERFORMANCE")) {
    GTEST_SKIP() << "Opt-in release performance exercise";
  }
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/performance-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QImage source(8000, 4000, QImage::Format_ARGB32_Premultiplied);
  source.fill(QColor(100, 200, 50, 128));
  ASSERT_TRUE(source.save(dir.filePath("1.png")));
  source.fill(QColor(50, 100, 200, 128));
  ASSERT_TRUE(source.save(dir.filePath("2.png")));
  source = {};
  ImageDocument document;
  QElapsedTimer clock;
  clock.start();
  qint64 last = 0;
  qint64 maximum = 0;
  int ticks = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    const auto now = clock.elapsed();
    maximum = std::max(maximum, now - last);
    last = now;
    ++ticks;
  });
  timer.start(1);
  document.open({QUrl::fromLocalFile(dir.filePath("1.png"))});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  RecordProperty("open_ms", clock.elapsed());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.canNext(); }));
  auto start = clock.elapsed();
  document.next();
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }));
  RecordProperty("navigation_ms", clock.elapsed() - start);
  start = clock.elapsed();
  for (int i = 0; i < 8; ++i) {
    document.transform(i);
    QCoreApplication::processEvents();
  }
  RecordProperty("transforms_ms", clock.elapsed() - start);
  document.transform(1);
  start = clock.elapsed();
  document.copyImage();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  RecordProperty("copy_ms", clock.elapsed() - start);
  if (qEnvironmentVariableIsSet("VIEWER_COPY_TRANSFER")) {
    const auto expectedSize =
        QImage::fromData(QGuiApplication::clipboard()->mimeData()->data("image/png"), "PNG").size();
    QProcess receiver;
    receiver.start(QStringLiteral(VIEWER_CLIPBOARD_PROBE), {"image", dir.filePath("received.png")});
    ASSERT_TRUE(QTest::qWaitFor([&] { return receiver.state() == QProcess::NotRunning; }, 30000));
    ASSERT_EQ(receiver.exitCode(), 0);
    EXPECT_EQ(QImage(dir.filePath("received.png")).size(), expectedSize);
    RecordProperty("copy_transfer_ms", clock.elapsed() - start);
  }
  QCoreApplication::processEvents();
  maximum = std::max(maximum, clock.elapsed() - last);
  RecordProperty("max_gui_timer_gap_ms", maximum);
  RecordProperty("timer_ticks", ticks);
  // Separate encoding sample after workflow timer measurements, with no receiver.
  timer.stop();
  auto encoding = std::async(std::launch::async, [snapshot = document.image()] {
    QElapsedTimer elapsed;
    elapsed.start();
    QByteArray png;
    QBuffer buffer(&png);
    const bool encoded = buffer.open(QIODevice::WriteOnly) && snapshot.save(&buffer, "PNG");
    return encoded ? elapsed.elapsed() : qint64{-1};
  });
  ASSERT_TRUE(
      QTest::qWaitFor([&] { return encoding.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }));
  const auto encoding_ms = encoding.get();
  ASSERT_GE(encoding_ms, 0);
  RecordProperty("standalone_png_encoding_ms", encoding_ms);
}

// The basic render loop keeps instrumentation on the GUI thread. A synchronized
// frame must contain the requested image/orientation before its swap is timed.
TEST(ReleasePerformance, NativeWorkflow) {
  if (!qEnvironmentVariableIsSet("VIEWER_NATIVE_PERFORMANCE")) {
    GTEST_SKIP() << "Opt-in native Wayland performance exercise";
  }
  ASSERT_EQ(QGuiApplication::platformName(), "wayland");
  ASSERT_EQ(qEnvironmentVariable("QSG_RENDER_LOOP"), "basic");
  const auto fixtureDir = qEnvironmentVariable("VIEWER_NATIVE_FIXTURE_DIR");
  const auto outputDir = qEnvironmentVariable("VIEWER_NATIVE_OUTPUT_DIR");
  ASSERT_FALSE(fixtureDir.isEmpty());
  ASSERT_FALSE(outputDir.isEmpty());
  ASSERT_TRUE(QDir().mkpath(outputDir));
  ASSERT_TRUE(QFile::exists(fixtureDir + "/1.png"));
  ASSERT_TRUE(QFile::exists(fixtureDir + "/2.png"));
  ImageDocument document;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  ASSERT_EQ(engine.rootObjects().size(), 1);
  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  ASSERT_NE(window, nullptr);
  window->resize(1000, 700);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  auto* canvas = window->findChild<ImageCanvas*>("imageCanvas");
  ASSERT_NE(canvas, nullptr);
  writeMarker(outputDir + "/viewer-ready", "ready");
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(QTest::qWaitFor([&] { return QFile::exists(outputDir + "/viewer-activated"); }, 60000));
  QTest::qWait(250);
  QElapsedTimer clock;
  clock.start();
  qint64 last = 0;
  qint64 maximum = 0;
  int ticks = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    const auto now = clock.elapsed();
    maximum = std::max(maximum, now - last);
    last = now;
    ++ticks;
  });
  timer.start(1);

  const auto frame = [&](const std::function<void()>& trigger, const std::function<bool()>& updated) -> qint64 {
    bool synchronized = false;
    qint64 completed = -1;
    const auto sync = QObject::connect(
        window, &QQuickWindow::afterSynchronizing, window,
        [&] {
          synchronized = updated() && document.state() == ImageDocument::Ready &&
                         canvas->image().cacheKey() == document.image().cacheKey();
        },
        Qt::DirectConnection);
    const auto swap = QObject::connect(
        window, &QQuickWindow::frameSwapped, window,
        [&] {
          if (synchronized && completed < 0) {
            completed = clock.elapsed();
          }
        },
        Qt::DirectConnection);
    const auto start = clock.elapsed();
    trigger();
    window->update();
    const bool ready = QTest::qWaitFor([&] { return completed >= 0; }, 60000);
    QObject::disconnect(sync);
    QObject::disconnect(swap);
    if (!ready) {
      ADD_FAILURE() << "No updated frame completed";
      return -1;
    }
    return completed - start;
  };
  const auto openMs = frame([&] { document.open({QUrl::fromLocalFile(fixtureDir + "/1.png")}); },
                            [&] { return !document.image().isNull(); });
  ASSERT_GE(openMs, 0);
  ASSERT_EQ(document.image().size(), QSize(8000, 4000));
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.canNext(); }, 30000));
  const auto firstKey = document.image().cacheKey();
  const auto navigationMs = frame([&] { document.next(); }, [&] { return document.image().cacheKey() != firstKey; });
  ASSERT_GE(navigationMs, 0);
  ASSERT_EQ(document.image().size(), QSize(8000, 4000));
  qint64 transformsMs = 0;
  // Four rotations, reflection, three rotations cover the eight orientations.
  for (int step = 0; step < 8; ++step) {
    int expected = -1;
    const auto duration = frame(
        [&] {
          document.transform(step == 4 ? 4 : 1);
          expected = document.orientation();
        },
        [&] { return canvas->orientation() == expected; });
    ASSERT_GE(duration, 0);
    RecordProperty("transform_" + std::to_string(step) + "_ms", duration);
    transformsMs += duration;
  }
  const auto resetMs = frame([&] { document.resetTransform(); }, [&] { return canvas->orientation() == 0; });
  ASSERT_GE(resetMs, 0);
  const auto rotateMs = frame([&] { document.transform(1); }, [&] { return canvas->orientation() == 1; });
  ASSERT_GE(rotateMs, 0);
  ASSERT_EQ(document.transformedDimensions(), QSize(4000, 8000));
  // Captures are outside rendering/copy timing, but inside whole-process RSS.
  ASSERT_TRUE(window->grabWindow().save(outputDir + "/rendered.png"));
  const auto copyStart = clock.elapsed();
  document.copyImage();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }, 60000));
  const auto copyPreparationMs = clock.elapsed() - copyStart;
  writeMarker(outputDir + "/copy-published", "published");
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(QTest::qWaitFor([&] { return QFile::exists(outputDir + "/receiver-complete"); }, 60000));
  QFile completion(outputDir + "/receiver-complete");
  ASSERT_TRUE(completion.open(QIODevice::ReadOnly));
  bool valid = false;
  const auto receivedAt = completion.readAll().trimmed().toLongLong(&valid);
  ASSERT_TRUE(valid);
  const auto copyToReceiverMs = receivedAt - clock.msecsSinceReference() - copyStart;
  ASSERT_GE(copyToReceiverMs, copyPreparationMs);
  maximum = std::max(maximum, clock.elapsed() - last);
  timer.stop();
  RecordProperty("open_ms", openMs);
  RecordProperty("navigation_ms", navigationMs);
  RecordProperty("transforms_ms", transformsMs);
  RecordProperty("copy_preparation_ms", copyPreparationMs);
  RecordProperty("copy_to_receiver_ms", copyToReceiverMs);
  RecordProperty("max_gui_timer_gap_ms", maximum);
  RecordProperty("timer_ticks", ticks);
  RecordProperty("window_width", window->width());
  RecordProperty("window_height", window->height());
  RecordProperty("window_scale", std::to_string(window->effectiveDevicePixelRatio()));
  RecordProperty("renderer_api", static_cast<int>(window->rendererInterface()->graphicsApi()));
  RecordProperty("render_loop", qEnvironmentVariable("QSG_RENDER_LOOP").toStdString());
  ASSERT_TRUE(QTest::qWaitFor([&] { return QFile::exists(outputDir + "/receiver-validated"); }, 60000));
}
