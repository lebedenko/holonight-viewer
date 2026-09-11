#include "decoded_image_cache.h"
#include "directory_model.h"
#include "image_canvas.h"
#include "image_document.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

namespace {
QString temporaryPattern() {
  QDir().mkpath(QStringLiteral(VIEWER_FIXTURE_DIR));
  return QStringLiteral(VIEWER_FIXTURE_DIR) + "/browse-XXXXXX";
}
QUrl writeFile(const QTemporaryDir& dir, const QString& name, bool image = false) {
  const auto path = dir.filePath(name);
  if (image) {
    QImage pixels(80, 60, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::red);
    if (!pixels.save(path, "PNG")) {
      return {};
    }
  } else {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("broken") != 6) {
      return {};
    }
  }
  return QUrl::fromLocalFile(path);
}
bool settled(ImageDocument& document) {
  return QTest::qWaitFor([&] { return !document.scanning() && document.state() != ImageDocument::Loading; });
}
DecodeResult solid() {
  QImage image(20, 10, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::blue);
  return {.image = image, .error = {}};
}
}  // namespace

TEST(Directory, OrderingFilteringAndRoles) {
  QTemporaryDir dir(temporaryPattern());
  ASSERT_TRUE(dir.isValid());
  for (const auto& name : {"image10.png", "image2.PNG", "Image02.png", "image02.png", "space 1.png", "зображення.png",
                           ".hidden.png", "extensionless", "ignore.txt"}) {
    ASSERT_FALSE(writeFile(dir, QString::fromUtf8(name)).isEmpty());
  }
  ASSERT_TRUE(QDir(dir.path()).mkdir("folder.png"));
  ASSERT_TRUE(QFile::link(dir.filePath("image2.PNG"), dir.filePath("link.png")));
  ASSERT_TRUE(QFile::link(dir.filePath("folder.png"), dir.filePath("directory-link.png")));
  ASSERT_TRUE(QFile::link(dir.filePath("missing.png"), dir.filePath("dangling.png")));
  const std::atomic_bool cancel{false};
  const auto selected = QUrl::fromLocalFile(dir.filePath("extensionless"));
  const auto result = scanDirectory(selected, cancel);
  ASSERT_TRUE(result.error.isEmpty());
  QStringList names;
  for (const auto& url : result.urls) {
    names.append(url.fileName());
  }
  EXPECT_EQ(names, (QStringList{"extensionless", "Image02.png", "image02.png", "image2.PNG", "image10.png", "link.png",
                                "space 1.png", QString::fromUtf8("зображення.png")}));
  EXPECT_TRUE(naturalFileNameLess("i999999999999999999999.png", "i1000000000000000000000.png"));
  EXPECT_TRUE(naturalFileNameLess("A0.png", "a00.png"));
  EXPECT_TRUE(naturalFileNameLess("image2a.png", "image02b.png"));
  DirectoryModel model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  model.scan(QUrl::fromLocalFile(dir.filePath(".hidden.png")));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !model.scanning(); }));
  EXPECT_EQ(model.rowCount(), 8);
  EXPECT_EQ(model.data(model.index(0), DirectoryModel::FileNameRole).toString(), ".hidden.png");
  EXPECT_EQ(model.data(model.index(0), DirectoryModel::UrlRole).toUrl().toLocalFile(), dir.filePath(".hidden.png"));
  EXPECT_FALSE(model.data({}, DirectoryModel::UrlRole).isValid());
  const auto failed = scanDirectory(QUrl::fromLocalFile(dir.filePath("absent/file.png")), cancel);
  EXPECT_EQ(failed.urls.size(), 1);
  EXPECT_FALSE(failed.error.isEmpty());
  QTemporaryDir empty(temporaryPattern());
  EXPECT_EQ(scanDirectory(QUrl::fromLocalFile(empty.filePath("missing")), cancel).urls.size(), 1);
}

