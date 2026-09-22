// Characterisation of the Qt GIF handler behaviours that the animation design relies on (DESIGN.md
// sections 3.5, 3.7 and 9). These tests pin what Qt does today; a failure after a Qt upgrade means the
// assumptions behind FrameSource, LoopPolicy and the rewind path must be revisited, not that the test is wrong.
#include "gif_fixture.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {
bool gifAvailable() { return gif::available(); }

constexpr auto kNoGifPlugin = "GIF handler is not installed";

int readAll(QImageReader& reader) {
  int count = 0;
  while (!reader.read().isNull()) {
    ++count;
  }
  return count;
}
}  // namespace

TEST(FrameSource, CountsAndLoopConvention) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  // Qt reports loopCount() as repeats after the first play: NETSCAPE 0 (forever) -> -1, absent -> 0, N -> N.
  struct Case {
    int field;
    int expectedLoop;
  };
  for (const auto& [field, expectedLoop] :
       {Case{.field = -1, .expectedLoop = 0}, Case{.field = 0, .expectedLoop = -1}, Case{.field = 1, .expectedLoop = 1},
        Case{.field = 2, .expectedLoop = 2}, Case{.field = 7, .expectedLoop = 7}}) {
    auto bytes = gif::bytes(
        "GIF89a", {{.delayCs = 10, .second = false}, {.delayCs = 10, .second = true}, {.delayCs = 10, .second = false}},
        field);
    QBuffer buffer(&bytes);
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
    QImageReader reader(&buffer);
    ASSERT_TRUE(reader.canRead());
    EXPECT_EQ(reader.imageCount(), 3) << "loop field " << field;
    EXPECT_EQ(reader.loopCount(), expectedLoop) << "loop field " << field;
    EXPECT_EQ(readAll(reader), 3) << "loop field " << field;
  }
}

TEST(FrameSource, DelayBelongsToTheFrameJustRead) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  auto bytes = gif::bytes("GIF89a", {{.delayCs = 20, .second = false},
                                     {.delayCs = 30, .second = true},
                                     {.delayCs = 0, .second = false},
                                     {.delayCs = 5, .second = true}});
  QBuffer buffer(&bytes);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
  QImageReader reader(&buffer);
  // Before the first read the value is a default, not frame 0's delay.
  EXPECT_EQ(reader.nextImageDelay(), 100);
  // Centiseconds become milliseconds; only a zero delay is defaulted (to 100 ms) by Qt. A 5 cs delay arrives
  // raw as 50 ms, so clampFrameDelay() in the controller is what enforces the 100 ms floor for 1..10 cs.
  QList<int> delays;
  while (!reader.read().isNull()) {
    delays.append(reader.nextImageDelay());
  }
  ASSERT_EQ(delays.size(), 4);
  EXPECT_EQ(delays[0], 200);
  EXPECT_EQ(delays[1], 300);
  EXPECT_EQ(delays[2], 100);
  EXPECT_EQ(delays[3], 50);
}

TEST(FrameSource, EndOfSequenceIsANullReadAndJumpToImageIsRefused) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QFile file(dir.filePath("three.gif"));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(gif::bytes(
      "GIF89a", {{.delayCs = 20, .second = false}, {.delayCs = 30, .second = true}, {.delayCs = 40, .second = false}}));
  file.close();
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));

  {
    QImageReader reader(&file);
    ASSERT_EQ(readAll(reader), 3);
    EXPECT_TRUE(reader.read().isNull());
    EXPECT_EQ(reader.error(), QImageReader::InvalidDataError);
    // DESIGN 9 assumed jumpToImage(0) rewinds; this Qt refuses it, both mid-sequence and at the end, so the
    // rewind path must reopen the reader on the same QFile.
    EXPECT_FALSE(reader.jumpToImage(0));
  }
  ASSERT_TRUE(file.seek(0));
  QImageReader again(&file);
  ASSERT_TRUE(again.canRead());
  const auto first = again.read();
  ASSERT_FALSE(first.isNull());
  EXPECT_EQ(again.nextImageDelay(), 200);
  EXPECT_EQ(again.imageCount(), 3);
  EXPECT_EQ(readAll(again), 2);
}

TEST(FrameSource, SizeCanUnderReportTheLogicalScreen) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  // A 1x1 frame on a 4096x2048 screen: size() reports 1x1 but read() returns the composed 4096x2048 canvas,
  // so a size() pre-check cannot bound GIF memory; the post-read check and the allocation limit do.
  auto bytes =
      gif::bytes("GIF89a", {{.delayCs = 10, .second = false}, {.delayCs = 10, .second = true}}, -1, 4096, 2048);
  QBuffer buffer(&bytes);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
  QImageReader reader(&buffer);
  EXPECT_EQ(reader.size(), QSize(1, 1));
  const auto image = reader.read();
  EXPECT_EQ(image.size(), QSize(4096, 2048));
}

TEST(FrameSource, AllocationLimitAppliesToEachReadNotTheSequence) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  // Application startup also sets the environment override, which wins over setAllocationLimit(), so set both.
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
  const auto setLimit = [](int mebibytes) {
    qputenv("QT_IMAGEIO_MAXALLOC", QByteArray::number(mebibytes));
    QImageReader::setAllocationLimit(mebibytes);
  };
  // 2048x2048 ARGB32 is 16 MiB per composed frame.
  auto bytes = gif::bytes(
      "GIF89a", {{.delayCs = 10, .second = false}, {.delayCs = 10, .second = true}, {.delayCs = 10, .second = false}},
      -1, 2048, 2048);
  // Enforcement itself is not asserted here: once any reader has run in the process, this Qt build's GIF handler
  // no longer picks up a lower limit, so a rejection check would depend on test order.
  {
    // Three 16 MiB frames read through a 16 MiB limit: nothing accumulates across reads.
    setLimit(16);
    QBuffer buffer(&bytes);
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
    QImageReader reader(&buffer);
    EXPECT_EQ(readAll(reader), 3);
  }
}

TEST(FrameSource, Gif87aIsSupportedAndSingleFrameReportsOneImage) {
  if (!gifAvailable()) {
    GTEST_SKIP() << kNoGifPlugin;
  }
  for (const auto* version : {"GIF87a", "GIF89a"}) {
    auto bytes = gif::bytes(version, {{.delayCs = 10, .second = false}});
    QBuffer buffer(&bytes);
    ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
    QImageReader reader(&buffer);
    ASSERT_TRUE(reader.canRead()) << version;
    EXPECT_EQ(reader.imageCount(), 1) << version;
    EXPECT_EQ(readAll(reader), 1) << version;
  }
}
