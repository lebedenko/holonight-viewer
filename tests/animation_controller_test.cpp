#include "animation_controller.h"

#include "fake_frame_source.h"
#include "gif_fixture.h"
#include "image_limits.h"
#include "manual_playback_clock.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <array>
#include <gtest/gtest.h>

namespace {
struct Shown {
  qint64 timeMs;
  int id;
};

// Drives an AnimationController inline on a fake clock and a scripted source: no threads, no sleeping.
class Harness {
 public:
  explicit Harness(std::vector<int> delays, int loopCount = 0) {
    script.delaysMs = std::move(delays);
    script.info = {.frameCount = static_cast<int>(script.delaysMs.size()), .loopCount = loopCount};
    auto manual = std::make_unique<ManualPlaybackClock>();
    clock = manual.get();
    controller = std::make_unique<AnimationController>(
        std::move(manual), [this] { return std::make_unique<FakeFrameSource>(script); },
        AnimationController::Execution::Inline);
    QObject::connect(controller.get(), &AnimationController::frameReady, controller.get(), [this](const QImage& frame) {
      shown.push_back({.timeMs = clock->nowMs(), .id = frameId(frame)});
    });
  }
  void start() const { controller->start(QStringLiteral("fake.gif"), QSize(2, 2)); }
  [[nodiscard]] std::vector<int> ids() const {
    std::vector<int> result;
    result.reserve(shown.size());
    for (const auto& entry : shown) {
      result.push_back(entry.id);
    }
    return result;
  }
  FakeScript script;
  ManualPlaybackClock* clock = nullptr;
  std::unique_ptr<AnimationController> controller;
  std::vector<Shown> shown;
};
}  // namespace

TEST(AnimationController, AutoplaysFrameZeroForItsFullDelayThenAdvances) {
  Harness harness({500, 300});
  harness.start();
  const auto& controller = *harness.controller;
  EXPECT_TRUE(controller.animated());
  EXPECT_TRUE(controller.playing());
  EXPECT_EQ(controller.frameIndex(), 0);
  EXPECT_EQ(controller.frameCount(), 2);
  harness.clock->advance(499);
  EXPECT_EQ(controller.frameIndex(), 0);
  EXPECT_TRUE(harness.shown.empty());
  harness.clock->advance(1);
  ASSERT_EQ(harness.shown.size(), 1U);
  EXPECT_EQ(harness.shown[0].timeMs, 500);
  EXPECT_EQ(controller.frameIndex(), 1);
  harness.clock->advance(299);
  EXPECT_TRUE(controller.playing());
  harness.clock->advance(1);
  // Play-once: the sequence stops on its last frame at that frame's deadline.
  EXPECT_FALSE(controller.playing());
  EXPECT_EQ(controller.frameIndex(), 1);
  EXPECT_EQ(harness.shown.size(), 1U);
  EXPECT_FALSE(harness.clock->armed());
}

TEST(AnimationController, ClampsShortDelaysAndKeepsLongOnes) {
  static_assert(clampFrameDelay(0) == 100);
  static_assert(clampFrameDelay(5) == 100);
  static_assert(clampFrameDelay(10) == 100);
  static_assert(clampFrameDelay(11) == 11);
  static_assert(clampFrameDelay(200) == 200);
  Harness harness({0, 5, 10, 11, 100, 200});
  harness.start();
  harness.clock->advance(2000);
  ASSERT_EQ(harness.shown.size(), 5U);
  const std::array<qint64, 5> expected{100, 200, 300, 311, 411};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(harness.shown.at(i).timeMs, expected.at(i)) << "frame " << i + 1;
    EXPECT_EQ(harness.shown.at(i).id, static_cast<int>(i) + 1);
  }
  EXPECT_FALSE(harness.controller->playing());
}

