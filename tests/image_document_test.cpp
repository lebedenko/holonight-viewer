#include "image_document.h"

#include "image_canvas.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTransform>

#include <atomic>
#include <gtest/gtest.h>

namespace {
QString fixturePath(const QString& name) {
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  return QStringLiteral(VIEWER_FIXTURE_DIR) + '/' + name;
}
QUrl writeFixture(const QString& name, const QByteArray& bytes) {
  QFile file(fixturePath(name));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
    return {};
  }
  return QUrl::fromLocalFile(file.fileName());
}
QByteArray encodedImage(const char* format) {
  QImage image(40, 20, QImage::Format_ARGB32);
  image.fill(QColor(255, 0, 0, 128));
  for (int row = 0; row < image.height(); ++row) {
    for (int column = 20; column < image.width(); ++column) {
      image.setPixelColor(column, row, row < 10 ? Qt::green : Qt::blue);
    }
  }
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, format)) {
    return {};
  }
  return bytes;
}
bool settled(ImageDocument& document) {
  return QTest::qWaitFor([&document] { return document.state() != ImageDocument::Loading; });
}
DecodeResult solidResult() {
  QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::red);
  return {.image = image, .error = {}};
}
}  // namespace

TEST(Document, FormatsAlphaPathsAndRecovery) {
  ImageDocument document;
  ASSERT_TRUE(QImageReader::supportedImageFormats().contains("png"));
  ASSERT_TRUE(QImageReader::supportedImageFormats().contains("jpeg"));
  EXPECT_EQ(QImageReader::allocationLimit(), 128);
  for (const auto* format : {"PNG", "JPEG"}) {
    const auto bytes = encodedImage(format);
    ASSERT_FALSE(bytes.isEmpty());
    const auto url = writeFixture(QString::fromUtf8("зображення space.") + format, bytes);
    ASSERT_TRUE(url.isValid());
    document.open({url});
    EXPECT_EQ(document.state(), ImageDocument::Loading);
    ASSERT_TRUE(settled(document));
    ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
    EXPECT_EQ(document.image().size(), QSize(40, 20));
    EXPECT_EQ(document.image().devicePixelRatio(), 1);
    if (QByteArray(format) == "PNG") {
      EXPECT_EQ(document.image().pixelColor(0, 0).alpha(), 128);
    }
    QFile original(url.toLocalFile());
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), bytes);
  }
  document.open({writeFixture("extensionless", encodedImage("PNG"))});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  const QList<QUrl> failures{
      QUrl::fromLocalFile(fixturePath("missing.png")), QUrl::fromLocalFile(QStringLiteral(VIEWER_FIXTURE_DIR)),
      writeFixture("unsupported.txt", "not an image"), writeFixture("corrupt.png", encodedImage("PNG").left(45)),
      QUrl("https://example.org/image.png")};
  for (const auto& url : failures) {
    document.open({url});
    ASSERT_TRUE(settled(document));
    EXPECT_EQ(document.state(), ImageDocument::Error);
    EXPECT_FALSE(document.error().isEmpty());
    EXPECT_TRUE(document.image().isNull());
  }
  document.open({});
  EXPECT_EQ(document.state(), ImageDocument::Error);
  document.open({failures.first(), failures.last()});
  EXPECT_EQ(document.state(), ImageDocument::Error);
  document.open({writeFixture("recovered.png", encodedImage("PNG"))});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
}

