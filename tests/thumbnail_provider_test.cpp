#include "thumbnail_provider.h"

#include "thumbnail_metrics.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickTextureFactory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QWaitCondition>

#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace {
constexpr int kBox = 256;

QImage solid(QSize size) {
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::red);
  return image;
}

// A decoder that records what it saw and can be held closed until a test opens the gate.
struct FakeDecoder {
  ThumbnailResult operator()(const ThumbnailRequest& request, const std::atomic_bool& cancelled) {
    {
      QMutexLocker lock(&mutex);
      recordedCalls.append(request.path);
      recordedThreads.append(QThread::currentThread());
      ++startedCount;
      while (blocked) {
        gate.wait(&mutex);
      }
      cancelledSeen = cancelledSeen || cancelled.load();
    }
    if (failing) {
      return {.image = {}, .sourceSize = {}, .error = QStringLiteral("boom")};
    }
    return {.image = solid(decodedSize), .sourceSize = sourceSize, .error = {}, .cacheEligible = cacheEligible};
  }
  void block() {
    QMutexLocker lock(&mutex);
    blocked = true;
  }
  void release() {
    QMutexLocker lock(&mutex);
    blocked = false;
    gate.wakeAll();
  }
  QStringList calls() const {
    QMutexLocker lock(&mutex);
    return recordedCalls;
  }
  QList<QThread*> threads() const {
    QMutexLocker lock(&mutex);
    return recordedThreads;
  }
  int started() const {
    QMutexLocker lock(&mutex);
    return startedCount;
  }
  bool sawCancelled() const {
    QMutexLocker lock(&mutex);
    return cancelledSeen;
  }

  QSize decodedSize{100, 50};
  QSize sourceSize{100, 50};
  bool failing = false;
  bool cacheEligible = true;

  mutable QMutex mutex;
  QWaitCondition gate;
  QStringList recordedCalls;
  QList<QThread*> recordedThreads;
  int startedCount = 0;
  bool blocked = false;
  bool cancelledSeen = false;
};

// Requests go through the same entry point QML uses; the provider forwards to the shared decoder.
struct Harness {
  explicit Harness(int threads = 2) : provider([this](auto&&... args) { return decoder(args...); }, threads) {
    for (const auto* name : {"a.png", "b.png"}) {
      QFile file(directory.filePath(name));
      EXPECT_TRUE(file.open(QIODevice::WriteOnly));
      EXPECT_EQ(file.write("fixture"), 7);
    }
  }
  QTemporaryDir directory;
  std::unique_ptr<QQuickImageResponse> request(const QString& path, int box = kBox, int generation = 0) {
    return std::unique_ptr<QQuickImageResponse>(
        provider.requestImageResponse(ThumbnailProvider::idFor(path, box, generation), {}));
  }
  FakeDecoder decoder;
  ThumbnailProvider provider;
};

bool waitFinished(QQuickImageResponse& response) {
  QSignalSpy spy(&response, &QQuickImageResponse::finished);
  return spy.count() > 0 || spy.wait(5000);
}

bool settled(const ThumbnailProvider& provider) {
  return QTest::qWaitFor([&] { return provider.pendingCount() == 0; }, 5000);
}
}  // namespace

TEST(ThumbnailProvider, DecodesOffTheGuiThreadAndDeliversTheImage) {
  Harness harness;
  const auto response = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(waitFinished(*response));
  ASSERT_EQ(harness.decoder.threads().size(), 1);
  EXPECT_NE(harness.decoder.threads().first(), QThread::currentThread());
  EXPECT_TRUE(response->errorString().isEmpty());
  const std::unique_ptr<QQuickTextureFactory> texture(response->textureFactory());
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->image().size(), QSize(100, 50));
  EXPECT_EQ(harness.provider.cache().count(), 1);
}

TEST(ThumbnailProvider, PoolSizeIsHalfTheIdealThreadsWithAMinimumOfOne) {
  EXPECT_EQ(ThumbnailProvider({}, thumbnailThreadCount(16)).maxThreadCount(), 8);
  EXPECT_EQ(ThumbnailProvider({}, thumbnailThreadCount(1)).maxThreadCount(), 1);
  EXPECT_EQ(ThumbnailProvider({}, thumbnailThreadCount(3)).maxThreadCount(), 1);
}