TEST(AnimationController, PlaysOnceWhenNoLoopIsDeclared) {
  Harness harness({100, 100, 100, 100, 100}, 0);
  harness.start();
  harness.clock->advance(10000);
  EXPECT_EQ(harness.ids(), (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(harness.controller->frameIndex(), 4);
  EXPECT_FALSE(harness.controller->playing());
  EXPECT_EQ(harness.script.rewinds, 0);
}

TEST(AnimationController, InfiniteLoopRewindsAndKeepsPlaying) {
  Harness harness({100, 100, 100}, -1);
  harness.start();
  harness.clock->advance(100 * 3 * 5);
  EXPECT_TRUE(harness.controller->playing());
  EXPECT_GE(harness.script.rewinds, 4);
  ASSERT_GT(harness.shown.size(), 6U);  // beyond twice the frame count
  const auto ids = harness.ids();
  for (size_t i = 0; i < ids.size(); ++i) {
    EXPECT_EQ(ids[i], static_cast<int>((i + 1) % 3));
  }
  // The loop is seamless: one frame every 100 ms, no burst and no gap.
  for (size_t i = 0; i < harness.shown.size(); ++i) {
    EXPECT_EQ(harness.shown[i].timeMs, static_cast<qint64>(100 * (i + 1)));
  }
}

TEST(AnimationController, RepeatCountFollowsQtConvention) {
  // Qt reports N repeats after the first play, so a loop count of 2 shows three plays in total.
  Harness harness({100, 100}, 2);
  harness.start();
  harness.clock->advance(10000);
  EXPECT_EQ(harness.ids(), (std::vector<int>{1, 0, 1, 0, 1}));
  EXPECT_EQ(harness.script.rewinds, 2);
  EXPECT_FALSE(harness.controller->playing());
  EXPECT_EQ(harness.controller->frameIndex(), 1);
}

TEST(AnimationController, ToggleAfterFinishReplaysFromFrameZero) {
  Harness harness({100, 100}, 0);
  harness.start();
  harness.clock->advance(1000);
  ASSERT_FALSE(harness.controller->playing());
  ASSERT_TRUE(harness.controller->canToggle());
  harness.controller->toggle();
  EXPECT_TRUE(harness.controller->playing());
  EXPECT_EQ(harness.controller->frameIndex(), 0);
  harness.clock->advance(100);
  EXPECT_EQ(harness.controller->frameIndex(), 1);
}

TEST(AnimationController, PauseKeepsTheRemainingDelayAndNeverRereads) {
  Harness harness({200, 200, 200}, 0);
  harness.start();
  harness.clock->advance(200);
  ASSERT_EQ(harness.controller->frameIndex(), 1);
  harness.clock->advance(50);
  const int reads = harness.script.reads;
  harness.controller->toggle();
  EXPECT_TRUE(harness.controller->userPaused());
  EXPECT_FALSE(harness.controller->playing());
  EXPECT_FALSE(harness.clock->armed());
  harness.clock->advance(10000);
  EXPECT_EQ(harness.controller->frameIndex(), 1);
  harness.controller->toggle();
  EXPECT_TRUE(harness.controller->playing());
  EXPECT_EQ(harness.script.reads, reads);
  harness.clock->advance(149);
  EXPECT_EQ(harness.controller->frameIndex(), 1);
  harness.clock->advance(1);
  EXPECT_EQ(harness.controller->frameIndex(), 2);
}

TEST(AnimationController, DamagedFrameStopsPlaybackOnTheLastGoodFrame) {
  Harness harness({100, 100, 100, 100});
  harness.script.damagedAt = 2;
  harness.start();
  harness.clock->advance(5000);
  const auto& controller = *harness.controller;
  EXPECT_EQ(controller.failureNotice(), QStringLiteral("Playback stopped: damaged frame"));
  EXPECT_FALSE(controller.playing());
  EXPECT_FALSE(controller.canToggle());
  EXPECT_EQ(controller.frameIndex(), 1);
  const auto before = harness.shown.size();
  harness.controller->toggle();
  EXPECT_FALSE(controller.playing());
  EXPECT_EQ(harness.shown.size(), before);
}

TEST(AnimationController, FrameLimitsMatchTheStaticPath) {
  EXPECT_TRUE(acceptableSize({8000, 4000}));  // exactly 32 million pixels
  EXPECT_FALSE(acceptableSize({8001, 4000}));
  EXPECT_FALSE(acceptableSize({32769, 1}));
  EXPECT_FALSE(acceptableSize({0, 10}));
  EXPECT_FALSE(acceptableImage(QImage()));
  EXPECT_TRUE(acceptableImage(QImage(4, 4, QImage::Format_ARGB32_Premultiplied)));
}

TEST(AnimationController, SingleFrameSourceNeverBecomesAnimated) {
  Harness harness({100});
  harness.start();
  EXPECT_FALSE(harness.controller->animated());
  EXPECT_FALSE(harness.controller->canToggle());
  EXPECT_FALSE(harness.controller->playing());
  EXPECT_TRUE(harness.controller->failureNotice().isEmpty());
  harness.controller->toggle();
  EXPECT_FALSE(harness.controller->userPaused());
}

TEST(AnimationController, StopDiscardsThePlaybackSession) {
  Harness harness({100, 100, 100}, -1);
  harness.start();
  harness.clock->advance(100);
  harness.controller->stop();
  EXPECT_FALSE(harness.controller->animated());
  EXPECT_FALSE(harness.controller->playing());
  EXPECT_EQ(harness.controller->frameCount(), 0);
  const auto shown = harness.shown.size();
  harness.clock->advance(10000);
  EXPECT_EQ(harness.shown.size(), shown);
}

TEST(AnimationController, PauseReasonsAreIndependentAndOrderInsensitive) {
  // Every order of applying and removing the three reasons must end in the same effective state.
  std::array<int, 3> order{0, 1, 2};
  bool more = true;
  while (more) {
    for (int mask = 0; mask < 8; ++mask) {
      Harness harness({100, 100, 100}, -1);
      harness.start();
      auto apply = [&](int reason, bool enabled) {
        if (reason == 0 && enabled != harness.controller->userPaused()) {
          harness.controller->toggle();
        } else if (reason == 1) {
          harness.controller->setSuspendedByModal(enabled);
        } else if (reason == 2) {
          harness.controller->setSuspendedByWindow(enabled);
        }
      };
      for (int reason : order) {
        apply(reason, ((mask >> reason) & 1) != 0);
      }
      const bool expected = mask == 0;
      EXPECT_EQ(harness.controller->playing(), expected) << "mask " << mask;
      EXPECT_EQ(harness.controller->userPaused(), (mask & 1) != 0);
      EXPECT_EQ(harness.clock->armed(), expected);
      // Removing the reasons in the reverse order returns to playing, unless the user paused.
      for (int i = 2; i >= 0; --i) {
        apply(order.at(static_cast<size_t>(i)), false);
      }
      EXPECT_TRUE(harness.controller->playing());
    }
    more = std::ranges::next_permutation(order).found;
  }
}

TEST(AnimationController, ModalCloseDoesNotResumeAUserPause) {
  Harness harness({100, 100, 100}, -1);
  harness.start();
  harness.controller->toggle();
  harness.controller->setSuspendedByModal(true);
  harness.controller->setSuspendedByModal(false);
  EXPECT_FALSE(harness.controller->playing());
  harness.controller->setSuspendedByWindow(true);
  harness.controller->setSuspendedByWindow(false);
  EXPECT_FALSE(harness.controller->playing());
  harness.controller->toggle();
  EXPECT_TRUE(harness.controller->playing());
}

TEST(AnimationController, MemoryDoesNotGrowWithFrameCount) {
  Harness harness(std::vector<int>(100, 100), -1);
  int firstPeak = 0;
  for (int run = 0; run < 10; ++run) {
    harness.shown.clear();
    harness.script.peak = 0;
    harness.start();
    harness.clock->advance(100 * 250);
    ASSERT_GT(harness.shown.size(), 100U);
    // Displayed frame, one look-ahead and the frame in flight: never a list of frames.
    EXPECT_LE(harness.script.peak, 4) << "run " << run;
    if (run == 0) {
      firstPeak = harness.script.peak;
    }
    EXPECT_EQ(harness.script.peak, firstPeak) << "run " << run;
    harness.controller->stop();
    EXPECT_EQ(harness.script.outstanding, 0) << "run " << run;
  }
}

TEST(FrameStreamer, DropsResultsOfAStaleGeneration) {
  FakeScript script;
  script.delaysMs = {100, 100, 100};
  auto current = std::make_shared<std::atomic<quint64>>(1);
  std::vector<StreamEvent::Kind> delivered;
  FrameStreamer streamer([&] { return std::make_unique<FakeFrameSource>(script); }, current,
                         [&](const StreamEvent& event) { delivered.push_back(event.kind); });
  // The session is replaced while frame 1 is being read: its result must never be delivered.
  int reads = 0;
  script.onRead = [&] {
    if (++reads == 2) {
      current->store(2);
    }
  };
  streamer.open(1, QStringLiteral("fake.gif"), QSize(2, 2));
  EXPECT_EQ(delivered, (std::vector<StreamEvent::Kind>{StreamEvent::Kind::Opened}));
  streamer.readNext(1);  // stale command: no work at all
  EXPECT_EQ(script.reads, 2);
  delivered.clear();
  streamer.open(1, QStringLiteral("fake.gif"), QSize(2, 2));  // stale open is skipped
  EXPECT_TRUE(delivered.empty());
}

TEST(AnimationController, ThreadedPlaybackOfARealGifThenStopAndShutdown) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QFile file(dir.filePath("loop.gif"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(gif::bytes("GIF89a",
                        {{.delayCs = 3, .second = false},
                         {.delayCs = 3, .second = true},
                         {.delayCs = 3, .second = false},
                         {.delayCs = 3, .second = true}},
                        0));
  file.close();
  AnimationController controller;
  QSignalSpy frames(&controller, &AnimationController::frameReady);
  controller.start(file.fileName(), QSize(1, 1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return controller.animated(); }, 3000));
  EXPECT_TRUE(QTest::qWaitFor([&] { return frames.count() >= 6; }, 3000));  // looped past four frames
  EXPECT_EQ(controller.frameCount(), 4);
  // Restarting mid-decode and stopping must neither crash nor publish stale frames.
  controller.start(file.fileName(), QSize(1, 1));
  controller.stop();
  const auto count = frames.count();
  QTest::qWait(150);
  EXPECT_EQ(frames.count(), count);
  EXPECT_FALSE(controller.animated());
  controller.start(file.fileName(), QSize(1, 1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return controller.animated(); }, 3000));
  QSignalSpy finished(&controller, &AnimationController::shutdownFinished);
  controller.shutdown();
  EXPECT_TRUE(finished.wait(3000));
}

TEST(AnimationController, TruncatedRealGifReportsDamageAfterTheLastGoodFrame) {
  if (!gif::available()) {
    GTEST_SKIP() << "GIF handler is not installed";
  }
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  auto bytes = gif::bytes(
      "GIF89a", {{.delayCs = 3, .second = false}, {.delayCs = 3, .second = true}, {.delayCs = 3, .second = false}}, 0);
  // Cut inside the third frame's pixel data: the header still declares it, but it cannot be decoded.
  bytes.chop(6);
  QFile file(dir.filePath("truncated.gif"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(bytes);
  file.close();
  AnimationController controller;
  controller.start(file.fileName(), QSize(1, 1));
  ASSERT_TRUE(QTest::qWaitFor([&] { return !controller.failureNotice().isEmpty(); }, 3000))
      << "animated=" << controller.animated() << " count=" << controller.frameCount();
  EXPECT_EQ(controller.failureNotice(), QStringLiteral("Playback stopped: damaged frame"));
  EXPECT_FALSE(controller.canToggle());
  EXPECT_FALSE(controller.playing());
  controller.stop();
  EXPECT_TRUE(controller.failureNotice().isEmpty());
}