TEST(Document, Orientation) {
  ImageDocument policy;
  const auto jpeg = encodedImage("JPEG");
  ASSERT_FALSE(jpeg.isEmpty());
  const auto base = QImage::fromData(jpeg);
  const std::atomic_bool cancel{false};
  for (int orientation = 1; orientation <= 8; ++orientation) {
    auto exif = QByteArray::fromHex("ffe1002245786966000049492a0008000000010012010300010000000100000000000000");
    exif[28] = static_cast<char>(orientation);
    const auto url =
        writeFixture(QStringLiteral("orientation-%1.jpg").arg(orientation), jpeg.left(2) + exif + jpeg.mid(2));
    const auto result = decodeImage(url, cancel);
    ASSERT_FALSE(result.image.isNull()) << result.error.toStdString();
    EXPECT_EQ(result.information.decodedSize, result.image.size());
    EXPECT_EQ(result.information.format, "JPEG");
    for (int temporary = 0; temporary < 8; ++temporary) {
      EXPECT_EQ(ImageOrientation::apply(result.image, temporary).size(),
                ImageOrientation::dimensions(temporary, result.image.size()));
    }
    const auto width = base.width();
    const auto height = base.height();
    EXPECT_EQ(result.image.size(), orientation < 5 ? base.size() : QSize(height, width));
    for (int row = 0; row < height; ++row) {
      for (int column = 0; column < width; ++column) {
        QPoint target;
        switch (orientation) {
          case 1:
            target = {column, row};
            break;
          case 2:
            target = {width - 1 - column, row};
            break;
          case 3:
            target = {width - 1 - column, height - 1 - row};
            break;
          case 4:
            target = {column, height - 1 - row};
            break;
          case 5:
            target = {row, column};
            break;
          case 6:
            target = {height - 1 - row, column};
            break;
          case 7:
            target = {height - 1 - row, width - 1 - column};
            break;
          case 8:
            target = {row, width - 1 - column};
            break;
          default:
            break;
        }
        ASSERT_EQ(result.image.pixelColor(target), base.pixelColor(column, row)) << orientation;
      }
    }
  }
}

TEST(Document, LimitsAndCancellation) {
  ImageDocument document;
  QImage wide(32769, 1, QImage::Format_RGB32);
  wide.fill(Qt::red);
  const auto path = fixturePath("too-wide.png");
  ASSERT_TRUE(wide.save(path));
  document.open({QUrl::fromLocalFile(path)});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(document.error().contains("limit"));
  QByteArray bitmap;
  QDataStream header(&bitmap, QIODevice::WriteOnly);
  header.setByteOrder(QDataStream::LittleEndian);
  header << quint16{0x4d42} << quint32{54} << quint32{0} << quint32{54} << quint32{40} << qint32{6000} << qint32{6000}
         << quint16{1} << quint16{24} << quint32{0} << quint32{0} << qint32{0} << qint32{0} << quint32{0} << quint32{0};
  document.open({writeFixture("too-many-pixels.bmp", bitmap)});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(document.error().contains("32 million"));
  QFile sparse(fixturePath("too-big.png"));
  ASSERT_TRUE(sparse.open(QIODevice::WriteOnly));
  ASSERT_TRUE(sparse.resize((256LL * 1024 * 1024) + 1));
  sparse.close();
  document.open({QUrl::fromLocalFile(sparse.fileName())});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(document.error().contains("256 MiB"));
  const std::atomic_bool cancelled{true};
  EXPECT_TRUE(decodeImage(QUrl::fromLocalFile(path), cancelled).image.isNull());
}

TEST(Document, OnlyNewestPendingRequestRuns) {
  std::atomic_bool release{false};
  std::atomic_int started{0};
  ImageDocument document([&](const QUrl&, const std::atomic_bool&) {
    const auto invocation = ++started;
    if (invocation == 1) {
      while (!release.load()) {
        QThread::msleep(1);
      }
    }
    return solidResult();
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  int presentations = 0;
  QObject::connect(&document, &ImageDocument::changed, [&] {
    if (document.state() == ImageDocument::Ready) {
      ++presentations;
    }
  });
  const auto url = QUrl::fromLocalFile(fixturePath("first.png"));
  document.open({url});
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load() == 1; }));
  for (int i = 0; i < 1000; ++i) {
    document.open({QUrl::fromLocalFile(fixturePath(QString::number(i) + ".png"))});
  }
  EXPECT_EQ(started.load(), 1);
  EXPECT_EQ(document.fileName(), "999.png");
  release.store(true);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(started.load(), 2);
  EXPECT_EQ(presentations, 1);
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_EQ(document.fileName(), "999.png");
}

TEST(Document, StaleErrorAndValidationCannotReplaceNewestState) {
  std::atomic_bool release{false};
  std::atomic_int started{0};
  ImageDocument document([&](const QUrl&, const std::atomic_bool&) {
    if (++started == 1) {
      while (!release.load()) {
        QThread::msleep(1);
      }
      return DecodeResult{.image = {}, .error = "stale failure"};
    }
    return solidResult();
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  QSignalSpy failures(&document, &ImageDocument::openingFailed);
  const auto local = QUrl::fromLocalFile(fixturePath("latest.png"));
  document.open({local});
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load() == 1; }));
  document.open({QUrl("https://example.org/no.png")});
  EXPECT_EQ(failures.size(), 1);
  const auto validation = document.error();
  release.store(true);
  QTest::qWait(30);
  EXPECT_EQ(document.error(), validation);
  EXPECT_EQ(started.load(), 1);
  document.open({local});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_TRUE(document.error().isEmpty());
  EXPECT_EQ(failures.size(), 1);
}

