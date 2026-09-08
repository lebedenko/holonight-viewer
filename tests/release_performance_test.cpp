#include "image_document.h"

#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <future>
#include <gtest/gtest.h>

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
