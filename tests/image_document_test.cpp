#include "image_document.h"

#include "frame_source.h"
#include "gif_fixture.h"
#include "image_canvas.h"
#include "image_limits.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTransform>

#include <algorithm>
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
  return {.image = image, .error = {}, .information = {}, .svgData = {}};
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
      return DecodeResult{.image = {}, .error = "stale failure", .information = {}, .svgData = {}};
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
    return DecodeResult{.image = {}, .error = "canceled failure", .information = {}, .svgData = {}};
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
      [](const QUrl&, const std::atomic_bool&) {
        return DecodeResult{.image = {}, .error = "expected failure", .information = {}, .svgData = {}};
      },
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
        return DecodeResult{.image = {}, .error = "prefetch failure", .information = {}, .svgData = {}};
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

namespace {
QUrl writeGif(const QString& name, const QList<gif::Frame>& frames, int loopField = -1) {
  return writeFixture(name, gif::bytes("GIF89a", frames, loopField));
}
QImage firstFrame(const QUrl& url) {
  QImageReader reader(url.toLocalFile());
  return reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
}  // namespace

TEST(Document, AnimatedGifAutoplaysAfterItsFirstFrame) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  ImageDocument document;
  const auto url = writeGif("two-frames.gif", {{.delayCs = 50, .second = false}, {.delayCs = 30, .second = true}});
  const auto first = firstFrame(url);
  QSignalSpy frames(&document, &ImageDocument::frameChanged);
  QSignalSpy changes(&document, &ImageDocument::changed);
  document.open({url});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  // Frame 0 is the ordinary static result; playback attaches afterwards.
  EXPECT_EQ(document.image(), first);
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); }));
  QElapsedTimer clock;
  clock.start();
  EXPECT_EQ(document.image(), first);
  ASSERT_TRUE(QTest::qWaitFor([&] { return frames.count() == 1; }, 3000));
  EXPECT_GE(clock.elapsed(), 350);  // frame 0 stays for (about) its whole 500 ms delay
  EXPECT_NE(document.image(), first);
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  // A frame swap never goes through changed(): only the state changes did.
  const auto changedAfterFrame = changes.count();
  QTest::qWait(50);
  EXPECT_EQ(changes.count(), changedAfterFrame);
}

TEST(Document, SingleFrameGifStaysStatic) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  ImageDocument document;
  document.open({writeGif("one-frame.gif", {{.delayCs = 10, .second = false}})});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  QTest::qWait(300);
  EXPECT_FALSE(document.animation()->animated());
  EXPECT_FALSE(document.animation()->canToggle());
  EXPECT_TRUE(document.animation()->failureNotice().isEmpty());
}

TEST(Document, RefreshAndNavigationRestartPlaybackFromFrameZero) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  ImageDocument document;
  const auto url =
      writeGif("restart.gif",
               {{.delayCs = 5, .second = false}, {.delayCs = 5, .second = true}, {.delayCs = 5, .second = false}}, 0);
  const auto first = firstFrame(url);
  document.open({url});
  ASSERT_TRUE(settled(document));
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->frameIndex() >= 1; }));
  document.animation()->toggle();
  ASSERT_TRUE(document.animation()->userPaused());
  document.refresh();
  // The old session is gone at once, and the reloaded one starts unpaused from frame 0.
  EXPECT_FALSE(document.animation()->animated());
  EXPECT_FALSE(document.animation()->userPaused());
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.image(), first);
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); }));
  EXPECT_TRUE(document.animation()->playing());
  EXPECT_TRUE(QTest::qWaitFor([&] { return document.animation()->frameIndex() >= 1; }));
}

TEST(Document, DamagedFirstFrameIsAnErrorNotAnAnimation) {
  ImageDocument document;
  document.open({writeFixture("broken.gif", QByteArray("GIF89a") + QByteArray(40, '\x7f'))});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(document.animation()->animated());
}

TEST(Document, CopyWhilePlayingUsesTheCurrentFrameAndKeepsPlaying) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  ImageDocument document;
  document.open({writeGif("copy.gif", {{.delayCs = 5, .second = false}, {.delayCs = 5, .second = true}}, 0)});
  ASSERT_TRUE(settled(document));
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->animated(); }));
  ASSERT_TRUE(QTest::qWaitFor([&] { return document.animation()->frameIndex() == 1; }));
  document.copyImage();
  EXPECT_TRUE(document.animation()->playing());
  EXPECT_TRUE(QTest::qWaitFor([&] { return !document.clipboard()->busy(); }));
  EXPECT_TRUE(document.animation()->playing());
  EXPECT_EQ(document.state(), ImageDocument::Ready);
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

TEST(Document, NameFiltersListTiffSuffixes) {
  const auto filters = ImageDocument::nameFilters();
  ASSERT_EQ(filters.size(), 2);
  const auto patterns = filters.first().section('(', 1).chopped(1).split(' ');
  EXPECT_TRUE(patterns.contains("*.tif"));
  EXPECT_TRUE(patterns.contains("*.tiff"));
}

TEST(Document, NameFiltersListSvgSuffix) {
  const auto filters = ImageDocument::nameFilters();
  const auto patterns = filters.first().section('(', 1).chopped(1).split(' ');
  EXPECT_TRUE(patterns.contains("*.svg"));
}

namespace {
// Hand-written, independent of QSvgRenderer: a Qt SVG-handler regression cannot hide behind a Qt-written fixture.
QByteArray svgWithViewBox() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 200\">"
         "<rect width=\"100\" height=\"200\" fill=\"#ff0000\"/></svg>";
}
QByteArray svgWithoutViewBox() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"40\">"
         "<rect width=\"60\" height=\"40\" fill=\"#0000ff\"/></svg>";
}
QByteArray malformedSvg() {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<svg xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"10\" height=\"10\"";
}
}  // namespace