TEST(Directory, BoundedPendingStaleRejectionAndShutdown) {
  std::atomic_int calls{0};
  std::atomic_bool release{false};
  DirectoryModel model([&](const QUrl& url, const std::atomic_bool&) {
    if (++calls == 1) {
      while (!release.load()) {
        QThread::msleep(1);
      }
    }
    return DirectoryResult{.urls = {url}, .error = {}};
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  model.scan(QUrl::fromLocalFile("/old/a.png"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return calls.load() == 1; }));
  for (int i = 0; i < 100; ++i) {
    model.scan(QUrl::fromLocalFile(QString("/new/%1.png").arg(i)));
  }
  EXPECT_EQ(calls.load(), 1);
  release.store(true);
  ASSERT_TRUE(QTest::qWaitFor([&] { return !model.scanning(); }));
  EXPECT_EQ(calls.load(), 2);
  EXPECT_EQ(model.urlAt(0).fileName(), "99.png");
  QSignalSpy done(&model, &DirectoryModel::shutdownFinished);
  model.scan(QUrl::fromLocalFile("/last.png"));
  model.shutdown();
  ASSERT_TRUE(QTest::qWaitFor([&] { return done.count() == 1; }));
}

TEST(Browsing, BoundariesErrorsDeletionAndRefresh) {
  QTemporaryDir dir(temporaryPattern());
  const auto first = writeFile(dir, "image1.png", true);
  const auto broken = writeFile(dir, "image2.png");
  const auto last = writeFile(dir, "image10.png", true);
  ImageDocument document;
  document.open({first});
  EXPECT_TRUE(document.scanning());
  EXPECT_FALSE(document.canNext());
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.position(), 1);
  EXPECT_EQ(document.count(), 3);
  document.previous();
  EXPECT_EQ(document.position(), 1);
  document.next();
  EXPECT_EQ(document.position(), 2);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(document.canNext());
  document.next();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_FALSE(document.canNext());
  document.next();
  EXPECT_EQ(document.position(), 3);
  ASSERT_TRUE(QFile::remove(last.toLocalFile()));
  ASSERT_FALSE(writeFile(dir, "image3.png", true).isEmpty());
  ASSERT_TRUE(QFile::rename(first.toLocalFile(), dir.filePath("image0.png")));
  document.refresh();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.count(), 4);
  EXPECT_EQ(document.position(), 4);
  EXPECT_EQ(document.state(), ImageDocument::Error);
  document.previous();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.fileName(), "image3.png");
  document.open({QUrl::fromLocalFile(dir.filePath("gone/file.png"))});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.count(), 1);
  EXPECT_FALSE(document.folderError().isEmpty());
  EXPECT_FALSE(document.error().isEmpty());
}

TEST(Browsing, RapidRequestsForegroundPriorityCacheAndSilentPrefetch) {
  QTemporaryDir dir(temporaryPattern());
  const auto first = writeFile(dir, "1.png");
  writeFile(dir, "2.png");
  writeFile(dir, "3.png");
  writeFile(dir, "4.png");
  std::atomic_int calls{0};
  std::atomic_bool prefetchStarted{false};
  std::atomic_bool release{false};
  ImageDocument document([&](const QUrl& url, const std::atomic_bool&) {
    ++calls;
    if (url.fileName() == "2.png") {
      prefetchStarted.store(true);
      while (!release.load()) {
        QThread::msleep(1);
      }
      return DecodeResult{.image = {}, .error = "broken neighbor"};
    }
    return solid();
  });
  const auto cleanup = qScopeGuard([&] { release.store(true); });
  document.open({first});
  ASSERT_TRUE(settled(document));
  ASSERT_TRUE(QTest::qWaitFor([&] { return prefetchStarted.load(); }));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  document.next();
  document.next();
  document.next();
  document.previous();
  EXPECT_EQ(document.position(), 3);
  EXPECT_EQ(document.state(), ImageDocument::Loading);
  EXPECT_EQ(calls.load(), 2);
  release.store(true);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.fileName(), "3.png");
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  ASSERT_TRUE(QTest::qWaitFor([&] { return calls.load() == 4; }));
  QTest::qWait(30);
  EXPECT_EQ(calls.load(), 4);  // one failed backward prefetch, no retry loop
  document.previous();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_EQ(document.error(), "broken neighbor");
  document.previous();
  ASSERT_TRUE(settled(document));
  const auto before = calls.load();
  EXPECT_EQ(document.state(), ImageDocument::Ready);  // first image retained in LRU
  document.refresh();
  ASSERT_TRUE(settled(document));
  EXPECT_GT(calls.load(), before);
}