TEST(ThumbnailProvider, TextureSizeIsTheLogicalSizeNotTheDevicePixels) {
  struct Case {
    QSize decoded;
    QSize source;
    QSize logical;
  };
  // Large source at 1.25, large source at 2, small source at 2 (never enlarged), and an SVG that scales up.
  for (const auto& expected : {Case{.decoded = {320, 240}, .source = {4000, 3000}, .logical = {256, 192}},
                               Case{.decoded = {512, 384}, .source = {4000, 3000}, .logical = {256, 192}},
                               Case{.decoded = {100, 100}, .source = {100, 100}, .logical = {100, 100}},
                               Case{.decoded = {512, 256}, .source = {256, 128}, .logical = {256, 128}}}) {
    Harness harness;
    harness.decoder.decodedSize = expected.decoded;
    harness.decoder.sourceSize = expected.source;
    const auto response = harness.request(harness.directory.filePath("a.png"));
    ASSERT_TRUE(waitFinished(*response));
    const std::unique_ptr<QQuickTextureFactory> texture(response->textureFactory());
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->textureSize(), expected.logical) << expected.source.width();
    EXPECT_EQ(texture->image().size(), expected.decoded);
  }
}

TEST(ThumbnailProvider, CacheHitDoesNotCallTheDecoder) {
  Harness harness;
  ASSERT_TRUE(waitFinished(*harness.request(harness.directory.filePath("a.png"))));
  ASSERT_TRUE(settled(harness.provider));
  const auto second = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(waitFinished(*second));
  EXPECT_EQ(harness.decoder.calls().size(), 1);
  const std::unique_ptr<QQuickTextureFactory> texture(second->textureFactory());
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->textureSize(), QSize(100, 50));
}

TEST(ThumbnailProvider, CacheHitFinishesAfterTheCallerCanConnect) {
  Harness harness;
  ASSERT_TRUE(waitFinished(*harness.request(harness.directory.filePath("a.png"))));
  ASSERT_TRUE(settled(harness.provider));
  const auto second = harness.request(harness.directory.filePath("a.png"));
  QSignalSpy spy(second.get(), &QQuickImageResponse::finished);
  EXPECT_EQ(spy.count(), 0);
  ASSERT_TRUE(spy.wait(5000));
}

TEST(ThumbnailProvider, KeyIncludesBoxGenerationAndPath) {
  Harness harness;
  const std::vector<std::unique_ptr<QQuickImageResponse>> responses = [&] {
    std::vector<std::unique_ptr<QQuickImageResponse>> made;
    made.push_back(harness.request(harness.directory.filePath("a.png"), kBox, 0));
    made.push_back(harness.request(harness.directory.filePath("a.png"), 320, 0));
    made.push_back(harness.request(harness.directory.filePath("a.png"), kBox, 1));
    made.push_back(harness.request(harness.directory.filePath("b.png"), kBox, 0));
    return made;
  }();
  ASSERT_EQ(responses.size(), 4U);
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.provider.cache().count() == 4; }, 5000));
  EXPECT_EQ(harness.decoder.calls().size(), 4);
}

TEST(ThumbnailProvider, PathsWithSpecialCharactersRoundTrip) {
  Harness harness;
  const QString path = QStringLiteral("/photos/a b?c#d%e/é 写真.png");
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  EXPECT_EQ(harness.decoder.calls(), QStringList{path});
}

TEST(ThumbnailProvider, CancelBeforeTheDecodeStartsPreventsIt) {
  Harness harness(1);
  harness.decoder.block();
  const auto first = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.decoder.started() == 1; }, 5000));
  const auto second = harness.request(harness.directory.filePath("b.png"));
  ASSERT_EQ(harness.provider.pendingCount(), 2);
  QSignalSpy secondFinished(second.get(), &QQuickImageResponse::finished);
  second->cancel();
  // Cancelled queue entries remain owned by the pool until it skips them.
  EXPECT_EQ(harness.provider.pendingCount(), 2);
  harness.decoder.release();
  ASSERT_TRUE(waitFinished(*first));
  ASSERT_TRUE(settled(harness.provider));
  // The cancelled request still finishes, so the caller can release it, but its decoder never ran.
  EXPECT_TRUE(secondFinished.count() > 0 || secondFinished.wait(5000));
  EXPECT_EQ(harness.decoder.calls(), QStringList{harness.directory.filePath("a.png")});
  EXPECT_EQ(harness.provider.cache().count(), 1);
}

TEST(ThumbnailProvider, CancelOfARunningDecodeIsNeverCached) {
  Harness harness;
  harness.decoder.block();
  const auto response = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.decoder.started() == 1; }, 5000));
  QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
  response->cancel();
  harness.decoder.release();
  ASSERT_TRUE(settled(harness.provider));
  QCoreApplication::processEvents();
  EXPECT_TRUE(harness.decoder.sawCancelled());
  EXPECT_EQ(harness.provider.cache().count(), 0);
  EXPECT_EQ(finished.count(), 1);
  EXPECT_EQ(response->textureFactory(), nullptr);
}