TEST(Document, ShutdownDrainsWithoutBlockingEventLoop) {
  std::atomic_bool entered{false};
  std::atomic_bool release{false};
  ImageDocument document([&](const QUrl&, const std::atomic_bool&) {
    entered.store(true);
    while (!release.load()) {
      QThread::msleep(1);
    }
    return DecodeResult{.image = {}, .error = "canceled failure"};
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  QSignalSpy failures(&document, &ImageDocument::openingFailed);
  QSignalSpy finished(&document, &ImageDocument::shutdownFinished);
  const auto url = QUrl::fromLocalFile(fixturePath("closing.png"));
  document.open({url});
  ASSERT_TRUE(QTest::qWaitFor([&] { return entered.load(); }));
  document.open({url});
  document.shutdown();
  QTest::qWait(10);
  EXPECT_TRUE(finished.isEmpty());
  release.store(true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !finished.isEmpty(); }));
  EXPECT_TRUE(document.image().isNull());
  EXPECT_TRUE(failures.isEmpty());
}

TEST(Document, CommandLinePathsAndRemoteRejection) {
  const auto url = writeFixture(QString::fromUtf8("frame:1 фото.png"), encodedImage("PNG"));
  const auto relative = QDir::current().relativeFilePath(url.toLocalFile());
  const auto original = QDir::currentPath();
  ASSERT_TRUE(QDir::setCurrent(QFileInfo(url.toLocalFile()).absolutePath()));
  const auto restore = qScopeGuard([&] { QDir::setCurrent(original); });
  for (const auto& argument : {url.fileName(), "./" + url.fileName(), url.toLocalFile(), url.toString()}) {
    EXPECT_EQ(commandLineUrl(argument), url);
  }
  EXPECT_FALSE(ImageDocument::isLocalUrl(commandLineUrl("missing:1.png")));
  EXPECT_EQ(commandLineUrl("./missing:1.png").toLocalFile(), QDir::current().absoluteFilePath("./missing:1.png"));
  EXPECT_EQ(commandLineUrl(QString::fromUtf8("фото space.png")).toLocalFile(),
            QDir::current().absoluteFilePath(QString::fromUtf8("фото space.png")));
  EXPECT_TRUE(ImageDocument::isLocalUrl(commandLineUrl("/tmp/a#b%.png")));
  EXPECT_FALSE(ImageDocument::isLocalUrl(commandLineUrl("https://example.org/a.png")));
  EXPECT_FALSE(ImageDocument::isLocalUrl(commandLineUrl("file://host/a.png")));
  EXPECT_FALSE(ImageDocument::isLocalUrl(commandLineUrl("file:///tmp/a.png?query")));
  EXPECT_FALSE(ImageDocument::isLocalUrl(commandLineUrl("file:///tmp/a.png#fragment")));
  ASSERT_TRUE(QDir::setCurrent(original));
  EXPECT_EQ(commandLineUrl(relative), url);
}

TEST(Document, FailureEventsIgnoreDirectoryChangesAndRepeatPerRequest) {
  std::atomic_bool release_scan{false};
  std::atomic_bool scan_started{false};
  ImageDocument document(
      [](const QUrl&, const std::atomic_bool&) { return DecodeResult{.image = {}, .error = "expected failure"}; },
      [&](const QUrl&, const std::atomic_bool&) {
        scan_started.store(true);
        while (!release_scan.load()) {
          QThread::msleep(1);
        }
        return DirectoryResult{};
      });
  const auto cleanup = qScopeGuard([&] { release_scan.store(true); });
  QSignalSpy failures(&document, &ImageDocument::openingFailed);
  QSignalSpy changes(&document, &ImageDocument::changed);
  const auto url = QUrl::fromLocalFile(fixturePath("event-failure.png"));
  document.open({url});
  ASSERT_TRUE(settled(document));
  ASSERT_TRUE(QTest::qWaitFor([&] { return scan_started.load(); }));
  ASSERT_EQ(failures.size(), 1);
  EXPECT_EQ(failures.first().at(0).toString(), "event-failure.png");
  EXPECT_EQ(failures.first().at(1).toString(), "expected failure");
  const auto before_scan = changes.size();
  release_scan.store(true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !document.scanning(); }));
  EXPECT_GT(changes.size(), before_scan);
  EXPECT_EQ(failures.size(), 1);
  document.resetTransform();
  EXPECT_EQ(failures.size(), 1);
  document.open({url});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(failures.size(), 2);
  document.refresh();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(failures.size(), 3);
  document.open({});
  EXPECT_EQ(failures.size(), 4);
  document.open({});
  EXPECT_EQ(failures.size(), 5);
}

