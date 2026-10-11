#include "image_loader.h"

#include "image_loader_test_access.h"

#include <QFile>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <gtest/gtest.h>

namespace {
DecodeResult decoded() {
  DecodeResult result;
  result.image = QImage(2, 2, QImage::Format_ARGB32_Premultiplied);
  result.image.fill(Qt::red);
  result.information.format = "PNG";
  result.outcome = HolonightImages::Outcome::Success;
  return result;
}
ImageLoader::Request request(quint64 token, ImageLoader::Kind kind = ImageLoader::Kind::Foreground) {
  return {.request_id = token,
          .url = QUrl::fromLocalFile(QString("/tmp/loader-%1.png").arg(token)),
          .cache_epoch = 1,
          .kind = kind};
}
}  // namespace

TEST(ImageLoader, ReplacesPendingWorkAndDeliversOnGuiThreadAfterClearingBusy) {
  QSemaphore release;
  std::atomic_bool started{false};
  std::atomic_int calls{0};
  ImageLoader loader(
      [&](const QUrl&, const std::atomic_bool&) {
        if (++calls == 1) {
          started.store(true);
          release.acquire();
        }
        return decoded();
      },
      64);
  const auto cleanup = qScopeGuard([&] { release.release(); });
  QList<quint64> tokens;
  QList<HolonightImages::Outcome> outcomes;
  QObject::connect(&loader, &ImageLoader::completed, &loader, [&](ImageLoader::Request value, DecodeResult result) {
    EXPECT_EQ(QThread::currentThread(), loader.thread());
    EXPECT_FALSE(loader.busy());
    tokens.append(value.request_id);
    outcomes.append(*result.outcome);
  });
  loader.submit(request(1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load(); }));
  loader.submit(request(2, ImageLoader::Kind::Prefetch));
  EXPECT_TRUE(loader.hasPending());
  loader.submit(request(3));
  loader.submit(request(4));
  release.release();
  ASSERT_TRUE(QTest::qWaitFor([&] { return tokens.size() == 2; }));
  EXPECT_EQ(tokens, (QList<quint64>{1, 4}));
  EXPECT_EQ(outcomes,
            (QList<HolonightImages::Outcome>{HolonightImages::Outcome::Cancelled, HolonightImages::Outcome::Success}));
  EXPECT_EQ(calls.load(), 2);
}

TEST(ImageLoader, PendingDecodeStartsOnlyAfterCompletionHandlerReturns) {
  QSemaphore release;
  std::atomic_bool started{false};
  std::atomic_int calls{0};
  ImageLoader loader(
      [&](const QUrl&, const std::atomic_bool&) {
        if (++calls == 1) {
          started.store(true);
          release.acquire();
        }
        return decoded();
      },
      64);
  const auto cleanup = qScopeGuard([&] { release.release(); });
  QList<quint64> tokens;
  QObject::connect(&loader, &ImageLoader::completed, &loader, [&](ImageLoader::Request value, DecodeResult) {
    tokens.append(value.request_id);
    if (value.request_id == 1) {
      loader.submit(request(3));
      EXPECT_FALSE(loader.busy());
      EXPECT_EQ(calls.load(), 1);
    }
  });
  loader.submit(request(1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load(); }));
  loader.submit(request(2));
  release.release();
  ASSERT_TRUE(QTest::qWaitFor([&] { return tokens.size() == 2; }));
  EXPECT_EQ(tokens, (QList<quint64>{1, 3}));
  EXPECT_EQ(calls.load(), 2);
}

TEST(ImageLoader, ShutdownDrainsBlockedDecoderAndDropsPendingWork) {
  QSemaphore release;
  std::atomic_bool started{false};
  std::atomic_bool cancelled{false};
  std::atomic_int calls{0};
  ImageLoader loader(
      [&](const QUrl&, const std::atomic_bool& cancel) {
        ++calls;
        started.store(true);
        release.acquire();
        cancelled.store(cancel.load());
        return decoded();
      },
      64);
  const auto cleanup = qScopeGuard([&] { release.release(); });
  int completions = 0;
  QObject::connect(&loader, &ImageLoader::completed, &loader,
                   [&](ImageLoader::Request, DecodeResult) { ++completions; });
  QSignalSpy finished(&loader, &ImageLoader::shutdownFinished);
  loader.submit(request(1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return started.load(); }));
  loader.submit(request(2));
  loader.shutdown();
  loader.shutdown();
  EXPECT_TRUE(finished.isEmpty());
  release.release();
  ASSERT_TRUE(QTest::qWaitFor([&] { return finished.count() == 1; }));
  EXPECT_TRUE(cancelled.load());
  EXPECT_EQ(completions, 0);
  EXPECT_EQ(calls.load(), 1);
  loader.submit(request(3));
  EXPECT_FALSE(loader.busy());
}

TEST(ImageLoader, AcceptedTentativeResultIsAdoptedBeforeTheNextRequest) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const auto makeFile = [&](const QString& name) {
    QFile file(directory.filePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
      return QUrl{};
    }
    file.write("fixture");
    return QUrl::fromLocalFile(file.fileName());
  };
  const auto pasted = makeFile("paste.png");
  const auto other = makeFile("other.png");
  ASSERT_FALSE(pasted.isEmpty());
  ASSERT_FALSE(other.isEmpty());
  std::atomic_int calls{0};
  ImageLoader loader(
      [&](const QUrl&, const std::atomic_bool&) {
        ++calls;
        return decoded();
      },
      64);
  int completions = 0;
  QObject::connect(&loader, &ImageLoader::completed, &loader, [&](ImageLoader::Request value, DecodeResult result) {
    ++completions;
    if (value.kind == ImageLoader::Kind::Tentative) {
      loader.retainTentative(value, result, 2);
      loader.submit({.request_id = 2, .url = other, .cache_epoch = 2});
    } else if (value.request_id == 2) {
      loader.submit({.request_id = 3, .url = pasted, .cache_epoch = 2});
    }
  });
  loader.submit({.request_id = 1, .url = pasted, .cache_epoch = 1, .kind = ImageLoader::Kind::Tentative});
  ASSERT_TRUE(QTest::qWaitFor([&] { return completions == 3; }));
  EXPECT_EQ(calls.load(), 2);
  EXPECT_EQ(ImageLoaderTestAccess::cacheBudget(loader).second, 48);
}