TEST(ThumbnailProvider, ResponseDestroyedMidDecodeIsSafe) {
  Harness harness;
  harness.decoder.block();
  auto response = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.decoder.started() == 1; }, 5000));
  response->cancel();
  response.reset();
  harness.decoder.release();
  ASSERT_TRUE(settled(harness.provider));
  QCoreApplication::processEvents();
  EXPECT_EQ(harness.provider.cache().count(), 0);
}

TEST(ThumbnailProvider, FailureAndBadIdsReportAnError) {
  Harness harness;
  harness.decoder.failing = true;
  const auto failed = harness.request(harness.directory.filePath("a.png"));
  ASSERT_TRUE(waitFinished(*failed));
  EXPECT_EQ(failed->errorString(), QLatin1String("boom"));
  EXPECT_EQ(failed->textureFactory(), nullptr);
  EXPECT_EQ(harness.provider.cache().count(), 0);

  const std::unique_ptr<QQuickImageResponse> bad(harness.provider.requestImageResponse("not-an-id", {}));
  ASSERT_TRUE(waitFinished(*bad));
  EXPECT_FALSE(bad->errorString().isEmpty());
  EXPECT_EQ(harness.decoder.calls().size(), 1);
}

TEST(ThumbnailProvider, BlockedDecoderLeavesTheGuiThreadFree) {
  Harness harness;
  harness.decoder.block();
  const auto response = harness.request(harness.directory.filePath("a.png"));
  QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
  QElapsedTimer timer;
  timer.start();
  for (int i = 0; i < 20; ++i) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  EXPECT_EQ(finished.count(), 0);
  EXPECT_LT(timer.elapsed(), 1000);
  harness.decoder.release();
  ASSERT_TRUE(finished.count() > 0 || finished.wait(5000));
}

TEST(ThumbnailProvider, ImageItemShowsLoadingThenReadyOrError) {
  FakeDecoder decoder;
  decoder.decodedSize = {320, 240};
  decoder.sourceSize = {4000, 3000};
  auto* raw = new ThumbnailProvider(
      [&decoder](const auto& request, const auto& cancelled) { return decoder(request, cancelled); }, 2);
  QQmlEngine engine;
  engine.addImageProvider("thumbnail", raw);
  QQmlComponent component(&engine);
  component.setData(R"(import QtQuick
Image { cache: false; asynchronous: true })",
                    QUrl());
  std::unique_ptr<QObject> object(component.create());
  ASSERT_NE(object, nullptr) << qPrintable(component.errorString());
  auto* image = qobject_cast<QQuickItem*>(object.get());
  const auto status = [&] { return image->property("status").toInt(); };

  decoder.block();
  image->setProperty("source", "image://thumbnail/" + ThumbnailProvider::idFor("/photos/a.png", 320, 0));
  ASSERT_TRUE(QTest::qWaitFor([&] { return decoder.started() == 1; }, 5000));
  EXPECT_EQ(status(), 2);  // Image.Loading
  decoder.release();
  ASSERT_TRUE(QTest::qWaitFor([&] { return status() == 1; }, 5000));  // Image.Ready
  EXPECT_EQ(image->implicitWidth(), 256);
  EXPECT_EQ(image->implicitHeight(), 192);

  decoder.failing = true;
  image->setProperty("source", "image://thumbnail/" + ThumbnailProvider::idFor("/photos/b.png", 320, 0));
  ASSERT_TRUE(QTest::qWaitFor([&] { return status() == 3; }, 5000));  // Image.Error
}

TEST(ThumbnailMetrics, ExposesTheBoxAndBuildsSources) {
  EXPECT_EQ(ThumbnailMetrics::boxSize(), kThumbnailBoxLogical);
  EXPECT_EQ(ThumbnailMetrics::devicePixels(1), 256);
  EXPECT_EQ(ThumbnailMetrics::devicePixels(1.25), 320);
  EXPECT_EQ(ThumbnailMetrics::devicePixels(2), 512);
  EXPECT_EQ(ThumbnailMetrics::devicePixels(2.0000000001), 512);

  const auto url = QUrl::fromLocalFile("/photos/a b#c.png");
  const auto source = ThumbnailMetrics::sourceFor(url, 1.25, 3);
  ASSERT_TRUE(source.startsWith(QLatin1String("image://thumbnail/320/3/")));
  EXPECT_EQ(source, QStringLiteral("image://thumbnail/") + ThumbnailProvider::idFor("/photos/a b#c.png", 320, 3));
  EXPECT_TRUE(ThumbnailMetrics::sourceFor(QUrl("https://example.com/a.png"), 1, 0).isEmpty());

  Harness harness;
  const auto identifier = source.sliced(QStringLiteral("image://thumbnail/").size());
  const std::unique_ptr<QQuickImageResponse> response(harness.provider.requestImageResponse(identifier, {}));
  ASSERT_TRUE(waitFinished(*response));
  EXPECT_EQ(harness.decoder.calls(), QStringList{"/photos/a b#c.png"});
}