TEST(Document, SuccessfulTransformsAndPrefetchFailuresDoNotReportOpeningFailures) {
  const auto selected = QUrl::fromLocalFile(fixturePath("selected.png"));
  const auto neighbor = QUrl::fromLocalFile(fixturePath("neighbor.png"));
  std::atomic_bool prefetched{false};
  ImageDocument document(
      [&](const QUrl& url, const std::atomic_bool&) {
        if (url == selected) {
          return solidResult();
        }
        prefetched.store(true);
        return DecodeResult{.image = {}, .error = "prefetch failure"};
      },
      [&](const QUrl&, const std::atomic_bool&) { return DirectoryResult{.urls = {selected, neighbor}, .error = {}}; });
  QSignalSpy failures(&document, &ImageDocument::openingFailed);
  document.open({selected});
  ASSERT_TRUE(settled(document));
  document.transform(1);
  document.resetTransform();
  ASSERT_TRUE(QTest::qWaitFor([&] { return prefetched.load(); }));
  QSignalSpy finished(&document, &ImageDocument::shutdownFinished);
  document.shutdown();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !finished.isEmpty(); }));
  EXPECT_TRUE(failures.isEmpty());
}

TEST(Canvas, FitsWithoutDistortion) {
  EXPECT_EQ(ImageCanvas::fitRect({4000, 2000}, {1000, 700}), QRectF(0, 100, 1000, 500));
  EXPECT_EQ(ImageCanvas::fitRect({20, 40}, {400, 300}), QRectF(125, 0, 150, 300));
  EXPECT_TRUE(ImageCanvas::fitRect({}, {1000, 700}).isEmpty());
  EXPECT_EQ(ImageCanvas::fitRect({4, 2}, {501.5, 400.25}), QRectF(0, 74.75, 501.5, 250.75));
}

TEST(Document, FirstAnimationFrameOnly) {
  ImageDocument document;
  if (!QImageReader::supportedImageFormats().contains("gif")) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  const auto header = QByteArray::fromHex("47494638396101000100800000000000ffffff");
  const auto frame = QByteArray::fromHex("21f90400010000002c0000000001000100000202440100");
  const auto url = writeFixture(
      "two-frames.gif", header + frame +
                            QByteArray(frame).replace(QByteArray::fromHex("4401"), QByteArray::fromHex("4c01")) +
                            QByteArray::fromHex("3b"));
  QImageReader reader(url.toLocalFile());
  ASSERT_EQ(reader.imageCount(), 2);
  const auto first = reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
  ASSERT_NE(reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied), first);
  document.open({url});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_EQ(document.image(), first);
  QTest::qWait(50);
  EXPECT_EQ(document.image(), first);
}

TEST(Document, UnreadableAndSpecialFiles) {
  ImageDocument document;
  const auto url = writeFixture("unreadable.png", encodedImage("PNG"));
  ASSERT_TRUE(QFile::setPermissions(url.toLocalFile(), {}));
  const auto restore =
      qScopeGuard([&] { QFile::setPermissions(url.toLocalFile(), QFile::ReadOwner | QFile::WriteOwner); });
  document.open({url});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(document.error().isEmpty());
  document.open({QUrl::fromLocalFile("/dev/null")});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
}

TEST(Document, LargeImageAndShutdown) {
  ImageDocument document;
  const auto path = fixturePath("large.png");
  {
    QImage large(6000, 4000, QImage::Format_RGB32);
    large.fill(Qt::green);
    ASSERT_TRUE(large.save(path));
  }
  document.open({QUrl::fromLocalFile(path)});
  int event_turns = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] { ++event_turns; });
  timer.start(1);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  EXPECT_EQ(document.image().size(), QSize(6000, 4000));
  EXPECT_GT(event_turns, 0);
  document.open({QUrl::fromLocalFile(path)});
  QSignalSpy finished(&document, &ImageDocument::shutdownFinished);
  document.shutdown();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !finished.isEmpty(); }));
}