TEST(Document, SvgWithViewBoxDecodesToViewBoxSize) {
  ImageDocument document;
  bool rendererWasValidWhenPublished = false;
  QObject::connect(&document, &ImageDocument::changed, [&] {
    if (document.state() == ImageDocument::Ready && document.svgRenderer() != nullptr) {
      rendererWasValidWhenPublished = document.svgRenderer()->isValid();
    }
  });
  document.open({writeFixture("valid-with-viewBox.svg", svgWithViewBox())});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  EXPECT_EQ(document.information().format, "SVG");
  EXPECT_EQ(document.information().decodedSize, QSize(100, 200));
  EXPECT_TRUE(document.image().isNull());
  ASSERT_NE(document.svgRenderer(), nullptr);
  EXPECT_TRUE(document.svgRenderer()->isValid());
  EXPECT_TRUE(rendererWasValidWhenPublished);
  EXPECT_EQ(document.transformedDimensions(), QSize(100, 200));
  document.transform(1);
  EXPECT_EQ(document.transformedDimensions(), QSize(200, 100));
}

TEST(Document, SvgResolvesRelativeLocalReferences) {
  ASSERT_TRUE(writeFixture("svg-reference.png", encodedImage("PNG")).isValid());
  ImageDocument document;
  document.open({writeFixture("with-relative-reference.svg",
                              "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\">"
                              "<image href=\"svg-reference.png\" width=\"40\" height=\"20\"/>"
                              "</svg>")});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  QImage output(40, 20, QImage::Format_ARGB32_Premultiplied);
  output.fill(Qt::transparent);
  QPainter painter(&output);
  document.svgRenderer()->render(&painter, output.rect());
  EXPECT_EQ(output.pixelColor(0, 0), QColor(255, 0, 0, 128));
}

TEST(Document, SvgWithoutViewBoxFallsBackToDefaultSize) {
  ImageDocument document;
  document.open({writeFixture("valid-without-viewBox.svg", svgWithoutViewBox())});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  EXPECT_EQ(document.information().decodedSize, QSize(60, 40));
}

TEST(Document, MalformedSvgFailsThroughTheGenericErrorPath) {
  ImageDocument document;
  QSignalSpy failures(&document, &ImageDocument::openingFailed);
  document.open({writeFixture("malformed.svg", malformedSvg())});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_FALSE(document.error().isEmpty());
  EXPECT_TRUE(document.image().isNull());
  EXPECT_EQ(document.svgRenderer(), nullptr);
  EXPECT_EQ(failures.size(), 1);
}

TEST(Document, OversizedSvgIsRejectedBeforeParsing) {
  ImageDocument document;
  QFile sparse(fixturePath("oversized.svg"));
  ASSERT_TRUE(sparse.open(QIODevice::WriteOnly));
  ASSERT_TRUE(sparse.resize(kSvgFileLimitBytes + 1));
  sparse.close();
  document.open({QUrl::fromLocalFile(sparse.fileName())});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(document.error().contains("10 MiB"));
}

TEST(Document, SvgHasNoCameraOrLocationSectionsAndPreviewIsRasterized) {
  ImageDocument document;
  document.open({writeFixture("preview.svg", svgWithViewBox())});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready) << document.error().toStdString();
  const auto sections = document.informationSections();
  for (const auto& section : sections) {
    const auto key = section.toMap().value("key").toString();
    EXPECT_NE(key, "Camera");
    EXPECT_NE(key, "Location");
  }
  const auto preview = document.previewImage();
  ASSERT_FALSE(preview.isNull());
  EXPECT_LE(std::max(preview.width(), preview.height()), 256);
  EXPECT_EQ(preview.width() * 200, preview.height() * 100);  // Same 100:200 aspect ratio as the source viewBox.
}

TEST(Document, SelectingAwayFromSvgClearsRendererAndBytes) {
  ImageDocument document;
  document.open({writeFixture("cleared.svg", svgWithViewBox())});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  ASSERT_NE(document.svgRenderer(), nullptr);
  document.open({writeFixture("after-svg.png", encodedImage("PNG"))});
  ASSERT_TRUE(settled(document));
  ASSERT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_EQ(document.svgRenderer(), nullptr);
  EXPECT_TRUE(document.previewImage().cacheKey() == document.image().cacheKey());
}

TEST(Document, ConstructionPreservesApplicationAllocationPolicy) {
  const auto original = QImageReader::allocationLimit();
  const auto originalEnvironment = qgetenv("QT_IMAGEIO_MAXALLOC");
  const auto restore = qScopeGuard([&] {
    QImageReader::setAllocationLimit(original);
    if (originalEnvironment.isNull()) {
      qunsetenv("QT_IMAGEIO_MAXALLOC");
    } else {
      qputenv("QT_IMAGEIO_MAXALLOC", originalEnvironment);
    }
  });
  qputenv("QT_IMAGEIO_MAXALLOC", "64");
  QImageReader::setAllocationLimit(64);
  // Qt may cache its environment override; construction must preserve the effective value.
  const auto effectiveLimit = QImageReader::allocationLimit();
  ImageDocument document;
  const auto frameSource = makeQtGifFrameSource();
  EXPECT_EQ(QImageReader::allocationLimit(), effectiveLimit);
  EXPECT_EQ(qgetenv("QT_IMAGEIO_MAXALLOC"), "64");
}
