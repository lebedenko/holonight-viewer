#include "gif_fixture.h"
#include "image_canvas.h"
#include "image_document.h"

#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
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
// Capture fixture identity before timing starts; retain no image pixels in memory.
void recordFixtures(const QDir& directory) {
  const auto destination = qEnvironmentVariable("VIEWER_PERFORMANCE_FIXTURE_MANIFEST");
  if (destination.isEmpty()) {
    return;
  }
  QJsonObject manifest;
  for (const auto& name : directory.entryList(QDir::Files, QDir::Name)) {
    QFile file(directory.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    ASSERT_TRUE(hash.addData(&file));
    manifest.insert(name, QJsonObject{{"sha256", QString::fromLatin1(hash.result().toHex())}, {"bytes", file.size()}});
  }
  QSaveFile output(destination);
  ASSERT_TRUE(output.open(QIODevice::WriteOnly));
  const auto bytes = QJsonDocument(manifest).toJson();
  ASSERT_EQ(output.write(bytes), bytes.size());
  ASSERT_TRUE(output.commit());
}
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
  recordFixtures(QDir(dir.path()));
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
  ASSERT_TRUE(QTest::qWaitFor(
      [&] {
        return document.state() == ImageDocument::Ready && document.localPath() == dir.filePath("1.png") &&
               document.image().size() == QSize(8000, 4000) && document.image().pixelColor(0, 0).red() > 75;
      },
      30000));
  RecordProperty("open_ms", clock.elapsed());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.canNext(); }));
  auto start = clock.elapsed();
  document.next();
  ASSERT_TRUE(QTest::qWaitFor(
      [&] {
        return document.state() == ImageDocument::Ready && document.localPath() == dir.filePath("2.png") &&
               document.image().size() == QSize(8000, 4000) && document.image().pixelColor(0, 0).red() < 75;
      },
      30000));
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

// Opt-in like the exercises above. A 32 MP animation (8000 x 4000 screen, so 122 MiB per frame) must play without
// the GUI thread ever stalling for 50 ms: decoding is off-thread and a swap only shares a QImage and repaints.
TEST(ReleasePerformance, AnimatedGifFrameSwap) {
  if (!qEnvironmentVariableIsSet("VIEWER_PERFORMANCE") || !gif::available()) {
    GTEST_SKIP() << "Opt-in release performance exercise";
  }
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/performance-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  QFile file(dir.filePath("large.gif"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(gif::bytes("GIF89a",
                        {{.delayCs = 10, .second = false},
                         {.delayCs = 10, .second = true},
                         {.delayCs = 10, .second = false},
                         {.delayCs = 10, .second = true}},
                        0, 8000, 4000));
  file.close();
  ImageDocument document;
  ImageCanvas canvas;
  canvas.setWidth(1600);
  canvas.setHeight(900);
  QObject::connect(&document, &ImageDocument::frameChanged, &canvas, [&] { canvas.replaceFrame(document.image()); });
  QElapsedTimer clock;
  clock.start();
  qint64 last = 0;
  qint64 maximum = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    const auto now = clock.elapsed();
    maximum = std::max(maximum, now - last);
    last = now;
  });
  timer.start(1);
  document.open({QUrl::fromLocalFile(file.fileName())});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() == ImageDocument::Ready; }, 20000));
  canvas.setImage(document.image());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); }, 20000));
  int swaps = 0;
  QObject::connect(document.animation(), &AnimationController::frameReady, &canvas, [&] { ++swaps; });
  last = clock.elapsed();
  maximum = 0;
  ASSERT_TRUE(QTest::qWaitFor([&] { return swaps >= 6; }, 60000));
  QElapsedTimer swap;
  swap.start();
  canvas.replaceFrame(document.image());
  const auto swapNs = swap.nsecsElapsed();
  RecordProperty("frame_pixels", 8000 * 4000);
  RecordProperty("replace_frame_ns", swapNs);
  RecordProperty("max_gui_stall_ms", maximum);
  RecordProperty("swaps", swaps);
  EXPECT_LT(maximum, 50) << "GUI thread stalled while playing a 32 MP GIF";
  EXPECT_LT(swapNs, 50'000'000);
}