TEST(Cache, ValidationLruAndBudgets) {
  QTemporaryDir dir(temporaryPattern());
  const auto first = writeFile(dir, "a.png");
  const auto second = writeFile(dir, "b.png");
  const auto third = writeFile(dir, "c.png");
  DecodedImageCache cache;
  const auto entry = [](const QUrl& url, int height = 10) {
    auto item = DecodedImageCache::metadata(url);
    item.image = QImage(4096, height, QImage::Format_ARGB32_Premultiplied);
    item.image.fill(Qt::red);
    return item;
  };
  cache.put(entry(first));
  cache.put(entry(second));
  auto hit = cache.take(first);
  ASSERT_TRUE(hit.has_value());
  cache.put(std::move(*hit));
  cache.put(entry(third));
  EXPECT_EQ(cache.count(), 2);
  EXPECT_FALSE(cache.take(second));  // least recently used
  QFile changed(first.toLocalFile());
  ASSERT_TRUE(changed.open(QIODevice::Append));
  changed.write("changed");
  changed.close();
  EXPECT_FALSE(cache.take(first));
  cache.put(entry(first));
  ASSERT_TRUE(changed.open(QIODevice::ReadWrite));
  ASSERT_TRUE(changed.setFileTime(QDateTime::currentDateTimeUtc().addSecs(-60), QFileDevice::FileModificationTime));
  changed.close();
  EXPECT_FALSE(cache.take(first));  // same size, different modification time
  cache.put(entry(first));
  EXPECT_TRUE(cache.take(QUrl::fromLocalFile(dir.path() + "/./a.png")));
  ASSERT_TRUE(QFile::link(first.toLocalFile(), dir.filePath("alias.png")));
  cache.put(entry(first));
  EXPECT_FALSE(cache.take(QUrl::fromLocalFile(dir.filePath("alias.png"))));
  cache.put(entry(third));
  ASSERT_TRUE(QFile::remove(third.toLocalFile()));
  EXPECT_FALSE(cache.take(third));
  cache.put(entry(first, 5000));
  cache.put(entry(second, 5000));
  EXPECT_EQ(cache.count(), 1);
  EXPECT_LE(cache.bytes(), DecodedImageCache::byteLimit);
  cache.put(entry(first, 8193));
  EXPECT_EQ(cache.count(), 1);
  cache.clear();
  EXPECT_EQ(cache.bytes(), 0);
}

TEST(Browsing, CacheHitAndMetadataInvalidation) {
  QTemporaryDir dir(temporaryPattern());
  const auto first = writeFile(dir, "1.png");
  const auto second = writeFile(dir, "2.png");
  std::atomic_int calls{0};
  ImageDocument document([&](const QUrl&, const std::atomic_bool&) {
    ++calls;
    return solid();
  });
  document.open({first});
  ASSERT_TRUE(settled(document));
  ASSERT_TRUE(QTest::qWaitFor([&] { return calls.load() == 2; }));
  QTest::qWait(30);
  document.next();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(calls.load(), 2);
  document.previous();
  ASSERT_TRUE(settled(document));
  QTest::qWait(30);
  EXPECT_EQ(calls.load(), 2);
  QFile changed(second.toLocalFile());
  ASSERT_TRUE(changed.open(QIODevice::Append));
  changed.write("different size");
  changed.close();
  document.next();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(calls.load(), 3);
}