TEST(ThumbnailMetrics, IsAvailableToQml) {
  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData(R"(import QtQuick
import HolonightViewer
Item {
  readonly property int box: ThumbnailMetrics.boxSize
  readonly property int pixels: ThumbnailMetrics.devicePixels(1.25)
  readonly property string source: ThumbnailMetrics.sourceFor("file:///photos/a.png", 2, 0)
})",
                    QUrl());
  std::unique_ptr<QObject> object(component.create());
  ASSERT_NE(object, nullptr) << qPrintable(component.errorString());
  EXPECT_EQ(object->property("box").toInt(), 256);
  EXPECT_EQ(object->property("pixels").toInt(), 320);
  EXPECT_TRUE(object->property("source").toString().startsWith(QLatin1String("image://thumbnail/512/0/")));
}

// Qt requests thumbnails and cancels them on its image-loading thread, so responses live off the GUI thread.
TEST(ThumbnailProvider, ConcurrentRequestsAndCancellationFinishOnTheirOwnThread) {
  Harness harness(4);
  QThread loader;
  QObject context;
  context.moveToThread(&loader);
  loader.start();
  std::atomic_int finished{0};
  std::atomic_int wrongThread{0};
  std::vector<QQuickImageResponse*> responses;
  constexpr int kCount = 60;
  QMetaObject::invokeMethod(
      &context,
      [&] {
        for (int i = 0; i < kCount; ++i) {
          auto* response = harness.provider.requestImageResponse(
              ThumbnailProvider::idFor(QStringLiteral("/photos/%1.png").arg(i), kBox, 0), {});
          QObject::connect(response, &QQuickImageResponse::finished, &context, [&] {
            ++finished;
            if (QThread::currentThread() != &loader) {
              ++wrongThread;
            }
          });
          responses.push_back(response);
          if (i % 2 == 0) {
            response->cancel();
          }
        }
      },
      Qt::BlockingQueuedConnection);
  // The GUI thread asks for the same keys while the loader thread's decodes complete and fill the cache.
  std::vector<std::unique_ptr<QQuickImageResponse>> mine;
  for (int i = 0; i < kCount; ++i) {
    mine.push_back(harness.request(QStringLiteral("/photos/%1.png").arg(i)));
    QCoreApplication::processEvents();
  }
  ASSERT_TRUE(QTest::qWaitFor([&] { return finished.load() == kCount; }, 10000));
  EXPECT_EQ(wrongThread.load(), 0);
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.provider.pendingCount() == 0; }, 10000));
  QMetaObject::invokeMethod(
      &context,
      [&] {
        for (int i = 0; i < kCount; ++i) {
          auto* response = responses[i];
          const std::unique_ptr<QQuickTextureFactory> texture(response->textureFactory());
          EXPECT_EQ(texture != nullptr, i % 2 != 0);
          delete response;
        }
      },
      Qt::BlockingQueuedConnection);
  mine.clear();
  EXPECT_LE(harness.provider.cache().count(), kCount);
  loader.quit();
  loader.wait();
}

TEST(ThumbnailProvider, ChangedAndDeletedSourcesMissTheMemoryCache) {
  Harness harness;
  const auto path = harness.directory.filePath("a.png");
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Append));
  file.write("replacement");
  file.close();
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  EXPECT_EQ(harness.decoder.calls().size(), 2);
  ASSERT_TRUE(file.remove());
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  EXPECT_EQ(harness.decoder.calls().size(), 3);
}

TEST(ThumbnailProvider, SourceChangedDuringDecodeIsNotCached) {
  Harness harness;
  harness.decoder.block();
  const auto path = harness.directory.filePath("a.png");
  const auto response = harness.request(path);
  ASSERT_TRUE(QTest::qWaitFor([&] { return harness.decoder.started() == 1; }));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Append));
  file.write("changed");
  file.close();
  harness.decoder.release();
  ASSERT_TRUE(waitFinished(*response));
  EXPECT_EQ(harness.provider.cache().count(), 0);
}

TEST(ThumbnailProvider, DependencySensitiveResultsAreNeverCached) {
  Harness harness;
  harness.decoder.cacheEligible = false;
  const auto path = harness.directory.filePath("a.png");
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  ASSERT_TRUE(waitFinished(*harness.request(path)));
  EXPECT_EQ(harness.decoder.calls().size(), 2);
  EXPECT_EQ(harness.provider.cache().count(), 0);
}