// The frame count comes from a whole-file scan on the animation thread. A 200 MiB file (a large comment block after
// two small frames) makes that scan slow; opening and displaying must not wait for it.
TEST(ReleasePerformance, AnimatedGifScanNeverBlocksTheGui) {
  if (!qEnvironmentVariableIsSet("VIEWER_PERFORMANCE") || !gif::available()) {
    GTEST_SKIP() << "Opt-in release performance exercise";
  }
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/performance-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  auto bytes = gif::bytes("GIF89a", {{.delayCs = 10, .second = false}, {.delayCs = 10, .second = true}}, 0);
  bytes.chop(1);  // trailer
  QFile file(dir.filePath("huge.gif"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(bytes);
  file.write(QByteArray::fromHex("21fe"));
  QByteArray chunk;  // 256-byte sub-blocks: length 255 then 255 bytes
  for (int i = 0; i < 256; ++i) {
    chunk.append(static_cast<char>(255)).append(QByteArray(255, 'x'));
  }
  constexpr int kChunks = 200 * 1024 * 1024 / 65536;
  for (int i = 0; i < kChunks; ++i) {
    file.write(chunk);
  }
  file.write(
      QByteArray::fromHex("00"
                          "3b"));
  file.close();
  ASSERT_GT(file.size(), 190LL * 1024 * 1024);
  ImageDocument document;
  QElapsedTimer clock;
  clock.start();
  qint64 last = 0;
  qint64 maximum = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    const auto now = clock.elapsed();
    maximum = std::max(maximum, now - last);
    last = now;
  });
  timer.start(1);
  document.open({QUrl::fromLocalFile(file.fileName())});
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.state() != ImageDocument::Loading; }, 60000));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  RecordProperty("open_ms", clock.elapsed());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); }, 60000));
  RecordProperty("animated_ms", clock.elapsed());
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->frameCount() == 2; }, 120000));
  RecordProperty("frame_count_known_ms", clock.elapsed());
  RecordProperty("file_bytes", file.size());
  RecordProperty("max_gui_stall_ms", maximum);
  EXPECT_LT(maximum, 50) << "GUI thread blocked while the sequence was scanned";
}

TEST(ReleasePerformance, RepeatedNavigation) {
  if (!qEnvironmentVariableIsSet("VIEWER_PERFORMANCE")) {
    GTEST_SKIP() << "Opt-in release performance exercise";
  }
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  QTemporaryDir dir(QStringLiteral(VIEWER_FIXTURE_DIR) + "/navigation-XXXXXX");
  ASSERT_TRUE(dir.isValid());
  constexpr int count = 12;
  constexpr int extent = 2048;
  const auto color = [](int index) { return QColor(20 + (index * 17), 100, 200 - (index * 11)); };
  const auto path = [&](int index) { return dir.filePath(QString("%1.png").arg(index, 2, 10, QLatin1Char('0'))); };
  for (int index = 0; index < count; ++index) {
    QImage source(extent, extent, QImage::Format_ARGB32_Premultiplied);
    source.fill(color(index));
    ASSERT_TRUE(source.save(path(index)));
  }
  recordFixtures(QDir(dir.path()));
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
  const auto ready = [&](int index) {
    return QTest::qWaitFor(
        [&] {
          return !document.scanning() && document.state() == ImageDocument::Ready &&
                 document.localPath() == path(index) && document.image().size() == QSize(extent, extent) &&
                 document.image().pixelColor(0, 0) == color(index);
        },
        30000);
  };
  const auto rss = [&](const std::string& name) {
    QFile status("/proc/self/status");
    ASSERT_TRUE(status.open(QIODevice::ReadOnly));
    const auto lines = status.readAll().split('\n');
    for (const auto& line : lines) {
      if (line.startsWith("VmRSS:")) {
        bool valid = false;
        const auto value = line.simplified().split(' ').at(1).toInt(&valid);
        ASSERT_TRUE(valid);
        RecordProperty(name, value);
        return;
      }
    }
    FAIL() << "No Linux RSS sample";
  };
  document.open({QUrl::fromLocalFile(path(0))});
  ASSERT_TRUE(ready(0));
  RecordProperty("first_open_ms", clock.elapsed());
  ASSERT_EQ(document.count(), count);
  rss("rss_first_open_kib");
  auto start = clock.elapsed();
  for (int cycle = 0; cycle < 10; ++cycle) {
    document.next();
    ASSERT_TRUE(ready(1));
    document.previous();
    ASSERT_TRUE(ready(0));
  }
  RecordProperty("warm_20_selections_ms", clock.elapsed() - start);
  rss("rss_warm_kib");
  // Twelve 16 MiB images exceed the existing 128 MiB decoded cache.
  for (int cycle = 0; cycle < 3; ++cycle) {
    start = clock.elapsed();
    for (int index = 1; index < count; ++index) {
      document.next();
      ASSERT_TRUE(ready(index));
    }
    for (int index = count - 2; index >= 0; --index) {
      document.previous();
      ASSERT_TRUE(ready(index));
    }
    RecordProperty("pressure_cycle_" + std::to_string(cycle) + "_ms", clock.elapsed() - start);
    rss("rss_pressure_cycle_" + std::to_string(cycle) + "_kib");
  }
  start = clock.elapsed();
  for (int index = 1; index < count; ++index) {
    document.next();
  }
  ASSERT_TRUE(ready(count - 1));
  RecordProperty("rapid_latest_selection_ms", clock.elapsed() - start);
  bool stopped = false;
  QObject::connect(&document, &ImageDocument::shutdownFinished, [&] { stopped = true; });
  document.previous();  // Shutdown while foreground/prefetch work can still be pending.
  start = clock.elapsed();
  document.shutdown();
  ASSERT_TRUE(QTest::qWaitFor([&] { return stopped; }, 30000));
  RecordProperty("shutdown_ms", clock.elapsed() - start);
  maximum = std::max(maximum, clock.elapsed() - last);
  timer.stop();
  RecordProperty("max_gui_timer_gap_ms", maximum);
  RecordProperty("timer_ticks", ticks);
  RecordProperty("image_bytes", extent * extent * 4);
  RecordProperty("image_count", count);
  rss("rss_shutdown_kib");
}