TEST(Viewer, FolderBrowsingControls) {
  QTemporaryDir dir(temporaryPattern());
  const auto first = writeFile(dir, "1.png", true);
  writeFile(dir, "2.png");
  writeFile(dir, "3.png", true);
  QImage large(2400, 1600, QImage::Format_ARGB32_Premultiplied);
  large.fill(Qt::blue);
  ASSERT_TRUE(large.save(first.toLocalFile()));
  ASSERT_TRUE(large.save(dir.filePath("3.png")));
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
  auto* canvas = window->findChild<ImageCanvas*>("imageCanvas");
  auto* next = window->findChild<QQuickItem*>("nextButton");
  auto* previous = window->findChild<QQuickItem*>("previousButton");
  ASSERT_NE(canvas, nullptr);
  ASSERT_NE(next, nullptr);
  ASSERT_NE(previous, nullptr);
  document.open({first});
  ASSERT_TRUE(settled(document));
  QTest::keyClick(window, Qt::Key_BracketRight);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.position(), 2);
  EXPECT_EQ(document.state(), ImageDocument::Error);
  EXPECT_TRUE(canvas->hasActiveFocus());
  const auto click = [&](QQuickItem* button) {
    QTest::mouseMove(window, QPoint(10, 70));
    QTest::qWait(20);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
  };
  click(next);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.position(), 3);
  EXPECT_TRUE(canvas->fitting());
  EXPECT_TRUE(canvas->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_1);
  QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
  const auto before = canvas->imageRect();
  QTest::keyClick(window, Qt::Key_Down);
  EXPECT_TRUE(canvas->canPan());
  EXPECT_LT(canvas->imageRect().y(), before.y());
  QTest::keyClick(window, Qt::Key_BracketLeft);
  ASSERT_TRUE(settled(document));
  click(previous);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.position(), 1);
  EXPECT_TRUE(canvas->fitting());
  ASSERT_FALSE(writeFile(dir, "4.png", true).isEmpty());
  QTest::keyClick(window, Qt::Key_R, Qt::ControlModifier);
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.count(), 4);
  QSignalSpy changes(&document, &ImageDocument::changed);
  window->setProperty("dialogRequested", true);
  QTest::qWait(50);
  QTest::keyClick(window, Qt::Key_BracketRight);
  QTest::keyClick(window, Qt::Key_R, Qt::ControlModifier);
  EXPECT_EQ(document.position(), 1);
  EXPECT_EQ(changes.count(), 0);
  EXPECT_FALSE(next->isEnabled());
  window->setProperty("dialogRequested", false);
  window->requestActivate();
  ASSERT_TRUE(QTest::qWaitForWindowActive(window));
  QTest::qWait(50);
  EXPECT_GT(canvas->height(), 0);
  const QString capture = qEnvironmentVariable("VIEWER_CAPTURE_PREFIX");
  if (!capture.isEmpty()) {
    ASSERT_TRUE(window->grabWindow().save(capture + "-browse-small.png"));
  }
  window->close();
}

TEST(Browsing, LargeFolderAndImages) {
  if (!qEnvironmentVariableIsSet("VIEWER_BROWSE_BENCHMARK")) {
    GTEST_SKIP() << "Opt-in resource measurement";
  }
  QTemporaryDir dir(temporaryPattern());
  for (int i = 0; i < 20000; ++i) {
    ASSERT_FALSE(writeFile(dir, QString("image%1.png").arg(i)).isEmpty());
  }
  {
    QImage pixels(6000, 4000, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::blue);
    ASSERT_TRUE(pixels.save(dir.filePath("image0.png")));
    ASSERT_TRUE(pixels.save(dir.filePath("image1.png")));
    ASSERT_TRUE(pixels.save(dir.filePath("image2.png")));
  }
  int ticks = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
  timer.start(1);
  QElapsedTimer elapsed;
  elapsed.start();
  ImageDocument document;
  qint64 scanMs = -1;
  QObject::connect(&document, &ImageDocument::changed, [&] {
    if (!document.scanning() && document.count() == 20000 && scanMs < 0) {
      scanMs = elapsed.elapsed();
    }
  });
  document.open({QUrl::fromLocalFile(dir.filePath("image0.png"))});
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.count(), 20000);
  EXPECT_EQ(document.image().size(), QSize(6000, 4000));
  const auto openMs = elapsed.elapsed();
  document.next();
  ASSERT_TRUE(settled(document));
  document.next();
  ASSERT_TRUE(settled(document));
  EXPECT_EQ(document.state(), ImageDocument::Ready);
  EXPECT_GT(ticks, 0);
  RecordProperty("scan_ms", scanMs);
  RecordProperty("open_ms", openMs);
  RecordProperty("two_navigations_ms", elapsed.elapsed() - openMs);
  RecordProperty("timer_ticks", ticks);
  QSignalSpy done(&document, &ImageDocument::shutdownFinished);
  document.refresh();
  document.shutdown();
  ASSERT_TRUE(QTest::qWaitFor([&] { return done.count() == 1; }));
}
